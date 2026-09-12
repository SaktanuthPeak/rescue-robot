<script lang="ts">
	import { resolve } from '$app/paths';
	import { AlertTriangle, Battery, Link2, Radio, RefreshCw, Wifi, WifiOff } from 'lucide-svelte';

	import AppContainer from '$lib/components/app-container.svelte';
	import { CameraCard } from '$lib/features/camera';
	import { sendRobotCommand, getRobotStatus } from '$lib/features/robot/api';
	import ArmPreview from '$lib/features/robot/ui/arm-preview.svelte';
	import MecanumDrivePreview from '$lib/features/robot/ui/mecanum-drive-preview.svelte';
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
	let commandInFlight = $state(false);

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

	async function emergencyStop() {
		await send({ channel: 'all', code: 0 });
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

			<section class="panel driving-panel">
				<div class="panel-heading">
					<div>
						<span class="section-index">01</span>
						<h2>Drive</h2>
					</div>
					<span class="panel-meta">mecanum live preview</span>
				</div>
				<div class="device-readout">
					<div><span>motor state</span><strong>{status.motor_status}</strong></div>
					<div class:good={status.motor_can_alive}>
						<span>CAN heartbeat</span><strong>{status.motor_can_alive ? 'LIVE' : 'TIMEOUT'}</strong>
					</div>
				</div>
				<MecanumDrivePreview
					motorCode={status.motor_code}
					active={isOnline && status.motor_can_alive}
					stale={!isOnline}
					compact
				/>
			</section>

			<section class="panel arm-panel">
				<div class="panel-heading">
					<div>
						<span class="section-index">02</span>
						<h2>Actuator mount</h2>
					</div>
					<span class="panel-meta">3-axis arm live preview</span>
				</div>
				<div class="device-readout">
					<div><span>arm state</span><strong>{status.arm_status}</strong></div>
					<div class:good={status.arm_can_alive}>
						<span>CAN heartbeat</span><strong>{status.arm_can_alive ? 'LIVE' : 'TIMEOUT'}</strong>
					</div>
				</div>
				<ArmPreview
					armCode={status.arm_code}
					active={isOnline && status.arm_can_alive}
					stale={!isOnline}
					compact
				/>
			</section>
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
	.workspace-layout > .overview-layout {
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
	.overview-layout {
		margin: 0;
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
