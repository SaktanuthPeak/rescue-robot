<script lang="ts">
	/**
	 * Estimated fire bearing, vector-summed from all four intensities.
	 *
	 * Hidden below BEARING_MIN_INTENSITY: with no meaningful signal the vector sum is
	 * pure noise, and a needle spinning randomly would imply information that is not
	 * there.
	 */
	import { BEARING_MIN_INTENSITY, CX, CY, R_INNER } from '../constants';
	import { polar } from '../geometry';
	import type { BearingEstimate } from '../bearing';

	interface Props {
		bearing: BearingEstimate;
		/** Muted when telemetry is stale. */
		dimmed?: boolean;
	}

	let { bearing, dimmed = false }: Props = $props();

	const visible = $derived(bearing.peak >= BEARING_MIN_INTENSITY);
	// Length encodes confidence: a diffuse reading (smoke, multiple sources) gives a
	// visibly shorter needle than a single crisp source.
	const length = $derived(28 + (R_INNER - 40) * bearing.confidence);
	const tip = $derived(polar(CX, CY, length, bearing.deg));
	const leftBarb = $derived(polar(CX, CY, length - 14, bearing.deg - 7));
	const rightBarb = $derived(polar(CX, CY, length - 14, bearing.deg + 7));
</script>

{#if visible}
	<g opacity={dimmed ? 0.3 : 1}>
		<title>
			Estimated fire bearing {Math.round(bearing.deg)}° · confidence {Math.round(
				bearing.confidence * 100
			)}%
		</title>
		<line
			x1={CX}
			y1={CY}
			x2={tip.x}
			y2={tip.y}
			class="stroke-flame-3"
			stroke-width="3"
			stroke-linecap="round"
		/>
		<path
			d="M {leftBarb.x} {leftBarb.y} L {tip.x} {tip.y} L {rightBarb.x} {rightBarb.y}"
			class="fill-flame-3"
			stroke="none"
		/>
	</g>
{/if}
