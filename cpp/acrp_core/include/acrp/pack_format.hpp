#pragma once

#include <cstdint>

namespace acrp {

// ── LapTelemetryPackV1 binary layout ────────────────────────────────────────
//
// All multi-byte integer fields are little-endian.
//
// The blob is structured in three contiguous sections:
//   [0]  PackHeaderV1           — fixed 36 bytes
//   [36] ChannelDescriptorV1[]  — channelCount × 16 bytes
//   [dataOffset] channel data   — one tightly-packed array per channel
//
// PackHeaderV1 byte map:
//   Offset  Size  Type    Field
//   ──────  ────  ──────  ─────────────────────────────────────────────────
//        0     4  char[4] magic              "ACTL" (0x41 0x43 0x54 0x4C)
//        4     2  uint16  version            always 1
//        6     2  uint16  headerBytes        fixed: 36
//        8     2  uint16  descriptorBytes    fixed: 16 (size of one ChannelDescriptorV1)
//       10     2  uint16  channelCount       number of channels in this pack
//       12     4  uint32  sampleCount        samples per channel (all channels share this count)
//       16     2  uint16  lapNumber          1-based lap number within the session
//       18     2  uint16  flags              bitmask — see kPackFlag* constants
//       20     4  uint32  lapTimeMs          lap duration in milliseconds
//       24     4  uint32  recordingIntervalUs  sample interval in microseconds
//       28     4  uint32  descriptorsOffset  always == headerBytes (36)
//       32     4  uint32  dataOffset         == descriptorsOffset + channelCount × descriptorBytes
//
// flags bitmask (PackHeaderV1::flags):
//   bit 0  kPackFlagHasCsp      — CSP EXT_PERCAR data present; channel 64 (clutch_raw) included
//   bit 1  kPackFlagIsComplete  — lap started at time 0 and was fully recorded
//   bit 2  kPackFlagIsValid     — lap data passed basic sanity checks
//
// ── ChannelDescriptorV1 byte map ────────────────────────────────────────────
//
//   Offset  Size  Type    Field
//   ──────  ────  ──────  ─────────────────────────────────────────────────
//        0     2  uint16  channelId    see ChannelId enum
//        2     1  uint8   valueType    see ChannelValueType: f32=1, u8=2, i8=3
//        3     1  uint8   reserved     always 0
//        4     4  uint32  sampleCount  must equal PackHeaderV1::sampleCount
//        8     4  uint32  byteOffset   offset into the data block relative to dataOffset
//       12     4  uint32  byteLength   total bytes for channel = sampleCount × sizeof(valueType)

constexpr std::uint16_t kPackFlagHasCsp = 1u << 0;
constexpr std::uint16_t kPackFlagIsComplete = 1u << 1;
constexpr std::uint16_t kPackFlagIsValid = 1u << 2;

// Scalar type stored in a channel's data array.
enum class ChannelValueType : std::uint8_t {
  f32 = 1,  // IEEE-754 single precision, 4 bytes per sample
  u8 = 2,   // unsigned byte, 1 byte per sample  (pedals, flags, gear)
  i8 = 3,   // signed byte, 1 byte per sample    (gear, relative values)
};

struct PackHeaderV1 {
  char magic[4];               // "ACTL"
  std::uint16_t version;       // 1
  std::uint16_t headerBytes;   // 36 (fixed)
  std::uint16_t descriptorBytes;  // 16 (fixed, size of one ChannelDescriptorV1)
  std::uint16_t channelCount;
  std::uint32_t sampleCount;   // same for every channel
  std::uint16_t lapNumber;     // 1-based
  std::uint16_t flags;         // kPackFlag* bitmask
  std::uint32_t lapTimeMs;
  std::uint32_t recordingIntervalUs;
  std::uint32_t descriptorsOffset;  // == headerBytes (36)
  // Channel data block starts here. ChannelDescriptorV1::byteOffset is relative to this field.
  std::uint32_t dataOffset;
};

struct ChannelDescriptorV1 {
  std::uint16_t channelId;      // see ChannelId enum
  ChannelValueType valueType;   // f32=1, u8=2, i8=3
  std::uint8_t reserved;        // always 0
  std::uint32_t sampleCount;    // must equal PackHeaderV1::sampleCount
  std::uint32_t byteOffset;     // byte offset into the data block, relative to dataOffset
  std::uint32_t byteLength;     // = sampleCount × sizeof(valueType)
};

}  // namespace acrp
