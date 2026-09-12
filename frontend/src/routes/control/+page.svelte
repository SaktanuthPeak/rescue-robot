<script lang="ts">
	import { resolve } from '$app/paths';
	import {
		AlertTriangle,
		ArrowDown,
		ArrowDownLeft,
		ArrowDownRight,
		ArrowLeft,
		ArrowRight,
		ArrowUp,
		ArrowUpLeft,
		Battery,
		CircleStop,
		Droplets,
		Link2,
		Radio,
		RefreshCw,
		Wifi,
		WifiOff
	} from 'lucide-svelte';

	import AppContainer from '$lib/components/app-container.svelte';
	import { CameraCard } from '$lib/features/camera';
	import { sendRobotCommand, getRobotStatus } from '$lib/features/robot/api';
	import CompactFieldMonitor from '$lib/features/telemetry/components/compact-field-monitor.svelte';
	import {
		INITIAL_ROBOT_STATUS,
		type RobotCommand,
		type RobotStatus
	} from '$lib/features/robot/schema';

	let status = $state<RobotStatus>(INITIAL_ROBOT_STATUS);
	let loading = $state(true);
	let refreshing = $state(false);
	let error = $state<string | null>(null);
	let commandError = $state<string | null>(null);
	let activeControl = $state<string | null>(null);
	let commandInFlight = $state(false);
	let heldCommand = $state<RobotCommand | null>(null);
	let holdTimer: ReturnType<typeof setInterval> | null = null;

	const armActionCode = {
		pumpOn: 11,
		pumpOff: 12,
		headUp: 13,
		headDown: 14
	} as const;

	const isOnline = $derived(status.state === 'streaming');
	const statusAge = $derived(
		status.last_frame_age_ms > 0 ? `${status.last_frame_age_ms} ms ago` : 'waiting'
	);
	const batteryLabel = $derived(
		status.battery_millivolts > 0 ? `${status.battery_volts.toFixed(2)} V` : 'n/a'
	);
	const linkLabel = $derived(
		status.state === 'streaming'
			? 'Receiver online'
			: status.state === 'connecting'
				? 'Connecting to receiver'
				: status.state === 'disabled'
					? 'Serial disabled'
					: 'Receiver offline'
	);

	$effect(() => {
		let stopped = false;
		const poll = async () => {
			try {
				const next = await getRobotStatus();
				if (!stopped) {
					status = next;
					error = null;
					loading = false;
				}
			} catch (cause) {
				if (!stopped) {
					error = cause instanceof Error ? cause.message : 'Backend unavailable';
					loading = false;
				}
			}
		};

		void poll();
		const timer = setInterval(() => void poll(), 500);
		return () => {
			stopped = true;
			clearInterval(timer);
		};
	});

	async function refresh() {
		refreshing = true;
		try {
			status = await getRobotStatus();
			error = null;
		} catch (cause) {
			error = cause instanceof Error ? cause.message : 'Backend unavailable';
		} finally {
			refreshing = false;
		}
	}

	async function send(command: RobotCommand) {
		commandError = null;
		commandInFlight = true;
		try {
			await sendRobotCommand(command);
		} catch (cause) {
			commandError = cause instanceof Error ? cause.message : 'Command failed';
		} finally {
			commandInFlight = false;
		}
	}

	async function press(channel: 'motor' | 'arm', code: number, key: string) {
		if (commandInFlight) return;
		activeControl = key;
		heldCommand = { channel, code };
		await send(heldCommand);
		if (activeControl !== key) return;
		if (holdTimer === null) {
			holdTimer = setInterval(() => {
				if (heldCommand) void send(heldCommand);
			}, 150);
		}
	}

	async function release(channel: 'motor' | 'arm') {
		if (holdTimer !== null) {
			clearInterval(holdTimer);
			holdTimer = null;
		}
		heldCommand = null;
		activeControl = null;
		await send({ channel, code: 0 });
	}

	async function emergencyStop() {
		if (holdTimer !== null) {
			clearInterval(holdTimer);
			holdTimer = null;
		}
		heldCommand = null;
		activeControl = null;
		await send({ channel: 'all', code: 0 });
	}

	function buttonClass(key: string) {
		return `control-button ${activeControl === key ? 'is-active' : ''}`;
	}
</script>

<svelte:head>
	<title>Durian Bot | Control</title>
	<meta
		name="description"
		content="Durian Bot field control panel for drive, actuator and IR safety commands."
	/>
</svelte:head>

<AppContainer>
	<div class="control-page">
		<header class="page-header">
			<div>
				<p class="eyebrow">DURIAN BOT / CONTROL DECK</p>
				<h1>Move through the row.</h1>
				<p class="subhead">ควบคุมผ่าน Raspberry Pi และตรวจสิ่งกีดขวางจาก IR sensor</p>
			</div>
			<div class="header-actions">
				<a class="back-link" href={resolve('/monitor')}>เปิด field monitor <Link2 size={14} /></a>
				<button
					class="refresh-button"
					type="button"
					onclick={refresh}
					disabled={refreshing}
					aria-label="Refresh receiver status"
				>
					<RefreshCw size={15} class={refreshing ? 'spin' : ''} />
					{refreshing ? 'กำลังเช็ค' : 'เช็คสถานะ'}
				</button>
				<button
					class="emergency-button header-emergency"
					type="button"
					onclick={emergencyStop}
					disabled={commandInFlight}><AlertTriangle size={15} /> หยุดทั้งหมด</button
				>
			</div>
		</header>

		<section class="status-rail" aria-label="Receiver status">
			<div class={`connection-state ${isOnline ? 'online' : 'offline'}`}>
				{#if isOnline}<Wifi size={17} />{:else}<WifiOff size={17} />{/if}
				<div>
					<strong>{linkLabel}</strong>
					<span>{status.port} · {status.baudrate} baud</span>
				</div>
			</div>
			<div class="status-stat battery-stat">
				<span>battery</span><strong><Battery size={15} /> {batteryLabel}</strong><small
					>ADC {status.battery_adc}</small
				>
			</div>
			<div class="status-stat"><span>last frame</span><strong>{statusAge}</strong></div>
			<div class="status-stat"><span>sequence</span><strong>{status.sequence}</strong></div>
			<div class="status-stat">
				<span>parse errors</span><strong class={status.parse_errors ? 'warn' : ''}
					>{status.parse_errors}</strong
				>
			</div>
		</section>

		{#if error}
			<div class="notice error" role="alert"><AlertTriangle size={17} /> {error}</div>
		{/if}
		{#if commandError}
			<div class="notice error" role="alert"><AlertTriangle size={17} /> {commandError}</div>
		{/if}
		{#if loading}
			<div class="notice"><Radio size={17} /> กำลังอ่านสถานะ receiver...</div>
		{/if}

		<div class="workspace-layout">
			<section class="overview-layout" aria-label="Live field overview">
				<CompactFieldMonitor />
				<CameraCard compact />
			</section>

			<div class="control-layout">
				<section class="panel driving-panel">
					<div class="panel-heading">
						<div>
							<span class="section-index">01</span>
							<h2>Drive</h2>
						</div>
						<span class="panel-meta">hold to move, release to stop</span>
					</div>
					<div class="device-readout">
						<div><span>motor state</span><strong>{status.motor_status}</strong></div>
						<div class:good={status.motor_can_alive}>
							<span>CAN heartbeat</span><strong
								>{status.motor_can_alive ? 'LIVE' : 'TIMEOUT'}</strong
							>
						</div>
					</div>
					<div class="d-pad" aria-label="Motor directional controls">
						<button
							class={buttonClass('motor-forward-left')}
							type="button"
							aria-label="Forward left"
							onpointerdown={() => press('motor', 5, 'motor-forward-left')}
							onpointerup={() => release('motor')}
							onpointercancel={() => release('motor')}
							onpointerleave={() => activeControl === 'motor-forward-left' && release('motor')}
							disabled={commandInFlight}><ArrowUpLeft /></button
						>
						<button
							class={buttonClass('motor-forward')}
							type="button"
							aria-label="Forward"
							onpointerdown={() => press('motor', 1, 'motor-forward')}
							onpointerup={() => release('motor')}
							onpointercancel={() => release('motor')}
							onpointerleave={() => activeControl === 'motor-forward' && release('motor')}
							disabled={commandInFlight}><ArrowUp /></button
						>
						<span class="d-pad-spacer" aria-hidden="true"></span>
						<button
							class={buttonClass('motor-left')}
							type="button"
							aria-label="Left"
							onpointerdown={() => press('motor', 3, 'motor-left')}
							onpointerup={() => release('motor')}
							onpointercancel={() => release('motor')}
							onpointerleave={() => activeControl === 'motor-left' && release('motor')}
							disabled={commandInFlight}><ArrowLeft /></button
						>
						<button
							class="stop-button"
							type="button"
							aria-label="Stop motor"
							onclick={() => release('motor')}><CircleStop /></button
						>
						<button
							class={buttonClass('motor-right')}
							type="button"
							aria-label="Right"
							onpointerdown={() => press('motor', 4, 'motor-right')}
							onpointerup={() => release('motor')}
							onpointercancel={() => release('motor')}
							onpointerleave={() => activeControl === 'motor-right' && release('motor')}
							disabled={commandInFlight}><ArrowRight /></button
						>
						<button
							class={buttonClass('motor-backward-left')}
							type="button"
							aria-label="Backward left"
							onpointerdown={() => press('motor', 7, 'motor-backward-left')}
							onpointerup={() => release('motor')}
							onpointercancel={() => release('motor')}
							onpointerleave={() => activeControl === 'motor-backward-left' && release('motor')}
							disabled={commandInFlight}><ArrowDownLeft /></button
						>
						<button
							class={buttonClass('motor-backward')}
							type="button"
							aria-label="Backward"
							onpointerdown={() => press('motor', 2, 'motor-backward')}
							onpointerup={() => release('motor')}
							onpointercancel={() => release('motor')}
							onpointerleave={() => activeControl === 'motor-backward' && release('motor')}
							disabled={commandInFlight}><ArrowDown /></button
						>
						<button
							class={buttonClass('motor-backward-right')}
							type="button"
							aria-label="Backward right"
							onpointerdown={() => press('motor', 8, 'motor-backward-right')}
							onpointerup={() => release('motor')}
							onpointercancel={() => release('motor')}
							onpointerleave={() => activeControl === 'motor-backward-right' && release('motor')}
							disabled={commandInFlight}><ArrowDownRight /></button
						>
					</div>
					<p class="control-hint">
						ปุ่มทิศทางจะส่งคำสั่งค้างจนกว่าจะปล่อย เพื่อให้หยุดเมื่อผู้ควบคุมเลิกกด
					</p>
				</section>

				<section class="panel arm-panel">
					<div class="panel-heading">
						<div>
							<span class="section-index">02</span>
							<h2>Actuator mount</h2>
						</div>
						<span class="panel-meta">arm, pump &amp; head controls</span>
					</div>
					<div class="device-readout">
						<div><span>arm state</span><strong>{status.arm_status}</strong></div>
						<div class:good={status.arm_can_alive}>
							<span>CAN heartbeat</span><strong>{status.arm_can_alive ? 'LIVE' : 'TIMEOUT'}</strong>
						</div>
					</div>
					<div class="arm-controls">
						<div class="arm-row">
							<button
								class={buttonClass('arm-left')}
								type="button"
								onpointerdown={() => press('arm', 3, 'arm-left')}
								onpointerup={() => release('arm')}
								onpointercancel={() => release('arm')}
								onpointerleave={() => activeControl === 'arm-left' && release('arm')}
								disabled={commandInFlight}><ArrowLeft /><span>ซ้าย</span></button
							>
							<button class="stop-button arm-stop" type="button" onclick={() => release('arm')}
								><CircleStop /><span>หยุด</span></button
							>
							<button
								class={buttonClass('arm-right')}
								type="button"
								onpointerdown={() => press('arm', 4, 'arm-right')}
								onpointerup={() => release('arm')}
								onpointercancel={() => release('arm')}
								onpointerleave={() => activeControl === 'arm-right' && release('arm')}
								disabled={commandInFlight}><ArrowRight /><span>ขวา</span></button
							>
						</div>
						<button
							class={buttonClass('arm-backward')}
							type="button"
							onpointerdown={() => press('arm', 2, 'arm-backward')}
							onpointerup={() => release('arm')}
							onpointercancel={() => release('arm')}
							onpointerleave={() => activeControl === 'arm-backward' && release('arm')}
							disabled={commandInFlight}><ArrowDown /><span>ลดแขนลง</span></button
						>
					</div>
					<div class="actuator-controls">
						<button
							type="button"
							onclick={() => send({ channel: 'arm', code: armActionCode.pumpOn })}
							disabled={commandInFlight}><Droplets size={18} /> เปิดปั๊ม</button
						>
						<button
							type="button"
							onclick={() => send({ channel: 'arm', code: armActionCode.pumpOff })}
							disabled={commandInFlight}><Droplets size={18} /> ปิดปั๊ม</button
						>
						<button
							class={buttonClass('head-up')}
							type="button"
							onpointerdown={() => press('arm', armActionCode.headUp, 'head-up')}
							onpointerup={() => release('arm')}
							onpointercancel={() => release('arm')}
							onpointerleave={() => activeControl === 'head-up' && release('arm')}
							disabled={commandInFlight}><ArrowUp size={18} /> หัวขึ้น</button
						>
						<button
							class={buttonClass('head-down')}
							type="button"
							onpointerdown={() => press('arm', armActionCode.headDown, 'head-down')}
							onpointerup={() => release('arm')}
							onpointercancel={() => release('arm')}
							onpointerleave={() => activeControl === 'head-down' && release('arm')}
							disabled={commandInFlight}><ArrowDown size={18} /> หัวลง</button
						>
					</div>
				</section>
			</div>
		</div>
	</div>
</AppContainer>

<style>
	:global(body) {
		background: var(--background);
	}
	:global(.dark) {
		--background: oklch(0.16 0.018 125);
		--card: oklch(0.205 0.022 125);
	}
	:global(button) {
		touch-action: manipulation;
	}
	.control-page {
		display: flex;
		flex-direction: column;
		gap: 0.65rem;
	}
	.control-page > .page-header,
	.control-page > .status-rail,
	.control-page > .notice,
	.control-page > .workspace-layout {
		margin-bottom: 0;
	}
	.page-header {
		display: flex;
		justify-content: space-between;
		align-items: end;
		gap: 1rem;
		margin: 0 auto 0.85rem;
		max-width: 1180px;
	}
	.eyebrow,
	.panel-meta,
	.device-readout span,
	.status-stat span {
		color: var(--muted-foreground);
		font-size: 0.68rem;
		letter-spacing: 0.12em;
		text-transform: uppercase;
	}
	.eyebrow {
		color: var(--primary);
		margin: 0 0 0.45rem;
		font-weight: 700;
	}
	h1 {
		color: var(--foreground);
		font-size: clamp(1.8rem, 4vw, 3.1rem);
		letter-spacing: -0.06em;
		line-height: 0.95;
		margin: 0;
	}
	.subhead {
		color: var(--muted-foreground);
		margin: 0.45rem 0 0;
		font-size: 0.82rem;
	}
	.header-actions {
		display: flex;
		align-items: center;
		gap: 0.6rem;
		flex-wrap: wrap;
		justify-content: flex-end;
	}
	.back-link,
	.refresh-button {
		color: var(--foreground);
		border: 1px solid var(--border);
		background: var(--card);
		border-radius: 0.35rem;
		display: inline-flex;
		align-items: center;
		gap: 0.45rem;
		min-height: 2.15rem;
		padding: 0.45rem 0.65rem;
		font-size: 0.7rem;
		transition: 0.2s ease;
	}
	.back-link:hover,
	.refresh-button:hover {
		border-color: var(--primary);
		color: var(--foreground);
	}
	.refresh-button:disabled {
		opacity: 0.55;
	}
	.spin {
		animation: spin 0.8s linear infinite;
	}
	.status-rail,
	.panel {
		border: 1px solid var(--border);
		background: var(--card);
		box-shadow: 0 18px 50px rgb(0 0 0 / 20%);
	}
	.status-rail {
		max-width: 1180px;
		margin: 0 auto 0.7rem;
		padding: 0.5rem;
		display: grid;
		grid-template-columns: minmax(220px, 1.7fr) repeat(4, 1fr);
		align-items: center;
		gap: 0.35rem;
		border-radius: 0.35rem;
	}
	.connection-state {
		display: flex;
		gap: 0.65rem;
		align-items: center;
		padding: 0.2rem 0.45rem;
		color: var(--muted-foreground);
	}
	.connection-state.online {
		color: var(--primary);
	}
	.connection-state.offline {
		color: var(--muted-foreground);
	}
	.connection-state strong {
		color: var(--foreground);
		display: block;
		font-size: 0.84rem;
	}
	.connection-state span {
		display: block;
		margin-top: 0.15rem;
		font-size: 0.72rem;
		color: var(--muted-foreground);
		font-variant-numeric: tabular-nums;
	}
	.status-stat {
		border-left: 1px solid var(--border);
		padding: 0.2rem 0.55rem;
	}
	.status-stat strong {
		color: var(--foreground);
		display: block;
		margin-top: 0.2rem;
		font-family: ui-monospace, SFMono-Regular, monospace;
		font-size: 0.85rem;
		font-variant-numeric: tabular-nums;
	}
	.status-stat strong :global(svg) {
		display: inline;
		vertical-align: -2px;
		color: var(--primary);
	}
	.status-stat small {
		display: block;
		color: var(--muted-foreground);
		font-size: 0.64rem;
		margin-top: 0.2rem;
	}
	.battery-stat strong {
		color: var(--primary);
	}
	.status-stat .warn {
		color: var(--flame-3);
	}
	.notice {
		max-width: 1180px;
		margin: 0 auto 0.7rem;
		border: 1px solid var(--border);
		border-radius: 0.35rem;
		color: var(--muted-foreground);
		display: flex;
		align-items: center;
		gap: 0.5rem;
		padding: 0.55rem 0.7rem;
		font-size: 0.76rem;
	}
	.notice.error {
		color: var(--destructive);
		border-color: color-mix(in oklab, var(--destructive), transparent 45%);
		background: color-mix(in oklab, var(--destructive), transparent 88%);
	}
	/* One operator workspace: drive beside camera, IR beside the arm. */
	.workspace-layout {
		width: min(100%, 1180px);
		margin: 0 auto;
		display: grid;
		grid-template-columns: minmax(0, 1.12fr) minmax(300px, 0.88fr);
		grid-template-areas:
			'drive camera'
			'ir arm';
		align-items: start;
		gap: 0.9rem;
	}
	.workspace-layout > .overview-layout,
	.workspace-layout > .control-layout {
		display: contents;
	}
	.workspace-layout .driving-panel {
		grid-area: drive;
	}
	.workspace-layout .arm-panel {
		grid-area: arm;
	}
	.workspace-layout .overview-layout :global(.compact-monitor) {
		grid-area: ir;
		width: 100%;
		min-width: 0;
	}
	.workspace-layout .overview-layout :global(.camera-card) {
		grid-area: camera;
		width: 100%;
		min-width: 0;
	}
	.control-layout,
	.overview-layout {
		margin: 0;
	}
	.d-pad-spacer {
		min-height: 3.15rem;
	}
	.panel {
		border-radius: 0.35rem;
		padding: clamp(0.75rem, 1.4vw, 1rem);
		min-height: 0;
	}
	.panel-heading {
		display: flex;
		justify-content: space-between;
		gap: 1rem;
		align-items: start;
		border-bottom: 1px solid var(--border);
		padding-bottom: 0.6rem;
	}
	.panel-heading > div {
		display: flex;
		align-items: baseline;
		gap: 0.6rem;
	}
	.section-index {
		color: var(--primary);
		font:
			700 0.72rem ui-monospace,
			monospace;
	}
	h2 {
		color: var(--foreground);
		margin: 0;
		font-size: 1.1rem;
		letter-spacing: -0.02em;
	}
	.panel-meta {
		white-space: nowrap;
		letter-spacing: 0.06em;
		font-size: 0.62rem;
	}
	.device-readout {
		display: flex;
		justify-content: space-between;
		padding: 0.65rem 0 0.7rem;
	}
	.device-readout strong {
		color: var(--flame-2);
		display: block;
		margin-top: 0.25rem;
		font:
			700 0.86rem ui-monospace,
			monospace;
		letter-spacing: 0.02em;
	}
	.device-readout .good strong {
		color: var(--primary);
	}
	.d-pad {
		width: min(100%, 320px);
		margin: 0.1rem auto 0.65rem;
		display: grid;
		grid-template-columns: repeat(3, 1fr);
		gap: 0.3rem;
	}
	.control-button,
	.stop-button {
		min-height: 3.15rem;
		border: 1px solid var(--border);
		border-radius: 0.35rem;
		background: var(--secondary);
		color: var(--foreground);
		display: flex;
		align-items: center;
		justify-content: center;
		transition:
			transform 0.15s ease,
			background 0.2s ease,
			border-color 0.2s ease,
			color 0.2s ease;
	}
	.control-button:hover {
		background: var(--accent);
		border-color: var(--primary);
		color: var(--foreground);
	}
	.control-button:active,
	.control-button.is-active {
		transform: translateY(2px) scale(0.98);
		background: color-mix(in oklab, var(--primary), var(--card) 55%);
		border-color: var(--primary);
		color: var(--primary-foreground);
	}
	.control-button :global(svg) {
		width: 1.15rem;
		height: 1.15rem;
	}
	.stop-button {
		color: var(--destructive);
		border-color: color-mix(in oklab, var(--destructive), transparent 45%);
		background: color-mix(in oklab, var(--destructive), transparent 88%);
	}
	.stop-button:hover {
		background: color-mix(in oklab, var(--destructive), transparent 75%);
		border-color: var(--destructive);
	}
	.control-hint {
		text-align: center;
		color: var(--muted-foreground);
		font-size: 0.65rem;
		line-height: 1.45;
		max-width: 33rem;
		margin: 0 auto;
	}
	.arm-controls {
		width: min(100%, 360px);
		margin: 0.2rem auto 0.7rem;
		display: grid;
		gap: 0.3rem;
	}
	.arm-controls button {
		min-height: 2.55rem;
	}
	.arm-controls span {
		font-size: 0.78rem;
	}
	.arm-row {
		display: grid;
		grid-template-columns: 1fr 1fr 1fr;
		gap: 0.3rem;
	}
	.arm-row button {
		display: flex;
		align-items: center;
		justify-content: center;
		gap: 0.35rem;
		flex-direction: column;
	}
	.arm-stop {
		min-height: 2.95rem !important;
	}
	.actuator-controls {
		border-top: 1px solid var(--border);
		padding-top: 0.7rem;
		display: grid;
		grid-template-columns: 1fr 1fr;
		gap: 0.3rem;
	}
	.actuator-controls button {
		border: 1px solid var(--border);
		border-radius: 0.35rem;
		background: var(--secondary);
		color: var(--foreground);
		min-height: 2.15rem;
		display: flex;
		align-items: center;
		justify-content: center;
		gap: 0.4rem;
		font-size: 0.76rem;
		transition: 0.2s ease;
	}
	.actuator-controls button:hover {
		color: var(--primary);
		border-color: var(--primary);
	}
	.emergency-button {
		background: var(--destructive);
		border: 1px solid var(--destructive);
		color: var(--primary-foreground);
		padding: 0.8rem 1rem;
		border-radius: 0.35rem;
		font-size: 0.75rem;
		font-weight: 700;
		letter-spacing: 0.03em;
		display: inline-flex;
		align-items: center;
		gap: 0.45rem;
		transition: 0.2s ease;
	}
	.emergency-button:hover {
		background: color-mix(in oklab, var(--destructive), white 12%);
		transform: translateY(-1px);
	}
	.emergency-button:disabled {
		opacity: 0.55;
	}
	.header-emergency {
		min-height: 2.15rem;
		padding: 0.45rem 0.7rem;
		font-size: 0.7rem;
	}
	@keyframes spin {
		to {
			transform: rotate(360deg);
		}
	}
	@media (max-width: 760px) {
		.page-header {
			align-items: start;
			flex-direction: column;
		}
		.header-actions {
			justify-content: flex-start;
		}
		.status-rail {
			grid-template-columns: 1fr 1fr;
		}
		.connection-state {
			grid-column: 1 / -1;
			border-bottom: 1px solid #302f2a;
			padding-bottom: 0.7rem;
		}
		.status-stat:nth-child(3) {
			border-left: 0;
		}
		.workspace-layout {
			grid-template-columns: 1fr;
			grid-template-areas:
				'drive'
				'camera'
				'ir'
				'arm';
		}
		.panel {
			min-height: auto;
		}
		.emergency-button {
			justify-content: center;
		}
	}
</style>
