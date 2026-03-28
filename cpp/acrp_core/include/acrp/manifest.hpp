#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace acrp {

struct LapManifestV1 {
  std::uint16_t lapNumber{};
  std::uint32_t lapTimeMs{};
  std::uint32_t sampleCount{};
  bool isComplete{};
  bool isValid{};
};

struct ReplayManifestV1 {
  std::string sourceSha256;
  std::string track;
  std::string trackConfig;
  std::uint32_t carIndex{};
  std::string driverName;
  std::uint32_t recordingIntervalMs{};
  std::vector<LapManifestV1> laps;
};

}  // namespace acrp
