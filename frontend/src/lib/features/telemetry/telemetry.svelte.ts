/**
 * Reactive telemetry store.
 *
 * Deliberately side-effect-free: it never raises toasts. Level hysteresis lives here and
 * notification lives in the page, so the two are independently testable and the store can
 * be driven from a test without a UI.
 *
 * Not a TanStack Query: Query models request/response caching, and pushing 10 frames a
 * second through `setQueryData` would thrash the cache and devtools for no benefit.
 */

import {
	MOCK_HINT_AFTER_ATTEMPTS,
	STALE_MS,
	TICK_MS,
	VERY_STALE_MS,
	type Level
} from './constants';
import { estimateBearing, highestLevel, type BearingEstimate } from './bearing';
import { MockTelemetrySource, type MockScenario } from './mock-source';
import { fillOpacity, heatColor, isInvalidReading, nextLevel, formatPercent } from './scale';
import {
	SIDES,
	TELEMETRY_SCHEMA_VERSION,
	telemetryFrameSchema,
	type Side,
	type TelemetryFrame
} from './schema';
import { EnvelopeFollower } from './smoothing';
import type { SourceStatus, SourceStatusMeta, TelemetrySource } from './source';
import { WsTelemetrySource } from './ws-source';

/** Per-side view model consumed directly by the components. */
export interface SideView {
	side: Side;
	raw: number;
	intensity: number;
	smooth: number;
	level: Level;
	heat: string;
	opacity: number;
	label: string;
	invalid: boolean;
}

function zeroBySide(): Record<Side, number> {
	return { front: 0, right: 0, rear: 0, left: 0 };
}

function clearBySide(): Record<Side, Level> {
	return { front: 'clear', right: 'clear', rear: 'clear', left: 'clear' };
}

interface TelemetryState {
	status: SourceStatus;
	frame: TelemetryFrame | null;
	lastMessageAt: number | null;
	now: number;
	attempt: number;
	nextRetryAt: number | null;
	lastError: string | null;
	messages: number;
	malformed: number;
	rateHz: number;
	versionMismatch: number | null;
	sourceKind: 'ws' | 'mock' | null;
	smooth: Record<Side, number>;
	levels: Record<Side, Level>;
}

class TelemetryStore {
	private state = $state<TelemetryState>({
		status: 'idle',
		frame: null,
		lastMessageAt: null,
		now: Date.now(),
		attempt: 0,
		nextRetryAt: null,
		lastError: null,
		messages: 0,
		malformed: 0,
		rateHz: 0,
		versionMismatch: null,
		sourceKind: null,
		smooth: zeroBySide(),
		levels: clearBySide()
	});

	/** Followers are plain objects, deliberately outside $state -- they are not view data. */
	private followers: Record<Side, EnvelopeFollower> = {
		front: new EnvelopeFollower(),
		right: new EnvelopeFollower(),
		rear: new EnvelopeFollower(),
		left: new EnvelopeFollower()
	};

	private source: TelemetrySource | null = null;
	private ticker: ReturnType<typeof setInterval> | null = null;
	private lastRawText = '';

	// ---- lifecycle ------------------------------------------------------------------

	/** Idempotent, so HMR and repeated effect runs cannot stack sockets. */
	start(options: { mock?: MockScenario | null } = {}): void {
		if (this.source) return;

		const source: TelemetrySource = options.mock
			? new MockTelemetrySource(options.mock)
			: new WsTelemetrySource();

		this.source = source;
		this.state.sourceKind = source.kind;
		source.start({
			onRaw: (text) => this.ingest(text),
			onStatus: (status, meta) => this.onStatus(status, meta)
		});

		this.ticker = setInterval(() => {
			this.state.now = Date.now();
		}, TICK_MS);
	}

	stop(): void {
		this.source?.stop();
		this.source = null;
		if (this.ticker !== null) {
			clearInterval(this.ticker);
			this.ticker = null;
		}
		this.state.status = 'idle';
		this.state.sourceKind = null;
	}

	reconnectNow(): void {
		this.source?.reconnectNow?.();
	}

	// ---- ingest ---------------------------------------------------------------------

	private onStatus(status: SourceStatus, meta?: SourceStatusMeta): void {
		this.state.status = status;
		if (meta?.attempt !== undefined) this.state.attempt = meta.attempt;
		this.state.nextRetryAt = meta?.nextRetryAt ?? null;
		if (meta?.error !== undefined) this.state.lastError = meta.error;
		this.state.now = Date.now();
	}

	private ingest(text: string): void {
		this.lastRawText = text;

		let json: unknown;
		try {
			json = JSON.parse(text);
		} catch {
			this.state.malformed += 1;
			return;
		}

		const parsed = telemetryFrameSchema.safeParse(json);
		if (!parsed.success) {
			// Keep the last good frame rather than blanking the dashboard on one bad line.
			this.state.malformed += 1;
			return;
		}

		const frame = parsed.data;
		if (frame.v !== TELEMETRY_SCHEMA_VERSION) {
			// Do not render numbers we may be misinterpreting.
			this.state.versionMismatch = frame.v;
			return;
		}
		this.state.versionMismatch = null;

		const now = Date.now();
		const dt = this.state.lastMessageAt === null ? TICK_MS : now - this.state.lastMessageAt;

		this.state.messages += 1;
		this.state.frame = frame;
		this.state.lastMessageAt = now;
		this.state.now = now;

		// EMA of the observed rate; a frozen Hz readout is itself a staleness tell.
		if (dt > 0) {
			const instant = 1000 / dt;
			this.state.rateHz =
				this.state.rateHz === 0 ? instant : this.state.rateHz * 0.8 + instant * 0.2;
		}

		const smooth = { ...this.state.smooth };
		const levels = { ...this.state.levels };
		for (const side of SIDES) {
			const channel = frame.flame[side];
			const invalid = isInvalidReading(channel.raw, frame.adc_max);
			// An invalid reading must not be smoothed toward 0 -- 0 reads as "no fire".
			if (invalid) continue;
			smooth[side] = this.followers[side].update(channel.intensity, dt);
			levels[side] = nextLevel(levels[side], smooth[side]);
		}
		this.state.smooth = smooth;
		this.state.levels = levels;
	}

	// ---- derived --------------------------------------------------------------------

	readonly dataAgeMs = $derived.by(() => {
		if (this.state.lastMessageAt === null) return Number.POSITIVE_INFINITY;
		return Math.max(this.state.now - this.state.lastMessageAt, 0);
	});

	readonly isStale = $derived.by(() => this.dataAgeMs > STALE_MS);
	readonly isVeryStale = $derived.by(() => this.dataAgeMs > VERY_STALE_MS);

	readonly isLive = $derived.by(
		() =>
			this.state.status === 'open' && !this.isStale && this.state.frame?.link.state === 'streaming'
	);

	readonly sides = $derived.by<SideView[]>(() => {
		const frame = this.state.frame;
		return SIDES.map((side) => {
			const channel = frame?.flame[side];
			const invalid = channel ? isInvalidReading(channel.raw, frame.adc_max) : true;
			const intensity = channel?.intensity ?? 0;
			const smooth = this.state.smooth[side];
			// Stale data must not present as a confident number.
			const displayed = invalid || this.isVeryStale ? null : intensity;
			return {
				side,
				raw: channel?.raw ?? Number.NaN,
				intensity,
				smooth,
				level: invalid || this.isStale ? 'clear' : this.state.levels[side],
				heat: heatColor(smooth),
				opacity: fillOpacity(smooth),
				label: formatPercent(displayed),
				invalid
			} satisfies SideView;
		});
	});

	readonly bearing = $derived.by<BearingEstimate>(() => estimateBearing(this.state.smooth));

	readonly maxLevel = $derived.by<Level>(() =>
		this.isStale ? 'clear' : highestLevel(this.state.levels)
	);

	readonly retryInMs = $derived.by(() => {
		if (this.state.nextRetryAt === null) return null;
		return Math.max(this.state.nextRetryAt - this.state.now, 0);
	});

	/** True when a mock is producing the data -- client-side or backend-side. */
	readonly isSimulated = $derived.by(
		() => this.state.sourceKind === 'mock' || this.state.frame?.link.source === 'mock'
	);

	readonly shouldHintMock = $derived.by(
		() =>
			this.state.sourceKind === 'ws' &&
			this.state.frame === null &&
			this.state.attempt >= MOCK_HINT_AFTER_ATTEMPTS
	);

	// ---- plain getters --------------------------------------------------------------

	get status(): SourceStatus {
		return this.state.status;
	}
	get frame(): TelemetryFrame | null {
		return this.state.frame;
	}
	get link() {
		return this.state.frame?.link ?? null;
	}
	get deviceStatus() {
		return this.state.frame?.status ?? null;
	}
	get attempt(): number {
		return this.state.attempt;
	}
	get messages(): number {
		return this.state.messages;
	}
	get malformed(): number {
		return this.state.malformed;
	}
	get rateHz(): number {
		return this.state.rateHz;
	}
	get versionMismatch(): number | null {
		return this.state.versionMismatch;
	}
	get lastError(): string | null {
		return this.state.lastError;
	}
	get sourceKind(): 'ws' | 'mock' | null {
		return this.state.sourceKind;
	}
	get levels(): Record<Side, Level> {
		return this.state.levels;
	}
	get endpoint(): string {
		return this.source?.describe ?? '(not started)';
	}
	get lastRaw(): string {
		return this.lastRawText;
	}
	get hasEverConnected(): boolean {
		return this.state.lastMessageAt !== null;
	}
}

export const telemetryStore = new TelemetryStore();
