/** Asymmetric envelope follower. Dependency-free and unit-testable. */

import { DT_MAX_MS, DT_MIN_MS, TAU_ATTACK_MS, TAU_DECAY_MS } from './constants';

/**
 * A fast-attack / slow-decay exponential follower.
 *
 * Analog flame readings are noisy and real flame flickers, so raw values make the
 * dashboard jitter. Symmetric smoothing would fix the jitter but also blunt genuine
 * spikes -- unacceptable for fire detection. Asymmetric time constants give both: a real
 * rise lands within roughly one frame (60ms), while dips decay over 600ms so noise and
 * flicker cannot collapse a wedge and re-grow it.
 *
 * `alpha` is derived from the measured `dt` rather than fixed, so behaviour is identical
 * at 10Hz, 5Hz, or with jitter.
 */
export class EnvelopeFollower {
	private value: number;
	private initialised = false;

	constructor(initial = 0) {
		this.value = initial;
	}

	get current(): number {
		return this.value;
	}

	update(input: number, dtMs: number): number {
		if (!Number.isFinite(input)) return this.value;

		// First real sample jumps straight to the value -- easing up from 0 would look
		// like a phantom ramp on page load.
		if (!this.initialised) {
			this.initialised = true;
			this.value = input;
			return this.value;
		}

		const dt = Math.min(Math.max(dtMs, DT_MIN_MS), DT_MAX_MS);
		const tau = input > this.value ? TAU_ATTACK_MS : TAU_DECAY_MS;
		const alpha = 1 - Math.exp(-dt / tau);
		this.value += alpha * (input - this.value);
		return this.value;
	}

	reset(value = 0): void {
		this.value = value;
		this.initialised = false;
	}
}
