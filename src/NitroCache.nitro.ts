import type { HybridObject } from 'react-native-nitro-modules';

export type CacheOptions = {
  ttl?: number; // in seconds
  forceRefresh?: boolean;
};

export type CacheEntry = {
  url: string;
  size: number;
  contentType: string;
  expiresAt: number; // in seconds, 0 for no expiration [default]
};

export type CacheStats = {
  totalEntries: number;
  totalSize: number;
};

export interface NitroCache extends HybridObject<{
  ios: 'c++';
  android: 'c++';
}> {
  get(url: string): Promise<CacheEntry | null>;
  getOrFetch(url: string, options?: CacheOptions): Promise<CacheEntry | null>;
  getBuffer(url: string): Promise<ArrayBuffer | null>;
  remove(url: string): Promise<void>;
  clear(): Promise<void>;
  has(url: string): boolean;
  getStats(): Promise<CacheStats>;
  getEntries(): Promise<CacheEntry[]>;
}
