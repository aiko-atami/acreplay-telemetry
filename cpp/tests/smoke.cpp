#include <algorithm>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <vector>

#include "acrp/api.hpp"
#include "acrp/channel_ids.hpp"
#include "acrp/pack_format.hpp"

namespace {

constexpr std::uint16_t kFlagHasCsp = 1u << 0;
constexpr std::uint16_t kFlagIsComplete = 1u << 1;
constexpr std::uint16_t kFlagIsValid = 1u << 2;

std::vector<std::byte> readFileBytes(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("Failed to open fixture replay");
  }
  input.seekg(0, std::ios::end);
  const auto size = input.tellg();
  input.seekg(0, std::ios::beg);

  std::vector<std::byte> bytes(static_cast<std::size_t>(size));
  input.read(reinterpret_cast<char*>(bytes.data()), size);
  if (!input) {
    throw std::runtime_error("Failed to read fixture replay");
  }
  return bytes;
}

std::uint16_t readU16(std::span<const std::byte> bytes, std::size_t offset) {
  const auto low = static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset]));
  const auto high = static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset + 1]));
  return static_cast<std::uint16_t>(low | static_cast<std::uint16_t>(high << 8));
}

std::uint32_t readU32(std::span<const std::byte> bytes, std::size_t offset) {
  return static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset])) |
         (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8) |
         (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 2])) << 16) |
         (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 3])) << 24);
}

float readF32(std::span<const std::byte> bytes, std::size_t offset) {
  return std::bit_cast<float>(readU32(bytes, offset));
}

std::int8_t readI8(std::span<const std::byte> bytes, std::size_t offset) {
  return std::bit_cast<std::int8_t>(std::to_integer<std::uint8_t>(bytes[offset]));
}

struct ChannelDescriptorView {
  std::uint16_t channelId{};
  acrp::ChannelValueType valueType{};
  std::uint32_t sampleCount{};
  std::uint32_t byteOffset{};
  std::uint32_t byteLength{};
};

ChannelDescriptorView readChannelDescriptor(std::span<const std::byte> pack, std::uint16_t index) {
  const auto channelCount = readU16(pack, 10);
  const auto descriptorBytes = readU16(pack, 8);
  const auto descriptorsOffset = readU32(pack, 28);
  assert(index < channelCount);
  const auto offset = descriptorsOffset + static_cast<std::size_t>(index) * descriptorBytes;
  return ChannelDescriptorView{
      .channelId = readU16(pack, offset),
      .valueType = static_cast<acrp::ChannelValueType>(std::to_integer<std::uint8_t>(pack[offset + 2])),
      .sampleCount = readU32(pack, offset + 4),
      .byteOffset = readU32(pack, offset + 8),
      .byteLength = readU32(pack, offset + 12),
  };
}

ChannelDescriptorView findChannelDescriptor(std::span<const std::byte> pack, std::uint16_t channelId) {
  const auto channelCount = readU16(pack, 10);
  for (std::uint16_t index = 0; index < channelCount; ++index) {
    const auto descriptor = readChannelDescriptor(pack, index);
    if (descriptor.channelId != channelId) {
      continue;
    }
    return descriptor;
  }

  throw std::runtime_error("Required channel is missing from pack");
}

std::size_t getChannelDataOffset(std::span<const std::byte> pack, std::uint16_t channelId) {
  const auto descriptor = findChannelDescriptor(pack, channelId);
  return readU32(pack, 32) + descriptor.byteOffset;
}

void validatePack(const acrp::ParsedLapPack& lapPack, const acrp::LapManifestV1& lapManifest) {
  const auto pack = std::span<const std::byte>(lapPack.bytes);
  assert(pack.size() >= 36);
  assert(std::to_integer<char>(pack[0]) == 'A');
  assert(std::to_integer<char>(pack[1]) == 'C');
  assert(std::to_integer<char>(pack[2]) == 'T');
  assert(std::to_integer<char>(pack[3]) == 'L');
  assert(readU16(pack, 4) == 1);
  assert(readU16(pack, 6) == 36);
  assert(readU16(pack, 8) == 16);

  const auto channelCount = readU16(pack, 10);
  const auto sampleCount = readU32(pack, 12);
  const auto lapNumber = readU16(pack, 16);
  const auto flags = readU16(pack, 18);
  const auto lapTimeMs = readU32(pack, 20);
  const auto recordingIntervalUs = readU32(pack, 24);
  const auto descriptorsOffset = readU32(pack, 28);
  const auto dataOffset = readU32(pack, 32);

  assert(channelCount > 0);
  assert(sampleCount > 0);
  assert(lapNumber == lapPack.lapNumber);
  assert(lapNumber == lapManifest.lapNumber);
  assert(sampleCount == lapManifest.sampleCount);
  assert(lapTimeMs == lapManifest.lapTimeMs);
  assert(descriptorsOffset == 36);
  assert(dataOffset == 36 + static_cast<std::uint32_t>(channelCount) * 16);
  assert(dataOffset <= pack.size());
  assert(recordingIntervalUs > 0);
  assert(((flags & kFlagIsComplete) != 0) == lapManifest.isComplete);
  assert(((flags & kFlagIsValid) != 0) == lapManifest.isValid);

  for (std::uint16_t index = 0; index < channelCount; ++index) {
    const auto descriptor = readChannelDescriptor(pack, index);
    const auto expectedElementSize = descriptor.valueType == acrp::ChannelValueType::f32 ? 4u : 1u;
    assert(descriptor.sampleCount == sampleCount);
    assert(descriptor.byteLength == sampleCount * expectedElementSize);
    assert(dataOffset + descriptor.byteOffset + descriptor.byteLength <= pack.size());
  }

  for (const auto channelId : {
           static_cast<std::uint16_t>(acrp::ChannelId::distance_m),
           static_cast<std::uint16_t>(acrp::ChannelId::time_ms),
           static_cast<std::uint16_t>(acrp::ChannelId::pos_x),
           static_cast<std::uint16_t>(acrp::ChannelId::pos_y),
           static_cast<std::uint16_t>(acrp::ChannelId::pos_z),
           static_cast<std::uint16_t>(acrp::ChannelId::yaw_rad),
           static_cast<std::uint16_t>(acrp::ChannelId::rot_x_rad),
           static_cast<std::uint16_t>(acrp::ChannelId::rot_y_rad),
           static_cast<std::uint16_t>(acrp::ChannelId::rot_z_rad),
           static_cast<std::uint16_t>(acrp::ChannelId::speed_kmh),
           static_cast<std::uint16_t>(acrp::ChannelId::vel_x_mps),
           static_cast<std::uint16_t>(acrp::ChannelId::vel_y_mps),
           static_cast<std::uint16_t>(acrp::ChannelId::vel_z_mps),
           static_cast<std::uint16_t>(acrp::ChannelId::throttle_raw),
           static_cast<std::uint16_t>(acrp::ChannelId::brake_raw),
           static_cast<std::uint16_t>(acrp::ChannelId::steer_deg),
           static_cast<std::uint16_t>(acrp::ChannelId::gear),
           static_cast<std::uint16_t>(acrp::ChannelId::rpm),
           static_cast<std::uint16_t>(acrp::ChannelId::coast),
           static_cast<std::uint16_t>(acrp::ChannelId::fuel_raw),
           static_cast<std::uint16_t>(acrp::ChannelId::fuel_per_lap_raw),
           static_cast<std::uint16_t>(acrp::ChannelId::boost_raw),
           static_cast<std::uint16_t>(acrp::ChannelId::engine_health_raw),
           static_cast<std::uint16_t>(acrp::ChannelId::gearbox_being_damaged),
           static_cast<std::uint16_t>(acrp::ChannelId::drivetrain_speed),
           static_cast<std::uint16_t>(acrp::ChannelId::wheel_ang_vel_fl),
           static_cast<std::uint16_t>(acrp::ChannelId::wheel_ang_vel_fr),
           static_cast<std::uint16_t>(acrp::ChannelId::wheel_ang_vel_rl),
           static_cast<std::uint16_t>(acrp::ChannelId::wheel_ang_vel_rr),
           static_cast<std::uint16_t>(acrp::ChannelId::wheel_slip_ratio_fl),
           static_cast<std::uint16_t>(acrp::ChannelId::wheel_slip_ratio_fr),
           static_cast<std::uint16_t>(acrp::ChannelId::wheel_slip_ratio_rl),
           static_cast<std::uint16_t>(acrp::ChannelId::wheel_slip_ratio_rr),
           static_cast<std::uint16_t>(acrp::ChannelId::wheel_nd_slip_fl),
           static_cast<std::uint16_t>(acrp::ChannelId::wheel_nd_slip_fr),
           static_cast<std::uint16_t>(acrp::ChannelId::wheel_nd_slip_rl),
           static_cast<std::uint16_t>(acrp::ChannelId::wheel_nd_slip_rr),
           static_cast<std::uint16_t>(acrp::ChannelId::wheel_load_fl_n),
           static_cast<std::uint16_t>(acrp::ChannelId::wheel_load_fr_n),
           static_cast<std::uint16_t>(acrp::ChannelId::wheel_load_rl_n),
           static_cast<std::uint16_t>(acrp::ChannelId::wheel_load_rr_n),
       }) {
    (void)findChannelDescriptor(pack, channelId);
  }

  const auto distanceOffset = getChannelDataOffset(pack, static_cast<std::uint16_t>(acrp::ChannelId::distance_m));
  const auto timeOffset = getChannelDataOffset(pack, static_cast<std::uint16_t>(acrp::ChannelId::time_ms));
  const auto speedOffset = getChannelDataOffset(pack, static_cast<std::uint16_t>(acrp::ChannelId::speed_kmh));
  const auto gearOffset = getChannelDataOffset(pack, static_cast<std::uint16_t>(acrp::ChannelId::gear));
  const auto yawOffset = getChannelDataOffset(pack, static_cast<std::uint16_t>(acrp::ChannelId::yaw_rad));

  float previousDistance = readF32(pack, distanceOffset);
  float previousTime = readF32(pack, timeOffset);
  assert(std::abs(previousDistance) < 0.001f);
  assert(std::isfinite(readF32(pack, yawOffset)));

  for (std::uint32_t sampleIndex = 1; sampleIndex < sampleCount; ++sampleIndex) {
    const auto distance = readF32(pack, distanceOffset + static_cast<std::size_t>(sampleIndex) * 4);
    const auto time = readF32(pack, timeOffset + static_cast<std::size_t>(sampleIndex) * 4);
    const auto speed = readF32(pack, speedOffset + static_cast<std::size_t>(sampleIndex) * 4);
    const auto gear = readI8(pack, gearOffset + static_cast<std::size_t>(sampleIndex));
    assert(distance >= previousDistance);
    assert(time >= previousTime);
    assert(std::isfinite(speed));
    assert(speed >= 0.0f);
    assert(gear >= -1);
    assert(gear <= 10);
    previousDistance = distance;
    previousTime = time;
  }

  const bool hasCsp = (flags & kFlagHasCsp) != 0;
  if (hasCsp) {
    const auto clutchDescriptor = findChannelDescriptor(pack, static_cast<std::uint16_t>(acrp::ChannelId::clutch_raw));
    assert(clutchDescriptor.valueType == acrp::ChannelValueType::u8);
  }
}

}  // namespace

int main() {
  const auto fixturePath = std::filesystem::path(__FILE__).parent_path() / "fixtures" / "example.acreplay";
  const auto replayBytes = readFileBytes(fixturePath);

  const auto manifest = acrp::inspectReplay(replayBytes);
  assert(!manifest.sourceSha256.empty());
  assert(!manifest.track.empty());
  assert(manifest.recordingIntervalMs > 0);
  assert(!manifest.laps.empty());

  const auto parsedCar = acrp::parseCar(replayBytes, 0);
  assert(parsedCar.manifest.carIndex == 0);
  assert(parsedCar.manifest.sourceSha256 == manifest.sourceSha256);
  assert(parsedCar.manifest.track == manifest.track);
  assert(parsedCar.manifest.trackConfig == manifest.trackConfig);
  assert(parsedCar.manifest.recordingIntervalMs == manifest.recordingIntervalMs);
  assert(parsedCar.manifest.laps.size() == parsedCar.lapPacks.size());
  assert(!parsedCar.lapPacks.empty());
  assert(parsedCar.manifest.laps.back().isComplete == false);

  for (std::size_t index = 0; index < parsedCar.lapPacks.size(); ++index) {
    assert(parsedCar.manifest.laps[index].lapNumber == parsedCar.lapPacks[index].lapNumber);
    validatePack(parsedCar.lapPacks[index], parsedCar.manifest.laps[index]);
  }

  std::cout << "fixture replay parsed successfully: " << parsedCar.lapPacks.size() << " lap packs\n";
  return 0;
}
