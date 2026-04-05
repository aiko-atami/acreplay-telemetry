import {
  attachWorkerHandler,
} from "@aikotami/acreplay-wasm/worker";
import { createEmscriptenModuleFactory } from "@aikotami/acreplay-wasm";

// @ts-expect-error — Emscripten glue JS has no types
import createModule from "../../../../cpp/build/wasm/acrp_wasm/acrp_wasm.js";

attachWorkerHandler(
  createEmscriptenModuleFactory(createModule, {
    locateFile(path: string) {
      return new URL(
        `../../../../cpp/build/wasm/acrp_wasm/${path}`,
        import.meta.url,
      ).href;
    },
  }),
);
