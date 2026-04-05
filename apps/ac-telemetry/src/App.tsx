import { useState } from "react";
import { useReplayParser } from "./hooks/useReplayParser.ts";
import { DropZone } from "./components/DropZone.tsx";
import { LapSelector, findBestLap } from "./components/LapSelector.tsx";
import { TelemetryPanel } from "./components/TelemetryPanel.tsx";

export function App() {
  const { state, parseFile, reset } = useReplayParser();
  const [selectedLap, setSelectedLap] = useState<number | null>(null);

  const handleFile = (file: File) => {
    setSelectedLap(null);
    parseFile(file);
  };

  // When data becomes ready, auto-select best lap
  const effectiveLap =
    state.status === "ready"
      ? (selectedLap ?? findBestLap(state.manifest))
      : null;

  const selectedPack =
    state.status === "ready" && effectiveLap != null
      ? state.carResult.lapPacks.find((lp) => lp.lapNumber === effectiveLap)
      : null;

  return (
    <div className="min-h-screen bg-background text-foreground">
      <header className="border-b border-border px-6 py-4">
        <div className="flex items-center justify-between">
          <h1 className="text-lg font-semibold tracking-tight">
            AC Telemetry
          </h1>
          {state.status === "ready" && (
            <button
              onClick={reset}
              className="rounded-md border border-border px-3 py-1 text-xs text-muted-foreground hover:text-foreground transition-colors"
            >
              Load another file
            </button>
          )}
        </div>
      </header>

      <main className="mx-auto max-w-7xl px-6 py-6 flex flex-col gap-6">
        {state.status === "idle" && (
          <DropZone onFile={handleFile} />
        )}

        {state.status === "parsing" && (
          <div className="flex flex-col items-center gap-3 py-16">
            <div className="h-8 w-8 animate-spin rounded-full border-2 border-muted-foreground border-t-foreground" />
            <p className="text-sm text-muted-foreground">
              Parsing <span className="text-foreground">{state.fileName}</span>…
            </p>
          </div>
        )}

        {state.status === "error" && (
          <div className="rounded-xl border border-destructive/50 bg-destructive/10 p-6 text-center">
            <p className="text-sm text-destructive-foreground">{state.message}</p>
            <button
              onClick={reset}
              className="mt-4 rounded-md border border-border px-4 py-1.5 text-xs text-muted-foreground hover:text-foreground transition-colors"
            >
              Try again
            </button>
          </div>
        )}

        {state.status === "ready" && effectiveLap != null && (
          <>
            <LapSelector
              manifest={state.manifest}
              selectedLap={effectiveLap}
              onSelectLap={setSelectedLap}
            />
            {selectedPack ? (
              <TelemetryPanel lapPack={selectedPack} />
            ) : (
              <p className="text-sm text-muted-foreground">
                No telemetry data for lap {effectiveLap}
              </p>
            )}
          </>
        )}
      </main>
    </div>
  );
}
