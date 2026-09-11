<script lang="ts">
	import { resolve } from '$app/paths';
	import {
		ArrowUpRight,
		BatteryCharging,
		Bot,
		Droplets,
		MapPinned,
		ShieldCheck
	} from 'lucide-svelte';
	import { useHealthStatus } from '$lib/features/health/queries';
	import AppContainer from '$lib/components/app-container.svelte';
	import { authStore } from '$lib/stores/auth.svelte';
	import LogoutButton from '$lib/features/login/components/logout-button.svelte';

	const healthQuery = useHealthStatus();
</script>

<svelte:head>
	<title>Durian Bot | Orchard Control</title>
	<meta
		name="description"
		content="Durian Bot orchard robot control and IR proximity monitoring dashboard."
	/>
</svelte:head>

<AppContainer>
	<section
		class="grid min-h-[min(720px,calc(100dvh-120px))] overflow-hidden rounded-sm border border-border/80 bg-card shadow-[0_24px_80px_color-mix(in_oklab,var(--foreground),transparent_88%)] lg:grid-cols-[0.86fr_1.14fr]"
	>
		<div class="relative z-10 flex flex-col justify-between gap-10 p-6 sm:p-9 lg:p-12">
			<div>
				<div
					class="mb-8 flex items-center gap-2 font-mono text-[10px] tracking-[0.2em] text-primary uppercase"
				>
					<span
						class="size-2 rounded-full bg-primary shadow-[0_0_0_4px_color-mix(in_oklab,var(--primary),transparent_84%)]"
					></span>
					System ready
				</div>
				<div class="mb-5 flex items-center gap-3 text-muted-foreground">
					<Bot class="size-5 text-primary" strokeWidth={1.6} />
					<span class="font-mono text-[11px] tracking-[0.18em] uppercase"
						>Field unit / durian orchard</span
					>
				</div>
				<h1
					class="max-w-xl text-5xl leading-[0.94] font-black tracking-[-0.07em] text-foreground sm:text-6xl"
				>
					Water the row.<br />
					Keep the canopy healthy.
				</h1>
				<p class="mt-6 max-w-md text-base leading-relaxed text-muted-foreground">
					Durian Bot helps operators guide a field robot, watch IR proximity, and keep the spray run
					visible.
				</p>
			</div>

			<div
				class="grid max-w-md grid-cols-2 border-y border-border/70 py-4 font-mono text-[11px] uppercase sm:grid-cols-3"
			>
				<div class="border-r border-border/70 pr-3">
					<span class="block text-muted-foreground">Drive</span>
					<strong class="mt-1 block text-primary">Ready</strong>
				</div>
				<div class="px-3 sm:border-r sm:border-border/70">
					<span class="block text-muted-foreground">IR array</span>
					<strong class="mt-1 block text-primary">Online</strong>
				</div>
				<div
					class="col-span-2 mt-4 border-t border-border/70 pt-4 sm:col-span-1 sm:mt-0 sm:border-t-0 sm:pt-0 sm:pl-3"
				>
					<span class="block text-muted-foreground">Spray</span>
					<strong class="mt-1 block text-foreground">Standby</strong>
				</div>
			</div>

			<div class="flex flex-wrap items-center gap-3">
				<a
					class="inline-flex min-h-11 items-center gap-2 rounded-sm bg-primary px-4 py-3 text-sm font-bold text-primary-foreground transition-transform duration-200 hover:-translate-y-0.5 active:translate-y-px"
					href={resolve('/monitor')}
				>
					Open monitor
					<ArrowUpRight class="size-4" />
				</a>
				<a
					class="inline-flex min-h-11 items-center gap-2 rounded-sm border border-border bg-background/50 px-4 py-3 text-sm font-semibold text-foreground transition-colors hover:bg-accent"
					href={resolve('/control')}
				>
					Drive the unit
				</a>
			</div>
		</div>

		<div
			class="relative min-h-[320px] overflow-hidden border-t border-border/70 lg:min-h-0 lg:border-t-0 lg:border-l"
		>
			<img
				src="/durian-bot-orchard.png"
				alt="Durian Bot spraying between durian trees"
				class="absolute inset-0 h-full w-full object-cover"
			/>
			<div
				class="absolute inset-0 bg-gradient-to-r from-card via-card/10 to-transparent lg:from-card/90 lg:via-card/20"
			></div>
			<div
				class="absolute right-5 bottom-5 left-5 grid gap-2 font-mono text-[10px] tracking-[0.14em] uppercase sm:grid-cols-3"
			>
				<div class="border border-white/20 bg-black/45 p-3 text-white backdrop-blur-sm">
					<Droplets class="mb-5 size-4 text-primary" />
					<span class="block text-white/60">Spray loop</span>
					<strong class="mt-1 block text-sm">Manual control</strong>
				</div>
				<div class="border border-white/20 bg-black/45 p-3 text-white backdrop-blur-sm">
					<MapPinned class="mb-5 size-4 text-primary" />
					<span class="block text-white/60">Route</span>
					<strong class="mt-1 block text-sm">Row tracking</strong>
				</div>
				<div class="border border-white/20 bg-black/45 p-3 text-white backdrop-blur-sm">
					<ShieldCheck class="mb-5 size-4 text-primary" />
					<span class="block text-white/60">Safety</span>
					<strong class="mt-1 block text-sm">IR stop assist</strong>
				</div>
			</div>
		</div>
	</section>

	<section class="grid gap-3 sm:grid-cols-2 lg:grid-cols-3">
		<div class="border border-border/70 bg-card/75 p-4">
			<div class="mb-6 flex items-center justify-between">
				<span class="font-mono text-[10px] tracking-[0.16em] text-muted-foreground uppercase"
					>Backend service</span
				>
				<span class={`size-2 rounded-full ${healthQuery.isError ? 'bg-destructive' : 'bg-primary'}`}
				></span>
			</div>
			<strong class="block text-lg"
				>{healthQuery.isLoading ? 'Checking' : healthQuery.isError ? 'Offline' : 'Healthy'}</strong
			>
			<p class="mt-1 text-sm text-muted-foreground">Telemetry and control gateway</p>
		</div>
		<div class="border border-border/70 bg-card/75 p-4">
			<div class="mb-6 flex items-center justify-between">
				<span class="font-mono text-[10px] tracking-[0.16em] text-muted-foreground uppercase"
					>Power rail</span
				>
				<BatteryCharging class="size-4 text-primary" />
			</div>
			<strong class="block text-lg">Waiting for live value</strong>
			<p class="mt-1 text-sm text-muted-foreground">Battery appears on the control page</p>
		</div>
		<div class="border border-border/70 bg-card/75 p-4 sm:col-span-2 lg:col-span-1">
			<div class="mb-6 flex items-center justify-between">
				<span class="font-mono text-[10px] tracking-[0.16em] text-muted-foreground uppercase"
					>Operator</span
				>
				<span class="font-mono text-[10px] text-primary uppercase">Access</span>
			</div>
			{#if authStore.isAuthenticated}
				<strong class="block truncate text-lg">{authStore.user?.username ?? 'Signed in'}</strong>
				<div class="mt-2"><LogoutButton /></div>
			{:else}
				<strong class="block text-lg">Guest mode</strong>
				<a
					class="mt-2 inline-flex text-sm font-semibold text-primary underline-offset-4 hover:underline"
					href={resolve('/login')}>Sign in to operate</a
				>
			{/if}
		</div>
	</section>
</AppContainer>
