<script lang="ts">
	import '../app.css';
	import favicon from '$lib/assets/favicon.svg';
	import { QueryClient, QueryClientProvider } from '@tanstack/svelte-query';
	import { Toaster } from '$lib/components/ui/sonner/index.js';
	import { PUBLIC_APP_TITLE } from '$env/static/public';
	import { SvelteQueryDevtools } from '@tanstack/svelte-query-devtools';
	import { ModeWatcher } from 'mode-watcher';

	let { children } = $props();

	const queryClient = new QueryClient({
		defaultOptions: {
			queries: {
				// ลบ enabled: browser ออก เพราะอาจทำให้ query ไม่ทำงาน
				staleTime: 60 * 1000, // 1 minute
				retry: 1
			}
		}
	});
</script>

<svelte:head>
	<link rel="icon" href={favicon} />
	<title>
		{PUBLIC_APP_TITLE}
	</title>
</svelte:head>

<!-- Dark is the field-operator default, with the existing mode toggle kept for daylight use. -->
<ModeWatcher defaultMode="dark" />

<Toaster position="top-right" richColors />

<QueryClientProvider client={queryClient}>
	{@render children?.()}
	<SvelteQueryDevtools />
</QueryClientProvider>
