import { channelIds } from "./channelIds.js";
import { findNearestSampleByDistance } from "./nearestSample.js";
import type { LapTelemetryPackView } from "./types.js";

/**
 * Builds a per-sample delta-time array between two laps aligned by track distance.
 *
 * For each sample `i` in `comp`, finds the closest sample in `base` by distance,
 * then computes `compTime[i] − baseTime[closestBaseIndex]`.
 * Positive values mean `comp` is slower than `base` at that point on track.
 *
 * @param base - The reference lap (e.g. personal best).
 * @param comp - The lap being compared against the reference.
 * @returns `Float32Array` of length `comp.header.sampleCount` with delta times in milliseconds.
 * @throws {Error} If either pack is missing `distance_m` or `time_ms` float channels.
 */
export function buildDeltaTime(
  base: LapTelemetryPackView,
  comp: LapTelemetryPackView,
): Float32Array {
  const baseDistance = base.channels.get(channelIds.distance_m);
  const baseTime = base.channels.get(channelIds.time_ms);
  const compDistance = comp.channels.get(channelIds.distance_m);
  const compTime = comp.channels.get(channelIds.time_ms);

  if (!(baseDistance instanceof Float32Array) || !(baseTime instanceof Float32Array)) {
    throw new Error("Base pack is missing distance_m/time_ms float channels");
  }
  if (!(compDistance instanceof Float32Array) || !(compTime instanceof Float32Array)) {
    throw new Error("Comp pack is missing distance_m/time_ms float channels");
  }

  const delta = new Float32Array(compDistance.length);
  for (let i = 0; i < compDistance.length; i += 1) {
    const baseIndex = findNearestSampleByDistance(base, compDistance[i]);
    delta[i] = compTime[i] - baseTime[baseIndex];
  }
  return delta;
}
