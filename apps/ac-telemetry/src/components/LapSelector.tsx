import type { ReplayManifestV1 } from "@aikotami/acreplay-wasm";

type LapSelectorProps = {
  manifest: ReplayManifestV1;
  selectedLap: number;
  onSelectLap: (lapNumber: number) => void;
};

function formatLapTime(ms: number): string {
  const minutes = Math.floor(ms / 60_000);
  const seconds = Math.floor((ms % 60_000) / 1000);
  const millis = ms % 1000;
  return `${minutes}:${String(seconds).padStart(2, "0")}.${String(millis).padStart(3, "0")}`;
}

export function LapSelector({ manifest, selectedLap, onSelectLap }: LapSelectorProps) {
  return (
    <div className="flex flex-wrap items-center gap-3">
      <label className="text-sm font-medium text-muted-foreground">Lap</label>
      <select
        value={selectedLap}
        onChange={(e) => onSelectLap(Number(e.target.value))}
        className="rounded-md border border-border bg-background px-3 py-1.5 text-sm text-foreground focus:outline-none focus:ring-2 focus:ring-ring"
      >
        {manifest.laps.map((lap) => (
          <option key={lap.lapNumber} value={lap.lapNumber}>
            Lap {lap.lapNumber} — {formatLapTime(lap.lapTimeMs)}
            {!lap.isComplete ? " (incomplete)" : ""}
            {!lap.isValid ? " (invalid)" : ""}
          </option>
        ))}
      </select>
      <span className="text-xs text-muted-foreground">
        {manifest.driverName} · {manifest.track}
        {manifest.trackConfig ? ` / ${manifest.trackConfig}` : ""}
      </span>
    </div>
  );
}

/** Find the fastest valid+complete lap, or first lap as fallback. */
export function findBestLap(manifest: ReplayManifestV1): number {
  const valid = manifest.laps.filter((l) => l.isComplete && l.isValid);
  if (valid.length > 0) {
    return valid.reduce((best, l) => (l.lapTimeMs < best.lapTimeMs ? l : best)).lapNumber;
  }
  return manifest.laps[0]?.lapNumber ?? 1;
}
