<script lang="ts">
	/**
	 * Compact IR overview for the operator page.
	 *
	 * The full field monitor remains intentionally detailed. This version keeps only the
	 * spatial warning picture and the four sensor values visible while driving.
	 */
	import { resolve } from '$app/paths';
	import { page } from '$app/state';
	import { ArrowUpRight, RefreshCw } from 'lucide-svelte';

	import { parseScenario } from '../mock-source';
	import { SIDE_LABELS, SIDES, type Side } from '../schema';
	import { telemetryStore } from '../telemetry.svelte';
	import RobotTopView from './robot-top-view.svelte';

	const mockScenario = $derived(parseScenario(page.url.searchParams.get('mock')));
	const sideViews = $derived(telemetryStore.sides);
	const statusLabel = $derived(
		telemetryStore.status === 'open'
			? telemetryStore.isSimulated
				? 'SIMULATED'
				: 'LIVE'
			: telemetryStore.status.toUpperCase()
	);
	const dataLabel = $derived(
		!Number.isFinite(telemetryStore.dataAgeMs)
			? 'NO DATA'
			: telemetryStore.isStale
				? `${(telemetryStore.dataAgeMs / 1000).toFixed(1)}s OLD`
				: `${telemetryStore.rateHz.toFixed(1)} HZ`
	);
	const strongestLabel = $derived(
		telemetryStore.frame?.strongest_direction
			? SIDE_LABELS[telemetryStore.frame.strongest_direction]
			: 'CLEAR'
	);

	$effect(() => {
		const scenario = mockScenario;
		telemetryStore.start({ mock: scenario });
		return () => telemetryStore.stop();
	});

	function viewFor(side: Side) {
		return sideViews.find((view) => view.side === side)!;
	}
</script>

<section class="compact-monitor" aria-labelledby="compact-monitor-title">
	<header class="compact-heading">
		<div>
			<p class="compact-kicker">LIVE PERIMETER / 4 IR CHANNELS</p>
			<h2 id="compact-monitor-title">Safety view</h2>
		</div>
		<div class="compact-actions">
			<span class:stale={telemetryStore.isStale} class="live-chip">
				<span class="live-dot"></span>{statusLabel}
			</span>
			<button
				class="reconnect-button"
				type="button"
				onclick={() => telemetryStore.reconnectNow()}
				aria-label="Reconnect IR telemetry"
				title="Reconnect IR telemetry"><RefreshCw size={13} /></button
			>
			<a class="monitor-link" href={resolve('/monitor')}>
				ละเอียด <ArrowUpRight size={13} />
			</a>
		</div>
	</header>

	<div class="compact-content">
		<div class="radar-box">
			<RobotTopView
				sides={sideViews}
				bearing={telemetryStore.bearing}
				stale={telemetryStore.isStale}
				veryStale={telemetryStore.isVeryStale}
			/>
		</div>

		<div class="sensor-column">
			<div class="sensor-grid">
				{#each SIDES as side (side)}
					{@const view = viewFor(side)}
					<article
						class="sensor-card"
						class:watch={view.level === 'watch' && !telemetryStore.isStale}
						class:warn={view.level === 'warn' && !telemetryStore.isStale}
						class:critical={view.level === 'critical' && !telemetryStore.isStale}
					>
						<div class="sensor-name">
							<span class="sensor-dot"></span>
							<span>IR {SIDE_LABELS[side]}</span>
						</div>
						<strong>{view.label}</strong>
						<span class="sensor-level">{telemetryStore.isStale ? 'OFFLINE' : view.level}</span>
					</article>
				{/each}
			</div>

			<div class="monitor-stats">
				<div><span>STRONGEST</span><strong>{strongestLabel}</strong></div>
				<div><span>BEARING</span><strong>{Math.round(telemetryStore.bearing.deg)}°</strong></div>
				<div><span>DATA</span><strong>{dataLabel}</strong></div>
			</div>
		</div>
	</div>
</section>

<style>
	.compact-monitor {
		width: min(100%, 1180px);
		margin: 0 auto;
		border: 1px solid var(--border);
		border-radius: 0.35rem;
		background: var(--card);
		box-shadow: 0 18px 50px rgb(0 0 0 / 16%);
		padding: 0.85rem;
	}
	.compact-heading {
		display: flex;
		align-items: center;
		justify-content: space-between;
		gap: 0.75rem;
		border-bottom: 1px solid var(--border);
		padding: 0.1rem 0.15rem 0.7rem;
	}
	.compact-kicker {
		margin: 0;
		color: var(--primary);
		font:
			700 0.6rem ui-monospace,
			monospace;
		letter-spacing: 0.15em;
	}
	.compact-heading h2 {
		margin: 0.2rem 0 0;
		color: var(--foreground);
		font-size: 1.02rem;
		letter-spacing: -0.02em;
	}
	.compact-actions,
	.live-chip,
	.monitor-link,
	.reconnect-button {
		display: inline-flex;
		align-items: center;
	}
	.compact-actions {
		gap: 0.35rem;
	}
	.live-chip {
		gap: 0.35rem;
		border: 1px solid color-mix(in oklab, var(--primary), transparent 55%);
		border-radius: 99px;
		padding: 0.28rem 0.5rem;
		color: var(--primary);
		font:
			700 0.59rem ui-monospace,
			monospace;
		letter-spacing: 0.09em;
	}
	.live-chip.stale {
		border-color: color-mix(in oklab, var(--stale), transparent 45%);
		color: var(--stale);
	}
	.live-dot,
	.sensor-dot {
		width: 0.38rem;
		height: 0.38rem;
		flex: 0 0 auto;
		border-radius: 50%;
		background: currentColor;
	}
	.live-dot {
		animation: live-pulse 1.5s ease-in-out infinite;
	}
	.reconnect-button,
	.monitor-link {
		justify-content: center;
		border: 1px solid var(--border);
		border-radius: 0.25rem;
		background: var(--background);
		color: var(--muted-foreground);
		min-height: 1.7rem;
		padding: 0.25rem 0.45rem;
		transition: 0.2s ease;
	}
	.reconnect-button:hover,
	.monitor-link:hover {
		border-color: var(--primary);
		color: var(--primary);
	}
	.monitor-link {
		gap: 0.2rem;
		font-size: 0.67rem;
		text-decoration: none;
	}
	.compact-content {
		display: grid;
		grid-template-columns: minmax(150px, 0.48fr) minmax(0, 1.52fr);
		align-items: center;
		gap: 0.9rem;
		padding: 0.8rem 0.15rem 0.05rem;
	}
	.radar-box {
		width: min(100%, 190px);
		margin: auto;
	}
	.sensor-column {
		min-width: 0;
	}
	.sensor-grid {
		display: grid;
		grid-template-columns: repeat(4, minmax(0, 1fr));
		gap: 0.45rem;
	}
	.sensor-card {
		min-width: 0;
		border: 1px solid var(--border);
		border-radius: 0.28rem;
		background: color-mix(in oklab, var(--background), var(--card) 45%);
		padding: 0.55rem;
		transition:
			border-color 0.2s ease,
			background 0.2s ease;
	}
	.sensor-card.watch {
		border-color: color-mix(in oklab, var(--flame-1), transparent 42%);
	}
	.sensor-card.warn {
		border-color: color-mix(in oklab, var(--flame-2), transparent 34%);
		background: color-mix(in oklab, var(--flame-2), transparent 91%);
	}
	.sensor-card.critical {
		border-color: color-mix(in oklab, var(--destructive), transparent 28%);
		background: color-mix(in oklab, var(--destructive), transparent 88%);
	}
	.sensor-name {
		display: flex;
		align-items: center;
		gap: 0.3rem;
		overflow: hidden;
		color: var(--muted-foreground);
		font:
			700 0.57rem ui-monospace,
			monospace;
		letter-spacing: 0.08em;
		white-space: nowrap;
	}
	.sensor-card strong {
		display: block;
		margin-top: 0.3rem;
		color: var(--foreground);
		font:
			700 clamp(1rem, 2vw, 1.3rem) ui-monospace,
			monospace;
		line-height: 1;
	}
	.sensor-level {
		display: block;
		margin-top: 0.35rem;
		color: var(--muted-foreground);
		font:
			700 0.54rem ui-monospace,
			monospace;
		letter-spacing: 0.08em;
		text-transform: uppercase;
	}
	.sensor-card.critical .sensor-dot,
	.sensor-card.critical strong,
	.sensor-card.critical .sensor-level {
		color: var(--destructive);
	}
	.sensor-card.warn .sensor-dot,
	.sensor-card.warn .sensor-level {
		color: var(--flame-2);
	}
	.monitor-stats {
		display: grid;
		grid-template-columns: repeat(3, 1fr);
		gap: 0.45rem;
		border-top: 1px solid var(--border);
		margin-top: 0.65rem;
		padding-top: 0.6rem;
	}
	.monitor-stats div {
		min-width: 0;
	}
	.monitor-stats span,
	.monitor-stats strong {
		display: block;
		overflow: hidden;
		text-overflow: ellipsis;
		white-space: nowrap;
	}
	.monitor-stats span {
		color: var(--muted-foreground);
		font:
			700 0.54rem ui-monospace,
			monospace;
		letter-spacing: 0.08em;
	}
	.monitor-stats strong {
		margin-top: 0.2rem;
		color: var(--foreground);
		font:
			700 0.72rem ui-monospace,
			monospace;
	}
	@keyframes live-pulse {
		0%,
		100% {
			opacity: 0.45;
		}
		50% {
			opacity: 1;
		}
	}
	@media (prefers-reduced-motion: reduce) {
		.live-dot {
			animation: none;
		}
	}
	@media (max-width: 620px) {
		.compact-content {
			grid-template-columns: 1fr;
		}
		.radar-box {
			width: min(100%, 205px);
		}
	}
	@media (max-width: 460px) {
		.compact-heading {
			align-items: flex-start;
			flex-direction: column;
		}
		.sensor-grid {
			grid-template-columns: repeat(2, minmax(0, 1fr));
		}
	}
</style>
