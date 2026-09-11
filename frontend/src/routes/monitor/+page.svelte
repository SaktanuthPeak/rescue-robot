<script lang="ts">
	import AppContainer from '$lib/components/app-container.svelte';
	import FieldMonitor from '$lib/features/telemetry/components/field-monitor.svelte';
</script>

<svelte:head>
	<title>Durian Bot | Field Monitor</title>
	<meta
		name="description"
		content="Durian Bot directional IR proximity monitor for orchard field operations."
	/>
</svelte:head>

<AppContainer>
	<FieldMonitor />
</AppContainer>
