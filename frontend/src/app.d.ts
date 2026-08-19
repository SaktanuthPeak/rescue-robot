// See https://svelte.dev/docs/kit/types#app.d.ts

// No App.Locals here: this is a static SPA (`ssr = false` in routes/+layout.ts), so
// there is no server request lifecycle for locals to live in. The API client is a plain
// module singleton in $lib/api/client.ts instead.

declare global {
	namespace App {
		// interface Error {}
		// interface PageData {}
		// interface PageState {}
		// interface Platform {}
		namespace Superforms {
			type Message = {
				type: 'error' | 'success';
				text: string;
				description?: string;
			};
		}
	}
}

export {};
