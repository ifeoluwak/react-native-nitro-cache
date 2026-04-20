import { NitroModules } from 'react-native-nitro-modules';
import type { NitroCache } from './NitroCache.nitro';

export const rnNitroCache =
  NitroModules.createHybridObject<NitroCache>('NitroCache');
