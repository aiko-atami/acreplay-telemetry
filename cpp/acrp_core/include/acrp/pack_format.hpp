#pragma once

#include <cstdint>

namespace acrp {

enum class ChannelValueType : std::uint8_t {
  f32 = 1,
  u8 = 2,
  i8 = 3,
};

struct PackHeaderV1 {
  char magic[4];
  std::uint16_t version;
  std::uint16_t headerBytes;
  std::uint16_t descriptorBytes;
  std::uint16_t channelCount;
  std::uint32_t sampleCount;
  std::uint16_t lapNumber;
  std::uint16_t flags;
  std::uint32_t lapTimeMs;
  std::uint32_t recordingIntervalUs;
  std::uint32_t descriptorsOffset;
  // Payload bytes start here. ChannelDescriptorV1::byteOffset is relative to this field.
  std::uint32_t dataOffset;
};

struct ChannelDescriptorV1 {
  std::uint16_t channelId;
  ChannelValueType valueType;
  std::uint8_t reserved;
  std::uint32_t sampleCount;
  std::uint32_t byteOffset;
  std::uint32_t byteLength;
};

}  // namespace acrp
