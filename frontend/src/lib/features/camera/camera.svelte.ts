import {
	fetchCameraStatus,
	sendCameraControl,
	getCameraStreamUrl,
	getCameraSnapshotUrl
} from './api';
import type { CameraStatus } from './schema';

export class CameraStore {
	enabled = $state(true);
	isStreaming = $state(true);
	showHud = $state(true);
	showCrosshair = $state(true);
	status = $state<CameraStatus | null>(null);
	isLoading = $state(false);
	error = $state<string | null>(null);
	cacheBuster = $state(Date.now());
	lastSnapshot = $state<string | null>(null);
	lastSnapshotAt = $state<number | null>(null);
	private pollTimer: ReturnType<typeof setInterval> | null = null;

	streamUrl = $derived.by(() => {
		if (!this.enabled || !this.isStreaming) return null;
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
		try {
			const st = await fetchCameraStatus();
			this.status = st;
			this.enabled = st.active;
			this.isStreaming = st.active;
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
		this.cacheBuster = Date.now();
		this.checkStatus();
	}

	async captureSnapshot(): Promise<string> {
		const url = getCameraSnapshotUrl();
		this.lastSnapshot = url;
		this.lastSnapshotAt = Date.now();
		return url;
	}
}

export const cameraStore = new CameraStore();
