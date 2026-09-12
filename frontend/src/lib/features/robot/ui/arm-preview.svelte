<script lang="ts">
	type Props = {
		armCode: number;
		active: boolean;
		stale?: boolean;
		compact?: boolean;
	};

	let { armCode, active, stale = false, compact = false }: Props = $props();

	const commands: Record<number, string> = {
		0: 'หยุด',
		1: 'ยกแขนขึ้น',
		2: 'ลดแขนลง',
		3: 'หมุนซ้าย',
		4: 'หมุนขวา',
		5: 'เฉียงหน้าซ้าย',
		6: 'เฉียงหน้าขวา',
		7: 'เฉียงหลังซ้าย',
		8: 'เฉียงหลังขวา',
		9: 'หมุนซ้าย',
		10: 'หมุนขวา',
		11: 'ปั๊มทำงาน',
		12: 'ปั๊มหยุด',
		13: 'หัวขึ้น',
		14: 'หัวลง'
	};

	const available = $derived(active && !stale);
	const isMoving = $derived([1, 2, 3, 4, 5, 6, 7, 8, 13, 14].includes(armCode));
	const statusText = $derived(
		stale ? 'ข้อมูลหมดอายุ' : !active ? 'รอข้อมูล' : (commands[armCode] ?? 'ไม่รู้จักคำสั่ง')
	);
	const baseAngle = $derived(
		[3, 5, 7].includes(armCode) ? -22 : [4, 6, 8].includes(armCode) ? 22 : 0
	);
	const shoulderAngle = $derived(
		[1, 5, 6].includes(armCode) ? -16 : [2, 7, 8].includes(armCode) ? 16 : 0
	);
	const elbowAngle = $derived(armCode === 13 ? -24 : armCode === 14 ? 24 : 0);
	const pumpOn = $derived(armCode === 11);
</script>

<div
	class={`arm-preview ${available ? '' : 'is-dimmed'} ${isMoving && available ? 'is-moving' : ''}`}
	class:is-compact={compact}
>
	<div class="preview-heading">
		<div>
			<span class="preview-kicker">ARM ACTUATOR</span>
			<strong>ภาพจำลองแขนกล</strong>
		</div>
		<span class={`preview-state ${isMoving && available ? 'is-moving' : ''}`}>
			<span class="status-dot"></span>{statusText}
		</span>
	</div>

	<svg
		viewBox="0 0 360 320"
		role="img"
		aria-label={`ภาพจำลองคำสั่งแขนกล: ${statusText}`}
		class="arm-svg"
	>
		<path d="M40 300H320" class="floor-line" />
		<text x="180" y="25" text-anchor="middle" class="orientation-label"
			>3-AXIS ARM / FRONT VIEW</text
		>
		<path d="M92 269A88 88 0 0 1 268 269" class="motion-arc" />

		<g class="base-platform">
			<rect x="110" y="274" width="140" height="26" rx="9" class="platform-shadow" />
			<rect x="116" y="266" width="128" height="30" rx="9" class="platform" />
			<ellipse cx="180" cy="266" rx="52" ry="14" class="base-ring" />
			<ellipse cx="180" cy="266" rx="24" ry="7" class="base-core" />
		</g>

		<g class="base-rotation" style={`--base-angle: ${baseAngle}deg`}>
			<rect x="164" y="242" width="32" height="32" rx="8" class="base-neck" />
			<g class="shoulder-group" style={`--shoulder-angle: ${shoulderAngle}deg`}>
				<rect x="164" y="142" width="32" height="132" rx="14" class="upper-segment" />
				<path d="M172 163V250M188 163V250" class="segment-highlight" />
				<text x="205" y="222" class="axis-tag">A2</text>
				<circle cx="180" cy="270" r="18" class="joint" />
				<circle cx="180" cy="270" r="7" class="joint-core" />

				<g class="elbow-group" style={`--elbow-angle: ${elbowAngle}deg`}>
					<rect x="164" y="54" width="32" height="104" rx="14" class="lower-segment" />
					<path d="M172 75V135M188 75V135" class="segment-highlight" />
					<text x="205" y="116" class="axis-tag">A3</text>
					<circle cx="180" cy="150" r="18" class="joint" />
					<circle cx="180" cy="150" r="7" class="joint-core" />

					<g class="tool-group">
						<rect x="158" y="20" width="44" height="38" rx="12" class="head-shell" />
						<path d="M166 34H194M166 43H186" class="head-detail" />
						<path d="M180 20V8M170 8H190" class="nozzle" />
						<circle cx="180" cy="56" r="6" class="head-joint" />
					</g>
				</g>
			</g>
			<text x="205" y="286" class="axis-tag">A1</text>
		</g>

		<g class={`pump-indicator ${pumpOn ? 'is-on' : ''}`}>
			<circle cx="286" cy="260" r="13" class="pump-light" />
			<text x="286" y="286" text-anchor="middle" class="pump-label">PUMP</text>
		</g>
	</svg>

	<div class="preview-footer">
		<span>คำสั่งแขนกล</span><strong>{armCode >= 0 ? armCode : '—'}</strong>
	</div>
	<p class="preview-note">ภาพจำลองจาก status ที่ได้รับ ไม่ใช่ตำแหน่งจริงจาก encoder</p>
</div>

<style>
	.arm-preview {
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
	.arm-svg {
		display: block;
		width: min(100%, 280px);
		margin: 0.6rem auto 0.15rem;
		overflow: visible;
	}
	.is-compact {
		padding: 0.65rem;
	}
	.is-compact .arm-svg {
		width: min(100%, 185px);
		margin-top: 0.35rem;
	}
	.is-compact .preview-heading strong {
		font-size: 0.76rem;
	}
	.is-compact .preview-state {
		font-size: 0.62rem;
		padding: 0.2rem 0.4rem;
	}
	.is-compact .preview-footer {
		padding-top: 0.4rem;
		font-size: 0.62rem;
	}
	.is-compact .preview-note {
		font-size: 0.58rem;
	}
	.arm-svg text {
		font-family: inherit;
	}
	.floor-line {
		stroke: var(--line);
		stroke-width: 2;
		stroke-dasharray: 4 7;
	}
	.orientation-label,
	.pump-label {
		fill: var(--muted);
		font-size: 9px;
		letter-spacing: 1.6px;
	}
	.platform-shadow {
		fill: #000;
		opacity: 0.25;
	}
	.platform {
		fill: var(--surface);
		stroke: var(--line);
		stroke-width: 2;
	}
	.base-ring {
		fill: color-mix(in oklab, var(--accent), transparent 91%);
		stroke: color-mix(in oklab, var(--accent), transparent 55%);
		stroke-width: 2;
	}
	.base-core {
		fill: var(--accent);
		opacity: 0.78;
	}
	.base-rotation {
		transform-box: view-box;
		transform-origin: 180px 270px;
		transform: rotate(var(--base-angle));
		transition: transform 0.35s cubic-bezier(0.22, 1, 0.36, 1);
	}
	.shoulder-group {
		transform-box: view-box;
		transform-origin: 180px 270px;
		transform: rotate(var(--shoulder-angle));
		transition: transform 0.35s cubic-bezier(0.22, 1, 0.36, 1);
	}
	.elbow-group {
		transform-box: view-box;
		transform-origin: 180px 150px;
		transform: rotate(var(--elbow-angle));
		transition: transform 0.35s cubic-bezier(0.22, 1, 0.36, 1);
	}
	.base-neck,
	.upper-segment,
	.lower-segment,
	.head-shell {
		fill: #192738;
		stroke: #52657a;
		stroke-width: 2;
	}
	.upper-segment,
	.lower-segment {
		fill: color-mix(in oklab, var(--surface), var(--accent) 12%);
	}
	.motion-arc {
		fill: none;
		stroke: color-mix(in oklab, var(--accent), transparent 58%);
		stroke-width: 1.5;
		stroke-dasharray: 3 7;
	}
	.axis-tag {
		fill: var(--accent);
		font:
			700 9px ui-monospace,
			monospace;
		letter-spacing: 1px;
	}
	.segment-highlight,
	.head-detail {
		fill: none;
		stroke: #9aabbd;
		stroke-linecap: round;
		opacity: 0.45;
	}
	.segment-highlight {
		stroke-width: 2;
	}
	.head-detail {
		stroke-width: 3;
	}
	.nozzle {
		fill: none;
		stroke: var(--accent);
		stroke-width: 4;
		stroke-linecap: round;
	}
	.joint,
	.head-joint {
		fill: #263c51;
		stroke: var(--accent);
		stroke-width: 2;
	}
	.joint-core {
		fill: var(--accent);
	}
	.is-moving .joint-core {
		animation: joint-pulse 1.1s ease-in-out infinite;
	}
	.pump-light {
		fill: #2a3642;
		stroke: var(--line);
		stroke-width: 2;
	}
	.pump-indicator.is-on .pump-light {
		fill: #ffd166;
		stroke: #ffe7a3;
		filter: drop-shadow(0 0 5px #ffd166);
	}
	.preview-footer {
		display: flex;
		align-items: center;
		justify-content: space-between;
		border-top: 1px solid var(--line);
		padding-top: 0.55rem;
		color: var(--muted);
		font-size: 0.68rem;
	}
	.preview-footer strong {
		color: var(--ink);
		font-family: ui-monospace, monospace;
	}
	.preview-note {
		margin: 0.45rem 0 0;
		color: var(--muted);
		font-size: 0.65rem;
		line-height: 1.35;
		text-align: center;
	}
	@media (prefers-reduced-motion: reduce) {
		.base-rotation,
		.shoulder-group,
		.elbow-group,
		.joint-core,
		.arm-preview {
			transition: none;
			animation: none;
		}
	}
	@keyframes joint-pulse {
		0%,
		100% {
			opacity: 0.72;
		}
		50% {
			opacity: 1;
		}
	}
</style>
