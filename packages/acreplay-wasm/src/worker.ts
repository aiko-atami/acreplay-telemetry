import { createReplayParser } from "./index.js";
import type { WasmModuleFactory } from "./module.js";
import { getResponseTransferList } from "./workerClient.js";
import type { ReplayWorkerRequest, ReplayWorkerResponse } from "./workerTypes.js";

let parserPromise: ReturnType<typeof createReplayParser> | null = null;

/**
 * Attaches the replay parser message handler to the current worker scope (`self`).
 *
 * Call this once at the top level of your worker entry file. The handler responds to
 * `init`, `inspectReplay`, and `parseCar` messages from {@link createReplayWorkerClient}.
 * The wasm module is loaded lazily on the first request (or eagerly on `init`).
 *
 * @param factory - A {@link WasmModuleFactory} that loads and initialises the wasm module.
 *
 * @example
 * ```ts
 * // worker.ts
 * import { attachWorkerHandler, createEmscriptenModuleFactory } from '@acreplay/wasm';
 * import createModule from './acrp.js';
 *
 * attachWorkerHandler(createEmscriptenModuleFactory(createModule));
 * ```
 */
export function attachWorkerHandler(factory: WasmModuleFactory): void {
  self.addEventListener("message", async (event: MessageEvent<ReplayWorkerRequest>) => {
    const message = event.data;

    try {
      if (message.type === "init") {
        parserPromise ??= createReplayParser(factory);
        await parserPromise;
        const response: ReplayWorkerResponse = { id: message.id, ok: true, type: "init" };
        self.postMessage(response);
        return;
      }

      if (parserPromise == null) {
        parserPromise = createReplayParser(factory);
      }

      const parser = await parserPromise;
      if (message.type === "inspectReplay") {
        const result = await parser.inspectReplay(message.fileBytes);
        const response: ReplayWorkerResponse = {
          id: message.id,
          ok: true,
          type: "inspectReplay",
          result,
        };
        self.postMessage(response);
        return;
      }

      const result = await parser.parseCar(message.fileBytes, message.carIndex);
      const response: ReplayWorkerResponse = { id: message.id, ok: true, type: "parseCar", result };
      self.postMessage(response, getResponseTransferList(response));
    } catch (error) {
      const response: ReplayWorkerResponse = {
        id: message.id,
        ok: false,
        type: message.type,
        error: error instanceof Error ? error.message : String(error),
      };
      self.postMessage(response);
    }
  });
}

export type * from "./workerTypes.js";
