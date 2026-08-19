<script lang="ts">
	/**
	 * Four *orthogonal* status chips.
	 *
	 * Keeping them separate is the point. Conflating "backend down", "socket down",
	 * "no Arduino", and "data is old" into one indicator is what makes robot dashboards
	 * hard to debug -- each of those has a different fix, and right now the interesting
	 * one is SERIAL, because there may be no board attached.
	 */
	import RefreshCw from '@lucide/svelte/icons/refresh-cw';
	import Moon from '@lucide/svelte/icons/moon';
	import Sun from '@lucide/svelte/icons/sun';
	import { mode, toggleMode } from 'mode-watcher';

	import { Button } from '$lib/components/ui/button/index.js';
	import { cn } from '$lib/utils/shadcn';
	import type { TelemetryLink, DeviceStatus } from '../schema';
	import type { SourceStatus } from '../source';

	interface Props {
		apiOk: boolean | null;
		wsStatus: SourceStatus;
		link: TelemetryLink | null;
		deviceStatus: DeviceStatus | null;
		dataAgeMs: number;
		stale: boolean;
		rateHz: number;
		attempt: number;
		retryInMs: number | null;
		simulated: boolean;
		onReconnect: () => void;
	}

	let {
		apiOk,
		wsStatus,
		link,
		deviceStatus,
		dataAgeMs,
		stale,
		rateHz,
		attempt,
		retryInMs,
		simulated,
		onReconnect
	}: Props = $props();

	type Tone = 'ok' | 'warn' | 'bad' | 'idle';

	const toneClasses: Record<Tone, string> = {
		ok: 'border-flame-0/40 bg-flame-0/10 text-foreground',
		warn: 'border-stale/50 bg-stale/10 text-stale',
		bad: 'border-destructive/50 bg-destructive/10 text-destructive',
		idle: 'border-border bg-muted text-muted-foreground'
	};

	const apiTone = $derived<Tone>(apiOk === null ? 'idle' : apiOk ? 'ok' : 'bad');
	const apiText = $derived(apiOk === null ? 'checking' : apiOk ? 'up' : 'down');

	const wsTone = $derived<Tone>(
		wsStatus === 'open'
			? 'ok'
			: wsStatus === 'reconnecting'
				? 'warn'
				: wsStatus === 'closed'
					? 'bad'
					: 'idle'
	);
	const wsText = $derived.by(() => {
		if (wsStatus === 'reconnecting') {
			const secs = retryInMs === null ? null : Math.ceil(retryInMs / 1000);
			return secs === null
				? `retrying (attempt ${attempt})`
				: `retry in ${secs}s (attempt ${attempt})`;
		}
		return wsStatus;
	});

	const serialTone = $derived<Tone>(
		link === null
			? 'idle'
			: link.state !== 'streaming'
				? 'bad'
				: link.source === 'mock'
					? 'warn'
					: 'ok'
	);
	const serialText = $derived(link === null ? '—' : `${link.source} · ${link.state}`);

	const dataTone = $derived<Tone>(!Number.isFinite(dataAgeMs) ? 'idle' : stale ? 'bad' : 'ok');
	const dataText = $derived.by(() => {
		if (!Number.isFinite(dataAgeMs)) return 'no data';
		if (stale) return `${(dataAgeMs / 1000).toFixed(1)}s old`;
		return `live · ${rateHz.toFixed(1)} Hz`;
	});
</script>

<div class="flex flex-wrap items-center gap-2">
	{#snippet chip(label: string, value: string, tone: Tone, pulse = false)}
		<span
			class={cn(
				'inline-flex items-center gap-1.5 rounded-md border px-2 py-1 text-xs',
				toneClasses[tone]
			)}
		>
			{#if pulse}
				<span
					class="inline-block size-1.5 animate-pulse rounded-full bg-current motion-reduce:animate-none"
				></span>
			{/if}
			<span class="font-semibold tracking-wider opacity-70">{label}</span>
			<span class="font-mono tabular-nums">{value}</span>
		</span>
	{/snippet}

	{@render chip('API', apiText, apiTone)}
	{@render chip('WS', wsText, wsTone)}
	{@render chip('SERIAL', serialText, serialTone)}
	{@render chip('DATA', dataText, dataTone, dataTone === 'ok')}

	{#if deviceStatus && deviceStatus !== 'OK'}
		{@render chip('DEVICE', deviceStatus, deviceStatus === 'FAULT' ? 'bad' : 'warn')}
	{/if}

	{#if simulated}
		{@render chip('MODE', 'simulated', 'warn')}
	{/if}

	<div class="ml-auto flex items-center gap-1">
		<Button variant="outline" size="sm" onclick={onReconnect} class="h-7 gap-1.5 text-xs">
			<RefreshCw class="size-3.5" />
			Reconnect
		</Button>
		<Button
			variant="ghost"
			size="icon"
			onclick={toggleMode}
			class="size-7"
			aria-label="Toggle colour theme"
		>
			{#if mode.current === 'dark'}
				<Sun class="size-3.5" />
			{:else}
				<Moon class="size-3.5" />
			{/if}
		</Button>
	</div>
</div>
