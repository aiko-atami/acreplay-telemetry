import type { AlignedData } from "uplot";

import { type ChannelId, channelIds } from "./channelIds.js";
import type { LapTelemetryPackView } from "./types.js";

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
