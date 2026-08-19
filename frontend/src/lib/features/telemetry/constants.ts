/** Tunables for the flame monitor. Grouped here so rate-dependent values are reviewable. */

import type { Side } from './schema';

// ---- geometry (SVG user units, one 400x400 viewBox) ---------------------------------

export const VIEWBOX = 400;
export const CX = 200;
export const CY = 200;

/**
 * Must clear the track corner at hypot(71, 75) ~= 103, and leave room for the side
 * labels at R_SIDE_LABEL -- the LEFT/RIGHT labels are horizontal, so they need more
 * radial clearance than their radius alone suggests.
 */
export const R_INNER = 124;
/** Leaves 10px for the critical pulse stroke inside the viewBox. */
export const R_MAX = 190;

/** 74deg of arc with 16deg gutters, so it reads as 4 discrete sensors, not a scanner. */
export const WEDGE_SPAN_DEG = 74;

/** 0deg is up (vehicle front); angles increase clockwise. */
export const SIDE_ANGLE_DEG: Record<Side, number> = {
	front: 0,
	right: 90,
	rear: 180,
	left: 270
};

/** Dotted reference arcs. Without these the graphic is a glow, not a gauge. */
export const GRID_INTENSITIES = [0.25, 0.5, 0.75] as const;

/** Radius for the side labels, in the clear ring between the hull and the wedges. */
export const R_SIDE_LABEL = 102;

// ---- levels -------------------------------------------------------------------------

export const LEVELS = ['clear', 'watch', 'warn', 'critical'] as const;
export type Level = (typeof LEVELS)[number];

/**
 * Asymmetric enter/exit thresholds. Hysteresis is what stops a value hovering at a
 * boundary from flapping the level -- and therefore from spamming alerts.
 * Evaluated against the SMOOTHED intensity.
 */
export const LEVEL_ENTER: Record<Exclude<Level, 'clear'>, number> = {
	watch: 0.15,
	warn: 0.4,
	critical: 0.7
};
export const LEVEL_EXIT: Record<Exclude<Level, 'clear'>, number> = {
	watch: 0.1,
	warn: 0.33,
	critical: 0.6
};

/** At or above this, show MAX instead of a percentage -- the reading is saturated. */
export const SATURATION_INTENSITY = 0.98;

/** Below this the bearing estimate is noise, so the needle stays hidden. */
export const BEARING_MIN_INTENSITY = 0.15;

// ---- smoothing ----------------------------------------------------------------------

/**
 * Envelope follower time constants. Fast attack so a real flame spike lands within about
 * one frame; slow decay so sensor noise and flame flicker cannot make a wedge collapse
 * and re-grow.
 */
export const TAU_ATTACK_MS = 60;
export const TAU_DECAY_MS = 600;

/** Clamp dt so a resumed background tab converges over a few frames instead of jumping. */
export const DT_MIN_MS = 1;
export const DT_MAX_MS = 1000;

// ---- freshness ----------------------------------------------------------------------
// Tuned for the backend's ~10Hz broadcast (~12x and ~40x the frame period). Raise these
// proportionally if the broadcast rate drops.

export const STALE_MS = 1200;
export const VERY_STALE_MS = 4000;

/** Drives the age counters and countdowns. */
export const TICK_MS = 250;

// ---- reconnect ----------------------------------------------------------------------

export const BACKOFF_BASE_MS = 500;
export const BACKOFF_FACTOR = 1.8;
export const BACKOFF_MAX_MS = 15_000;

/** Only after this many consecutive failures does the dev-only mock hint appear. */
export const MOCK_HINT_AFTER_ATTEMPTS = 3;

// ---- alerts -------------------------------------------------------------------------

/** Per-side cooldown, so a flame sitting at the boundary cannot spam toasts. */
export const ALERT_COOLDOWN_MS = 10_000;

// ---- misc ---------------------------------------------------------------------------

/** WebSocket path on the backend. */
export const WS_PATH = '/v1/telemetry/ws';
