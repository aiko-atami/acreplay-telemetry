import type { AlignedData } from "uplot";

import { type ChannelId, channelIds } from "./channelIds.js";
import type { LapTelemetryPackView } from "./types.js";

/**
 * Builds a uPlot-compatible {@link AlignedData} array from a lap telemetry pack.
 *
 * The first series is always `distance_m` (the X axis). Each entry in `channels`
 * appends the corresponding typed array as a Y series in the same order.
 *
 * @param view - A parsed lap telemetry pack to extract series from.
 * @param channels - Ordered list of {@link ChannelId} values to include as Y series.
 * @returns An `AlignedData` array suitable for passing directly to `new uPlot(opts, data)`.
 * @throws {Error} If `view` is missing `distance_m` or any of the requested channels.
 */
export function toUplotSeries(view: LapTelemetryPackView, channels: ChannelId[]): AlignedData {
  const distance = view.channels.get(channelIds.distance_m);
  if (!(distance instanceof Float32Array)) {
    throw new Error("Pack is missing distance_m float channel");
  }

  const aligned: (number[] | Float32Array | Int8Array | Uint8Array)[] = [distance];
  for (const channelId of channels) {
    const channel = view.channels.get(channelId);
    if (channel == null) {
      throw new Error(`Pack is missing channel ${channelId}`);
    }
    aligned.push(channel);
  }

  return aligned as AlignedData;
}
