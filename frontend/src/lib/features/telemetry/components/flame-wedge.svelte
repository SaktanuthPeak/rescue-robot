<script lang="ts">
	/**
	 * One directional flame sensor, drawn as an annular sector whose radius and colour
	 * both encode intensity.
	 *
	 * Two redundant channels on purpose: radius is pre-attentive (you see it without
	 * looking), colour carries the finer gradation. Either alone would work; together
	 * they survive a glance and a colour-vision difference.
	 */
	import { SIDE_ANGLE_DEG, GRID_INTENSITIES, type Level } from '../constants';
	import { gridArcPath, trackPath, wedgePath } from '../geometry';
	import { LEVEL_LABELS } from '../scale';
	import { SIDE_LABELS, type Side } from '../schema';

	interface Props {
		side: Side;
		/** Smoothed intensity, 0..1 -- drives the geometry. */
		smooth: number;
		/** Instantaneous intensity, for the tooltip only. */
		intensity: number;
		level: Level;
		heat: string;
		opacity: number;
		label: string;
		invalid: boolean;
	}

	let { side, smooth, intensity, level, heat, opacity, label, invalid }: Props = $props();

	const centre = $derived(SIDE_ANGLE_DEG[side]);
	const track = $derived(trackPath(centre));
	const live = $derived(wedgePath(centre, smooth));
	const isCritical = $derived(level === 'critical');

	const title = $derived(
		invalid
			? `${SIDE_LABELS[side]}: invalid reading`
			: `${SIDE_LABELS[side]}: ${label} (${LEVEL_LABELS[level]}) — raw ${Math.round(intensity * 1000) / 1000}`
	);
</script>

<g>
	<title>{title}</title>

	<!-- Full-range backdrop, so an empty wedge still shows the sensor exists -->
	<path d={track} class="fill-sensor-track" opacity="0.55" />

	<!-- Gauge gridlines: what makes this read as a measurement, not a glow -->
	{#each GRID_INTENSITIES as gridValue (gridValue)}
		<path
			d={gridArcPath(centre, gridValue)}
			class="stroke-sensor-grid"
			fill="none"
			stroke-width="1"
			stroke-dasharray="2 4"
			opacity="0.8"
		/>
	{/each}

	{#if invalid}
		<path d={track} fill="url(#invalid-hatch)" opacity="0.5" />
	{:else}
		<!--
			The heat colour is injected as a custom property and consumed by a scoped CSS
			rule below. Setting fill="{heat}" directly would work for a literal colour but
			breaks the moment it is a var()-based color-mix, so this route is used
			consistently.
		-->
		<g style="--heat: {heat}">
			<path
				d={live}
				class="heat-fill transition-[fill-opacity] duration-150"
				fill-opacity={opacity}
			/>
			{#if isCritical}
				<path
					d={live}
					class="heat-stroke animate-pulse motion-reduce:animate-none"
					fill="none"
					stroke-width="2.5"
				/>
			{/if}
		</g>
	{/if}
</g>

<style>
	.heat-fill {
		fill: var(--heat);
	}

	.heat-stroke {
		stroke: var(--heat);
	}
</style>
