import { PUBLIC_API_URL } from '$env/static/public';
import type { CameraConfig, CameraControlPayload, CameraStatus } from './schema';

const BASE_URL = PUBLIC_API_URL || '';

export async function fetchCameraStatus(): Promise<CameraStatus> {
	const res = await fetch(`${BASE_URL}/v1/camera/status`);
	if (!res.ok) {
		throw new Error(`Failed to fetch camera status: ${res.statusText}`);
	}
	return res.json();
}

export async function sendCameraControl(payload: CameraControlPayload): Promise<CameraStatus> {
	const res = await fetch(`${BASE_URL}/v1/camera/control`, {
		method: 'POST',
		headers: {
			'Content-Type': 'application/json'
		},
		body: JSON.stringify(payload)
	});
	if (!res.ok) {
		throw new Error(`Failed to send camera control: ${res.statusText}`);
	}
	return res.json();
}

export async function fetchCameraConfig(): Promise<CameraConfig> {
	const res = await fetch(`${BASE_URL}/v1/camera/config`);
	if (!res.ok) {
		throw new Error(`Failed to fetch camera config: ${res.statusText}`);
	}
	return res.json();
}

export function getCameraStreamUrl(cacheBuster?: number): string {
	return `${BASE_URL}/v1/camera/stream${cacheBuster ? `?t=${cacheBuster}` : ''}`;
}

export function getCameraSnapshotUrl(): string {
	return `${BASE_URL}/v1/camera/snapshot?t=${Date.now()}`;
}
