<script lang="ts">
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
	import { INITIAL_ROBOT_STATUS, type RobotCommand, type RobotStatus } from '$lib/features/robot/schema';

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
	const statusAge = $derived(status.last_frame_age_ms > 0 ? `${status.last_frame_age_ms} ms ago` : 'waiting');
	const batteryLabel = $derived(status.battery_millivolts > 0 ? `${status.battery_volts.toFixed(2)} V` : '—');
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
	<title>FireBot Control</title>
	<meta
		name="description"
		content="FireBot receiver control panel for motor, arm and gripper commands."
	/>
</svelte:head>

<AppContainer>
	<header class="page-header">
		<div>
			<p class="eyebrow">FIREBOT / COMMAND DECK</p>
			<h1>Robot control</h1>
			<p class="subhead">ควบคุมผ่าน Raspberry Pi → USB Serial → Arduino receiver</p>
		</div>
		<div class="header-actions">
			<a class="back-link" href="/monitor/">เปิด flame monitor <Link2 size={14} /></a>
			<button class="refresh-button" type="button" onclick={refresh} disabled={refreshing} aria-label="Refresh receiver status">
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
		<div class="status-stat battery-stat"><span>battery sensor</span><strong><Battery size={15} /> {batteryLabel}</strong><small>ADC {status.battery_adc}</small></div>
		<div class="status-stat"><span>last frame</span><strong>{statusAge}</strong></div>
		<div class="status-stat"><span>sequence</span><strong>{status.sequence}</strong></div>
		<div class="status-stat"><span>parse errors</span><strong class={status.parse_errors ? 'warn' : ''}>{status.parse_errors}</strong></div>
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
				<div><span class="section-index">01</span><h2>Drive</h2></div>
				<span class="panel-meta">hold to move · release to stop</span>
			</div>
			<div class="device-readout">
				<div><span>motor state</span><strong>{status.motor_status}</strong></div>
				<div class:good={status.motor_can_alive}><span>CAN heartbeat</span><strong>{status.motor_can_alive ? 'LIVE' : 'TIMEOUT'}</strong></div>
			</div>

			<div class="d-pad" aria-label="Motor directional controls">
				<button class={buttonClass('motor-forward-left')} type="button" aria-label="Forward left" onpointerdown={() => press('motor', 5, 'motor-forward-left')} onpointerup={() => release('motor')} onpointercancel={() => release('motor')} onpointerleave={() => activeControl === 'motor-forward-left' && release('motor')} disabled={commandInFlight}><ArrowUpLeft /></button>
				<button class={buttonClass('motor-forward')} type="button" aria-label="Forward" onpointerdown={() => press('motor', 1, 'motor-forward')} onpointerup={() => release('motor')} onpointercancel={() => release('motor')} onpointerleave={() => activeControl === 'motor-forward' && release('motor')} disabled={commandInFlight}><ArrowUp /></button>
				<button class={buttonClass('motor-forward-right')} type="button" aria-label="Forward right" onpointerdown={() => press('motor', 6, 'motor-forward-right')} onpointerup={() => release('motor')} onpointercancel={() => release('motor')} onpointerleave={() => activeControl === 'motor-forward-right' && release('motor')} disabled={commandInFlight}><ArrowUpRight /></button>
				<button class={buttonClass('motor-left')} type="button" aria-label="Left" onpointerdown={() => press('motor', 3, 'motor-left')} onpointerup={() => release('motor')} onpointercancel={() => release('motor')} onpointerleave={() => activeControl === 'motor-left' && release('motor')} disabled={commandInFlight}><ArrowLeft /></button>
				<button class="stop-button" type="button" aria-label="Stop motor" onclick={() => release('motor')}><CircleStop /></button>
				<button class={buttonClass('motor-right')} type="button" aria-label="Right" onpointerdown={() => press('motor', 4, 'motor-right')} onpointerup={() => release('motor')} onpointercancel={() => release('motor')} onpointerleave={() => activeControl === 'motor-right' && release('motor')} disabled={commandInFlight}><ArrowRight /></button>
				<button class={buttonClass('motor-backward-left')} type="button" aria-label="Backward left" onpointerdown={() => press('motor', 7, 'motor-backward-left')} onpointerup={() => release('motor')} onpointercancel={() => release('motor')} onpointerleave={() => activeControl === 'motor-backward-left' && release('motor')} disabled={commandInFlight}><ArrowDownLeft /></button>
				<button class={buttonClass('motor-backward')} type="button" aria-label="Backward" onpointerdown={() => press('motor', 2, 'motor-backward')} onpointerup={() => release('motor')} onpointercancel={() => release('motor')} onpointerleave={() => activeControl === 'motor-backward' && release('motor')} disabled={commandInFlight}><ArrowDown /></button>
				<button class={buttonClass('motor-backward-right')} type="button" aria-label="Backward right" onpointerdown={() => press('motor', 8, 'motor-backward-right')} onpointerup={() => release('motor')} onpointercancel={() => release('motor')} onpointerleave={() => activeControl === 'motor-backward-right' && release('motor')} disabled={commandInFlight}><ArrowDownRight /></button>
			</div>
			<p class="control-hint">ปุ่มทิศทางจะส่งคำสั่งค้างจนกว่าจะปล่อย เพื่อให้หยุดเมื่อผู้ควบคุมเลิกกด</p>
		</section>

		<section class="panel arm-panel">
			<div class="panel-heading">
				<div><span class="section-index">02</span><h2>Arm & gripper</h2></div>
				<span class="panel-meta">receiver channel</span>
			</div>
			<div class="device-readout">
				<div><span>arm state</span><strong>{status.arm_status}</strong></div>
				<div class:good={status.arm_can_alive}><span>CAN heartbeat</span><strong>{status.arm_can_alive ? 'LIVE' : 'TIMEOUT'}</strong></div>
			</div>
			<div class="arm-controls">
				<button class={buttonClass('arm-forward')} type="button" onpointerdown={() => press('arm', 1, 'arm-forward')} onpointerup={() => release('arm')} onpointercancel={() => release('arm')} onpointerleave={() => activeControl === 'arm-forward' && release('arm')} disabled={commandInFlight}><ArrowUp /><span>ยกแขนขึ้น</span></button>
				<div class="arm-row">
					<button class={buttonClass('arm-left')} type="button" onpointerdown={() => press('arm', 3, 'arm-left')} onpointerup={() => release('arm')} onpointercancel={() => release('arm')} onpointerleave={() => activeControl === 'arm-left' && release('arm')} disabled={commandInFlight}><ArrowLeft /><span>ซ้าย</span></button>
					<button class="stop-button arm-stop" type="button" onclick={() => release('arm')}><CircleStop /><span>หยุด</span></button>
					<button class={buttonClass('arm-right')} type="button" onpointerdown={() => press('arm', 4, 'arm-right')} onpointerup={() => release('arm')} onpointercancel={() => release('arm')} onpointerleave={() => activeControl === 'arm-right' && release('arm')} disabled={commandInFlight}><ArrowRight /><span>ขวา</span></button>
				</div>
				<button class={buttonClass('arm-backward')} type="button" onpointerdown={() => press('arm', 2, 'arm-backward')} onpointerup={() => release('arm')} onpointercancel={() => release('arm')} onpointerleave={() => activeControl === 'arm-backward' && release('arm')} disabled={commandInFlight}><ArrowDown /><span>ลดแขนลง</span></button>
			</div>
			<div class="gripper-controls">
				<button type="button" onclick={() => send({ channel: 'arm', code: 9 })} disabled={commandInFlight}><Grip size={18} /> ปล่อย gripper</button>
				<button type="button" onclick={() => send({ channel: 'arm', code: 10 })} disabled={commandInFlight}><Grip size={18} /> หนีบ gripper</button>
			</div>
		</section>
	</main>

	<section class="bottom-bar">
		<div><span>last command</span><strong>{status.last_command ?? '—'}</strong></div>
		<button class="emergency-button" type="button" onclick={emergencyStop} disabled={commandInFlight}><AlertTriangle size={17} /> EMERGENCY STOP — หยุดทั้งหมด</button>
	</section>
</AppContainer>

<style>
	:global(body) { background: #0e0e0d; }
	:global(.dark) { --background: #0e0e0d; --card: #171716; --border: oklch(1 0 0 / 10%); }
	:global(button) { touch-action: manipulation; }
	.page-header { display: flex; justify-content: space-between; align-items: end; gap: 1rem; margin: 0 auto 1.3rem; max-width: 1180px; }
	.eyebrow, .panel-meta, .device-readout span, .status-stat span, .bottom-bar span { color: #99958c; font-size: .68rem; letter-spacing: .12em; text-transform: uppercase; }
	.eyebrow { color: #d19742; margin: 0 0 .45rem; font-weight: 700; }
	h1 { color: #f3eee5; font-size: clamp(2rem, 5vw, 3.7rem); letter-spacing: -.06em; line-height: .95; margin: 0; }
	.subhead { color: #aaa69d; margin: .65rem 0 0; font-size: .9rem; }
	.header-actions { display: flex; align-items: center; gap: .6rem; flex-wrap: wrap; justify-content: flex-end; }
	.back-link, .refresh-button { color: #cfc8bb; border: 1px solid #373630; background: #171716; border-radius: .5rem; display: inline-flex; align-items: center; gap: .45rem; min-height: 2.35rem; padding: .55rem .75rem; font-size: .78rem; transition: .2s ease; }
	.back-link:hover, .refresh-button:hover { border-color: #a87432; color: #f3eee5; }
	.refresh-button:disabled { opacity: .55; }
	.spin { animation: spin .8s linear infinite; }
	.status-rail, .panel, .bottom-bar { border: 1px solid #302f2a; background: #171716; box-shadow: 0 18px 50px rgb(0 0 0 / 20%); }
	.status-rail { max-width: 1180px; margin: 0 auto 1rem; padding: .7rem; display: grid; grid-template-columns: minmax(220px, 1.7fr) repeat(4, 1fr); align-items: center; gap: .35rem; border-radius: .65rem; }
	.connection-state { display: flex; gap: .65rem; align-items: center; padding: .35rem .55rem; color: #a4a097; }
	.connection-state.online { color: #d8b26a; }
	.connection-state.offline { color: #b7aaa0; }
	.connection-state strong { color: #eee8dd; display: block; font-size: .84rem; }
	.connection-state span { display: block; margin-top: .15rem; font-size: .72rem; color: #8c8981; font-variant-numeric: tabular-nums; }
	.status-stat { border-left: 1px solid #302f2a; padding: .25rem .75rem; }
	.status-stat strong { color: #ece6dc; display: block; margin-top: .2rem; font-family: ui-monospace, SFMono-Regular, monospace; font-size: .85rem; font-variant-numeric: tabular-nums; }
	.status-stat strong :global(svg) { display: inline; vertical-align: -2px; color: #d19742; }
	.status-stat small { display: block; color: #77736b; font-size: .64rem; margin-top: .2rem; }
	.battery-stat strong { color: #d9b06a; }
	.status-stat .warn { color: #dc895e; }
	.notice { max-width: 1180px; margin: 0 auto 1rem; border: 1px solid #39362e; border-radius: .5rem; color: #bfb8ac; display: flex; align-items: center; gap: .5rem; padding: .7rem .8rem; font-size: .82rem; }
	.notice.error { color: #efaa8a; border-color: #744a3d; background: #2b1916; }
	.control-layout { max-width: 1180px; margin: 0 auto; display: grid; grid-template-columns: 1.08fr .92fr; gap: 1rem; }
	.panel { border-radius: .75rem; padding: clamp(1rem, 2vw, 1.45rem); min-height: 430px; }
	.panel-heading { display: flex; justify-content: space-between; gap: 1rem; align-items: start; border-bottom: 1px solid #302f2a; padding-bottom: .85rem; }
	.panel-heading > div { display: flex; align-items: baseline; gap: .6rem; }
	.section-index { color: #a87432; font: 700 .72rem ui-monospace, monospace; }
	h2 { color: #eee8dd; margin: 0; font-size: 1.1rem; letter-spacing: -.02em; }
	.panel-meta { white-space: nowrap; letter-spacing: .06em; font-size: .62rem; }
	.device-readout { display: flex; justify-content: space-between; padding: 1rem 0 1.15rem; }
	.device-readout strong { color: #e8a95b; display: block; margin-top: .25rem; font: 700 1rem ui-monospace, monospace; letter-spacing: .02em; }
	.device-readout .good strong { color: #a5bd8f; }
	.d-pad { width: min(100%, 360px); margin: .1rem auto 1rem; display: grid; grid-template-columns: repeat(3, 1fr); gap: .45rem; }
	.control-button, .stop-button { min-height: 5.2rem; border: 1px solid #3b3933; border-radius: .65rem; background: #22221f; color: #c9c2b7; display: flex; align-items: center; justify-content: center; transition: transform .15s ease, background .2s ease, border-color .2s ease, color .2s ease; }
	.control-button:hover { background: #302b22; border-color: #a87432; color: #f1c274; }
	.control-button:active, .control-button.is-active { transform: translateY(2px) scale(.98); background: #6f4822; border-color: #d19742; color: #fff1d1; }
	.control-button :global(svg) { width: 1.35rem; height: 1.35rem; }
	.stop-button { color: #d98e70; border-color: #744a3d; background: #2a1a17; }
	.stop-button:hover { background: #49241c; border-color: #d16c4f; }
	.control-hint { text-align: center; color: #77736b; font-size: .74rem; line-height: 1.45; max-width: 33rem; margin: 0 auto; }
	.arm-controls { width: min(100%, 380px); margin: .25rem auto 1rem; display: grid; gap: .45rem; }
	.arm-controls button { min-height: 3.6rem; }
	.arm-controls span { font-size: .78rem; }
	.arm-row { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: .45rem; }
	.arm-row button { display: flex; align-items: center; justify-content: center; gap: .35rem; flex-direction: column; }
	.arm-stop { min-height: 4.1rem !important; }
	.gripper-controls { border-top: 1px solid #302f2a; padding-top: 1rem; display: grid; grid-template-columns: 1fr 1fr; gap: .5rem; }
	.gripper-controls button { border: 1px solid #3b3933; border-radius: .5rem; background: #22221f; color: #c8c0b4; min-height: 2.7rem; display: flex; align-items: center; justify-content: center; gap: .4rem; font-size: .76rem; transition: .2s ease; }
	.gripper-controls button:hover { color: #f1c274; border-color: #a87432; }
	.bottom-bar { max-width: 1180px; margin: 1rem auto 0; border-radius: .65rem; padding: .7rem; display: flex; align-items: center; justify-content: space-between; gap: 1rem; }
	.bottom-bar strong { color: #d6cec1; font: .8rem ui-monospace, monospace; display: block; margin-top: .25rem; }
	.emergency-button { background: #682d21; border: 1px solid #b95d45; color: #ffe8db; padding: .8rem 1rem; border-radius: .5rem; font-size: .75rem; font-weight: 700; letter-spacing: .03em; display: inline-flex; align-items: center; gap: .45rem; transition: .2s ease; }
	.emergency-button:hover { background: #8a3828; transform: translateY(-1px); }
	.emergency-button:disabled { opacity: .55; }
	@keyframes spin { to { transform: rotate(360deg); } }
	@media (max-width: 760px) {
		.page-header { align-items: start; flex-direction: column; }
		.header-actions { justify-content: flex-start; }
		.status-rail { grid-template-columns: 1fr 1fr; }
		.connection-state { grid-column: 1 / -1; border-bottom: 1px solid #302f2a; padding-bottom: .7rem; }
		.status-stat:nth-child(3) { border-left: 0; }
		.control-layout { grid-template-columns: 1fr; }
		.panel { min-height: auto; }
		.bottom-bar { align-items: stretch; flex-direction: column; }
		.emergency-button { justify-content: center; }
	}
</style>
