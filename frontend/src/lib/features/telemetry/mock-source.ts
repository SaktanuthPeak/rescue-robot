/**
 * Client-side telemetry simulator.
 *
 * The backend has its own mock, but this one exists for two things the backend's cannot
 * do: let the dashboard be developed with the backend entirely down, and provide
 * *deterministic scenarios* that drive the visuals into specific states on demand.
 *
 * It emits real JSON text through `onRaw`, so mock data goes through exactly the same Zod
 * validation path a live socket does.
 */

import { TELEMETRY_SCHEMA_VERSION, SIDES, type Side } from './schema';
import type { SourceHandlers, TelemetrySource } from './source';

export const MOCK_SCENARIOS = ['approach', 'spike', 'dropout'] as const;
export type MockScenario = (typeof MOCK_SCENARIOS)[number];

export function parseScenario(value: string | null): MockScenario | null {
	if (!value) return null;
	if (value === '1' || value === 'true' || value === 'on') return 'approach';
	return (MOCK_SCENARIOS as readonly string[]).includes(value)
		? (value as MockScenario)
		: 'approach';
}

const ADC_MAX = 1023;
const THRESHOLD = 400;
const TICK_MS = 100;
const BEAM_EXPONENT = 2.2;
const BACKSCATTER = 0.03;
const NOISE_SIGMA = 0.02;

/** Small fixed offsets so the readings do not look synthetically symmetric. */
const SENSOR_OFFSET: Record<Side, number> = {
	front: 0.012,
	right: -0.008,
	rear: 0.019,
	left: -0.004
};

const SIDE_BEARING_RAD: Record<Side, number> = {
	front: 0,
	right: Math.PI / 2,
	rear: Math.PI,
	left: (3 * Math.PI) / 2
};

/** Box-Muller, so noise looks like sensor noise rather than uniform hash. */
function gaussian(sigma: number): number {
	const u = Math.max(Math.random(), Number.EPSILON);
	const v = Math.random();
	return sigma * Math.sqrt(-2 * Math.log(u)) * Math.cos(2 * Math.PI * v);
}

export class MockTelemetrySource implements TelemetrySource {
	readonly kind = 'mock' as const;

	private timer: ReturnType<typeof setInterval> | null = null;
	private handlers: SourceHandlers | null = null;
	private elapsedMs = 0;
	private seq = 0;

	constructor(private readonly scenario: MockScenario = 'approach') {}

	get describe(): string {
		return `mock:${this.scenario}`;
	}

	start(handlers: SourceHandlers): void {
		if (this.timer !== null) return;
		this.handlers = handlers;
		this.elapsedMs = 0;
		this.seq = 0;
		handlers.onStatus('open');
		this.timer = setInterval(() => this.tick(), TICK_MS);
	}

	stop(): void {
		if (this.timer !== null) {
			clearInterval(this.timer);
			this.timer = null;
		}
		this.handlers?.onStatus('closed');
	}

	/** In the dropout scenario, telemetry goes silent for 4s out of every 10s. */
	private isSilent(): boolean {
		if (this.scenario !== 'dropout') return false;
		return this.elapsedMs % 10_000 >= 6000;
	}

	private tick(): void {
		this.elapsedMs += TICK_MS;
		if (this.isSilent()) return; // emit nothing: the store must detect staleness

		this.seq = (this.seq + 1) % 65536;
		this.handlers?.onRaw(JSON.stringify(this.buildFrame()));
	}

	private sourceStrength(): number {
		const t = this.elapsedMs / 1000;
		if (this.scenario === 'spike') {
			// Quiet, then a hard 300ms spike every 5s -- verifies fast attack / slow decay.
			return this.elapsedMs % 5000 < 300 ? 0.95 : 0.08;
		}
		// Slowly breathing source, so intensities are not a flat ring.
		return 0.55 + 0.42 * Math.sin(t * 0.31);
	}

	private bearingRad(): number {
		if (this.scenario === 'spike') return 0; // stays on the front sensor
		// One full orbit every 20s.
		return ((this.elapsedMs / 20_000) % 1) * 2 * Math.PI;
	}

	private buildFrame(): unknown {
		const strength = this.sourceStrength();
		const bearing = this.bearingRad();

		const flame: Record<string, { raw: number; intensity: number; detected: boolean }> = {};
		let anyDetected = false;
		let strongest: Side | null = null;
		let strongestIntensity = 0;

		for (const side of SIDES) {
			const delta = Math.cos(bearing - SIDE_BEARING_RAD[side]);
			const lobe = delta > 0 ? Math.pow(delta, BEAM_EXPONENT) : 0;
			let intensity = lobe > 0 ? strength * lobe : BACKSCATTER;
			intensity += SENSOR_OFFSET[side] + gaussian(NOISE_SIGMA);
			intensity = Math.min(Math.max(intensity, 0), 1);

			// Round-trip through raw ADC with active-low polarity, matching real hardware.
			const raw = Math.round((1 - intensity) * ADC_MAX);
			const detected = raw <= THRESHOLD;
			if (detected) {
				anyDetected = true;
				if (intensity > strongestIntensity) {
					strongestIntensity = intensity;
					strongest = side;
				}
			}

			flame[side] = {
				raw,
				intensity: Math.round(intensity * 1000) / 1000,
				detected
			};
		}

		return {
			type: 'telemetry',
			v: TELEMETRY_SCHEMA_VERSION,
			ts: Date.now(),
			seq: this.seq,
			status: 'OK',
			adc_max: ADC_MAX,
			link: {
				source: 'mock',
				state: 'streaming',
				last_frame_age_ms: 0,
				dropped_frames: 0,
				parse_errors: 0
			},
			flame,
			flame_detected: anyDetected,
			strongest_direction: strongest
		};
	}
}
