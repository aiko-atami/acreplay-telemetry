#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "acrp/manifest.hpp"

namespace acrp {

struct ParsedLapPack {
  std::uint16_t lapNumber{};
  std::vector<std::byte> bytes;
};

struct ParsedCarResult {
  ReplayManifestV1 manifest;
  std::vector<ParsedLapPack> lapPacks;
};

[[nodiscard]] ReplayManifestV1 inspectReplay(std::span<const std::byte> replayBytes);
[[nodiscard]] ParsedCarResult parseCar(std::span<const std::byte> replayBytes, std::uint32_t carIndex);

}  // namespace acrp
