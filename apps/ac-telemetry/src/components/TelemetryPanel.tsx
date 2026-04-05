import { useMemo } from "react";
import { readLapPack, toUplotSeries, channelIds } from "@aikotami/actelemetry";
import type { ParsedLapPackV1 } from "@aikotami/acreplay-wasm";
import type { AlignedData } from "uplot";
import uPlot from "uplot";
import { UplotChart } from "./UplotChart.tsx";

type TelemetryPanelProps = {
  lapPack: ParsedLapPackV1;
};

const SYNC_KEY = "telemetry";

function scaleU8toPercent(data: Uint8Array | Float32Array | Int8Array): Float32Array {
  const out = new Float32Array(data.length);
  for (let i = 0; i < data.length; i++) {
    out[i] = (data[i]! / 255) * 100;
  }
  return out;
}

export function TelemetryPanel({ lapPack }: TelemetryPanelProps) {
  const view = useMemo(
    () => readLapPack(lapPack.bytes.buffer as ArrayBuffer),
    [lapPack],
  );

  const speedData = useMemo(
    () => toUplotSeries(view, [channelIds.speed_kmh]),
    [view],
  );

  const throttleData = useMemo((): AlignedData => {
    const distance = view.channels.get(channelIds.distance_m) as Float32Array;
    const raw = view.channels.get(channelIds.throttle_raw);
    return [distance, raw ? scaleU8toPercent(raw) : new Float32Array(0)];
  }, [view]);

  const brakeData = useMemo((): AlignedData => {
    const distance = view.channels.get(channelIds.distance_m) as Float32Array;
    const raw = view.channels.get(channelIds.brake_raw);
    return [distance, raw ? scaleU8toPercent(raw) : new Float32Array(0)];
  }, [view]);

  const gearData = useMemo(
    () => toUplotSeries(view, [channelIds.gear]),
    [view],
  );

  const rpmData = useMemo(
    () => toUplotSeries(view, [channelIds.rpm]),
    [view],
  );

  return (
    <div className="flex flex-col gap-2">
      <UplotChart
        syncKey={SYNC_KEY}
        title="Speed (km/h)"
        data={speedData}
        options={{
          series: [{}, { stroke: "#22d3ee", width: 1.5, label: "Speed" }],
          axes: [{}, { label: "km/h" }],
        }}
      />
      <UplotChart
        syncKey={SYNC_KEY}
        title="Throttle (%)"
        data={throttleData}
        options={{
          series: [{}, { stroke: "#4ade80", width: 1.5, fill: "rgba(74,222,128,0.1)", label: "Throttle" }],
          axes: [{}, { label: "%" }],
        }}
      />
      <UplotChart
        syncKey={SYNC_KEY}
        title="Brake (%)"
        data={brakeData}
        options={{
          series: [{}, { stroke: "#f87171", width: 1.5, fill: "rgba(248,113,113,0.1)", label: "Brake" }],
          axes: [{}, { label: "%" }],
        }}
      />
      <UplotChart
        syncKey={SYNC_KEY}
        title="Gear"
        data={gearData}
        height={120}
        options={{
          series: [{}, { stroke: "#c084fc", width: 1.5, label: "Gear", paths: uPlotStairsPath }],
          axes: [{}, { label: "Gear" }],
        }}
      />
      <UplotChart
        syncKey={SYNC_KEY}
        title="RPM"
        data={rpmData}
        options={{
          series: [{}, { stroke: "#fbbf24", width: 1.5, label: "RPM" }],
          axes: [{}, { label: "RPM" }],
        }}
      />
    </div>
  );
}

/** Staircase path builder for discrete data like gear */
const uPlotStairsPath: uPlot.Series.PathBuilder = (u, seriesIdx, idx0, idx1): uPlot.Series.Paths | null => {
  const s = u.series[seriesIdx]!;
  const xdata = u.data[0]!;
  const ydata = u.data[seriesIdx]!;
  const stroke = new Path2D();

  let prevX: number | null = null;
  let prevY: number | null = null;

  for (let i = idx0; i <= idx1; i++) {
    const xVal = xdata[i];
    const yVal = ydata[i];
    if (xVal == null || yVal == null) continue;

    const cx = Math.round(u.valToPos(xVal, "x", true));
    const cy = Math.round(u.valToPos(yVal, s.scale!, true));

    if (prevX === null) {
      stroke.moveTo(cx, cy);
    } else {
      stroke.lineTo(cx, prevY!);
      stroke.lineTo(cx, cy);
    }
    prevX = cx;
    prevY = cy;
  }

  return { stroke, fill: undefined, clip: undefined, band: undefined, gaps: undefined, flags: 0 };
};
