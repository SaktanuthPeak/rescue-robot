import { PUBLIC_API_URL } from '$env/static/public';

import { robotStatusSchema, type RobotCommand, type RobotStatus } from './schema';

const BASE_URL = PUBLIC_API_URL || '';

async function request<T>(path: string, init?: RequestInit): Promise<T> {
	const response = await fetch(`${BASE_URL}${path}`, {
		headers: { 'Content-Type': 'application/json', ...(init?.headers ?? {}) },
		...init
	});

	const body = (await response.json().catch(() => null)) as unknown;
	if (!response.ok) {
		const detail = typeof body === 'object' && body !== null && 'detail' in body ? body.detail : null;
		throw new Error(typeof detail === 'string' ? detail : `Request failed (${response.status})`);
	}
	return body as T;
}

export async function getRobotStatus(): Promise<RobotStatus> {
	return robotStatusSchema.parse(await request('/v1/robot/status'));
}

export async function sendRobotCommand(command: RobotCommand): Promise<void> {
	await request('/v1/robot/command', {
		method: 'POST',
		body: JSON.stringify(command)
	});
}
