export type CameraSourceMode = 'auto' | 'v4l2' | 'picam' | 'mock';

export interface CameraStatus {
	active: boolean;
	source: CameraSourceMode;
	device: string;
	width: number;
	height: number;
	fps: number;
	frame_count: number;
	last_frame_age_ms: number;
	is_hardware: boolean;
}

export interface CameraControlPayload {
	active?: boolean;
	width?: number;
	height?: number;
	fps?: number;
}

export interface CameraConfig {
	default_device: string;
	default_source: string;
	width: number;
	height: number;
	fps: number;
	auto_start: boolean;
}
