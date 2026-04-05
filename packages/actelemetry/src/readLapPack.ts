import type {
  ChannelArray,
  ChannelDescriptorV1,
  ChannelValueType,
  LapTelemetryPackView,
  PackHeaderV1,
} from "./types.js";
import { validateLapPack } from "./validateLapPack.js";

function readChannelArray(
  buffer: ArrayBuffer,
  dataOffset: number,
  descriptor: ChannelDescriptorV1,
): ChannelArray {
  const start = dataOffset + descriptor.byteOffset;
  const end = start + descriptor.byteLength;
  if (end > buffer.byteLength) {
    throw new Error(`Channel ${descriptor.channelId} exceeds buffer length`);
  }

  if (descriptor.valueType === 1) {
    return new Float32Array(buffer.slice(start, end));
  }
  if (descriptor.valueType === 2) {
    return new Uint8Array(buffer.slice(start, end));
  }
  return new Int8Array(buffer.slice(start, end));
}

/**
 * Parses a raw `LapTelemetryPackV1` binary blob into a structured {@link LapTelemetryPackView}.
 *
 * Validates the buffer first (see {@link validateLapPack}), reads the fixed-layout header,
 * iterates the channel descriptor table, and slices each channel into a typed array.
 *
 * @param buffer - Raw `ArrayBuffer` containing the `.acpack` binary data.
 * @returns Parsed lap telemetry view with header, descriptors, and a channel map.
 * @throws {Error} If the buffer is malformed, too small, or has an unsupported version.
 */
export function readLapPack(buffer: ArrayBuffer): LapTelemetryPackView {
  validateLapPack(buffer);

  const view = new DataView(buffer);
  // Read PackHeaderV1 field by field using the fixed byte-offset layout.
  // Bytes 0–3 are the "ACTL" magic; validateLapPack already confirmed them.
  const header: PackHeaderV1 = {
    magic: "ACTL",
    version: view.getUint16(4, true), // +4  uint16 version
    headerBytes: view.getUint16(6, true), // +6  uint16 headerBytes (36)
    descriptorBytes: view.getUint16(8, true), // +8  uint16 descriptorBytes (16)
    channelCount: view.getUint16(10, true), // +10 uint16 channelCount
    sampleCount: view.getUint32(12, true), // +12 uint32 sampleCount
    lapNumber: view.getUint16(16, true), // +16 uint16 lapNumber (1-based)
    flags: view.getUint16(18, true), // +18 uint16 flags (PACK_FLAGS bitmask)
    lapTimeMs: view.getUint32(20, true), // +20 uint32 lapTimeMs
    recordingIntervalUs: view.getUint32(24, true), // +24 uint32 recordingIntervalUs
    descriptorsOffset: view.getUint32(28, true), // +28 uint32 descriptorsOffset (== 36)
    dataOffset: view.getUint32(32, true), // +32 uint32 dataOffset
  };

  const descriptors: ChannelDescriptorV1[] = [];
  const channels = new Map<number, ChannelArray>();

  for (let index = 0; index < header.channelCount; index += 1) {
    const offset = header.descriptorsOffset + index * header.descriptorBytes;
    const descriptor: ChannelDescriptorV1 = {
      channelId: view.getUint16(offset + 0, true), // +0  uint16 channelId
      valueType: view.getUint8(offset + 2) as ChannelValueType, // +2  uint8  valueType (1/2/3)
      // +3: reserved byte (always 0), not stored in the TS view
      sampleCount: view.getUint32(offset + 4, true), // +4  uint32 sampleCount
      byteOffset: view.getUint32(offset + 8, true), // +8  uint32 byteOffset (→ dataOffset)
      byteLength: view.getUint32(offset + 12, true), // +12 uint32 byteLength
    };

    if (descriptor.sampleCount !== header.sampleCount) {
      throw new Error(
        `Channel ${descriptor.channelId} has sampleCount ${descriptor.sampleCount}, expected ${header.sampleCount}`,
      );
    }

    descriptors.push(descriptor);
    // The reader returns one typed array per channel id instead of a row-oriented structure so
    // charting and nearest-sample lookups can work directly on contiguous channel buffers.
    channels.set(descriptor.channelId, readChannelArray(buffer, header.dataOffset, descriptor));
  }

  return {
    header,
    descriptors,
    channels,
  };
}
