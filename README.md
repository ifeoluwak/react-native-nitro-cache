# react-native-nitro-cache

**High-performance HTTP(S) file cache for React Native built for offline-friendly workflows (PDFs, EPUBs, audio, video, binaries), not a single UI component.**

Built from the ground up with **Nitro Modules** and C++ for speed and efficiency. Delivers a small API (`getOrFetch`, `get`, `getBuffer`, …) you can use from any feature: document viewers, parsers, media players, background sync.

New Architecture support.

## Why nitro-cache?

Offline-capable apps usually need **reliable on-disk caching for arbitrary HTTP(S) downloads**: manifests, templates, receipts, training packs, media, and other assets that must be available when connectivity drops not only assets rendered by a single UI primitive.

`nitro-cache` is a **general-purpose** cache for any HTTP(S) asset, built on **Nitro Modules / JSI** with a C++ core. It exposes introspection APIs (`getEntries`, `getStats`), direct buffer access (`getBuffer`), TTL + forced refresh, and returns **absolute file paths** your app can pass to native viewers and parsers.

## Features

- Simple API: easy to use focused primitives (`getOrFetch`, `get`, `remove`, `clear`, …)
- General-purpose: any HTTP(S) asset (PDF, EPUB, audio, video, small binaries), not tied to a UI pattern
- C++ cache core for blazing fast retrievals: in-memory index + Nitro bindings
- TTL and Force refresh support
- New Architecture ready
- Read a cached file as an `ArrayBuffer` for in-memory consumers

## Installation

```sh
npm install react-native-nitro-cache react-native-nitro-modules
```

> `react-native-nitro-modules` is a required peer dependency.

### iOS

```sh
cd ios && pod install
```

### Android

No extra steps — autolinking handles it.

### Expo

Works with Expo via [Continuous Native Generation](https://docs.expo.dev/workflow/continuous-native-generation/) (`expo prebuild`) or a [development build](https://docs.expo.dev/develop/development-builds/introduction/).

```sh
npx expo prebuild
```

> ❌ **Not compatible with Expo Go.** Like all libraries with custom native code (and all Nitro Modules), `react-native-nitro-cache` requires a development build or a prebuilt project. Expo Go ships a fixed set of native modules and cannot load this one.

## Quick start

```ts
import { rnNitroCache } from 'react-native-nitro-cache';

const entry = await rnNitroCache.getOrFetch('https://cdn.example.com/docs/field-handbook.pdf', {
  ttl: 60 * 60, // optional: keep fresh for 1 hour
});

if (entry) {
  console.log(entry.url); // absolute path on disk — hand this to a viewer/parser/player
  console.log(entry.size); // bytes
  console.log(entry.contentType); // e.g. "application/pdf"
  console.log(entry.expiresAt); // 0 if no TTL; otherwise expiry time in ms since epoch
}
```

**Typical consumers**

- Pass `entry.url` to a PDF/EPUB reader, media player, or file upload module.
- Prefer disk paths for large documents; use `getBuffer(url)` only for intentionally small in-memory reads (see API section).

## API

All methods live on the singleton `rnNitroCache`.

### `getOrFetch(url, options?): Promise<CacheEntry | null>`

Returns the cached entry if present and still valid; otherwise downloads the file, stores it, and returns the new entry. Resolves to `null` on non-2xx responses or download errors.

```ts
type CacheEntry = {
  url: string;         // absolute path to the file on disk
  size: number;        // bytes
  contentType: string; // e.g. "application/pdf" or "application/epub+zip"
  expiresAt: number;   // 0 means no expiration; otherwise expiry instant in ms since epoch
};

type CacheOptions = {
  ttl?: number;          // seconds the entry stays valid; omit or 0 for no expiration
  forceRefresh?: boolean; // bypass the cache and re-download, replacing any existing entry
};
```

**Examples**

```ts
// Cache for 1 hour
await rnNitroCache.getOrFetch('https://cdn.example.com/docs/field-handbook.pdf', {
  ttl: 60 * 60,
});

// Force a fresh download even if a valid entry exists
await rnNitroCache.getOrFetch('https://cdn.example.com/books/onboarding-guide.epub', {
  forceRefresh: true,
});
```

### `get(url): Promise<CacheEntry | null>`

Returns the cached entry for `url`, or `null` if not cached or expired. Does not trigger a download.

### `has(url): boolean`

Synchronous existence check against the in-memory index.

### `getBuffer(url): Promise<ArrayBuffer | null>`

Reads the cached file's bytes into an `ArrayBuffer`. Returns `null` if expired or if the URL isn't cached.
- Use for small files (<10MB) you truly want fully in memory (tiny sidecar files, compact text, small binaries).
- For large files (video, big PDFs, EPUBs, big downloads), use `get(url)` instead.
- Attempting to load >100MB files as ArrayBuffer may cause out-of-memory crashes.

### `remove(url): Promise<void>`

Deletes the file and removes the entry from the index.

### `clear(): Promise<void>`

Empties the entire cache directory and in-memory index.

### `getStats(): Promise<CacheStats>`

```ts
type CacheStats = {
  totalEntries: number;
  totalSize: number; // bytes
};
```

### `getEntries(): Promise<CacheEntry[]>`

Returns every cached entry with its absolute on-disk path.

## Storage location

Files are written to the app's platform cache directory under `nitro-cache/`:

- iOS: `…/Library/Caches/nitro-cache/`
- Android: `…/cache/nitro-cache/`

Because this is the OS-managed cache, the system may reclaim files under disk pressure. Treat the cache as best-effort, not durable storage.

Filenames are derived as `<sha256(url)>.<ext>`, where `<ext>` comes from the response `Content-Type`.

## Cache limits & eviction

`nitro-cache` enforces soft size and entry-count caps. Once exceeded, the **least recently used** entries are evicted automatically.

| | Trigger (high-water mark) | Pruned down to (low-water mark) |
| --- | --- | --- |
| Total cache size | 500 MB | 480 MB |
| Entry count | 300 | 290 |

When a `getOrFetch` call sees the cache above either high-water mark, it evicts the least-recently-used entries until **both** counters are below their low-water marks. Each successful read (`get`, `has`, `getBuffer`, or a `getOrFetch` cache hit) marks the entry as recently used and moves it to the front of the LRU list.


If you want full manual control, use `remove(url)` to evict specific entries or `clear()` to wipe the entire cache.

## Notes & limitations

- Only `http` and `https` URLs are supported.
- This package depends on `react-native-nitro-modules`; see its docs for minimum RN versions.

## Testing

`react-native-nitro-cache` is a JSI / Nitro Module, its hybrid object is constructed at import time and reaches into native code that doesn't exist in Node. If a Jest test transitively imports this package, it will throw at module-load time.

A drop-in Jest mock ships with the package. Add this to your Jest setup file (e.g. `jest.setup.ts`):

```ts
jest.mock('react-native-nitro-cache', () =>
  require('react-native-nitro-cache/jest-mock')
);
```

…and make sure that file is referenced from your Jest config:

```ts
// jest.config.ts
export default {
  preset: 'react-native',
  setupFiles: ['./jest.setup.ts'],
};
```

The mock exposes the same `rnNitroCache` object with no-op implementations:

- async methods resolve to `null` / `void` / empty stats / empty array
- `has()` returns `false`

Override per-test with `jest.spyOn` when you need a specific value:

```ts
import { rnNitroCache } from 'react-native-nitro-cache';

it('opens the cached handbook PDF', async () => {
  jest.spyOn(rnNitroCache, 'getOrFetch').mockResolvedValueOnce({
    url: '/tmp/cached.pdf',
    size: 1024,
    contentType: 'application/pdf',
    expiresAt: 0,
  });

  // ...render and assert
});
```

## License

MIT © [ifeoluwa](https://github.com/ifeoluwak)
