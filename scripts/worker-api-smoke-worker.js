import { parentPort } from "node:worker_threads";
import { resolve } from "node:path";

import createAcrpModule from "../cpp/build/wasm/acrp_wasm/acrp_wasm.js";
import { createEmscriptenModuleFactory } from "../packages/acreplay-wasm/dist/index.js";
import { attachWorkerHandler } from "../packages/acreplay-wasm/dist/worker.js";

const repoRoot = resolve(import.meta.dirname, "..");
const wasmPath = resolve(repoRoot, "cpp/build/wasm/acrp_wasm/acrp_wasm.wasm");

if (parentPort == null) {
  throw new Error("worker-api-smoke-worker must run inside a worker thread");
}

const listeners = new Set();

globalThis.self = {
  addEventListener(type, listener) {
    if (type !== "message") {
      throw new Error(`Unsupported worker event type: ${type}`);
    }

    const wrappedListener = (data) => listener({ data });
    listeners.add(wrappedListener);
    parentPort.on("message", wrappedListener);
  },
  postMessage(value, transferList) {
    parentPort.postMessage(value, transferList);
  },
};

attachWorkerHandler(
  createEmscriptenModuleFactory(createAcrpModule, {
    locateFile(path, scriptDirectory) {
      if (path.endsWith(".wasm")) {
        return wasmPath;
      }
      return new URL(path, scriptDirectory).toString();
    },
  }),
);
