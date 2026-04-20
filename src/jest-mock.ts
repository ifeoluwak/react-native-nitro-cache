import type { CacheEntry, CacheStats, NitroCache } from './NitroCache.nitro';

export type {
  CacheEntry,
  CacheOptions,
  CacheStats,
  NitroCache,
} from './NitroCache.nitro';

export const rnNitroCache = {
  get: (_url: string): Promise<CacheEntry | null> => Promise.resolve(null),
  getOrFetch: (_url: string): Promise<CacheEntry | null> =>
    Promise.resolve(null),
  getBuffer: (_url: string): Promise<ArrayBuffer | null> =>
    Promise.resolve(null),
  remove: (_url: string): Promise<void> => Promise.resolve(),
  clear: (): Promise<void> => Promise.resolve(),
  has: (_url: string): boolean => false,
  getStats: (): Promise<CacheStats> =>
    Promise.resolve({ totalEntries: 0, totalSize: 0 }),
  getEntries: (): Promise<CacheEntry[]> => Promise.resolve([]),
} as unknown as NitroCache;
