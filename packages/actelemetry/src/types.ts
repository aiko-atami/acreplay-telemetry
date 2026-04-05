// ── LapTelemetryPackV1 binary layout ────────────────────────────────────────
//
// All multi-byte integer fields are little-endian. The blob has three sections:
//   [0]          PackHeaderV1 (36 bytes, fixed)
//   [36]         ChannelDescriptorV1[] (channelCount × 16 bytes)
//   [dataOffset] channel data arrays (one typed array per channel)
//
// PackHeaderV1 byte map:
//   Offset  Size  Field
//   ──────  ────  ──────────────────────────────────────────────────────────
//        0     4  magic[4]              "ACTL" (0x41 0x43 0x54 0x4C)
//        4     2  version               always 1
//        6     2  headerBytes           fixed: 36
//        8     2  descriptorBytes       fixed: 16 (bytes per ChannelDescriptorV1)
//       10     2  channelCount
//       12     4  sampleCount           same for every channel in this pack
//       16     2  lapNumber             1-based lap number in the session
//       18     2  flags                 see PACK_FLAGS bitmask
//       20     4  lapTimeMs
//       24     4  recordingIntervalUs   sample interval in microseconds
//       28     4  descriptorsOffset     always == headerBytes (36)
//       32     4  dataOffset            == descriptorsOffset + channelCount × descriptorBytes
//
// ChannelDescriptorV1 byte map (offset relative to start of descriptor record):
//   Offset  Size  Field
//   ──────  ────  ──────────────────────────────────────────────────────────
//        0     2  channelId             see channelIds constant map
//        2     1  valueType             1=f32, 2=u8, 3=i8 (see ChannelValueType)
//        3     1  reserved              always 0 — skip when reading
//        4     4  sampleCount           must equal PackHeaderV1.sampleCount
//        8     4  byteOffset            offset into data block relative to dataOffset
//       12     4  byteLength            = sampleCount × sizeof(valueType)

/**
 * Numeric tag stored in {@link ChannelDescriptorV1.valueType}.
 * - `1` → `Float32Array` (4 bytes/sample) — positions, velocities, RPM, etc.
 * - `2` → `Uint8Array`   (1 byte/sample, 0–255) — pedals, boolean flags, fuel, health
 * - `3` → `Int8Array`    (1 byte/sample, −128..127) — gear
 */
export type ChannelValueType = 1 | 2 | 3;

/** Bitmask flags stored in {@link PackHeaderV1.flags}. */
export const PACK_FLAGS = {
  /** CSP EXT_PERCAR data present; channel 64 (`clutch_raw`) is included in this pack. */
  HAS_CSP: 1 << 0,
  /** Lap started at time 0 and was fully recorded without interruption. */
  IS_COMPLETE: 1 << 1,
  /** Lap data passed basic sanity checks during export. */
  IS_VALID: 1 << 2,
} as const;

/** Parsed representation of the 36-byte `PackHeaderV1` at the start of a `.acpack` blob. */
export type PackHeaderV1 = {
  /** Always `"ACTL"` — identifies the binary format. */
  magic: string;
  /** Format version. Always `1` in the current spec. */
  version: number;
  /** Size of this header in bytes. Always `36`. */
  headerBytes: number;
  /** Size of one {@link ChannelDescriptorV1} record in bytes. Always `16`. */
  descriptorBytes: number;
  /** Number of channels stored in this pack. */
  channelCount: number;
  /** Number of samples per channel. All channels share this count. */
  sampleCount: number;
  /** 1-based lap index within the recording session. */
  lapNumber: number;
  /** {@link PACK_FLAGS} bitmask. Check `HAS_CSP`, `IS_COMPLETE`, `IS_VALID`. */
  flags: number;
  /** Total lap time in milliseconds. */
  lapTimeMs: number;
  /** Sample interval in microseconds. Reciprocal gives the recording frequency. */
  recordingIntervalUs: number;
  /** Byte offset to the descriptor table. Always equals `headerBytes` (36). */
  descriptorsOffset: number;
  /** Byte offset to the channel data block. Equals `descriptorsOffset + channelCount × descriptorBytes`. */
  dataOffset: number;
};

/** Parsed representation of one 16-byte `ChannelDescriptorV1` record. */
export type ChannelDescriptorV1 = {
  /** Channel identifier. See {@link channelIds} for the full mapping. */
  channelId: number;
  /** Numeric encoding of the element type. See {@link ChannelValueType}. */
  valueType: ChannelValueType;
  /** Number of samples. Must equal {@link PackHeaderV1.sampleCount}. */
  sampleCount: number;
  /** Byte offset into the data block, relative to {@link PackHeaderV1.dataOffset}. */
  byteOffset: number;
  /** Total byte length of this channel's data array (`sampleCount × sizeof(valueType)`). */
  byteLength: number;
};

/**
 * Typed array holding all samples for one channel.
 * - `Float32Array` — {@link ChannelValueType} `1` (f32)
 * - `Uint8Array`   — {@link ChannelValueType} `2` (u8): pedals, boolean flags, engine health
 * - `Int8Array`    — {@link ChannelValueType} `3` (i8): gear (−1=reverse, 0=neutral, 1=first…)
 */
export type ChannelArray = Float32Array | Uint8Array | Int8Array;

/** Fully parsed view of a `LapTelemetryPackV1` binary blob. */
export type LapTelemetryPackView = {
  /** Parsed pack header. */
  header: PackHeaderV1;
  /** One descriptor per channel, in the order they appear in the binary. */
  descriptors: ChannelDescriptorV1[];
  /** Map from `channelId` → typed array of {@link PackHeaderV1.sampleCount} values. */
  channels: Map<number, ChannelArray>;
};
