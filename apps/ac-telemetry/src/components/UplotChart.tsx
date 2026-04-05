import { useEffect, useRef } from "react";
import uPlot from "uplot";
import type { AlignedData, Options } from "uplot";
import "uplot/dist/uPlot.min.css";

type UplotChartProps = {
  data: AlignedData;
  options: Partial<Options>;
  syncKey: string;
  title: string;
  height?: number;
};

const cursorSync = new Map<string, uPlot.SyncPubSub>();

function getSync(key: string): uPlot.SyncPubSub {
  let sync = cursorSync.get(key);
  if (!sync) {
    sync = uPlot.sync(key);
    cursorSync.set(key, sync);
  }
  return sync;
}

export function UplotChart({
  data,
  options,
  syncKey,
  title,
  height = 160,
}: UplotChartProps) {
  const containerRef = useRef<HTMLDivElement>(null);
  const chartRef = useRef<uPlot | null>(null);

  useEffect(() => {
    const el = containerRef.current;
    if (!el) return;

    const sync = getSync(syncKey);

    const opts: Options = {
      width: el.clientWidth,
      height,
      title,
      ...options,
      cursor: {
        sync: { key: sync.key },
        drag: { x: true, y: false },
        ...options.cursor,
      },
      axes: [
        {
          label: "Distance (m)",
          stroke: "#888",
          grid: { stroke: "rgba(255,255,255,0.06)" },
          ticks: { stroke: "rgba(255,255,255,0.06)" },
          ...(options.axes?.[0] ?? {}),
        },
        {
          stroke: "#888",
          grid: { stroke: "rgba(255,255,255,0.06)" },
          ticks: { stroke: "rgba(255,255,255,0.06)" },
          ...(options.axes?.[1] ?? {}),
        },
      ],
      series: options.series ?? [
        {},
        { stroke: "#22d3ee", width: 1.5 },
      ],
    };

    const chart = new uPlot(opts, data, el);
    chartRef.current = chart;

    const ro = new ResizeObserver(() => {
      chart.setSize({ width: el.clientWidth, height });
    });
    ro.observe(el);

    return () => {
      ro.disconnect();
      chart.destroy();
      chartRef.current = null;
    };
  }, [data, options, syncKey, title, height]);

  return <div ref={containerRef} className="w-full" />;
}
