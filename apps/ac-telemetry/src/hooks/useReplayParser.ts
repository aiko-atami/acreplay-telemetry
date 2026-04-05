import { useCallback, useEffect, useRef, useState } from "react";
import { createReplayWorkerClient } from "@aikotami/acreplay-wasm";
import type {
  ParsedCarResultV1,
  ReplayManifestV1,
  WasmReplayWorkerClient,
} from "@aikotami/acreplay-wasm";

export type ReplayParserState =
  | { status: "idle" }
  | { status: "parsing"; fileName: string }
  | { status: "ready"; manifest: ReplayManifestV1; carResult: ParsedCarResultV1 }
  | { status: "error"; message: string };

export function useReplayParser() {
  const clientRef = useRef<WasmReplayWorkerClient | null>(null);
  const [state, setState] = useState<ReplayParserState>({ status: "idle" });

  useEffect(() => {
    const worker = new Worker(
      new URL("../worker/replay.worker.ts", import.meta.url),
      { type: "module" },
    );
    const client = createReplayWorkerClient(worker);
    clientRef.current = client;

    return () => {
      client.terminate();
      clientRef.current = null;
    };
  }, []);

  const parseFile = useCallback(async (file: File) => {
    const client = clientRef.current;
    if (!client) return;

    setState({ status: "parsing", fileName: file.name });

    try {
      const buffer = await file.arrayBuffer();
      // We need two copies since the buffer is transferred (detached) on first call
      const bufferCopy = buffer.slice(0);
      const manifest = await client.inspectReplay(buffer);
      const carResult = await client.parseCar(bufferCopy, 0);

      setState({ status: "ready", manifest, carResult });
    } catch (err) {
      setState({
        status: "error",
        message: err instanceof Error ? err.message : String(err),
      });
    }
  }, []);

  const reset = useCallback(() => {
    setState({ status: "idle" });
  }, []);

  return { state, parseFile, reset };
}
