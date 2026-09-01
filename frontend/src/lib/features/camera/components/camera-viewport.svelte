<script lang="ts">
	import { toast } from 'svelte-sonner';
	import {
		Camera,
		CameraOff,
		Crosshair,
		Eye,
		EyeOff,
		Flame,
		Maximize2,
		Minimize2,
		Power,
		RefreshCw,
		Sparkles
	} from 'lucide-svelte';

	import { Button } from '$lib/components/ui/button/index.js';
	import { cameraStore } from '../camera.svelte';

	interface Props {
		strongestDirection?: string | null;
		bearingDeg?: number | null;
	}

	let { strongestDirection = null, bearingDeg = null }: Props = $props();

	let containerEl = $state<HTMLDivElement | null>(null);
	let isFullscreen = $state(false);
	let imgError = $state(false);
	let flash = $state(false);

	function toggleFullscreen() {
		if (!containerEl) return;
		if (!document.fullscreenElement) {
			containerEl
				.requestFullscreen?.()
				.then(() => {
					isFullscreen = true;
				})
				.catch(() => {
					// Fullscreen permission fallback
				});
		} else {
			document.exitFullscreen?.().then(() => {
				isFullscreen = false;
			});
		}
	}

	function handleImgError() {
		imgError = true;
	}

	function handleImgLoad() {
		imgError = false;
	}

	async function handleSnapshot() {
		flash = true;
		setTimeout(() => (flash = false), 250);
		try {
			const url = await cameraStore.captureSnapshot();
			toast.success('Camera Snapshot Captured', {
				description: 'บันทึกภาพจากกล้องสำเร็จ'
			});
		} catch (err) {
			toast.error('Snapshot failed');
		}
	}

	function handleRefresh() {
		imgError = false;
		cameraStore.refreshStream();
		toast.info('Reloading video stream...');
	}
</script>

<div
	bind:this={containerEl}
	class="relative flex aspect-4/3 w-full flex-col items-center justify-center overflow-hidden rounded-lg border border-border bg-black/95 text-white shadow-inner select-none"
>
	<!-- Flash animation on snapshot -->
	{#if flash}
		<div
			class="pointer-events-none absolute inset-0 z-50 animate-out bg-white/80 duration-300 fade-out"
		></div>
	{/if}

	<!-- Video Stream Display -->
	{#if cameraStore.enabled && cameraStore.streamUrl && !imgError}
		<img
			src={cameraStore.streamUrl}
			alt="FireBot Live Camera Feed"
			class="h-full w-full object-contain"
			onerror={handleImgError}
			onload={handleImgLoad}
		/>
	{:else if !cameraStore.enabled}
		<!-- Camera Powered Off Graphic -->
		<div
			class="flex flex-col items-center justify-center gap-3 p-6 text-center text-muted-foreground"
		>
			<div class="rounded-full bg-secondary/20 p-4 text-muted-foreground">
				<CameraOff class="h-10 w-10 opacity-70" />
			</div>
			<div>
				<p class="font-medium text-foreground">กล้องปิดการทำงาน (Camera Standby)</p>
				<p class="text-xs text-muted-foreground">
					กดปุ่ม "เปิดกล้อง" เพื่อเริ่มรับภาพสตรีมสดจาก Raspberry Pi
				</p>
			</div>
			<Button
				variant="default"
				size="sm"
				class="gap-1.5"
				onclick={() => cameraStore.togglePower()}
				disabled={cameraStore.isLoading}
			>
				<Power class="h-4 w-4 text-emerald-400" />
				เปิดกล้อง (Turn ON)
			</Button>
		</div>
	{:else}
		<!-- Stream Disconnected / Connecting Error State -->
		<div
			class="flex flex-col items-center justify-center gap-3 p-6 text-center text-muted-foreground"
		>
			<div class="rounded-full bg-destructive/10 p-4 text-destructive">
				<RefreshCw class="h-8 w-8 animate-spin" />
			</div>
			<div>
				<p class="font-medium text-foreground">กำลังเชื่อมต่อสัญญาณกล้อง...</p>
				<p class="text-xs text-muted-foreground">
					{cameraStore.error ?? 'รอการส่งข้อมูลภาพจาก Raspberry Pi MJPEG stream'}
				</p>
			</div>
			<Button variant="outline" size="sm" class="gap-1 text-xs" onclick={handleRefresh}>
				<RefreshCw class="h-3.5 w-3.5" />
				ลองใหม่อีกครั้ง
			</Button>
		</div>
	{/if}

	<!-- Tactical HUD Overlay -->
	{#if cameraStore.enabled && cameraStore.showHud && !imgError}
		<!-- Top Bar: Status, Source Mode, Resolution & Clock -->
		<div
			class="pointer-events-none absolute inset-x-0 top-0 flex items-center justify-between p-3 text-xs"
		>
			<div class="flex items-center gap-2">
				{#if cameraStore.isHardware}
					<span
						class="inline-flex items-center gap-1.5 rounded-md bg-emerald-500/20 px-2 py-0.5 font-semibold text-emerald-400 backdrop-blur-xs"
					>
						<span class="h-2 w-2 animate-pulse rounded-full bg-emerald-400"></span>
						LIVE (HW)
					</span>
				{:else}
					<span
						class="inline-flex items-center gap-1.5 rounded-md bg-amber-500/20 px-2 py-0.5 font-semibold text-amber-300 backdrop-blur-xs"
					>
						<span class="h-2 w-2 animate-pulse rounded-full bg-amber-400"></span>
						SIMULATED HUD
					</span>
				{/if}
				<span class="rounded bg-black/60 px-2 py-0.5 font-mono text-zinc-300 backdrop-blur-xs">
					{cameraStore.resolutionLabel} · {cameraStore.fps} FPS
				</span>
			</div>

			<!-- Strongest Flame Heading (if detected) -->
			{#if strongestDirection}
				<div
					class="flex items-center gap-1 rounded border border-red-500/30 bg-red-950/80 px-2 py-0.5 font-medium text-red-300 backdrop-blur-xs"
				>
					<Flame class="h-3.5 w-3.5 animate-bounce text-red-400" />
					<span>ไฟ: {strongestDirection.toUpperCase()}</span>
					{#if bearingDeg !== null}
						<span>({Math.round(bearingDeg)}°)</span>
					{/if}
				</div>
			{/if}
		</div>

		<!-- Center Crosshair Reticle -->
		{#if cameraStore.showCrosshair}
			<div class="pointer-events-none absolute inset-0 flex items-center justify-center">
				<div class="relative h-20 w-20">
					<!-- Center dot -->
					<div class="absolute inset-0 m-auto h-1.5 w-1.5 rounded-full bg-emerald-400/80"></div>
					<!-- Corner brackets -->
					<div
						class="absolute top-0 left-0 h-4 w-4 border-t-2 border-l-2 border-emerald-400/70"
					></div>
					<div
						class="absolute top-0 right-0 h-4 w-4 border-t-2 border-r-2 border-emerald-400/70"
					></div>
					<div
						class="absolute bottom-0 left-0 h-4 w-4 border-b-2 border-l-2 border-emerald-400/70"
					></div>
					<div
						class="absolute right-0 bottom-0 h-4 w-4 border-r-2 border-b-2 border-emerald-400/70"
					></div>
				</div>
			</div>
		{/if}

		<!-- Bottom HUD Bar -->
		<div
			class="pointer-events-none absolute inset-x-0 bottom-12 flex items-end justify-between px-3 font-mono text-[11px] text-zinc-400"
		>
			<div class="rounded bg-black/60 px-1.5 py-0.5 backdrop-blur-xs">
				DEVICE: {cameraStore.status?.device ?? '/dev/video0'}
			</div>
			<div class="rounded bg-black/60 px-1.5 py-0.5 backdrop-blur-xs">
				FRAMES: {cameraStore.status?.frame_count ?? 0}
			</div>
		</div>
	{/if}

	<!-- Bottom Control Action Strip -->
	<div
		class="absolute inset-x-0 bottom-0 flex items-center justify-between border-t border-white/10 bg-black/80 p-2 backdrop-blur-sm"
	>
		<div class="flex items-center gap-1.5">
			<!-- Power Toggle Button -->
			<Button
				variant={cameraStore.enabled ? 'secondary' : 'default'}
				size="sm"
				class="h-8 gap-1 text-xs"
				onclick={() => cameraStore.togglePower()}
				disabled={cameraStore.isLoading}
			>
				<Power class="h-3.5 w-3.5 {cameraStore.enabled ? 'text-emerald-400' : 'text-zinc-400'}" />
				{cameraStore.enabled ? 'ปิดกล้อง' : 'เปิดกล้อง'}
			</Button>

			<!-- Snapshot Capture Button -->
			{#if cameraStore.enabled}
				<Button
					variant="ghost"
					size="sm"
					class="h-8 gap-1 text-xs text-white hover:bg-white/10"
					onclick={handleSnapshot}
				>
					<Camera class="h-3.5 w-3.5 text-blue-400" />
					บันทึกภาพ
				</Button>
			{/if}
		</div>

		<div class="flex items-center gap-1">
			<!-- Toggle HUD Overlays -->
			<Button
				variant="ghost"
				size="icon"
				class="h-8 w-8 text-white hover:bg-white/10"
				title={cameraStore.showHud ? 'ซ่อน HUD' : 'แสดง HUD'}
				onclick={() => cameraStore.toggleHud()}
			>
				{#if cameraStore.showHud}
					<Eye class="h-4 w-4 text-emerald-400" />
				{:else}
					<EyeOff class="h-4 w-4 text-zinc-400" />
				{/if}
			</Button>

			<!-- Toggle Crosshair -->
			<Button
				variant="ghost"
				size="icon"
				class="h-8 w-8 text-white hover:bg-white/10"
				title="เปิด/ปิด เป้าเล็ง (Crosshair)"
				onclick={() => cameraStore.toggleCrosshair()}
			>
				<Crosshair
					class="h-4 w-4 {cameraStore.showCrosshair ? 'text-emerald-400' : 'text-zinc-400'}"
				/>
			</Button>

			<!-- Refresh Stream -->
			<Button
				variant="ghost"
				size="icon"
				class="h-8 w-8 text-white hover:bg-white/10"
				title="รีเฟรชสตรีม"
				onclick={handleRefresh}
			>
				<RefreshCw class="h-4 w-4 text-zinc-300" />
			</Button>

			<!-- Fullscreen Toggle -->
			<Button
				variant="ghost"
				size="icon"
				class="h-8 w-8 text-white hover:bg-white/10"
				title="ขยายเต็มจอ"
				onclick={toggleFullscreen}
			>
				{#if isFullscreen}
					<Minimize2 class="h-4 w-4" />
				{:else}
					<Maximize2 class="h-4 w-4" />
				{/if}
			</Button>
		</div>
	</div>
</div>
