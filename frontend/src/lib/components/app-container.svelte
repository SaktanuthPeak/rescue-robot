<script lang="ts">
	import { resolve } from '$app/paths';
	import { Bot, LayoutDashboard, RadioTower, Sprout } from 'lucide-svelte';

	let { children } = $props();
</script>

<div class="min-h-[100dvh] px-3 py-3 text-foreground sm:px-5 sm:py-5">
	<header
		class="mx-auto mb-5 flex max-w-[1400px] items-center justify-between gap-4 border-b border-border/70 pb-3"
	>
		<a
			href={resolve('/')}
			class="group flex min-w-0 items-center gap-3"
			aria-label="Durian Bot home"
		>
			<span
				class="grid size-10 shrink-0 place-items-center rounded-sm border border-primary/45 bg-primary/10 text-primary transition-transform duration-200 group-hover:-rotate-3"
			>
				<Bot class="size-5" strokeWidth={1.8} />
			</span>
			<span class="min-w-0">
				<span class="block truncate text-sm font-black tracking-[0.16em] text-foreground uppercase"
					>Durian Bot</span
				>
				<span
					class="block truncate font-mono text-[10px] tracking-[0.16em] text-muted-foreground uppercase"
					>Orchard field unit</span
				>
			</span>
		</a>

		<nav
			class="flex items-center gap-1 font-mono text-[10px] tracking-[0.12em] uppercase"
			aria-label="Primary"
		>
			<a
				class="hidden items-center gap-2 rounded-sm px-3 py-2 text-muted-foreground transition-colors hover:bg-accent hover:text-foreground sm:flex"
				href={resolve('/monitor')}
			>
				<LayoutDashboard class="size-3.5" />
				Monitor
			</a>
			<a
				class="hidden items-center gap-2 rounded-sm px-3 py-2 text-muted-foreground transition-colors hover:bg-accent hover:text-foreground sm:flex"
				href={resolve('/control')}
			>
				<RadioTower class="size-3.5" />
				Control
			</a>
			<span class="hidden items-center gap-1.5 border-l border-border/70 pl-3 text-primary md:flex">
				<Sprout class="size-3.5" />
				IR ready
			</span>
		</nav>
	</header>

	<main class="mx-auto flex max-w-[1400px] flex-col gap-5">
		{@render children?.()}
	</main>
</div>
