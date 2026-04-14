import type { HybridObject } from 'react-native-nitro-modules';

export type CacheOptions = {
  ttl?: number;
  forceRefresh?: boolean;
  headers?: Record<string, string>;
};

export type CacheEntry = {
  url: string;
  buffer: ArrayBuffer;
  size: number;
  expiry: number;
  contentType: string;
};

export type CacheStats = {
  totalEntries: number;
  totalSize: number;
};

export type CacheConfig = {
  maxEntries?: number;
  maxSize?: number;
  defaultTTL?: number;
  directory?: string;
};

export interface NitroCache extends HybridObject<{
  ios: 'c++';
  android: 'c++';
}> {
  get(url: string): Promise<CacheEntry | null>;
  getOrFetch(url: string, options?: CacheOptions): Promise<CacheEntry | null>;
  getBuffer(url: string): Promise<ArrayBuffer | null>;
  set(
    url: string,
    buffer: ArrayBuffer,
    contentType: string,
    options?: CacheOptions
  ): Promise<void>;
  remove(url: string): Promise<void>;
  clear(): Promise<void>;
  configure(config: CacheConfig): Promise<void>;

  has(url: string): boolean;
  getStats(): Promise<CacheStats>;
  getEntries(): Promise<CacheEntry[]>;
}
