/** Summary of a single recorded lap within a replay session. */
export type LapManifestV1 = {
  /** 1-based lap index within the session. */
  lapNumber: number;
  /** Total lap time in milliseconds. */
  lapTimeMs: number;
  /** Number of telemetry samples recorded for this lap. */
  sampleCount: number;
  /** `true` when the lap was fully recorded from time 0 without interruption. */
  isComplete: boolean;
  /** `true` when the lap data passed basic sanity checks during export. */
  isValid: boolean;
};

/**
 * High-level metadata about an Assetto Corsa replay file.
 * Returned by {@link WasmReplayParser.inspectReplay} without parsing full telemetry.
 */
export type ReplayManifestV1 = {
  /** SHA-256 hex digest of the source `.acreplay` file. */
  sourceSha256: string;
  /** Track identifier as stored in the replay (e.g. `"ks_nordschleife"`). */
  track: string;
  /** Track layout/config identifier. Empty string when the track has only one layout. */
  trackConfig: string;
  /** Zero-based index of the car within the replay's car array. */
  carIndex: number;
  /** Driver display name. */
  driverName: string;
  /** Telemetry sample interval in milliseconds. */
  recordingIntervalMs: number;
  /** Ordered list of laps recorded for this car. */
  laps: LapManifestV1[];
};

/** Raw telemetry bytes for a single lap, ready to pass to `readLapPack`. */
export type ParsedLapPackV1 = {
  /** 1-based lap index matching {@link LapManifestV1.lapNumber}. */
  lapNumber: number;
  /** Binary `.acpack` blob for this lap. */
  bytes: Uint8Array;
};

/** Complete result of parsing one car from an Assetto Corsa replay file. */
export type ParsedCarResultV1 = {
  /** Replay-level metadata for the parsed car. */
  manifest: ReplayManifestV1;
  /** One entry per recorded lap, each containing the raw telemetry pack bytes. */
  lapPacks: ParsedLapPackV1[];
};

/**
 * High-level interface for parsing Assetto Corsa replay files.
 *
 * Use {@link createReplayParser} to instantiate directly, or
 * {@link createReplayWorkerClient} to run parsing off the main thread in a Web Worker.
 */
export type WasmReplayParser = {
  /**
   * Reads only the metadata from a replay file without decoding full telemetry.
   * Much faster than {@link parseCar} when you only need lap list or driver info.
   *
   * @param fileBytes - Raw bytes of the `.acreplay` file.
   * @returns Parsed replay manifest.
   * @throws {Error} If the file is not a valid Assetto Corsa replay.
   */
  inspectReplay(fileBytes: ArrayBuffer): Promise<ReplayManifestV1>;
  /**
   * Fully parses one car's telemetry from a replay file.
   *
   * @param fileBytes - Raw bytes of the `.acreplay` file.
   * @param carIndex - Zero-based index of the car to parse (see {@link ReplayManifestV1.carIndex}).
   * @returns Manifest and per-lap binary telemetry packs.
   * @throws {Error} If the file is invalid or `carIndex` is out of range.
   */
  parseCar(fileBytes: ArrayBuffer, carIndex: number): Promise<ParsedCarResultV1>;
};
