# react-native-nitro-cache

**High-performance, General-purpose unified cache for images, videos, and files in React Native.**

Built from the ground up with **Nitro Modules** and C++ for speed and efficiency. Delivers a simple, focused API that just works.

New Architecture support.

## Why another cache library?

Most React Native caching libraries are tied to the `<Image>` component and predate the New Architecture. `nitro-cache` is a **general-purpose** cache for any HTTP(S) asset — images, JSON blobs, small binaries — built on **Nitro Modules / JSI** with a C++ core. It exposes introspection APIs (`getEntries`, `getStats`), direct buffer access (`getBuffer`), and works with any rendering primitive, not just `<Image>`. See the [comparison](#comparison) below.

## Features

- Simple API: easy to use focused primitives (`getOrFetch`, `get`, `remove`, `clear`, …)
- General-purpose: any HTTP(S) asset (images, JSON blobs, small binaries), not tied to a specific UI pattern
- C++ cache core for blazing fast retrievals: in-memory index + Nitro bindings
- Background downloading & streaming support
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

## Quick start

```ts
import { rnNitroCache } from 'react-native-nitro-cache';

const entry = await rnNitroCache.getOrFetch(
  'https://example.com/image.jpg'
);

if (entry) {
  console.log(entry.url);         // absolute path on disk
  console.log(entry.size);        // bytes
  console.log(entry.contentType); // e.g. "image/jpeg"
  console.log(entry.expiresAt); // e.g. 177789283673
}
```

Use the returned path directly with `<Image>`:

```tsx
import { Image } from 'react-native';

<Image source={{ uri: `file://${entry.url}` }} style={{ width: 200, height: 200 }} />
```

## API

All methods live on the singleton `rnNitroCache`.

### `getOrFetch(url, options?): Promise<CacheEntry | null>`

Returns the cached entry if present and still valid; otherwise downloads the file, stores it, and returns the new entry. Resolves to `null` on non-2xx responses or download errors.

```ts
type CacheEntry = {
  url: string;         // absolute path to the file on disk
  size: number;        // bytes
  contentType: string; // e.g. "image/jpeg"
  expiresAt: number;   // unix seconds; 0 means no expiration
};

type CacheOptions = {
  ttl?: number;          // seconds the entry stays valid; omit or 0 for no expiration
  forceRefresh?: boolean; // bypass the cache and re-download, replacing any existing entry
};
```

**Examples**

```ts
// Cache for 1 hour
await rnNitroCache.getOrFetch('https://example.com/image.jpg', {
  ttl: 60 * 60,
});

// Force a fresh download even if a valid entry exists
await rnNitroCache.getOrFetch('https://example.com/image.jpg', {
  forceRefresh: true,
});
```

### `get(url): Promise<CacheEntry | null>`

Returns the cached entry for `url`, or `null` if not cached or expired. Does not trigger a download.

### `has(url): boolean`

Synchronous existence check against the in-memory index.

### `getBuffer(url): Promise<ArrayBuffer | null>`

Reads the cached file's bytes into an `ArrayBuffer`. Returns `null` if expired or if the URL isn't cached.
- Use for small files (<10MB) like thumbnails, JSON, or small images.
- For large files (videos, high-res images), use `get(url)` instead.
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

## Notes & limitations

- Only `http` and `https` URLs are supported.
- This package depends on `react-native-nitro-modules`; see its docs for minimum RN versions.

## Comparison

| Capability | `react-native-nitro-cache` | `react-native-fast-image` | `expo-image` |
| --- | :---: | :---: | :---: |
| Unified (Images + Videos + Files) | ✅ | ⚠️ image-only | ⚠️ image-only |
| New Architecture support | ✅ | ⚠️ community fork available | ✅ good |
| JSI-based C++ | ✅ (Nitro + C++) | ❌ native modules | ❌ native modules |
| Read cached bytes as `ArrayBuffer` | ✅ | ❌ | ❌ |
| Inspect cache (`getEntries`, `getStats`) | ✅ | ❌ | ❌ |
| Programmatic `remove(url)` | ✅ | ❌ | ⚠️ limited |
| TTL / forced refresh | ✅ | ❌ | ❌ |

Legend: ✅ supported · ⚠️ partial / caveats · ❌ not supported · 🚧 planned

> _Last verified: April 2026. Capabilities of other libraries change over time — please open an issue if a row is out of date._

## License

MIT © [ifeoluwa](https://github.com/ifeoluwak)
