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
		ArrowUpRight,
		Battery,
		CircleStop,
		Grip,
		Link2,
		Radio,
		RefreshCw,
		Wifi,
		WifiOff
	} from 'lucide-svelte';

	import AppContainer from '$lib/components/app-container.svelte';
	import { sendRobotCommand, getRobotStatus } from '$lib/features/robot/api';
	import MecanumDrivePreview from '$lib/features/robot/ui/mecanum-drive-preview.svelte';
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
	const motorCommandPreviewActive = $derived(
		activeControl?.startsWith('motor-') === true && heldCommand?.channel === 'motor'
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

	<main class="control-layout">
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
					<span>CAN heartbeat</span><strong>{status.motor_can_alive ? 'LIVE' : 'TIMEOUT'}</strong>
				</div>
			</div>
			<MecanumDrivePreview
				motorCode={motorCommandPreviewActive
					? (heldCommand?.code ?? status.motor_code)
					: status.motor_code}
				active={motorCommandPreviewActive || (isOnline && status.motor_can_alive)}
				stale={!isOnline && !motorCommandPreviewActive}
			/>

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
				<button
					class={buttonClass('motor-forward-right')}
					type="button"
					aria-label="Forward right"
					onpointerdown={() => press('motor', 6, 'motor-forward-right')}
					onpointerup={() => release('motor')}
					onpointercancel={() => release('motor')}
					onpointerleave={() => activeControl === 'motor-forward-right' && release('motor')}
					disabled={commandInFlight}><ArrowUpRight /></button
				>
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
				<span class="panel-meta">nozzle mount channel</span>
			</div>
			<div class="device-readout">
				<div><span>arm state</span><strong>{status.arm_status}</strong></div>
				<div class:good={status.arm_can_alive}>
					<span>CAN heartbeat</span><strong>{status.arm_can_alive ? 'LIVE' : 'TIMEOUT'}</strong>
				</div>
			</div>
			<div class="arm-controls">
				<button
					class={buttonClass('arm-forward')}
					type="button"
					onpointerdown={() => press('arm', 1, 'arm-forward')}
					onpointerup={() => release('arm')}
					onpointercancel={() => release('arm')}
					onpointerleave={() => activeControl === 'arm-forward' && release('arm')}
					disabled={commandInFlight}><ArrowUp /><span>ยกแขนขึ้น</span></button
				>
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
			<div class="gripper-controls">
				<button
					type="button"
					onclick={() => send({ channel: 'arm', code: 9 })}
					disabled={commandInFlight}><Grip size={18} /> เปิด nozzle mount</button
				>
				<button
					type="button"
					onclick={() => send({ channel: 'arm', code: 10 })}
					disabled={commandInFlight}><Grip size={18} /> ปิด nozzle mount</button
				>
			</div>
		</section>
	</main>

	<section class="bottom-bar">
		<div><span>last command</span><strong>{status.last_command ?? 'none'}</strong></div>
		<button
			class="emergency-button"
			type="button"
			onclick={emergencyStop}
			disabled={commandInFlight}><AlertTriangle size={17} /> EMERGENCY STOP: หยุดทั้งหมด</button
		>
	</section>
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
	.page-header {
		display: flex;
		justify-content: space-between;
		align-items: end;
		gap: 1rem;
		margin: 0 auto 1.3rem;
		max-width: 1180px;
	}
	.eyebrow,
	.panel-meta,
	.device-readout span,
	.status-stat span,
	.bottom-bar span {
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
		font-size: clamp(2rem, 5vw, 3.7rem);
		letter-spacing: -0.06em;
		line-height: 0.95;
		margin: 0;
	}
	.subhead {
		color: var(--muted-foreground);
		margin: 0.65rem 0 0;
		font-size: 0.9rem;
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
		min-height: 2.35rem;
		padding: 0.55rem 0.75rem;
		font-size: 0.78rem;
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
	.panel,
	.bottom-bar {
		border: 1px solid var(--border);
		background: var(--card);
		box-shadow: 0 18px 50px rgb(0 0 0 / 20%);
	}
	.status-rail {
		max-width: 1180px;
		margin: 0 auto 1rem;
		padding: 0.7rem;
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
		padding: 0.35rem 0.55rem;
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
		padding: 0.25rem 0.75rem;
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
		margin: 0 auto 1rem;
		border: 1px solid var(--border);
		border-radius: 0.35rem;
		color: var(--muted-foreground);
		display: flex;
		align-items: center;
		gap: 0.5rem;
		padding: 0.7rem 0.8rem;
		font-size: 0.82rem;
	}
	.notice.error {
		color: var(--destructive);
		border-color: color-mix(in oklab, var(--destructive), transparent 45%);
		background: color-mix(in oklab, var(--destructive), transparent 88%);
	}
	.control-layout {
		max-width: 1180px;
		margin: 0 auto;
		display: grid;
		grid-template-columns: 1.08fr 0.92fr;
		gap: 1rem;
	}
	.panel {
		border-radius: 0.35rem;
		padding: clamp(1rem, 2vw, 1.45rem);
		min-height: 430px;
	}
	.panel-heading {
		display: flex;
		justify-content: space-between;
		gap: 1rem;
		align-items: start;
		border-bottom: 1px solid var(--border);
		padding-bottom: 0.85rem;
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
		padding: 1rem 0 1.15rem;
	}
	.device-readout strong {
		color: var(--flame-2);
		display: block;
		margin-top: 0.25rem;
		font:
			700 1rem ui-monospace,
			monospace;
		letter-spacing: 0.02em;
	}
	.device-readout .good strong {
		color: var(--primary);
	}
	.d-pad {
		width: min(100%, 360px);
		margin: 0.1rem auto 1rem;
		display: grid;
		grid-template-columns: repeat(3, 1fr);
		gap: 0.45rem;
	}
	.control-button,
	.stop-button {
		min-height: 5.2rem;
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
		width: 1.35rem;
		height: 1.35rem;
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
		font-size: 0.74rem;
		line-height: 1.45;
		max-width: 33rem;
		margin: 0 auto;
	}
	.arm-controls {
		width: min(100%, 380px);
		margin: 0.25rem auto 1rem;
		display: grid;
		gap: 0.45rem;
	}
	.arm-controls button {
		min-height: 3.6rem;
	}
	.arm-controls span {
		font-size: 0.78rem;
	}
	.arm-row {
		display: grid;
		grid-template-columns: 1fr 1fr 1fr;
		gap: 0.45rem;
	}
	.arm-row button {
		display: flex;
		align-items: center;
		justify-content: center;
		gap: 0.35rem;
		flex-direction: column;
	}
	.arm-stop {
		min-height: 4.1rem !important;
	}
	.gripper-controls {
		border-top: 1px solid var(--border);
		padding-top: 1rem;
		display: grid;
		grid-template-columns: 1fr 1fr;
		gap: 0.5rem;
	}
	.gripper-controls button {
		border: 1px solid var(--border);
		border-radius: 0.35rem;
		background: var(--secondary);
		color: var(--foreground);
		min-height: 2.7rem;
		display: flex;
		align-items: center;
		justify-content: center;
		gap: 0.4rem;
		font-size: 0.76rem;
		transition: 0.2s ease;
	}
	.gripper-controls button:hover {
		color: var(--primary);
		border-color: var(--primary);
	}
	.bottom-bar {
		max-width: 1180px;
		margin: 1rem auto 0;
		border-radius: 0.35rem;
		padding: 0.7rem;
		display: flex;
		align-items: center;
		justify-content: space-between;
		gap: 1rem;
	}
	.bottom-bar strong {
		color: var(--foreground);
		font:
			0.8rem ui-monospace,
			monospace;
		display: block;
		margin-top: 0.25rem;
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
		.control-layout {
			grid-template-columns: 1fr;
		}
		.panel {
			min-height: auto;
		}
		.bottom-bar {
			align-items: stretch;
			flex-direction: column;
		}
		.emergency-button {
			justify-content: center;
		}
	}
</style>
