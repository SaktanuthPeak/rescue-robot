/** WebSocket transport with jittered exponential backoff. */

import { PUBLIC_API_URL } from '$env/static/public';

import { BACKOFF_BASE_MS, BACKOFF_FACTOR, BACKOFF_MAX_MS, WS_PATH } from './constants';
import type { SourceHandlers, TelemetrySource } from './source';

/**
 * Derive the WebSocket URL from PUBLIC_API_URL so `.env` stays the single source of
 * truth -- a second env var for the same backend is one more thing to get out of sync.
 */
export function resolveWsUrl(apiUrl: string = PUBLIC_API_URL): string {
	const url = new URL(apiUrl);
	url.protocol = url.protocol === 'https:' ? 'wss:' : 'ws:';
	url.pathname = WS_PATH;
	url.search = '';
	url.hash = '';
	return url.toString();
}

export class WsTelemetrySource implements TelemetrySource {
	readonly kind = 'ws' as const;

	private socket: WebSocket | null = null;
	private handlers: SourceHandlers | null = null;
	private retryTimer: ReturnType<typeof setTimeout> | null = null;
	private attempt = 0;
	private stopped = true;
	/** Set once a frame actually arrives; gates the backoff reset. */
	private sawFrame = false;
	private url = '';
	private onlineListener: (() => void) | null = null;

	get describe(): string {
		return this.url || '(not yet resolved)';
	}

	start(handlers: SourceHandlers): void {
		if (!this.stopped) return;
		this.stopped = false;
		this.handlers = handlers;
		// Resolved here rather than at module scope so nothing touches `window`/env
		// during import.
		this.url = resolveWsUrl();

		this.onlineListener = () => {
			// Network came back -- do not make the user wait out a 15s backoff.
			if (!this.stopped && !this.socket) this.reconnectNow();
		};
		window.addEventListener('online', this.onlineListener);

		this.connect();
	}

	stop(): void {
		this.stopped = true;
		this.clearRetry();
		if (this.onlineListener) {
			window.removeEventListener('online', this.onlineListener);
			this.onlineListener = null;
		}
		this.teardownSocket();
		this.handlers?.onStatus('closed');
	}

	reconnectNow(): void {
		if (this.stopped) return;
		this.clearRetry();
		this.teardownSocket();
		this.attempt = 0;
		this.connect();
	}

	private clearRetry(): void {
		if (this.retryTimer !== null) {
			clearTimeout(this.retryTimer);
			this.retryTimer = null;
		}
	}

	private teardownSocket(): void {
		if (!this.socket) return;
		// Drop handlers before closing so a close event cannot schedule a reconnect for
		// a socket we are deliberately discarding.
		this.socket.onopen = null;
		this.socket.onmessage = null;
		this.socket.onerror = null;
		this.socket.onclose = null;
		try {
			this.socket.close();
		} catch {
			// Already closing; nothing to do.
		}
		this.socket = null;
	}

	private connect(): void {
		if (this.stopped) return;

		this.handlers?.onStatus(this.attempt === 0 ? 'connecting' : 'reconnecting', {
			attempt: this.attempt
		});

		let socket: WebSocket;
		try {
			socket = new WebSocket(this.url);
		} catch (error) {
			this.scheduleRetry(error instanceof Error ? error.message : 'bad WebSocket URL');
			return;
		}
		this.socket = socket;
		this.sawFrame = false;

		socket.onopen = () => {
			// Deliberately NOT resetting `attempt` here. A server that accepts and
			// immediately drops would otherwise produce a hot reconnect loop; the reset
			// waits for evidence of a real frame.
			this.handlers?.onStatus('open', { attempt: this.attempt });
		};

		socket.onmessage = (event) => {
			if (typeof event.data !== 'string') return;
			if (!this.sawFrame) {
				this.sawFrame = true;
				this.attempt = 0;
			}
			this.handlers?.onRaw(event.data);
		};

		socket.onerror = () => {
			// Browsers give no detail here; onclose carries the actionable code.
		};

		socket.onclose = (event) => {
			this.socket = null;
			if (this.stopped) return;
			const reason = event.reason || `code ${event.code}`;
			this.scheduleRetry(reason);
		};
	}

	private scheduleRetry(error?: string): void {
		if (this.stopped) return;

		const exponential = BACKOFF_BASE_MS * Math.pow(BACKOFF_FACTOR, this.attempt);
		const capped = Math.min(exponential, BACKOFF_MAX_MS);
		// Full jitter: without it, every client that dropped together retries together.
		const delay = Math.round(capped * (0.5 + Math.random() * 0.5));
		this.attempt += 1;

		this.handlers?.onStatus('reconnecting', {
			attempt: this.attempt,
			nextRetryAt: Date.now() + delay,
			error
		});

		this.clearRetry();
		// Unlimited attempts on purpose: a monitoring dashboard should never give up.
		this.retryTimer = setTimeout(() => {
			this.retryTimer = null;
			this.connect();
		}, delay);
	}
}
