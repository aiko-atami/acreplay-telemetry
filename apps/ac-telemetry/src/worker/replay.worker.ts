import {
  attachWorkerHandler,
} from "@aikotami/acreplay-wasm/worker";
import { createEmscriptenModuleFactory } from "@aikotami/acreplay-wasm";

// @ts-expect-error — Emscripten glue JS has no types
import createModule from "@aikotami/acreplay-wasm/wasm";
import wasmUrl from "@aikotami/acreplay-wasm/wasm/acrp_wasm.wasm?url";

attachWorkerHandler(
  createEmscriptenModuleFactory(createModule, {
    locateFile(path: string) {
      if (path.endsWith(".wasm")) return wasmUrl;
      return path;
    },
  }),
);
