<script lang="ts">
	/**
	 * Single-page field monitor shared by the home console and the legacy monitor route.
	 * The directional IR readout stays spatially aligned with the tracked vehicle.
	 */
	import { resolve } from '$app/paths';
	import { page } from '$app/state';
	import { dev } from '$app/environment';
	import { ArrowUpRight, Crosshair, Droplets, RadioTower } from 'lucide-svelte';
	import { toast } from 'svelte-sonner';

	import { useHealthStatus } from '$lib/features/health/queries';
	import { ALERT_COOLDOWN_MS, type Level } from '$lib/features/telemetry/constants';
	import { MOCK_SCENARIOS, parseScenario } from '$lib/features/telemetry/mock-source';
	import { SIDE_LABELS, SIDES, type Side } from '$lib/features/telemetry/schema';
	import { telemetryStore } from '$lib/features/telemetry/telemetry.svelte';
	import FlameReadout from './flame-readout.svelte';
	import RobotTopView from './robot-top-view.svelte';
	import SimulatedRibbon from './simulated-ribbon.svelte';
	import TelemetryDebug from './telemetry-debug.svelte';
	import TelemetryStatusBar from './telemetry-status-bar.svelte';
	import { CameraCard } from '$lib/features/camera';

	const mockScenario = $derived(parseScenario(page.url.searchParams.get('mock')));
	const showDebug = $derived(page.url.searchParams.get('debug') !== null);

	const health = useHealthStatus();
	const apiOk = $derived(health.isPending ? null : !health.isError);

	$effect(() => {
		const scenario = mockScenario;
		telemetryStore.start({ mock: scenario });
		return () => telemetryStore.stop();
	});

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
		return sideViews.find((view) => view.side === side)!;
	}

	function openMock(event: MouseEvent, scenario: string) {
		event.preventDefault();
		window.location.href = `${resolve('/')}?mock=${scenario}`;
	}
</script>

<section class="space-y-5">
	<header class="grid gap-5 border-b border-border/70 pb-5 lg:grid-cols-[1fr_auto] lg:items-end">
		<div>
			<p class="mb-2 font-mono text-[10px] tracking-[0.2em] text-primary uppercase">
				Durian Bot / Orchard awareness
			</p>
			<h2 class="max-w-2xl text-3xl leading-none font-black tracking-[-0.06em] sm:text-5xl">
				Read the row before you drive.
			</h2>
			<p class="mt-3 max-w-xl text-sm leading-relaxed text-muted-foreground">
				ดูทิศทางรอบตัวหุ่นยนต์จาก IR proximity sensors แล้วตัดสินใจเดินหรือเปิดชุดฉีดน้ำจากจอเดียว
			</p>
		</div>
		<div class="flex flex-wrap items-center gap-2 lg:justify-end">
			<a
				class="inline-flex min-h-10 items-center gap-2 rounded-sm border border-border bg-background px-3 py-2 text-sm font-semibold transition-colors hover:bg-accent"
				href={resolve('/control')}
			>
				<RadioTower class="size-4 text-primary" />
				Control deck
			</a>
			<span
				class="inline-flex min-h-10 items-center gap-2 rounded-sm border border-primary/30 bg-primary/10 px-3 py-2 font-mono text-[10px] tracking-[0.14em] text-primary uppercase"
			>
				<Crosshair class="size-3.5" />
				IR field online
			</span>
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
			class="rounded-sm border border-destructive bg-destructive/10 px-3 py-2 text-sm text-destructive"
			role="alert"
		>
			<strong>Telemetry version mismatch.</strong>
			Backend ส่ง protocol v{telemetryStore.versionMismatch} แต่ dashboard รองรับ v1 กรุณาอัปเดต frontend
		</div>
	{:else}
		{#if telemetryStore.isSimulated}
			<SimulatedRibbon detail={mockScenario ? `client mock: ${mockScenario}` : 'backend mock'} />
		{/if}

		{#if telemetryStore.isStale && telemetryStore.hasEverConnected}
			<div
				class="rounded-sm border border-stale/50 bg-stale/10 px-3 py-2 text-sm text-stale"
				role="alert"
			>
				Telemetry stale. ข้อมูลล่าสุดเมื่อ {(telemetryStore.dataAgeMs / 1000).toFixed(1)} วินาทีที่แล้ว
			</div>
		{/if}

		{#if telemetryStore.shouldHintMock}
			<div class="rounded-sm border border-border bg-card px-3 py-2 text-sm text-muted-foreground">
				เชื่อมต่อ backend ไม่ได้ ({telemetryStore.endpoint})
				{#if dev}
					<span class="ml-1">เปิดข้อมูลจำลอง:</span>
					{#each MOCK_SCENARIOS as scenario, i (scenario)}
						<a
							class="text-foreground underline underline-offset-2"
							href={resolve('/')}
							onclick={(event) => openMock(event, scenario)}>?mock={scenario}</a
						>{i < MOCK_SCENARIOS.length - 1 ? ', ' : ''}
					{/each}
				{/if}
			</div>
		{/if}

		<div class="grid grid-cols-1 items-start gap-4 lg:grid-cols-12">
			<section class="overflow-hidden rounded-sm border border-border/80 bg-card lg:col-span-7">
				<div
					class="flex flex-wrap items-start justify-between gap-3 border-b border-border/70 px-4 py-4 sm:px-5"
				>
					<div>
						<p class="font-mono text-[10px] tracking-[0.18em] text-primary uppercase">
							Spatial readout
						</p>
						<h3 class="mt-1 text-xl font-black tracking-[-0.04em]">IR proximity field</h3>
					</div>
					<div
						class="text-right font-mono text-[10px] tracking-[0.12em] text-muted-foreground uppercase"
					>
						<div>4 channels / 360° awareness</div>
						<div class="mt-1 text-primary">front is top</div>
					</div>
				</div>
				<div class="grid min-h-[360px] place-items-center px-5 py-6 sm:min-h-[470px] sm:px-10">
					<div class="w-full max-w-[500px]">
						<RobotTopView
							sides={sideViews}
							bearing={telemetryStore.bearing}
							stale={telemetryStore.isStale}
							veryStale={telemetryStore.isVeryStale}
						/>
					</div>
				</div>
				<div
					class="grid grid-cols-3 border-t border-border/70 bg-background/45 font-mono text-[10px] uppercase"
				>
					<div class="border-r border-border/70 px-4 py-3">
						<span class="block text-muted-foreground">Direction</span>
						<strong class="mt-1 block text-sm text-foreground">
							{telemetryStore.frame?.strongest_direction
								? SIDE_LABELS[telemetryStore.frame.strongest_direction]
								: 'CLEAR'}
						</strong>
					</div>
					<div class="border-r border-border/70 px-4 py-3">
						<span class="block text-muted-foreground">Bearing</span>
						<strong class="mt-1 block text-sm text-foreground"
							>{Math.round(telemetryStore.bearing.deg)}°</strong
						>
					</div>
					<div class="px-4 py-3">
						<span class="block text-muted-foreground">Confidence</span>
						<strong class="mt-1 block text-sm text-primary"
							>{Math.round(telemetryStore.bearing.confidence * 100)}%</strong
						>
					</div>
				</div>
			</section>

			<div class="space-y-4 lg:col-span-5">
				<CameraCard
					strongestDirection={telemetryStore.frame?.strongest_direction}
					bearingDeg={telemetryStore.bearing.deg}
				/>
				<section
					class="grid grid-cols-2 gap-px overflow-hidden rounded-sm border border-border/80 bg-border/70"
				>
					<div class="bg-card p-4">
						<Droplets class="mb-5 size-4 text-primary" />
						<span
							class="block font-mono text-[10px] tracking-[0.14em] text-muted-foreground uppercase"
							>Spray system</span
						>
						<strong class="mt-1 block text-sm">Standby</strong>
					</div>
					<div class="bg-card p-4">
						<ArrowUpRight class="mb-5 size-4 text-primary" />
						<span
							class="block font-mono text-[10px] tracking-[0.14em] text-muted-foreground uppercase"
							>Operator action</span
						>
						<a
							class="mt-1 inline-flex items-center gap-1 text-sm font-semibold text-primary underline-offset-4 hover:underline"
							href={resolve('/control')}
						>
							Drive unit
							<ArrowUpRight class="size-3" />
						</a>
					</div>
				</section>
			</div>
		</div>

		<section>
			<div class="mb-3 flex flex-wrap items-end justify-between gap-3">
				<div>
					<p class="font-mono text-[10px] tracking-[0.18em] text-primary uppercase">
						Perimeter scan
					</p>
					<h3 class="mt-1 text-xl font-black tracking-[-0.04em]">Four directional channels</h3>
				</div>
				<p class="text-sm text-muted-foreground">Higher values indicate a closer IR return.</p>
			</div>
			<div class="grid grid-cols-1 gap-3 sm:grid-cols-2 lg:grid-cols-4">
				{#each SIDES as side (side)}
					<FlameReadout
						view={viewFor(side)}
						adcMax={telemetryStore.frame?.adc_max ?? null}
						stale={telemetryStore.isStale}
					/>
				{/each}
			</div>
		</section>
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
</section>
