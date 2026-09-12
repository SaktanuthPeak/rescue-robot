<script lang="ts">
	import { Camera, CameraOff, Sparkles } from 'lucide-svelte';
	import {
		Card,
		CardContent,
		CardDescription,
		CardHeader,
		CardTitle
	} from '$lib/components/ui/card/index.js';
	import CameraViewport from './camera-viewport.svelte';
	import { cameraStore } from '../camera.svelte';

	interface Props {
		strongestDirection?: string | null;
		bearingDeg?: number | null;
		compact?: boolean;
	}

	let { strongestDirection = null, bearingDeg = null, compact = false }: Props = $props();

	$effect(() => {
		cameraStore.start();
		return () => cameraStore.stop();
	});
</script>

<Card
	class={`camera-card overflow-hidden rounded-sm border-border/80 bg-card ${compact ? 'compact' : ''}`}
>
	<CardHeader class={compact ? 'p-3 pb-2' : 'pb-3'}>
		<div class="flex flex-wrap items-center justify-between gap-2">
			<div class="flex items-center gap-2">
				<div class="rounded-md bg-primary/10 p-1.5 text-primary">
					<Camera class={compact ? 'h-3.5 w-3.5' : 'h-4 w-4'} />
				</div>
				<div>
					<CardTitle class={compact ? 'text-sm font-semibold' : 'text-base font-semibold'}
						>Orchard camera</CardTitle
					>
					<CardDescription class={compact ? 'text-[10px]' : 'text-xs'}>
						Raspberry Pi feed ({cameraStore.resolutionLabel} @ {cameraStore.fps} FPS)
						{#if cameraStore.connectionState === 'standby'}
							· standby
						{:else if !cameraStore.isConnected}
							· disconnected
						{/if}
					</CardDescription>
				</div>
			</div>

			<div class="flex items-center gap-1.5 text-xs">
				{#if cameraStore.connectionState === 'live'}
					<span
						class="inline-flex items-center gap-1 rounded-full bg-emerald-500/10 px-2 py-0.5 text-xs font-medium text-emerald-500"
					>
						<span class="h-1.5 w-1.5 rounded-full bg-emerald-500"></span>
						Field camera
					</span>
				{:else if cameraStore.connectionState === 'simulated'}
					<span
						class="inline-flex items-center gap-1 rounded-full bg-amber-500/10 px-2 py-0.5 text-xs font-medium text-amber-500"
					>
						<Sparkles class="h-3 w-3" />
						Simulated feed
					</span>
				{:else if cameraStore.connectionState === 'standby'}
					<span
						class="inline-flex items-center gap-1 rounded-full bg-zinc-500/10 px-2 py-0.5 text-xs font-medium text-zinc-400"
					>
						<span class="h-1.5 w-1.5 rounded-full bg-zinc-400"></span>
						Disabled
					</span>
				{:else if cameraStore.connectionState === 'connecting'}
					<span
						class="inline-flex items-center gap-1 rounded-full bg-amber-500/10 px-2 py-0.5 text-xs font-medium text-amber-500"
					>
						<span class="h-1.5 w-1.5 animate-pulse rounded-full bg-amber-500"></span>
						Connecting
					</span>
				{:else}
					<span
						class="inline-flex items-center gap-1 rounded-full bg-red-500/10 px-2 py-0.5 text-xs font-medium text-red-400"
					>
						<CameraOff class="h-3 w-3" />
						Disconnected
					</span>
				{/if}
			</div>
		</div>
	</CardHeader>
	<CardContent class={compact ? 'p-2 pt-0' : 'p-3 pt-0'}>
		<CameraViewport {strongestDirection} {bearingDeg} {compact} />
	</CardContent>
</Card>
