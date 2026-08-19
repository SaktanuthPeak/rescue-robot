/**
 * Estimate the fire's bearing from the four sensor intensities.
 *
 * This is the payoff of using analog sensors instead of digital: four binary
 * detect/no-detect flags can only say "something on the right", whereas four graded
 * readings vector-sum into an actual direction. It answers the operator's real question
 * -- which way is the fire? -- directly.
 */

import { SIDE_ANGLE_DEG, type Level } from './constants';
import { SIDES, type Side } from './schema';

const DEG_TO_RAD = Math.PI / 180;

export interface BearingEstimate {
	/** Degrees clockwise from vehicle front. */
	deg: number;
	/**
	 * 0..1. High when the readings agree on one direction; low when they are diffuse
	 * (smoke filling a room, or several sources), which is itself worth showing.
	 */
	confidence: number;
	/** Strongest single intensity, for deciding whether to render at all. */
	peak: number;
}

export function estimateBearing(intensities: Record<Side, number>): BearingEstimate {
	let x = 0;
	let y = 0;
	let total = 0;
	let peak = 0;

	for (const side of SIDES) {
		const weight = Math.max(intensities[side] ?? 0, 0);
		if (!Number.isFinite(weight)) continue;
		const rad = SIDE_ANGLE_DEG[side] * DEG_TO_RAD;
		x += weight * Math.sin(rad);
		y += weight * Math.cos(rad);
		total += weight;
		peak = Math.max(peak, weight);
	}

	if (total <= 0) return { deg: 0, confidence: 0, peak: 0 };

	const magnitude = Math.hypot(x, y);
	// atan2(x, y) with y as the "up" axis gives degrees clockwise from north.
	const deg = (Math.atan2(x, y) * 180) / Math.PI;

	return {
		deg: (deg + 360) % 360,
		confidence: Math.min(magnitude / total, 1),
		peak
	};
}

/** Worst level across all sides, for the page-level alert state. */
export function highestLevel(levels: Record<Side, Level>): Level {
	const order: Level[] = ['clear', 'watch', 'warn', 'critical'];
	let worst: Level = 'clear';
	for (const side of SIDES) {
		if (order.indexOf(levels[side]) > order.indexOf(worst)) worst = levels[side];
	}
	return worst;
}
