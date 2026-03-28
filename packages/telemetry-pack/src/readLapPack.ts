import type {ChannelArray, ChannelDescriptorV1, ChannelValueType, LapTelemetryPackView, PackHeaderV1,} from './types.js'
import {validateLapPack} from './validateLapPack.js'

function readChannelArray(
    buffer: ArrayBuffer,
    dataOffset: number,
    descriptor: ChannelDescriptorV1,
    ): ChannelArray {
  const start = dataOffset + descriptor.byteOffset
  const end = start + descriptor.byteLength
  if (end > buffer.byteLength) {
    throw new Error(`Channel ${descriptor.channelId} exceeds buffer length`)
  }

  if (descriptor.valueType === 1) {
    return new Float32Array(buffer.slice(start, end))
  }
  if (descriptor.valueType === 2) {
    return new Uint8Array(buffer.slice(start, end))
  }
  return new Int8Array(buffer.slice(start, end))
}

export function readLapPack(buffer: ArrayBuffer): LapTelemetryPackView {
  validateLapPack(buffer)

  const view = new DataView(buffer)
  const header: PackHeaderV1 = {
    magic: 'ACTL',
    version: view.getUint16(4, true),
    headerBytes: view.getUint16(6, true),
    descriptorBytes: view.getUint16(8, true),
    channelCount: view.getUint16(10, true),
    sampleCount: view.getUint32(12, true),
    lapNumber: view.getUint16(16, true),
    flags: view.getUint16(18, true),
    lapTimeMs: view.getUint32(20, true),
    recordingIntervalUs: view.getUint32(24, true),
    descriptorsOffset: view.getUint32(28, true),
    dataOffset: view.getUint32(32, true),
  }

  const descriptors: ChannelDescriptorV1[] = []
  const channels = new Map<number, ChannelArray>()

  for (let index = 0; index < header.channelCount; index += 1) {
    const offset = header.descriptorsOffset + index * header.descriptorBytes
    const descriptor: ChannelDescriptorV1 = {
      channelId: view.getUint16(offset + 0, true),
      valueType: view.getUint8(offset + 2) as ChannelValueType,
      sampleCount: view.getUint32(offset + 4, true),
      byteOffset: view.getUint32(offset + 8, true),
      byteLength: view.getUint32(offset + 12, true),
    }

    if (descriptor.sampleCount !== header.sampleCount) {
      throw new Error(
          `Channel ${descriptor.channelId} has sampleCount ${descriptor.sampleCount}, expected ${header.sampleCount}`,
      )
    }

    descriptors.push(descriptor)
    // The reader returns one typed array per channel id instead of a row-oriented structure so
    // charting and nearest-sample lookups can work directly on contiguous channel buffers.
    channels.set(
        descriptor.channelId,
        readChannelArray(buffer, header.dataOffset, descriptor),
    )
  }

  return {
    header,
    descriptors,
    channels,
  }
}
