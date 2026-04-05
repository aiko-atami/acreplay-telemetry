import { loadNativeModule, type WasmModuleFactory } from "./module.js";
import type { ParsedCarResultV1, ReplayManifestV1, WasmReplayParser } from "./types.js";

function decodeBase64(base64: string): Uint8Array {
  const binary = atob(base64);
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i += 1) {
    bytes[i] = binary.charCodeAt(i);
  }
  return bytes;
}

function writeInput(module: Awaited<ReturnType<typeof loadNativeModule>>, input: ArrayBuffer) {
  // The C ABI expects replay bytes to live in wasm linear memory for the duration of the call.
  const ptr = module._acrp_alloc_buffer(input.byteLength);
  module.HEAPU8.set(new Uint8Array(input), ptr);
  return ptr;
}

function readJsonResult<T>(
  module: Awaited<ReturnType<typeof loadNativeModule>>,
  status: number,
  outputPtrPtr: number,
  outputLenPtr: number,
): T {
  if (status !== 0) {
    const errorPtr = module._acrp_last_error_message();
    const errorLen = module._acrp_last_error_length();
    throw new Error(module.UTF8ToString(errorPtr, errorLen));
  }

  const outputPtr = module.getValue(outputPtrPtr, "*");
  const outputLen = module.getValue(outputLenPtr, "i32");
  // JSON keeps the JS boundary stable while the binary telemetry payload stays inside base64.
  const json = new TextDecoder().decode(module.HEAPU8.slice(outputPtr, outputPtr + outputLen));
  module._acrp_free_buffer(outputPtr);
  return JSON.parse(json) as T;
}

function normalizeParseCarResult(json: {
  manifest: ReplayManifestV1;
  lapPacks: { lapNumber: number; base64: string }[];
}): ParsedCarResultV1 {
  return {
    manifest: json.manifest,
    lapPacks: json.lapPacks.map((lap) => ({
      lapNumber: lap.lapNumber,
      bytes: decodeBase64(lap.base64),
    })),
  };
}

/**
 * Creates a {@link WasmReplayParser} that runs directly on the calling thread.
 *
 * For use cases where a Web Worker is not available or needed (e.g. Node.js scripts,
 * tests). For browser apps, prefer {@link createReplayWorkerClient} to avoid blocking
 * the main thread during file parsing.
 *
 * @param factory - A {@link WasmModuleFactory} returned by {@link createEmscriptenModuleFactory}.
 * @returns An initialised parser instance.
 *
 * @example
 * ```ts
 * import createModule from './acrp.js';
 * import { createReplayParser, createEmscriptenModuleFactory } from '@acreplay/wasm';
 *
 * const parser = await createReplayParser(createEmscriptenModuleFactory(createModule));
 * const manifest = await parser.inspectReplay(fileBytes);
 * ```
 */
export async function createReplayParser(factory: WasmModuleFactory): Promise<WasmReplayParser> {
  const module = await loadNativeModule(factory);

  return {
    async inspectReplay(fileBytes: ArrayBuffer): Promise<ReplayManifestV1> {
      const inputPtr = writeInput(module, fileBytes);
      const outputPtrPtr = module._acrp_alloc_buffer(4);
      const outputLenPtr = module._acrp_alloc_buffer(4);
      try {
        const status = module._acrp_inspect_json(
          inputPtr,
          fileBytes.byteLength,
          outputPtrPtr,
          outputLenPtr,
        );
        return readJsonResult<ReplayManifestV1>(module, status, outputPtrPtr, outputLenPtr);
      } finally {
        module._acrp_free_buffer(inputPtr);
        module._acrp_free_buffer(outputPtrPtr);
        module._acrp_free_buffer(outputLenPtr);
      }
    },

    async parseCar(fileBytes: ArrayBuffer, carIndex: number): Promise<ParsedCarResultV1> {
      const inputPtr = writeInput(module, fileBytes);
      const outputPtrPtr = module._acrp_alloc_buffer(4);
      const outputLenPtr = module._acrp_alloc_buffer(4);
      try {
        const status = module._acrp_parse_car_json(
          inputPtr,
          fileBytes.byteLength,
          carIndex,
          outputPtrPtr,
          outputLenPtr,
        );
        const json = readJsonResult<{
          manifest: ReplayManifestV1;
          lapPacks: { lapNumber: number; base64: string }[];
        }>(module, status, outputPtrPtr, outputLenPtr);
        return normalizeParseCarResult(json);
      } finally {
        module._acrp_free_buffer(inputPtr);
        module._acrp_free_buffer(outputPtrPtr);
        module._acrp_free_buffer(outputLenPtr);
      }
    },
  };
}

export * from "./types.js";
export * from "./module.js";
export * from "./workerClient.js";
export type * from "./workerTypes.js";
