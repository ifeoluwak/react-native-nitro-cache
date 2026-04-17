import type { HybridObject } from 'react-native-nitro-modules';

export type DownloadResult = {
  /** Absolute path to the written file */
  filePath: string;
  /** Bytes written */
  byteCount: number;
  /** HTTP status */
  statusCode: number;
  contentType: string;
};

/**
 * Platform-native cache root (app cache directory + nitro-cache subfolder).
 * Implemented in Swift on iOS and Kotlin on Android.
 *
 * `downloadFile` streams a URL to disk directly under the nitro-cache root directory with bounded
 * concurrency (default 4). `relativePath` must be a relative subpath (no `..`).
 *
 * `deleteFile` removes one entry directly under the nitro-cache root (same path rules). Missing
 * paths count as success. Both methods return `true` on success and `false` on
 * invalid input or I/O failure.
 *
 * `clearCache` deletes everything inside the nitro-cache root directory but keeps the root directory.
 */
export interface NitroCacheFolder extends HybridObject<{
  ios: 'swift';
  android: 'kotlin';
}> {
  getCacheDirectory(): string;
  downloadFile(url: string): Promise<DownloadResult>;
  setMaxConcurrentDownloads(max: number): void;
  deleteFile(relativePath: string): boolean;
  clearCache(): boolean;
  hashURL(url: string): string;
}
