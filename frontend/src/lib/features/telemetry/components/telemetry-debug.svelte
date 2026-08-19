<script lang="ts">
	/**
	 * Bring-up panel. Native <details> so it costs nothing when collapsed.
	 *
	 * Exists for the moment the dashboard shows nothing and you need to know *why*:
	 * is the URL wrong, is the socket open but silent, are frames arriving but failing
	 * validation? Each of those has a different fix and a different counter here.
	 */
	import { SIDE_LABELS } from '../schema';
	import type { SideView } from '../telemetry.svelte';
	import type { TelemetryLink } from '../schema';
	import type { SourceStatus } from '../source';

	interface Props {
		open?: boolean;
		endpoint: string;
		status: SourceStatus;
		attempt: number;
		retryInMs: number | null;
		messages: number;
		malformed: number;
		rateHz: number;
		dataAgeMs: number;
		link: TelemetryLink | null;
		sides: SideView[];
		lastRaw: string;
		lastError: string | null;
	}

	let {
		open = false,
		endpoint,
		status,
		attempt,
		retryInMs,
		messages,
		malformed,
		rateHz,
		dataAgeMs,
		link,
		sides,
		lastRaw,
		lastError
	}: Props = $props();

	const prettyRaw = $derived.by(() => {
		if (!lastRaw) return '(nothing received yet)';
		try {
			return JSON.stringify(JSON.parse(lastRaw), null, 2);
		} catch {
			return lastRaw;
		}
	});
</script>

<details {open} class="rounded-md border border-border text-xs">
	<summary class="cursor-pointer px-3 py-2 text-muted-foreground select-none">
		Debug / bring-up
	</summary>

	<div class="space-y-3 border-t px-3 py-3">
		<dl class="grid grid-cols-2 gap-x-4 gap-y-1 font-mono tabular-nums sm:grid-cols-3">
			<div><dt class="inline text-muted-foreground">endpoint</dt></div>
			<div class="col-span-1 truncate sm:col-span-2"><dd class="inline">{endpoint}</dd></div>

			<div><dt class="inline text-muted-foreground">status</dt></div>
			<div class="col-span-1 sm:col-span-2"><dd class="inline">{status}</dd></div>

			<div><dt class="inline text-muted-foreground">attempt</dt></div>
			<div class="col-span-1 sm:col-span-2">
				<dd class="inline">
					{attempt}{retryInMs === null ? '' : ` (retry in ${Math.ceil(retryInMs / 1000)}s)`}
				</dd>
			</div>

			<div><dt class="inline text-muted-foreground">messages</dt></div>
			<div class="col-span-1 sm:col-span-2"><dd class="inline">{messages}</dd></div>

			<div><dt class="inline text-muted-foreground">malformed</dt></div>
			<div class="col-span-1 sm:col-span-2">
				<dd class="inline" class:text-destructive={malformed > 0}>{malformed}</dd>
			</div>

			<div><dt class="inline text-muted-foreground">rate</dt></div>
			<div class="col-span-1 sm:col-span-2"><dd class="inline">{rateHz.toFixed(2)} Hz</dd></div>

			<div><dt class="inline text-muted-foreground">data age</dt></div>
			<div class="col-span-1 sm:col-span-2">
				<dd class="inline">
					{Number.isFinite(dataAgeMs) ? `${dataAgeMs} ms` : '—'}
				</dd>
			</div>

			{#if link}
				<div><dt class="inline text-muted-foreground">dropped</dt></div>
				<div class="col-span-1 sm:col-span-2"><dd class="inline">{link.dropped_frames}</dd></div>

				<div><dt class="inline text-muted-foreground">parse errors</dt></div>
				<div class="col-span-1 sm:col-span-2"><dd class="inline">{link.parse_errors}</dd></div>
			{/if}

			{#if lastError}
				<div><dt class="inline text-muted-foreground">last error</dt></div>
				<div class="col-span-1 sm:col-span-2"><dd class="inline">{lastError}</dd></div>
			{/if}
		</dl>

		<div class="overflow-x-auto">
			<table class="w-full font-mono tabular-nums">
				<thead class="text-muted-foreground">
					<tr class="text-left">
						<th class="pr-3 font-normal">side</th>
						<th class="pr-3 font-normal">raw</th>
						<th class="pr-3 font-normal">intensity</th>
						<th class="pr-3 font-normal">smooth</th>
						<th class="pr-3 font-normal">level</th>
					</tr>
				</thead>
				<tbody>
					{#each sides as view (view.side)}
						<tr>
							<td class="pr-3">{SIDE_LABELS[view.side]}</td>
							<td class="pr-3">{Number.isFinite(view.raw) ? view.raw : '—'}</td>
							<td class="pr-3">{view.intensity.toFixed(3)}</td>
							<td class="pr-3">{view.smooth.toFixed(3)}</td>
							<td class="pr-3">{view.level}</td>
						</tr>
					{/each}
				</tbody>
			</table>
		</div>

		<div>
			<div class="mb-1 text-muted-foreground">last frame</div>
			<pre
				class="max-h-64 overflow-auto rounded bg-muted p-2 font-mono text-[11px]">{prettyRaw}</pre>
		</div>
	</div>
</details>
