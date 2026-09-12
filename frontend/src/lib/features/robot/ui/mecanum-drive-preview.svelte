<script lang="ts">
	type WheelId = 'FL' | 'FR' | 'BL' | 'BR';
	type WheelSigns = Record<WheelId, -1 | 0 | 1>;

	interface Props {
		motorCode: number;
		active: boolean;
		stale?: boolean;
	}

	let { motorCode, active, stale = false }: Props = $props();

	const wheels: { id: WheelId; x: number; y: number; tilt: number }[] = [
		{ id: 'FL', x: 70, y: 116, tilt: 26 },
		{ id: 'FR', x: 290, y: 116, tilt: -26 },
		{ id: 'BL', x: 70, y: 304, tilt: -26 },
		{ id: 'BR', x: 290, y: 304, tilt: 26 }
	];

	// Command directions from the supplied mapping, not encoder measurements.
	const stopped: WheelSigns = { FL: 0, FR: 0, BL: 0, BR: 0 };
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
	const commands: Record<number, string> = {
		0: 'หยุด',
		1: 'เดินหน้า',
		2: 'ถอยหลัง',
		3: 'เลื่อนซ้าย',
		4: 'เลื่อนขวา',
		5: 'เฉียงหน้าซ้าย',
		6: 'เฉียงหน้าขวา',
		7: 'เฉียงหลังซ้าย',
		8: 'เฉียงหลังขวา',
		11: 'หมุนซ้าย',
		12: 'หมุนขวา'
	};

	const available = $derived(active && !stale);
	const known = $derived(Object.hasOwn(commandSigns, motorCode));
	const signs = $derived(available && known ? commandSigns[motorCode] : stopped);
	const movingCount = $derived(Object.values(signs).filter((sign) => sign !== 0).length);
	const motionLabel = $derived(
		stale ? 'ข้อมูลหมดอายุ' : !active ? 'รอข้อมูล' : (commands[motorCode] ?? 'ไม่รู้จักคำสั่ง')
	);

	// Exactly one roller pitch per cycle makes the repeating tread seamless.
	const rollerOffsets = Array.from({ length: 11 }, (_, index) => index * 18 - 45);
</script>

<div class="drive-preview" class:is-dimmed={!available}>
	<div class="preview-heading">
		<div>
			<span class="preview-kicker">MECANUM DRIVE</span>
			<strong>ระบบขับเคลื่อน</strong>
		</div>
		<span class="preview-state" class:is-moving={movingCount > 0}>
			<span class="status-dot"></span>{motionLabel}
		</span>
	</div>

	<svg
		viewBox="0 0 360 420"
		role="img"
		aria-label={`ภาพจำลองคำสั่งล้อ mecanum: ${motionLabel}`}
		class="robot-svg"
	>
		<text x="180" y="23" text-anchor="middle" class="front-label">หน้ารถ</text>
		<path d="M180 53V35M173 42L180 35L187 42" class="front-arrow" />

		<!-- Axles and chassis stay fixed. Only the tread surface moves. -->
		<path d="M70 116H290M70 304H290" class="axles" />
		<rect x="107" y="65" width="146" height="296" rx="25" class="chassis" />
		<rect x="119" y="77" width="122" height="272" rx="18" class="chassis-panel" />
		<path d="M151 99H209M151 327H209" class="chassis-detail" />
		<circle cx="180" cy="187" r="22" class="hub-ring" />
		<path d="M180 198V175M172 183L180 175L188 183" class="front-arrow" />
		<text x="180" y="236" text-anchor="middle" class="body-title">MECANUM</text>
		<text x="180" y="256" text-anchor="middle" class="body-caption">มุมมองด้านบน</text>

		{#each wheels as wheel (wheel.id)}
			{@const sign = signs[wheel.id]}
			<g
				transform={`translate(${wheel.x} ${wheel.y})`}
				class="wheel"
				class:running={sign !== 0}
				class:reverse={sign < 0}
			>
				<title
					>{wheel.id}: {sign > 0
						? 'คำสั่งเดินหน้า +'
						: sign < 0
							? 'คำสั่งถอยหลัง −'
							: 'ไม่มีคำสั่งหมุน'}</title
				>
				<text y="-65" text-anchor="middle" class="wheel-label">{wheel.id}</text>
				<rect x="-29" y="-51" width="58" height="102" rx="17" class="wheel-shadow" />
				<rect x="-27" y="-53" width="54" height="102" rx="16" class="wheel-shell" />

				<!-- A nested SVG clips the tread without shared document IDs. -->
				<svg
					x="-20"
					y="-43"
					width="40"
					height="82"
					viewBox="0 0 40 82"
					overflow="hidden"
					aria-hidden="true"
				>
					<rect width="40" height="82" class="tread-bed" />
					<g class="tread">
						{#each rollerOffsets as offset (offset)}
							<g transform={`translate(20 ${offset}) rotate(${wheel.tilt})`}>
								<rect x="-28" y="-5.5" width="56" height="11" rx="5.5" class="roller" />
								<path d="M-22 -2H22" class="roller-shine" />
							</g>
						{/each}
					</g>
					<path d="M2 0V82M38 0V82" class="edge-shade" />
					<path d="M0 1H40M0 81H40" class="end-shade" />
				</svg>

				<rect x="-27" y="-53" width="54" height="102" rx="16" class="wheel-outline" />
				<path d="M-24 -29V25M24 -29V25" class="sidewall-shine" />

				{#if sign !== 0}
					<g transform={`translate(${wheel.x < 180 ? -43 : 43} -2)`} class="wheel-arrow">
						<path d={sign > 0 ? 'M0 18V-18M-5 -11L0 -18L5 -11' : 'M0 -18V18M-5 11L0 18L5 11'} />
					</g>
				{/if}
				<text y="70" text-anchor="middle" class="wheel-direction">
					{sign > 0 ? '↑ +' : sign < 0 ? '↓ −' : '—'}
				</text>
			</g>
		{/each}
	</svg>

	<div class="preview-footer">
		<span>คำสั่งหมุนล้อ</span><strong>{movingCount}<span> / 4</span></strong>
	</div>
	<p class="preview-note">จำลองจากคำสั่ง +/− ไม่ใช่ความเร็วจริงจาก encoder</p>
</div>

<style>
	.drive-preview {
		--accent: var(--primary, #22c9ad);
		--ink: var(--foreground, #e5edf5);
		--muted: var(--muted-foreground, #91a1b5);
		--line: var(--border, #2b3849);
		--surface: var(--card, #141e2c);
		border: 1px solid var(--line);
		border-radius: 0.85rem;
		background: color-mix(in oklab, var(--background, #0c1420), var(--surface) 58%);
		padding: 0.9rem;
		transition: opacity 0.25s ease;
	}
	.is-dimmed {
		opacity: 0.58;
	}
	.preview-heading {
		display: flex;
		flex-wrap: wrap;
		align-items: center;
		justify-content: space-between;
		gap: 0.65rem;
	}
	.preview-kicker {
		color: var(--accent);
		font-size: 0.58rem;
		font-weight: 700;
		letter-spacing: 0.13em;
	}
	.preview-heading strong {
		display: block;
		color: var(--ink);
		font-size: 0.86rem;
		margin-top: 0.2rem;
	}
	.preview-state {
		display: inline-flex;
		align-items: center;
		gap: 0.4rem;
		padding: 0.32rem 0.55rem;
		border: 1px solid var(--line);
		border-radius: 99px;
		color: var(--muted);
		font-size: 0.7rem;
	}
	.preview-state.is-moving {
		color: var(--accent);
		background: color-mix(in oklab, var(--accent), transparent 92%);
	}
	.status-dot {
		width: 6px;
		height: 6px;
		border-radius: 50%;
		background: currentColor;
	}
	.robot-svg {
		display: block;
		width: min(100%, 280px);
		margin: 0.6rem auto 0.15rem;
	}
	svg text {
		font-family: inherit;
	}
	.front-label,
	.body-caption {
		fill: var(--muted);
		font-size: 11px;
	}
	.front-arrow {
		fill: none;
		stroke: var(--accent);
		stroke-width: 2;
		stroke-linecap: round;
		stroke-linejoin: round;
	}
	.axles {
		stroke: var(--line);
		stroke-width: 9;
	}
	.chassis {
		fill: var(--surface);
		stroke: var(--line);
		stroke-width: 2;
	}
	.chassis-panel {
		fill: color-mix(in oklab, var(--surface), var(--ink) 3%);
		stroke: var(--line);
	}
	.chassis-detail {
		stroke: var(--line);
		stroke-width: 4;
		stroke-linecap: round;
	}
	.hub-ring {
		fill: color-mix(in oklab, var(--accent), transparent 92%);
		stroke: color-mix(in oklab, var(--accent), transparent 65%);
	}
	.body-title {
		fill: var(--ink);
		font-size: 12px;
		font-weight: 700;
		letter-spacing: 1.6px;
	}
	.wheel-label {
		fill: var(--ink);
		font:
			700 13px ui-monospace,
			monospace;
		letter-spacing: 1px;
	}
	.wheel-shadow {
		fill: #000;
		opacity: 0.22;
	}
	.wheel-shell {
		fill: #111a26;
	}
	.tread-bed {
		fill: #0a1019;
	}
	.roller {
		fill: #546378;
		stroke: #222e3e;
		stroke-width: 1;
	}
	.roller-shine {
		stroke: #a5b5c8;
		stroke-width: 1.3;
		stroke-linecap: round;
		opacity: 0.5;
	}
	.wheel-outline {
		fill: none;
		stroke: #526074;
		stroke-width: 1.5;
	}
	.sidewall-shine {
		stroke: #8592a5;
		stroke-width: 1;
		opacity: 0.35;
	}
	.edge-shade {
		stroke: #000;
		stroke-width: 6;
		opacity: 0.23;
	}
	.end-shade {
		stroke: #000;
		stroke-width: 5;
		opacity: 0.35;
	}
	.wheel-direction {
		fill: var(--muted);
		font:
			700 13px ui-monospace,
			monospace;
	}
	.wheel-arrow {
		fill: none;
		stroke: var(--accent);
		stroke-width: 2;
		stroke-linecap: round;
		stroke-linejoin: round;
	}
	.running .wheel-outline {
		stroke: var(--accent);
		stroke-width: 2;
	}
	.running .roller {
		fill: color-mix(in oklab, #546378, var(--accent) 48%);
	}
	.running .wheel-direction {
		fill: var(--accent);
	}

	/* Move only the repeating rollers. The shell never rotates. */
	.tread {
		animation: tread-roll 0.45s linear infinite;
		animation-play-state: paused;
	}
	.running .tread {
		animation-play-state: running;
	}
	.reverse .tread {
		animation-direction: reverse;
	}
	@keyframes tread-roll {
		from {
			transform: translateY(0);
		}
		to {
			transform: translateY(-18px);
		}
	}
	.preview-footer {
		display: flex;
		justify-content: space-between;
		align-items: center;
		padding-top: 0.65rem;
		border-top: 1px solid var(--line);
		color: var(--muted);
		font-size: 0.72rem;
	}
	.preview-footer strong {
		color: var(--accent);
		font:
			700 0.85rem ui-monospace,
			monospace;
	}
	.preview-footer strong span {
		color: var(--muted);
		font-weight: 400;
	}
	.preview-note {
		color: var(--muted);
		font-size: 0.63rem;
		line-height: 1.5;
		text-align: center;
		margin: 0.65rem 0 0;
	}
	@media (prefers-reduced-motion: reduce) {
		.tread,
		.running .tread {
			animation: none;
		}
		.drive-preview {
			transition: none;
		}
	}
</style>
