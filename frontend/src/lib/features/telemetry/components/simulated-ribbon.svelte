<script lang="ts">
	/**
	 * Persistent marker that the numbers on screen are not real.
	 *
	 * Non-dismissible and deliberately in a non-alarm colour: it must never be mistaken
	 * for a fire alert, and it must never be possible to forget it is there. Shown for a
	 * client-side mock *and* when the backend reports `link.source === 'mock'`.
	 */
	import TriangleAlert from '@lucide/svelte/icons/triangle-alert';

	interface Props {
		detail?: string;
	}

	let { detail }: Props = $props();
</script>

<div
	class="flex items-center gap-2 rounded-md border border-stale/50 px-3 py-2 text-xs text-stale"
	style="background-image: repeating-linear-gradient(45deg, color-mix(in oklab, var(--stale), transparent 88%) 0 8px, transparent 8px 16px);"
	role="status"
>
	<TriangleAlert class="size-4 shrink-0" />
	<span class="font-bold tracking-widest uppercase">simulated data</span>
	<span class="opacity-80">
		— ไม่ใช่ค่าจากเซนเซอร์จริง{detail ? ` (${detail})` : ''}
	</span>
</div>
