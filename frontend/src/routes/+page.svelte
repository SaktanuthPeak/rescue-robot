<script lang="ts">
	import { resolve } from '$app/paths';
	import { Bot, Droplets, MapPinned, ShieldCheck } from 'lucide-svelte';

	import AppContainer from '$lib/components/app-container.svelte';
	import FieldMonitor from '$lib/features/telemetry/components/field-monitor.svelte';
	import { authStore } from '$lib/stores/auth.svelte';
	import LogoutButton from '$lib/features/login/components/logout-button.svelte';
</script>

<svelte:head>
	<title>Durian Bot | Operator Console</title>
	<meta
		name="description"
		content="Durian Bot single-page orchard operator console with directional IR monitoring."
	/>
</svelte:head>

<AppContainer>
	<section
		class="relative isolate overflow-hidden rounded-sm border border-border/80 bg-card shadow-[0_24px_80px_color-mix(in_oklab,var(--foreground),transparent_88%)]"
	>
		<img
			src="/durian-bot-orchard.png"
			alt="Durian Bot spraying between durian trees"
			class="absolute inset-0 -z-20 h-full w-full object-cover object-center opacity-45"
		/>
		<div class="absolute inset-0 -z-10 bg-gradient-to-r from-card via-card/90 to-card/35"></div>
		<div
			class="absolute inset-0 -z-10 bg-[linear-gradient(90deg,transparent_0%,color-mix(in_oklab,var(--primary),transparent_94%)_100%)]"
		></div>

		<div class="grid gap-8 p-6 sm:p-9 lg:grid-cols-[1.2fr_0.8fr] lg:items-end lg:p-12">
			<div>
				<div
					class="mb-7 flex items-center gap-2 font-mono text-[10px] tracking-[0.2em] text-primary uppercase"
				>
					<span
						class="size-2 rounded-full bg-primary shadow-[0_0_0_4px_color-mix(in_oklab,var(--primary),transparent_84%)]"
					></span>
					One screen / field ready
				</div>
				<div class="mb-4 flex items-center gap-3 text-muted-foreground">
					<Bot class="size-5 text-primary" strokeWidth={1.6} />
					<span class="font-mono text-[11px] tracking-[0.18em] uppercase"
						>Durian Bot operator console</span
					>
				</div>
				<h1 class="max-w-2xl text-4xl leading-[0.94] font-black tracking-[-0.07em] sm:text-6xl">
					One field view.<br />Every safe move.
				</h1>
				<p class="mt-5 max-w-xl text-sm leading-relaxed text-muted-foreground sm:text-base">
					อ่านทิศทางรอบตัวหุ่นยนต์ ดูแนวต้นทุเรียน และตัดสินใจเดินหรือฉีดน้ำจากหน้าเดียว
				</p>
			</div>

			<div
				class="grid grid-cols-2 gap-px overflow-hidden rounded-sm border border-white/15 bg-white/15 font-mono text-[10px] tracking-[0.12em] uppercase backdrop-blur-sm"
			>
				<div class="bg-black/50 p-4 text-white">
					<Droplets class="mb-6 size-4 text-primary" />
					<span class="block text-white/55">Mission</span>
					<strong class="mt-1 block text-sm">Water the row</strong>
				</div>
				<div class="bg-black/50 p-4 text-white">
					<MapPinned class="mb-6 size-4 text-primary" />
					<span class="block text-white/55">Navigation</span>
					<strong class="mt-1 block text-sm">Track the lane</strong>
				</div>
				<div class="bg-black/50 p-4 text-white">
					<ShieldCheck class="mb-6 size-4 text-primary" />
					<span class="block text-white/55">Safety</span>
					<strong class="mt-1 block text-sm">IR stop assist</strong>
				</div>
				<div class="bg-black/50 p-4 text-white">
					<Bot class="mb-6 size-4 text-primary" />
					<span class="block text-white/55">Unit</span>
					<strong class="mt-1 block text-sm">Field robot</strong>
				</div>
			</div>
		</div>
	</section>

	<div class="flex flex-wrap items-center justify-between gap-3 border-b border-border/70 pb-4">
		<div>
			<p class="font-mono text-[10px] tracking-[0.18em] text-primary uppercase">Live operations</p>
			<p class="mt-1 text-sm text-muted-foreground">ระบบควบคุมและ telemetry รวมอยู่ในหน้าเดียว</p>
		</div>
		<div class="flex items-center gap-2">
			{#if authStore.isAuthenticated}
				<span class="max-w-36 truncate font-mono text-xs text-muted-foreground"
					>{authStore.user?.username ?? 'Signed in'}</span
				>
				<LogoutButton />
			{:else}
				<a
					class="inline-flex min-h-9 items-center rounded-sm border border-border bg-background px-3 py-2 text-xs font-semibold transition-colors hover:bg-accent"
					href={resolve('/login')}>Sign in to operate</a
				>
			{/if}
		</div>
	</div>

	<FieldMonitor />
</AppContainer>
