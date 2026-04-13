import { NitroModules } from 'react-native-nitro-modules';
import type { NitroCache } from './NitroCache.nitro';

const NitroCacheHybridObject =
  NitroModules.createHybridObject<NitroCache>('NitroCache');

export function multiply(a: number, b: number): number {
  return NitroCacheHybridObject.multiply(a, b);
}
