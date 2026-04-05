import type { ParsedCarResultV1, ReplayManifestV1, WasmReplayParser } from "./types.js";
import type { ReplayWorkerRequest, ReplayWorkerResponse } from "./workerTypes.js";

type WorkerLike = Pick<
  Worker,
  "addEventListener" | "removeEventListener" | "postMessage" | "terminate"
>;

/**
 * Extends {@link WasmReplayParser} with lifecycle methods for the underlying Web Worker.
 *
 * Obtain an instance via {@link createReplayWorkerClient}.
 */
export type WasmReplayWorkerClient = WasmReplayParser & {
  /**
   * Sends the `init` message to the worker and waits for the wasm module to load.
   * Subsequent calls to `inspectReplay` / `parseCar` call this automatically,
   * but calling it eagerly lets you surface load errors at a predictable time.
   */
  init(): Promise<void>;
  /** Rejects all pending requests and terminates the underlying worker. */
  terminate(): void;
};

type PendingRequest = {
  reject(error: unknown): void;
  resolve(response: ReplayWorkerResponse): void;
};

function getTransferList(message: ReplayWorkerRequest): Transferable[] {
  if (message.type === "inspectReplay" || message.type === "parseCar") {
    return [message.fileBytes];
  }
  return [];
}

function getResponseTransferList(response: ReplayWorkerResponse): Transferable[] {
  if (!response.ok || response.type !== "parseCar") {
    return [];
  }

  return response.result.lapPacks.map((lap) => lap.bytes.buffer);
}

/**
 * Creates a {@link WasmReplayWorkerClient} that proxies calls to a Web Worker
 * running {@link attachWorkerHandler}.
 *
 * File bytes are transferred (zero-copy) to the worker via `postMessage` transferables.
 * The parsed result bytes are transferred back in the same way.
 *
 * @param worker - Any `Worker`-like object with `postMessage`, `addEventListener`,
 *   `removeEventListener`, and `terminate`. Pass a real `Worker` or a test double.
 * @returns A client that implements {@link WasmReplayParser} plus `init()` / `terminate()`.
 *
 * @example
 * ```ts
 * const worker = new Worker(new URL('./worker.js', import.meta.url), { type: 'module' });
 * const client = createReplayWorkerClient(worker);
 * const manifest = await client.inspectReplay(fileBytes);
 * ```
 */
export function createReplayWorkerClient(worker: WorkerLike): WasmReplayWorkerClient {
  let nextRequestId = 1;
  const pending = new Map<number, PendingRequest>();
  let initPromise: Promise<void> | null = null;

  const onMessage = (event: MessageEvent<ReplayWorkerResponse>) => {
    const response = event.data;
    const request = pending.get(response.id);
    if (request == null) {
      return;
    }
    pending.delete(response.id);
    request.resolve(response);
  };

  const onMessageError = (event: Event) => {
    const error =
      event instanceof ErrorEvent
        ? (event.error ?? event.message)
        : new Error("Worker message error");
    for (const request of pending.values()) {
      request.reject(error);
    }
    pending.clear();
  };

  const onError = (event: Event) => {
    const error =
      event instanceof ErrorEvent ? (event.error ?? event.message) : new Error("Worker error");
    for (const request of pending.values()) {
      request.reject(error);
    }
    pending.clear();
  };

  worker.addEventListener("message", onMessage as EventListener);
  worker.addEventListener("messageerror", onMessageError);
  worker.addEventListener("error", onError);

  function sendRequest(message: ReplayWorkerRequest): Promise<ReplayWorkerResponse> {
    return new Promise((resolve, reject) => {
      pending.set(message.id, { resolve, reject });
      try {
        worker.postMessage(message, getTransferList(message));
      } catch (error) {
        pending.delete(message.id);
        reject(error);
      }
    });
  }

  function createRequestId(): number {
    const requestId = nextRequestId;
    nextRequestId += 1;
    return requestId;
  }

  async function ensureInitialized(): Promise<void> {
    if (initPromise != null) {
      return initPromise;
    }

    initPromise = (async () => {
      const response = await sendRequest({ id: createRequestId(), type: "init" });
      if (!response.ok) {
        throw new Error(response.error);
      }
    })();

    try {
      await initPromise;
    } catch (error) {
      initPromise = null;
      throw error;
    }
  }

  async function inspectReplay(fileBytes: ArrayBuffer): Promise<ReplayManifestV1> {
    await ensureInitialized();
    const response = await sendRequest({ id: createRequestId(), type: "inspectReplay", fileBytes });
    if (!response.ok) {
      throw new Error(response.error);
    }
    if (response.type !== "inspectReplay") {
      throw new Error(`Unexpected worker response type: ${response.type}`);
    }
    return response.result;
  }

  async function parseCar(fileBytes: ArrayBuffer, carIndex: number): Promise<ParsedCarResultV1> {
    await ensureInitialized();
    const response = await sendRequest({
      id: createRequestId(),
      type: "parseCar",
      fileBytes,
      carIndex,
    });
    if (!response.ok) {
      throw new Error(response.error);
    }
    if (response.type !== "parseCar") {
      throw new Error(`Unexpected worker response type: ${response.type}`);
    }
    return response.result;
  }

  function terminate(): void {
    worker.removeEventListener("message", onMessage as EventListener);
    worker.removeEventListener("messageerror", onMessageError);
    worker.removeEventListener("error", onError);

    const error = new Error("Replay worker client terminated");
    for (const request of pending.values()) {
      request.reject(error);
    }
    pending.clear();
    worker.terminate();
  }

  return {
    init: ensureInitialized,
    inspectReplay,
    parseCar,
    terminate,
  };
}

export { getResponseTransferList };
