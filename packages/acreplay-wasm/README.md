# @aikotami/acreplay-wasm

Browser-facing TypeScript wrapper around the Assetto Corsa replay parser compiled to WebAssembly.

## What it does

- loads the wasm parser module
- reads raw replay bytes
- exposes replay inspection and per-car parsing APIs
- returns lap telemetry payloads as binary lap-pack bytes

## Position in the pipeline

`raw replay file` -> `@aikotami/acreplay-wasm` -> `lapPack bytes`

## Public API

Root entrypoint:

- `createReplayParser(factory)` for direct main-thread use
- `createReplayWorkerClient(worker)` for talking to a dedicated worker
- `createEmscriptenModuleFactory(...)` and shared types

Worker entrypoint:

- `@aikotami/acreplay-wasm/worker` exports `attachWorkerHandler(factory)`

## Worker usage

Recommended pattern for modern bundlers is a module worker created with `new Worker(new URL(...), import.meta.url)`.

`parser.worker.ts`

```ts
import { createEmscriptenModuleFactory } from "@aikotami/acreplay-wasm";
import { attachWorkerHandler } from "@aikotami/acreplay-wasm/worker";
import createParserModule from "./acrp_wasm.js";

attachWorkerHandler(createEmscriptenModuleFactory(createParserModule));
```

`main.ts`

```ts
import { createReplayWorkerClient } from "@aikotami/acreplay-wasm";

const worker = new Worker(new URL("./parser.worker.ts", import.meta.url), {
  type: "module",
});

const parser = createReplayWorkerClient(worker);
await parser.init();

const manifest = await parser.inspectReplay(fileBytes.slice(0));
const parsedCar = await parser.parseCar(fileBytes, 0);
```

`createReplayWorkerClient()` sends request `ArrayBuffer`s with transferables for zero-copy handoff to the worker. After calling `inspectReplay()` or `parseCar()`, the original `fileBytes` buffer on the calling thread is detached and should be treated as consumed.
