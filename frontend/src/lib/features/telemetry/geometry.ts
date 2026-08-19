/**
 * SVG path maths for the sensor wedges.
 *
 * Convention: 0deg is straight up (vehicle front) and angles increase clockwise, which
 * matches how an operator reads a top-down view. In SVG the Y axis points down, hence
 * `y = cy - r*cos` rather than `+`.
 */

import { CX, CY, R_INNER, R_MAX, WEDGE_SPAN_DEG } from './constants';

export interface Point {
	x: number;
	y: number;
}

const DEG_TO_RAD = Math.PI / 180;

/** Round to 2dp: keeps path strings short and avoids pointless DOM churn. */
function round(value: number): number {
	return Math.round(value * 100) / 100;
}

export function polar(cx: number, cy: number, r: number, deg: number): Point {
	const rad = deg * DEG_TO_RAD;
	return {
		x: round(cx + r * Math.sin(rad)),
		y: round(cy - r * Math.cos(rad))
	};
}

/**
 * An annular sector (donut wedge) from r0 out to r1, spanning startDeg..endDeg clockwise.
 * This is the shape a directional sensor's coverage actually has, which is why the
 * graphic is SVG rather than CSS.
 */
export function annularSector(
	cx: number,
	cy: number,
	r0: number,
	r1: number,
	startDeg: number,
	endDeg: number
): string {
	const largeArc = Math.abs(endDeg - startDeg) > 180 ? 1 : 0;
	const outerStart = polar(cx, cy, r1, startDeg);
	const outerEnd = polar(cx, cy, r1, endDeg);
	const innerEnd = polar(cx, cy, r0, endDeg);
	const innerStart = polar(cx, cy, r0, startDeg);

	return [
		`M ${outerStart.x} ${outerStart.y}`,
		`A ${r1} ${r1} 0 ${largeArc} 1 ${outerEnd.x} ${outerEnd.y}`,
		`L ${innerEnd.x} ${innerEnd.y}`,
		`A ${r0} ${r0} 0 ${largeArc} 0 ${innerStart.x} ${innerStart.y}`,
		'Z'
	].join(' ');
}

/** An open arc, for gridlines and threshold markers. */
export function arcPath(
	cx: number,
	cy: number,
	r: number,
	startDeg: number,
	endDeg: number
): string {
	const largeArc = Math.abs(endDeg - startDeg) > 180 ? 1 : 0;
	const start = polar(cx, cy, r, startDeg);
	const end = polar(cx, cy, r, endDeg);
	return `M ${start.x} ${start.y} A ${r} ${r} 0 ${largeArc} 1 ${end.x} ${end.y}`;
}

/**
 * Map intensity to the wedge's outer radius.
 *
 * The square-root ease is deliberate: at intensity 0.05 a linear map would produce a
 * sub-pixel sliver that reads as "nothing", whereas sqrt gives roughly 18px -- visible
 * as "something faint", which is the honest reading.
 */
export function radiusForValue(intensity: number): number {
	const clamped = Math.min(Math.max(intensity, 0), 1);
	return R_INNER + (R_MAX - R_INNER) * Math.sqrt(clamped);
}

/** Start/end angles for one side's wedge, centred on its bearing. */
export function wedgeAngles(centreDeg: number): { start: number; end: number } {
	const half = WEDGE_SPAN_DEG / 2;
	return { start: centreDeg - half, end: centreDeg + half };
}

/** The full wedge path for a side at a given intensity. */
export function wedgePath(centreDeg: number, intensity: number): string {
	const { start, end } = wedgeAngles(centreDeg);
	return annularSector(CX, CY, R_INNER, radiusForValue(intensity), start, end);
}

/** The muted backdrop wedge showing each sensor's full range. */
export function trackPath(centreDeg: number): string {
	const { start, end } = wedgeAngles(centreDeg);
	return annularSector(CX, CY, R_INNER, R_MAX, start, end);
}

/** A gridline arc across one side's wedge at a reference intensity. */
export function gridArcPath(centreDeg: number, intensity: number): string {
	const { start, end } = wedgeAngles(centreDeg);
	return arcPath(CX, CY, radiusForValue(intensity), start, end);
}
