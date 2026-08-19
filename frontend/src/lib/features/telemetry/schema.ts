/**
 * Wire contract for flame telemetry. This is the frontend's source of truth.
 *
 * Every inbound frame runs through `safeParse` rather than a bare type assertion: the
 * payload originates from a parser reading a flaky serial line, and a truncated or
 * malformed frame must never be allowed to corrupt the dashboard. A `type`-only
 * declaration would give compile-time comfort and zero runtime protection.
 *
 * The backend declares the same shape as `response_model=TelemetryFrame` on
 * `GET /v1/telemetry`, so `pnpm openapi` documents it and drift is visible. The `v`
 * field is what makes a real mismatch loud instead of silently misread.
 *
 * See docs/firebot-spec.md for the normative definition.
 */

import { z } from 'zod';

/** Sensor order matches Arduino pins A0..A3 and the FB1 line protocol. */
export const SIDES = ['front', 'right', 'rear', 'left'] as const;
export type Side = (typeof SIDES)[number];

/** Bump only alongside the backend's TELEMETRY_PROTOCOL_VERSION. */
export const TELEMETRY_SCHEMA_VERSION = 1;

export const flameChannelSchema = z.object({
	/** Untouched ADC reading. Debug readout only -- never drive visuals from this. */
	raw: z.number().int(),
	/** Polarity-normalised, 1.0 = strongest flame. Drive all visuals from this. */
	intensity: z.number(),
	/** Threshold crossing, decided backend-side so polarity lives in one place. */
	detected: z.boolean()
});

export const flameChannelsSchema = z.object({
	front: flameChannelSchema,
	right: flameChannelSchema,
	rear: flameChannelSchema,
	left: flameChannelSchema
});

export const linkStateSchema = z.enum(['streaming', 'connecting', 'disconnected']);
export const sourceKindSchema = z.enum(['mock', 'serial']);
export const deviceStatusSchema = z.enum(['OK', 'WARN', 'FAULT']);

export const telemetryLinkSchema = z.object({
	/** 'mock' means simulated data -- the UI must say so visibly. */
	source: sourceKindSchema,
	/** Anything other than 'streaming' means the gauges are not live. */
	state: linkStateSchema,
	/** Measured server-side with a monotonic clock, so clock skew cannot fake freshness. */
	last_frame_age_ms: z.number().int(),
	dropped_frames: z.number().int(),
	parse_errors: z.number().int()
});

export const telemetryFrameSchema = z.object({
	type: z.literal('telemetry'),
	v: z.number().int(),
	/** Epoch milliseconds, when the backend received the sample. */
	ts: z.number().int(),
	seq: z.number().int(),
	status: deviceStatusSchema,
	/** ADC full scale, so the UI never hardcodes 1023. */
	adc_max: z.number().int(),
	link: telemetryLinkSchema,
	flame: flameChannelsSchema,
	flame_detected: z.boolean(),
	strongest_direction: z.enum(SIDES).nullable()
});

export const telemetryConfigSchema = z.object({
	source: sourceKindSchema,
	adc_max: z.number().int(),
	threshold: z.number().int(),
	active_low: z.boolean(),
	/** The threshold on the same 0..1 scale as intensity, for drawing a gauge marker. */
	threshold_intensity: z.number(),
	mock_interval_ms: z.number().int(),
	stale_after_ms: z.number().int()
});

export type FlameChannel = z.infer<typeof flameChannelSchema>;
export type FlameChannels = z.infer<typeof flameChannelsSchema>;
export type TelemetryLink = z.infer<typeof telemetryLinkSchema>;
export type TelemetryFrame = z.infer<typeof telemetryFrameSchema>;
export type TelemetryConfig = z.infer<typeof telemetryConfigSchema>;
export type LinkState = z.infer<typeof linkStateSchema>;
export type SourceKind = z.infer<typeof sourceKindSchema>;
export type DeviceStatus = z.infer<typeof deviceStatusSchema>;

/** Human labels for the four sides. */
export const SIDE_LABELS: Record<Side, string> = {
	front: 'FRONT',
	right: 'RIGHT',
	rear: 'REAR',
	left: 'LEFT'
};
