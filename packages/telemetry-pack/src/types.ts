export type ChannelValueType = 1|2|3

export type PackHeaderV1 = {
  magic: string
version: number
headerBytes: number
descriptorBytes: number
channelCount: number
sampleCount: number
lapNumber: number
flags: number
lapTimeMs: number
recordingIntervalUs: number
descriptorsOffset: number
  dataOffset: number
}

export type ChannelDescriptorV1 = {
  channelId: number
  valueType: ChannelValueType
  sampleCount: number
  byteOffset: number
  byteLength: number
}

export type ChannelArray = Float32Array | Uint8Array | Int8Array

  export type LapTelemetryPackView = {
    header: PackHeaderV1
  descriptors: ChannelDescriptorV1[]
  channels: Map<number, ChannelArray>
}
