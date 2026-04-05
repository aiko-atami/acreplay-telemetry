import { channelIds } from "./channelIds.js";
import type { LapTelemetryPackView } from "./types.js";

/**
 * Finds the index of the sample in `view` whose `distance_m` value is closest to `distanceM`.
 *
 * Uses a linear scan — suitable for typical lap lengths (≤ 20 000 samples).
 *
 * @param view - A parsed lap telemetry pack that must contain a `distance_m` float channel.
 * @param distanceM - Track distance in metres to search for.
 * @returns Zero-based sample index with the smallest absolute distance difference.
 * @throws {Error} If `view` is missing the `distance_m` float channel.
 */
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
