<script lang="ts">
	/**
	 * Top-down tracked vehicle. Stateless -- it never reflects sensor data, so the robot
	 * stays a fixed frame of reference while the wedges around it move.
	 *
	 * All colours come from Tailwind `fill-*` / `stroke-*` utilities, which compile to CSS
	 * *properties*. That is what makes a light/dark switch repaint it for free. Never use
	 * `fill="var(--x)"` here: SVG presentation attributes do not resolve var() and render
	 * black.
	 */
	import { CX, CY } from '../constants';

	const TRACK_OFFSET = 58;
	const TRACK_WIDTH = 26;
	const TRACK_HEIGHT = 150;
	const TREAD_COUNT = 7;

	const bodyWidth = 96;
	const bodyHeight = 132;
	const bodyLeft = CX - bodyWidth / 2;
	const bodyTop = CY - bodyHeight / 2;

	// Nose: a beveled front edge plus a chevron, so "which way is forward" is unambiguous
	// even at a glance in a stressful context.
	const noseY = bodyTop - 16;
	const nosePath = `M ${bodyLeft + 10} ${bodyTop + 6}
		L ${CX} ${noseY}
		L ${bodyLeft + bodyWidth - 10} ${bodyTop + 6} Z`;

	const chevronPath = `M ${CX - 16} ${bodyTop + 34} L ${CX} ${bodyTop + 16} L ${CX + 16} ${bodyTop + 34}`;

	const treadYs = Array.from(
		{ length: TREAD_COUNT },
		(_, i) => CY - TRACK_HEIGHT / 2 + ((i + 0.5) * TRACK_HEIGHT) / TREAD_COUNT
	);
</script>

<g>
	<!-- Tracks first, so the hull overlaps them like a real chassis -->
	{#each [-TRACK_OFFSET, TRACK_OFFSET] as offset (offset)}
		<rect
			x={CX + offset - TRACK_WIDTH / 2}
			y={CY - TRACK_HEIGHT / 2}
			width={TRACK_WIDTH}
			height={TRACK_HEIGHT}
			rx="8"
			class="fill-vehicle-track stroke-vehicle-stroke"
			stroke-width="1.5"
		/>
		{#each treadYs as ty (ty)}
			<line
				x1={CX + offset - TRACK_WIDTH / 2 + 4}
				y1={ty}
				x2={CX + offset + TRACK_WIDTH / 2 - 4}
				y2={ty}
				class="stroke-vehicle-stroke"
				stroke-width="1.5"
				opacity="0.7"
			/>
		{/each}
	{/each}

	<path d={nosePath} class="fill-vehicle-body stroke-vehicle-stroke" stroke-width="1.5" />

	<rect
		x={bodyLeft}
		y={bodyTop}
		width={bodyWidth}
		height={bodyHeight}
		rx="14"
		class="fill-vehicle-body stroke-vehicle-stroke"
		stroke-width="2"
	/>

	<path
		d={chevronPath}
		class="stroke-vehicle-accent"
		fill="none"
		stroke-width="3"
		stroke-linecap="round"
		stroke-linejoin="round"
	/>

	<!-- Centre hub: the origin the bearing needle pivots on -->
	<circle cx={CX} cy={CY} r="7" class="fill-vehicle-accent" opacity="0.35" />
	<circle cx={CX} cy={CY} r="2.5" class="fill-vehicle-accent" />
</g>
