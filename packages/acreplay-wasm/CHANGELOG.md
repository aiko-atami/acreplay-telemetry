# @aikotami/acreplay-wasm

## 0.2.0

### Minor Changes

- Add `WasmReplayWorkerClient` for off-thread parsing via Web Worker.

  New exports from `./worker`: `WasmReplayWorkerClient`, `WorkerRequest`, `WorkerResponse` — run WASM replay parsing off the main thread with a message-based API.

  All public API symbols now include JSDoc with `@param`, `@returns`, `@throws`, and `@example`.
