<script lang="ts">
	/**
	 * Durian Bot field monitor.
	 *
	 * Layout is spatially congruent with the hardware: the four readout cards sit on the
	 * four sides of the vehicle graphic, matching where the sensors physically are, so an
	 * operator never has to translate "card 2" into "right side".
	 */
	import { resolve } from '$app/paths';
	import { page } from '$app/state';
	import { dev } from '$app/environment';
	import { toast } from 'svelte-sonner';

	import AppContainer from '$lib/components/app-container.svelte';
	import { Button } from '$lib/components/ui/button/index.js';
	import { useHealthStatus } from '$lib/features/health/queries';
	import { ALERT_COOLDOWN_MS, type Level } from '$lib/features/telemetry/constants';
	import { MOCK_SCENARIOS, parseScenario } from '$lib/features/telemetry/mock-source';
	import { SIDE_LABELS, SIDES, type Side } from '$lib/features/telemetry/schema';
	import { telemetryStore } from '$lib/features/telemetry/telemetry.svelte';
	import FlameReadout from '$lib/features/telemetry/components/flame-readout.svelte';
	import RobotTopView from '$lib/features/telemetry/components/robot-top-view.svelte';
	import SimulatedRibbon from '$lib/features/telemetry/components/simulated-ribbon.svelte';
	import TelemetryDebug from '$lib/features/telemetry/components/telemetry-debug.svelte';
	import TelemetryStatusBar from '$lib/features/telemetry/components/telemetry-status-bar.svelte';
	import { CameraCard } from '$lib/features/camera';

	const mockScenario = $derived(parseScenario(page.url.searchParams.get('mock')));
	const showDebug = $derived(page.url.searchParams.get('debug') !== null);

	function openMock(event: MouseEvent, scenario: string) {
		event.preventDefault();
		window.location.href = `${resolve('/monitor')}?mock=${scenario}`;
	}

	const health = useHealthStatus();
	const apiOk = $derived(health.isPending ? null : !health.isError);

	// Connection lifecycle. An $effect guarantees browser-only execution and gives correct
	// teardown, so navigating away or an HMR reload cannot leak a socket.
	$effect(() => {
		const scenario = mockScenario;
		telemetryStore.start({ mock: scenario });
		return () => telemetryStore.stop();
	});

	// Alerting lives in the page, not the store: hysteresis (store) and notification
	// (here) stay independently testable. These maps are deliberately NOT reactive state
	// -- they are bookkeeping, and making them reactive would re-trigger the effect.
	const previousLevels: Record<Side, Level> = {
		front: 'clear',
		right: 'clear',
		rear: 'clear',
		left: 'clear'
	};
	const lastAlertAt: Record<Side, number> = { front: 0, right: 0, rear: 0, left: 0 };

	$effect(() => {
		const levels = telemetryStore.levels;
		const now = Date.now();
		for (const side of SIDES) {
			const level = levels[side];
			const escalatedToCritical = level === 'critical' && previousLevels[side] !== 'critical';
			previousLevels[side] = level;
			if (!escalatedToCritical) continue;
			if (now - lastAlertAt[side] < ALERT_COOLDOWN_MS) continue;
			lastAlertAt[side] = now;
			toast.error(`IR obstacle alert: ${SIDE_LABELS[side]}`, {
				description: 'ตรวจพบสิ่งกีดขวางจาก IR sensor'
			});
		}
	});

	const sideViews = $derived(telemetryStore.sides);
	function viewFor(side: Side) {
		return sideViews.find((v) => v.side === side)!;
	}
</script>

<svelte:head>
	<title>Durian Bot | Field Monitor</title>
</svelte:head>

<AppContainer>
	<header class="flex flex-wrap items-end justify-between gap-4 border-b border-border/70 pb-4">
		<div>
			<p class="mb-2 font-mono text-[10px] tracking-[0.18em] text-primary uppercase">
				Durian Bot / Field Monitor
			</p>
			<h1 class="text-3xl font-black tracking-[-0.05em] sm:text-4xl">Orchard awareness</h1>
			<p class="mt-2 max-w-xl text-sm text-muted-foreground">
				ตรวจสภาพรอบตัวหุ่นยนต์จาก IR sensors และดูภาพสวนแบบ live
			</p>
		</div>
		<div class="flex items-center gap-1">
			<Button variant="ghost" size="sm" href={resolve('/control')}>ควบคุมหุ่นยนต์</Button>
			<Button variant="ghost" size="sm" href={resolve('/')}>หน้าหลัก</Button>
		</div>
	</header>

	<TelemetryStatusBar
		{apiOk}
		wsStatus={telemetryStore.status}
		link={telemetryStore.link}
		deviceStatus={telemetryStore.deviceStatus}
		dataAgeMs={telemetryStore.dataAgeMs}
		stale={telemetryStore.isStale}
		rateHz={telemetryStore.rateHz}
		attempt={telemetryStore.attempt}
		retryInMs={telemetryStore.retryInMs}
		simulated={telemetryStore.isSimulated}
		onReconnect={() => telemetryStore.reconnectNow()}
	/>

	{#if telemetryStore.versionMismatch !== null}
		<div
			class="rounded-md border border-destructive bg-destructive/10 px-3 py-2 text-sm text-destructive"
			role="alert"
		>
			<strong>Telemetry version mismatch.</strong>
			Backend ส่ง protocol v{telemetryStore.versionMismatch} แต่ dashboard รองรับ v1. ไม่แสดงค่าเพื่อเลี่ยงการอ่านผิด
			กรุณาอัปเดต frontend
		</div>
	{:else}
		{#if telemetryStore.isSimulated}
			<SimulatedRibbon detail={mockScenario ? `client mock: ${mockScenario}` : 'backend mock'} />
		{/if}

		{#if telemetryStore.isStale && telemetryStore.hasEverConnected}
			<div
				class="rounded-md border border-stale/50 bg-stale/10 px-3 py-2 text-sm text-stale"
				role="alert"
			>
				Telemetry stale. ข้อมูลล่าสุดเมื่อ {(telemetryStore.dataAgeMs / 1000).toFixed(1)} วินาทีที่แล้ว
			</div>
		{/if}

		{#if telemetryStore.shouldHintMock}
			<div class="rounded-md border border-border px-3 py-2 text-sm text-muted-foreground">
				เชื่อมต่อ backend ไม่ได้ ({telemetryStore.endpoint})
				{#if dev}
					เปิดด้วย
					{#each MOCK_SCENARIOS as scenario, i (scenario)}
						<a
							class="text-foreground underline"
							href={resolve('/monitor')}
							onclick={(event) => openMock(event, scenario)}>?mock={scenario}</a
						>{i < MOCK_SCENARIOS.length - 1 ? ', ' : ''}
					{/each}
					เพื่อดู UI ด้วยข้อมูลจำลอง
				{/if}
			</div>
		{/if}

		<!--
			Responsive Mission Grid:
			- lg+: 12 cols (5 cols Camera Stream, 3 cols Robot Top-View, 4 cols IR Sensor Cards)
			- md/sm: stacked cleanly with full responsiveness
		-->
		<div class="grid grid-cols-1 items-start gap-4 lg:grid-cols-12">
			<!-- Camera Feed Column -->
			<div class="lg:col-span-5">
				<CameraCard
					strongestDirection={telemetryStore.frame?.strongest_direction}
					bearingDeg={telemetryStore.bearing.deg}
				/>
			</div>

			<!-- Robot 360 Diagram Column -->
			<div class="mx-auto w-full max-w-sm lg:col-span-3 lg:mx-0">
				<RobotTopView
					sides={sideViews}
					bearing={telemetryStore.bearing}
					stale={telemetryStore.isStale}
					veryStale={telemetryStore.isVeryStale}
				/>

				{#if telemetryStore.frame?.strongest_direction}
					<p class="mt-2 text-center text-sm text-muted-foreground">
						ทิศที่ตรวจพบมากที่สุด:
						<strong class="text-foreground"
							>{SIDE_LABELS[telemetryStore.frame.strongest_direction]}</strong
						>
						· bearing ประมาณ {Math.round(telemetryStore.bearing.deg)}° (confidence {Math.round(
							telemetryStore.bearing.confidence * 100
						)}%)
					</p>
				{/if}
			</div>

			<!-- IR Sensor Readout Cards Column -->
			<div class="grid grid-cols-2 gap-3 lg:col-span-4">
				<div class="col-span-2">
					<FlameReadout
						view={viewFor('front')}
						adcMax={telemetryStore.frame?.adc_max ?? null}
						stale={telemetryStore.isStale}
					/>
				</div>

				<FlameReadout
					view={viewFor('left')}
					adcMax={telemetryStore.frame?.adc_max ?? null}
					stale={telemetryStore.isStale}
				/>

				<FlameReadout
					view={viewFor('right')}
					adcMax={telemetryStore.frame?.adc_max ?? null}
					stale={telemetryStore.isStale}
				/>

				<div class="col-span-2">
					<FlameReadout
						view={viewFor('rear')}
						adcMax={telemetryStore.frame?.adc_max ?? null}
						stale={telemetryStore.isStale}
					/>
				</div>
			</div>
		</div>
	{/if}

	<TelemetryDebug
		open={showDebug}
		endpoint={telemetryStore.endpoint}
		status={telemetryStore.status}
		attempt={telemetryStore.attempt}
		retryInMs={telemetryStore.retryInMs}
		messages={telemetryStore.messages}
		malformed={telemetryStore.malformed}
		rateHz={telemetryStore.rateHz}
		dataAgeMs={telemetryStore.dataAgeMs}
		link={telemetryStore.link}
		sides={sideViews}
		lastRaw={telemetryStore.lastRaw}
		lastError={telemetryStore.lastError}
	/>
</AppContainer>
