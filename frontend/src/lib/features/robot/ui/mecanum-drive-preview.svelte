<script lang="ts">
	type WheelId = 'FL' | 'FR' | 'BL' | 'BR';

	interface Props {
		motorCode: number;
		active: boolean;
		stale?: boolean;
	}

	interface WheelLayout {
		id: WheelId;
		x: number;
		y: number;
		rollerTilt: 'left' | 'right';
	}

	type WheelSigns = Record<WheelId, -1 | 0 | 1>;

	let { motorCode, active, stale = false }: Props = $props();

	const wheelLayout: WheelLayout[] = [
		{ id: 'FL', x: 70, y: 112, rollerTilt: 'right' },
		{ id: 'FR', x: 290, y: 112, rollerTilt: 'left' },
		{ id: 'BL', x: 70, y: 308, rollerTilt: 'left' },
		{ id: 'BR', x: 290, y: 308, rollerTilt: 'right' }
	];

	const stopped: WheelSigns = { FL: 0, FR: 0, BL: 0, BR: 0 };

	// These signs mirror firmware/motor_controller/motor.cpp. They describe the
	// commanded wheel direction, not measured encoder feedback.
	const commandSigns: Record<number, WheelSigns> = {
		0: stopped,
		1: { FL: 1, FR: 1, BL: 1, BR: 1 },
		2: { FL: -1, FR: -1, BL: -1, BR: -1 },
		3: { FL: -1, FR: 1, BL: 1, BR: -1 },
		4: { FL: 1, FR: -1, BL: -1, BR: 1 },
		5: { FL: 0, FR: 1, BL: 1, BR: 0 },
		6: { FL: 1, FR: 0, BL: 0, BR: 1 },
		7: { FL: -1, FR: 0, BL: 0, BR: -1 },
		8: { FL: 0, FR: -1, BL: -1, BR: 0 },
		11: { FL: -1, FR: 1, BL: -1, BR: 1 },
		12: { FL: 1, FR: -1, BL: 1, BR: -1 }
	};

	const signs = $derived(active && !stale ? (commandSigns[motorCode] ?? stopped) : stopped);
	const movingCount = $derived(Object.values(signs).filter((sign) => sign !== 0).length);
	const motionLabel = $derived(
		!active || stale
			? 'หยุด / รอข้อมูล'
			: movingCount === 0
				? 'หยุด'
				: `${movingCount} ล้อกำลังหมุน`
	);

	function wheelClass(sign: number) {
		if (sign > 0) return 'wheel-motion wheel-motion-positive';
		if (sign < 0) return 'wheel-motion wheel-motion-negative';
		return 'wheel-motion';
	}

	function directionLabel(sign: number) {
		return sign > 0 ? '↻ +' : sign < 0 ? '↺ −' : '—';
	}
</script>

<div class="drive-preview" class:is-dimmed={stale || !active}>
	<div class="preview-heading">
		<div>
			<span class="preview-kicker">COMMAND PREVIEW</span>
			<strong>ล้อ mecanum</strong>
		</div>
		<span class="preview-state">{motionLabel}</span>
	</div>

	<svg
		viewBox="0 0 360 420"
		role="img"
		aria-label={`Mecanum wheel command preview: ${motionLabel}`}
		class="robot-svg"
	>
		<path class="direction-line" d="M180 28V55" />
		<path class="direction-head" d="M171 40L180 28L189 40" />
		<text x="180" y="20" text-anchor="middle" class="direction-label">หน้า</text>

		<rect x="108" y="45" width="144" height="330" rx="28" class="chassis" />
		<path d="M132 45L180 20L228 45" class="chassis-nose" />
		<path d="M150 112H210M150 308H210" class="chassis-detail" />
		<circle cx="180" cy="210" r="9" class="chassis-hub" />

		{#each wheelLayout as wheel (wheel.id)}
			{@const sign = signs[wheel.id]}
			<g transform={`translate(${wheel.x} ${wheel.y})`}>
				<g class={wheelClass(sign)}>
					<rect x="-27" y="-48" width="54" height="96" rx="16" class="wheel-shell" />
					<rect x="-19" y="-39" width="38" height="78" rx="11" class="wheel-core" />
					{#each [-25, -12, 1, 14, 27] as offset (offset)}
						<line
							x1={wheel.rollerTilt === 'right' ? -15 : 15}
							y1={offset - 7}
							x2={wheel.rollerTilt === 'right' ? 15 : -15}
							y2={offset + 7}
							class="roller-mark"
						/>
					{/each}
				</g>
				<text y="-63" text-anchor="middle" class="wheel-label">{wheel.id}</text>
				<text
					y="66"
					text-anchor="middle"
					class:direction-active={sign !== 0}
					class="wheel-direction"
				>
					{directionLabel(sign)}
				</text>
			</g>
		{/each}
	</svg>

	<p class="preview-note">ทิศ +/− อ้างอิงจากคำสั่งใน firmware ยังไม่ใช่ค่าความเร็วจาก encoder</p>
</div>

<style>
	.drive-preview {
		border: 1px solid var(--border);
		border-radius: 0.35rem;
		background: color-mix(in oklab, var(--background), var(--card) 58%);
		padding: 0.8rem;
		transition: opacity 0.25s ease;
	}
	.drive-preview.is-dimmed {
		opacity: 0.58;
	}
	.preview-heading {
		display: flex;
		align-items: center;
		justify-content: space-between;
		gap: 0.75rem;
		padding: 0.1rem 0.15rem 0.5rem;
	}
	.preview-heading strong {
		color: var(--foreground);
		display: block;
		font-size: 0.82rem;
		margin-top: 0.15rem;
	}
	.preview-kicker {
		color: var(--primary);
		font-size: 0.58rem;
		font-weight: 700;
		letter-spacing: 0.12em;
	}
	.preview-state {
		color: var(--muted-foreground);
		font-size: 0.68rem;
		font-variant-numeric: tabular-nums;
		white-space: nowrap;
	}
	.robot-svg {
		display: block;
		margin: 0 auto;
		max-height: 285px;
		width: min(100%, 245px);
	}
	.direction-line,
	.direction-head {
		fill: none;
		stroke: var(--primary);
		stroke-linecap: round;
		stroke-width: 2;
	}
	.direction-label,
	.wheel-label,
	.wheel-direction {
		fill: var(--muted-foreground);
		font-family: ui-monospace, SFMono-Regular, monospace;
		font-size: 11px;
		font-weight: 700;
		letter-spacing: 0.05em;
	}
	.wheel-label {
		fill: var(--foreground);
		font-size: 12px;
	}
	.wheel-direction {
		font-size: 13px;
	}
	.direction-active {
		fill: var(--primary);
	}
	.chassis {
		fill: color-mix(in oklab, var(--card), var(--foreground) 5%);
		stroke: var(--border);
		stroke-width: 2;
	}
	.chassis-nose {
		fill: color-mix(in oklab, var(--card), var(--primary) 8%);
		stroke: var(--border);
		stroke-width: 2;
	}
	.chassis-detail {
		fill: none;
		stroke: var(--border);
		stroke-width: 1.5;
	}
	.chassis-hub {
		fill: var(--primary);
		opacity: 0.7;
	}
	.wheel-shell {
		fill: color-mix(in oklab, var(--secondary), var(--foreground) 7%);
		stroke: var(--border);
		stroke-width: 2;
	}
	.wheel-core {
		fill: color-mix(in oklab, var(--card), var(--foreground) 8%);
		stroke: var(--muted-foreground);
		stroke-width: 1.2;
	}
	.roller-mark {
		stroke: var(--muted-foreground);
		stroke-linecap: round;
		stroke-width: 2;
		opacity: 0.7;
	}
	.wheel-motion {
		transform-box: fill-box;
		transform-origin: center;
	}
	.wheel-motion-positive,
	.wheel-motion-negative {
		animation-duration: 0.72s;
		animation-iteration-count: infinite;
		animation-timing-function: linear;
	}
	.wheel-motion-positive {
		animation-name: wheel-forward;
	}
	.wheel-motion-negative {
		animation-name: wheel-reverse;
	}
	.wheel-motion-positive .wheel-shell,
	.wheel-motion-negative .wheel-shell {
		stroke: var(--primary);
	}
	.wheel-motion-positive .roller-mark,
	.wheel-motion-negative .roller-mark {
		stroke: var(--primary);
		opacity: 1;
	}
	.preview-note {
		color: var(--muted-foreground);
		font-size: 0.65rem;
		line-height: 1.35;
		margin: 0.25rem 0 0;
		text-align: center;
	}
	@keyframes wheel-forward {
		from {
			transform: rotate(0deg);
		}
		to {
			transform: rotate(360deg);
		}
	}
	@keyframes wheel-reverse {
		from {
			transform: rotate(360deg);
		}
		to {
			transform: rotate(0deg);
		}
	}
	@media (prefers-reduced-motion: reduce) {
		.wheel-motion-positive,
		.wheel-motion-negative {
			animation-play-state: paused;
		}
	}
</style>
