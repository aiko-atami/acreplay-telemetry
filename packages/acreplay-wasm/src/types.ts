export type LapManifestV1 = {
  lapNumber: number
lapTimeMs: number
sampleCount: number
isComplete: boolean
  isValid: boolean
}

export type ReplayManifestV1 = {
  sourceSha256: string
  track: string
  trackConfig: string
  carIndex: number
  driverName: string
  recordingIntervalMs: number
  laps: LapManifestV1[]
}

export type ParsedLapPackV1 = {
  lapNumber: number
  bytes: Uint8Array
}

export type ParsedCarResultV1 = {
  manifest: ReplayManifestV1
  lapPacks: ParsedLapPackV1[]
}

export type WasmReplayParser = {
  inspectReplay(fileBytes: ArrayBuffer): Promise<ReplayManifestV1>
  parseCar(fileBytes: ArrayBuffer, carIndex: number): Promise<ParsedCarResultV1>
}
