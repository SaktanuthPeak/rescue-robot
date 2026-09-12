import {
	fetchCameraStatus,
	sendCameraControl,
	getCameraStreamUrl,
	getCameraSnapshotUrl
} from './api';
import type { CameraStatus } from './schema';

const FRAME_STALE_MS = 2500;

export class CameraStore {
	enabled = $state(true);
	isStreaming = $state(true);
	showHud = $state(true);
	showCrosshair = $state(true);
	status = $state<CameraStatus | null>(null);
	isLoading = $state(false);
	error = $state<string | null>(null);
	streamError = $state(false);
	streamReady = $state(false);
	cacheBuster = $state(Date.now());
	lastSnapshot = $state<string | null>(null);
	lastSnapshotAt = $state<number | null>(null);
	private pollTimer: ReturnType<typeof setInterval> | null = null;

	hasBackendSignal = $derived.by(() => {
		if (this.error || !this.status || !this.status.active || !this.enabled || !this.isStreaming) {
			return false;
		}
		if (this.status.frame_count === 0) return false;
		return this.status.last_frame_age_ms <= FRAME_STALE_MS;
	});

	connectionState = $derived.by<'connecting' | 'live' | 'simulated' | 'standby' | 'disconnected'>(
		() => {
			if (this.error || this.streamError) return 'disconnected';
			if (!this.status || this.isLoading) return 'connecting';
			if (!this.status.active || !this.enabled || !this.isStreaming) return 'standby';
			if (this.status.frame_count === 0) return 'connecting';
			if (this.status.last_frame_age_ms > FRAME_STALE_MS) return 'disconnected';
			if (!this.streamReady) return 'connecting';
			return this.status.is_hardware ? 'live' : 'simulated';
		}
	);
	isConnected = $derived(this.hasBackendSignal && this.streamReady && !this.streamError);

	streamUrl = $derived.by(() => {
		if (!this.hasBackendSignal || this.streamError) return null;
		return getCameraStreamUrl(this.cacheBuster);
	});

	isHardware = $derived(this.status?.is_hardware ?? false);
	source = $derived(this.status?.source ?? 'mock');
	width = $derived(this.status?.width ?? 640);
	height = $derived(this.status?.height ?? 480);
	fps = $derived(this.status?.fps ?? 15);
	resolutionLabel = $derived(`${this.width}×${this.height}`);

	async start() {
		this.isLoading = true;
		this.error = null;
		this.streamError = false;
		this.streamReady = false;
		try {
			const st = await fetchCameraStatus();
			this.status = st;
			this.enabled = st.active;
			this.isStreaming = st.active;
			if (!st.active || st.last_frame_age_ms > FRAME_STALE_MS) this.streamReady = false;
		} catch (err) {
			this.error = err instanceof Error ? err.message : 'Cannot reach camera backend';
		} finally {
			this.isLoading = false;
		}

		if (!this.pollTimer) {
			this.pollTimer = setInterval(() => this.checkStatus(), 4000);
		}
	}

	stop() {
		if (this.pollTimer) {
			clearInterval(this.pollTimer);
			this.pollTimer = null;
		}
	}

	async checkStatus() {
		try {
			const st = await fetchCameraStatus();
			this.status = st;
			this.error = null;
			if (!st.active || st.last_frame_age_ms > FRAME_STALE_MS) this.streamReady = false;
		} catch (err) {
			// Keep previous status but note error
			this.error = err instanceof Error ? err.message : 'Camera check failed';
		}
	}

	async togglePower() {
		const nextState = !this.enabled;
		this.isLoading = true;
		try {
			const updated = await sendCameraControl({ active: nextState });
			this.status = updated;
			this.enabled = updated.active;
			this.isStreaming = updated.active;
			this.streamError = false;
			this.streamReady = false;
			this.cacheBuster = Date.now();
			this.error = null;
		} catch (err) {
			this.error = err instanceof Error ? err.message : 'Control failed';
		} finally {
			this.isLoading = false;
		}
	}

	toggleHud() {
		this.showHud = !this.showHud;
	}

	toggleCrosshair() {
		this.showCrosshair = !this.showCrosshair;
	}

	refreshStream() {
		this.streamError = false;
		this.streamReady = false;
		this.cacheBuster = Date.now();
		this.checkStatus();
	}

	markStreamError() {
		this.streamError = true;
	}

	markStreamConnected() {
		this.streamError = false;
		this.streamReady = true;
	}

	async captureSnapshot(): Promise<string> {
		const url = getCameraSnapshotUrl();
		this.lastSnapshot = url;
		this.lastSnapshotAt = Date.now();
		return url;
	}
}

export const cameraStore = new CameraStore();
