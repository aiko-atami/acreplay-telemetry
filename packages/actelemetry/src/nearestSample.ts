import { channelIds } from "./channelIds.js";
import type { LapTelemetryPackView } from "./types.js";

export function findNearestSampleByDistance(view: LapTelemetryPackView, distanceM: number): number {
  const distance = view.channels.get(channelIds.distance_m);
  if (!(distance instanceof Float32Array)) {
    throw new Error("Pack is missing distance_m float channel");
  }

  let bestIndex = 0;
  let bestDelta = Number.POSITIVE_INFINITY;
  for (let i = 0; i < distance.length; i += 1) {
    const currentDelta = Math.abs(distance[i] - distanceM);
    if (currentDelta < bestDelta) {
      bestDelta = currentDelta;
      bestIndex = i;
    }
  }
  return bestIndex;
}
