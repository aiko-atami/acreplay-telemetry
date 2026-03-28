#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "acrp/api.hpp"

extern "C" {
std::uint8_t* acrp_alloc_buffer(std::uint32_t len);
void acrp_free_buffer(std::uint8_t* ptr);
const char* acrp_last_error_message();
std::uint32_t acrp_last_error_length();
int acrp_inspect_json(const std::uint8_t* input_ptr, std::uint32_t input_len, std::uint8_t** output_ptr,
                      std::uint32_t* output_len);
int acrp_parse_car_json(const std::uint8_t* input_ptr, std::uint32_t input_len, std::uint32_t car_index,
                        std::uint8_t** output_ptr, std::uint32_t* output_len);
}

namespace {

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

std::string copyJsonBuffer(std::uint8_t* ptr, std::uint32_t len) {
  std::string json(reinterpret_cast<const char*>(ptr), len);
  acrp_free_buffer(ptr);
  return json;
}

std::string readLastError() { return std::string(acrp_last_error_message(), acrp_last_error_length()); }

std::string extractStringField(std::string_view json, std::string_view key) {
  const auto marker = "\"" + std::string(key) + "\":\"";
  const auto start = json.find(marker);
  if (start == std::string_view::npos) {
    throw std::runtime_error("Missing string field in JSON");
  }
  const auto valueStart = start + marker.size();
  const auto valueEnd = json.find('"', valueStart);
  if (valueEnd == std::string_view::npos) {
    throw std::runtime_error("Unterminated string field in JSON");
  }
  return std::string(json.substr(valueStart, valueEnd - valueStart));
}

std::uint32_t extractUintField(std::string_view json, std::string_view key) {
  const auto marker = "\"" + std::string(key) + "\":";
  const auto start = json.find(marker);
  if (start == std::string_view::npos) {
    throw std::runtime_error("Missing numeric field in JSON");
  }
  const auto valueStart = start + marker.size();
  const auto valueEnd = json.find_first_not_of("0123456789", valueStart);
  return static_cast<std::uint32_t>(std::stoul(std::string(json.substr(valueStart, valueEnd - valueStart))));
}

std::size_t countOccurrences(std::string_view text, std::string_view needle) {
  std::size_t count = 0;
  std::size_t offset = 0;
  while ((offset = text.find(needle, offset)) != std::string_view::npos) {
    ++count;
    offset += needle.size();
  }
  return count;
}

std::vector<std::byte> base64Decode(std::string_view encoded) {
  auto decodeChar = [](char ch) -> std::uint8_t {
    if (ch >= 'A' && ch <= 'Z') {
      return static_cast<std::uint8_t>(ch - 'A');
    }
    if (ch >= 'a' && ch <= 'z') {
      return static_cast<std::uint8_t>(ch - 'a' + 26);
    }
    if (ch >= '0' && ch <= '9') {
      return static_cast<std::uint8_t>(ch - '0' + 52);
    }
    if (ch == '+') {
      return 62;
    }
    if (ch == '/') {
      return 63;
    }
    throw std::runtime_error("Invalid base64 character");
  };

  if (encoded.size() % 4 != 0) {
    throw std::runtime_error("Invalid base64 length");
  }

  std::vector<std::byte> decoded;
  decoded.reserve((encoded.size() / 4) * 3);
  for (std::size_t i = 0; i < encoded.size(); i += 4) {
    const auto a = decodeChar(encoded[i]);
    const auto b = decodeChar(encoded[i + 1]);
    const bool hasC = encoded[i + 2] != '=';
    const bool hasD = encoded[i + 3] != '=';
    const auto c = hasC ? decodeChar(encoded[i + 2]) : 0;
    const auto d = hasD ? decodeChar(encoded[i + 3]) : 0;

    const auto packed = (static_cast<std::uint32_t>(a) << 18) | (static_cast<std::uint32_t>(b) << 12) |
                        (static_cast<std::uint32_t>(c) << 6) | static_cast<std::uint32_t>(d);

    decoded.push_back(static_cast<std::byte>((packed >> 16) & 0xFF));
    if (hasC) {
      decoded.push_back(static_cast<std::byte>((packed >> 8) & 0xFF));
    }
    if (hasD) {
      decoded.push_back(static_cast<std::byte>(packed & 0xFF));
    }
  }
  return decoded;
}

std::vector<std::string> extractLapPackBase64(std::string_view json) {
  std::vector<std::string> lapPacks;
  std::size_t offset = 0;
  static constexpr std::string_view kMarker = "\"base64\":\"";

  while ((offset = json.find(kMarker, offset)) != std::string_view::npos) {
    const auto valueStart = offset + kMarker.size();
    const auto valueEnd = json.find('"', valueStart);
    if (valueEnd == std::string_view::npos) {
      throw std::runtime_error("Unterminated base64 field in JSON");
    }
    lapPacks.emplace_back(json.substr(valueStart, valueEnd - valueStart));
    offset = valueEnd + 1;
  }

  return lapPacks;
}

}  // namespace

int main() {
  const auto fixturePath = std::filesystem::path(__FILE__).parent_path() / "fixtures" / "example.acreplay";
  const auto replayBytes = readFileBytes(fixturePath);
  const auto nativeManifest = acrp::inspectReplay(replayBytes);
  const auto nativeCar = acrp::parseCar(replayBytes, 0);

  std::uint8_t* inspectJsonPtr = nullptr;
  std::uint32_t inspectJsonLen = 0;
  const auto inspectStatus =
      acrp_inspect_json(reinterpret_cast<const std::uint8_t*>(replayBytes.data()),
                        static_cast<std::uint32_t>(replayBytes.size()), &inspectJsonPtr, &inspectJsonLen);
  assert(inspectStatus == 0);
  const auto inspectJson = copyJsonBuffer(inspectJsonPtr, inspectJsonLen);

  assert(extractStringField(inspectJson, "sourceSha256") == nativeManifest.sourceSha256);
  assert(extractStringField(inspectJson, "track") == nativeManifest.track);
  assert(extractStringField(inspectJson, "trackConfig") == nativeManifest.trackConfig);
  assert(extractStringField(inspectJson, "driverName") == nativeManifest.driverName);
  assert(extractUintField(inspectJson, "carIndex") == nativeManifest.carIndex);
  assert(extractUintField(inspectJson, "recordingIntervalMs") == nativeManifest.recordingIntervalMs);
  assert(countOccurrences(inspectJson, "\"lapNumber\":") == nativeManifest.laps.size());

  std::uint8_t* parseJsonPtr = nullptr;
  std::uint32_t parseJsonLen = 0;
  const auto parseStatus =
      acrp_parse_car_json(reinterpret_cast<const std::uint8_t*>(replayBytes.data()),
                          static_cast<std::uint32_t>(replayBytes.size()), 0, &parseJsonPtr, &parseJsonLen);
  assert(parseStatus == 0);
  const auto parseJson = copyJsonBuffer(parseJsonPtr, parseJsonLen);

  assert(extractStringField(parseJson, "sourceSha256") == nativeCar.manifest.sourceSha256);
  assert(extractStringField(parseJson, "track") == nativeCar.manifest.track);
  assert(extractStringField(parseJson, "trackConfig") == nativeCar.manifest.trackConfig);
  assert(extractStringField(parseJson, "driverName") == nativeCar.manifest.driverName);
  assert(extractUintField(parseJson, "carIndex") == nativeCar.manifest.carIndex);
  assert(extractUintField(parseJson, "recordingIntervalMs") == nativeCar.manifest.recordingIntervalMs);

  const auto wasmLapPacks = extractLapPackBase64(parseJson);
  assert(wasmLapPacks.size() == nativeCar.lapPacks.size());
  for (std::size_t index = 0; index < wasmLapPacks.size(); ++index) {
    const auto decoded = base64Decode(wasmLapPacks[index]);
    assert(decoded == nativeCar.lapPacks[index].bytes);
  }

  std::uint8_t* invalidPtr = acrp_alloc_buffer(4);
  invalidPtr[0] = 0;
  invalidPtr[1] = 1;
  invalidPtr[2] = 2;
  invalidPtr[3] = 3;
  std::uint8_t* outputPtr = nullptr;
  std::uint32_t outputLen = 0;
  const auto invalidStatus = acrp_inspect_json(invalidPtr, 4, &outputPtr, &outputLen);
  assert(invalidStatus != 0);
  assert(!readLastError().empty());
  acrp_free_buffer(invalidPtr);

  const auto invalidCarStatus =
      acrp_parse_car_json(reinterpret_cast<const std::uint8_t*>(replayBytes.data()),
                          static_cast<std::uint32_t>(replayBytes.size()), 999, &outputPtr, &outputLen);
  assert(invalidCarStatus != 0);
  assert(readLastError().find("Car index is out of range") != std::string::npos);

  std::cout << "wasm abi parity verified: " << nativeCar.lapPacks.size() << " lap packs\n";
  return 0;
}
