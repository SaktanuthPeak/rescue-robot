/** Intensity -> colour, opacity, level, and label. Pure functions, no runes. */

import { LEVEL_ENTER, LEVEL_EXIT, SATURATION_INTENSITY, type Level } from './constants';

/**
 * Blend across the four flame stops in oklab.
 *
 * oklab interpolation (not sRGB) is what keeps the amber->orange->red midpoints from
 * going muddy grey. Because the stops are CSS custom properties, a theme switch
 * repaints every wedge for free -- no JS involved.
 *
 * The blend percentage is rounded to a whole number so a jittering sensor does not
 * rewrite the style string on every single frame.
 */
export function heatColor(intensity: number): string {
	const clamped = Math.min(Math.max(intensity, 0), 1);
	const scaled = clamped * 3; // 3 gaps between 4 stops
	const lower = Math.min(Math.floor(scaled), 2);
	const upper = lower + 1;
	const mix = Math.round((scaled - lower) * 100);
	return `color-mix(in oklab, var(--flame-${lower}), var(--flame-${upper}) ${mix}%)`;
}

/** Low readings whisper, high readings shout. */
export function fillOpacity(intensity: number): number {
	const clamped = Math.min(Math.max(intensity, 0), 1);
	return Math.round((0.18 + 0.62 * clamped) * 1000) / 1000;
}

/**
 * Advance the level with hysteresis: rising uses the ENTER thresholds, falling uses the
 * lower EXIT thresholds. Without this, a value resting on a boundary oscillates and
 * every oscillation is an alert.
 */
export function nextLevel(previous: Level, intensity: number): Level {
	if (!Number.isFinite(intensity)) return 'clear';

	const order: Level[] = ['clear', 'watch', 'warn', 'critical'];
	const currentIndex = order.indexOf(previous);

	// Escalate as far as the ENTER thresholds allow.
	let target: Level = 'clear';
	if (intensity >= LEVEL_ENTER.critical) target = 'critical';
	else if (intensity >= LEVEL_ENTER.warn) target = 'warn';
	else if (intensity >= LEVEL_ENTER.watch) target = 'watch';

	if (order.indexOf(target) > currentIndex) return target;

	// De-escalate only once the value drops past the EXIT threshold of the current level.
	if (previous === 'critical') return intensity < LEVEL_EXIT.critical ? 'warn' : 'critical';
	if (previous === 'warn') return intensity < LEVEL_EXIT.warn ? 'watch' : 'warn';
	if (previous === 'watch') return intensity < LEVEL_EXIT.watch ? 'clear' : 'watch';
	return 'clear';
}

/**
 * Format an intensity for display.
 *
 * `MAX` rather than a number at saturation: a pegged sensor could mean direct flame, a
 * blinded sensor, or a wiring fault, and those are indistinguishable -- printing "99%"
 * would imply a precision that does not exist. Invalid readings render `--`, never 0,
 * because 0 reads as "no fire".
 */
export function formatPercent(intensity: number | null | undefined): string {
	if (intensity === null || intensity === undefined || !Number.isFinite(intensity)) {
		return '--';
	}
	if (intensity >= SATURATION_INTENSITY) return 'MAX';
	return `${Math.round(intensity * 100)}%`;
}

/** True when a channel's raw reading cannot be trusted. */
export function isInvalidReading(raw: number, adcMax: number): boolean {
	return !Number.isFinite(raw) || raw < 0 || raw > adcMax;
}

/** Tailwind classes for a level chip. */
export function levelClasses(level: Level): string {
	switch (level) {
		case 'critical':
			return 'bg-destructive/15 text-destructive border-destructive/40';
		case 'warn':
			return 'bg-flame-2/15 text-flame-2 border-flame-2/40';
		case 'watch':
			return 'bg-flame-1/15 text-flame-1 border-flame-1/40';
		default:
			return 'bg-muted text-muted-foreground border-border';
	}
}

export const LEVEL_LABELS: Record<Level, string> = {
	clear: 'CLEAR',
	watch: 'WATCH',
	warn: 'WARN',
	critical: 'FIRE'
};
