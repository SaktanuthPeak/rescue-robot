<script lang="ts">
	/** Numeric readout for one side. The precise counterpart to the graphic's shape. */
	import * as Card from '$lib/components/ui/card/index.js';
	import { cn } from '$lib/utils/shadcn';
	import { LEVEL_LABELS, levelClasses } from '../scale';
	import { SIDE_LABELS } from '../schema';
	import type { SideView } from '../telemetry.svelte';

	interface Props {
		view: SideView;
		/** Raw ADC full scale, shown alongside the raw value for context. */
		adcMax: number | null;
		stale?: boolean;
	}

	let { view, adcMax, stale = false }: Props = $props();

	const isCritical = $derived(view.level === 'critical' && !stale);
</script>

<Card.Root
	class={cn(
		'gap-0 py-3 transition-colors duration-200',
		isCritical && 'border-destructive ring-1 ring-destructive/30'
	)}
>
	<Card.Content class="px-3">
		<div class="flex items-center justify-between gap-2">
			<span class="text-xs font-semibold tracking-widest text-muted-foreground">
				{SIDE_LABELS[view.side]}
			</span>
			<span
				class={cn(
					'rounded border px-1.5 py-0.5 text-[10px] font-bold tracking-wider',
					levelClasses(stale ? 'clear' : view.level)
				)}
			>
				{stale ? '—' : LEVEL_LABELS[view.level]}
			</span>
		</div>

		<!-- tabular-nums stops the digits shifting width as the value changes -->
		<div class="mt-1 font-mono text-3xl leading-none font-semibold tabular-nums">
			{view.label}
		</div>

		<div class="mt-1 font-mono text-[11px] text-muted-foreground tabular-nums">
			{#if view.invalid || adcMax === null}
				raw —
			{:else}
				raw {view.raw}<span class="opacity-50">/{adcMax}</span>
			{/if}
		</div>
	</Card.Content>
</Card.Root>
