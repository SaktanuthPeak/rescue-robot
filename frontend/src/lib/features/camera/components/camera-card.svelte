<script lang="ts">
	import { Camera, Sparkles } from 'lucide-svelte';
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
	}

	let { strongestDirection = null, bearingDeg = null }: Props = $props();

	$effect(() => {
		cameraStore.start();
		return () => cameraStore.stop();
	});
</script>

<Card class="overflow-hidden border-border/80 bg-card">
	<CardHeader class="pb-3">
		<div class="flex flex-wrap items-center justify-between gap-2">
			<div class="flex items-center gap-2">
				<div class="rounded-md bg-primary/10 p-1.5 text-primary">
					<Camera class="h-4 w-4" />
				</div>
				<div>
					<CardTitle class="text-base font-semibold">RPi Camera Feed — สตรีมภาพสด</CardTitle>
					<CardDescription class="text-xs">
						Raspberry Pi MJPEG Low-Latency Stream ({cameraStore.resolutionLabel} @ {cameraStore.fps} FPS)
					</CardDescription>
				</div>
			</div>

			<div class="flex items-center gap-1.5 text-xs">
				{#if cameraStore.enabled}
					{#if cameraStore.isHardware}
						<span
							class="inline-flex items-center gap-1 rounded-full bg-emerald-500/10 px-2 py-0.5 text-xs font-medium text-emerald-500"
						>
							<span class="h-1.5 w-1.5 rounded-full bg-emerald-500"></span>
							Hardware Camera
						</span>
					{:else}
						<span
							class="inline-flex items-center gap-1 rounded-full bg-amber-500/10 px-2 py-0.5 text-xs font-medium text-amber-500"
						>
							<Sparkles class="h-3 w-3" />
							Simulated Mode
						</span>
					{/if}
				{:else}
					<span
						class="inline-flex items-center gap-1 rounded-full bg-zinc-500/10 px-2 py-0.5 text-xs font-medium text-zinc-400"
					>
						<span class="h-1.5 w-1.5 rounded-full bg-zinc-400"></span>
						Disabled
					</span>
				{/if}
			</div>
		</div>
	</CardHeader>
	<CardContent class="p-3 pt-0">
		<CameraViewport {strongestDirection} {bearingDeg} />
	</CardContent>
</Card>
