<script lang="ts">
	/**
	 * The whole top-down scene: vehicle, four sensor wedges, bearing needle.
	 *
	 * Staleness degrades the *entire* graphic (grayscale + reduced opacity + a hatch
	 * overlay) rather than showing a small badge. Stale data that still looks live is the
	 * dangerous failure mode in a monitoring UI, so it is made impossible to miss.
	 */
	import { CX, CY, R_SIDE_LABEL, SIDE_ANGLE_DEG, VIEWBOX } from '../constants';
	import { polar } from '../geometry';
	import { SIDE_LABELS, SIDES } from '../schema';
	import type { SideView } from '../telemetry.svelte';
	import type { BearingEstimate } from '../bearing';
	import BearingNeedle from './bearing-needle.svelte';
	import FlameWedge from './flame-wedge.svelte';
	import VehicleBody from './vehicle-body.svelte';

	interface Props {
		sides: SideView[];
		bearing: BearingEstimate;
		stale?: boolean;
		veryStale?: boolean;
	}

	let { sides, bearing, stale = false, veryStale = false }: Props = $props();

	const ariaLabel = $derived(
		stale
			? 'Robot flame sensor view — telemetry stale, readings not live'
			: `Robot flame sensor view — ${sides.map((s) => `${SIDE_LABELS[s.side]} ${s.label}`).join(', ')}`
	);
</script>

<div class="relative aspect-square w-full">
	<svg
		viewBox="0 0 {VIEWBOX} {VIEWBOX}"
		preserveAspectRatio="xMidYMid meet"
		role="img"
		aria-label={ariaLabel}
		class="h-full w-full transition-[opacity,filter] duration-300 {stale
			? 'opacity-40 grayscale'
			: ''}"
	>
		<defs>
			<pattern
				id="invalid-hatch"
				patternUnits="userSpaceOnUse"
				width="8"
				height="8"
				patternTransform="rotate(45)"
			>
				<line x1="0" y1="0" x2="0" y2="8" class="stroke-muted-foreground" stroke-width="2" />
			</pattern>
		</defs>

		{#each sides as view (view.side)}
			<FlameWedge
				side={view.side}
				smooth={view.smooth}
				intensity={view.intensity}
				level={view.level}
				heat={view.heat}
				opacity={view.opacity}
				label={view.label}
				invalid={view.invalid}
			/>
		{/each}

		<VehicleBody />

		<BearingNeedle {bearing} dimmed={stale} />

		<!-- Side letters inside the wedge ring, so orientation never needs a legend -->
		{#each SIDES as side (side)}
			{@const at = polar(CX, CY, R_SIDE_LABEL, SIDE_ANGLE_DEG[side])}
			<!--
				Full words, not initials: RIGHT and REAR share an initial, and on a
				fire-direction display "R" that could mean either is worse than no label.
			-->
			<text
				x={at.x}
				y={at.y}
				text-anchor="middle"
				dominant-baseline="central"
				class="fill-muted-foreground text-[10px] font-semibold tracking-wide"
			>
				{SIDE_LABELS[side]}
			</text>
		{/each}
	</svg>

	{#if stale}
		<div
			class="pointer-events-none absolute inset-0 flex items-center justify-center"
			aria-hidden="true"
		>
			<span
				class="rounded-md border border-stale bg-background/80 px-3 py-1 text-xs font-bold tracking-widest text-stale uppercase"
			>
				{veryStale ? 'no live data' : 'stale'}
			</span>
		</div>
	{/if}
</div>
