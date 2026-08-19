/**
 * Transport interface for telemetry.
 *
 * Sources are deliberately transport-only: they hand raw text to `onRaw` and report
 * connection status. All parsing, Zod validation, and counting lives in the store, so
 * there is exactly one validation gate no matter where bytes came from. That is also why
 * the mock emits real JSON text rather than objects -- it exercises the same code path a
 * live socket does, including the malformed-frame branch.
 */

export type SourceStatus = 'idle' | 'connecting' | 'open' | 'reconnecting' | 'closed';

export interface SourceHandlers {
	/** One inbound message, still unparsed. */
	onRaw: (text: string) => void;
	/** Connection status changed. `detail` is surfaced in the UI when present. */
	onStatus: (status: SourceStatus, detail?: SourceStatusMeta) => void;
}

export interface SourceStatusMeta {
	/** Consecutive failed connection attempts. */
	attempt?: number;
	/** Epoch ms when the next retry fires, for a live countdown. */
	nextRetryAt?: number;
	/** Human-readable reason, e.g. a close code. */
	error?: string;
}

export interface TelemetrySource {
	readonly kind: 'ws' | 'mock';
	/** Idempotent. */
	start(handlers: SourceHandlers): void;
	stop(): void;
	/** Force an immediate reconnect, cancelling any pending backoff. */
	reconnectNow?(): void;
	/** Resolved endpoint, shown in the debug panel. */
	readonly describe: string;
}
