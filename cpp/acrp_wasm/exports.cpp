#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "acrp/api.hpp"
#include "acrp/manifest.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

namespace {

std::string g_last_error;
std::vector<std::unique_ptr<std::byte[]>> g_owned_buffers;

std::string escapeJson(std::string_view value) {
  std::string out;
  out.reserve(value.size() + 8);
  for (const char ch : value) {
    switch (ch) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out += ch;
        break;
    }
  }
  return out;
}

std::string base64Encode(std::span<const std::byte> bytes) {
  static constexpr char kAlphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  std::string out;
  out.reserve(((bytes.size() + 2) / 3) * 4);
  for (std::size_t i = 0; i < bytes.size(); i += 3) {
    const auto a = std::to_integer<std::uint32_t>(bytes[i]);
    const auto b = i + 1 < bytes.size() ? std::to_integer<std::uint32_t>(bytes[i + 1]) : 0u;
    const auto c = i + 2 < bytes.size() ? std::to_integer<std::uint32_t>(bytes[i + 2]) : 0u;
    const auto packed = (a << 16) | (b << 8) | c;

    out += kAlphabet[(packed >> 18) & 0x3F];
    out += kAlphabet[(packed >> 12) & 0x3F];
    out += i + 1 < bytes.size() ? kAlphabet[(packed >> 6) & 0x3F] : '=';
    out += i + 2 < bytes.size() ? kAlphabet[packed & 0x3F] : '=';
  }
  return out;
}

std::string toJson(const acrp::ReplayManifestV1& manifest) {
  std::string json = "{";
  json += "\"sourceSha256\":\"" + escapeJson(manifest.sourceSha256) + "\",";
  json += "\"track\":\"" + escapeJson(manifest.track) + "\",";
  json += "\"trackConfig\":\"" + escapeJson(manifest.trackConfig) + "\",";
  json += "\"carIndex\":" + std::to_string(manifest.carIndex) + ",";
  json += "\"driverName\":\"" + escapeJson(manifest.driverName) + "\",";
  json += "\"recordingIntervalMs\":" + std::to_string(manifest.recordingIntervalMs) + ",";
  json += "\"laps\":[";
  for (std::size_t i = 0; i < manifest.laps.size(); ++i) {
    const auto& lap = manifest.laps[i];
    if (i > 0) {
      json += ",";
    }
    json += "{";
    json += "\"lapNumber\":" + std::to_string(lap.lapNumber) + ",";
    json += "\"lapTimeMs\":" + std::to_string(lap.lapTimeMs) + ",";
    json += "\"sampleCount\":" + std::to_string(lap.sampleCount) + ",";
    json += "\"isComplete\":";
    json += lap.isComplete ? "true" : "false";
    json += ",\"isValid\":";
    json += lap.isValid ? "true" : "false";
    json += "}";
  }
  json += "]}";
  return json;
}

std::string toJson(const acrp::ParsedCarResult& result) {
  std::string json = "{";
  json += "\"manifest\":";
  json += toJson(result.manifest);
  json += ",\"lapPacks\":[";
  for (std::size_t i = 0; i < result.lapPacks.size(); ++i) {
    const auto& lap = result.lapPacks[i];
    if (i > 0) {
      json += ",";
    }
    json += "{";
    json += "\"lapNumber\":" + std::to_string(lap.lapNumber) + ",";
    json += "\"base64\":\"" + base64Encode(lap.bytes) + "\"";
    json += "}";
  }
  json += "]}";
  return json;
}

int writeJsonResult(std::string json, std::uint8_t** output_ptr, std::uint32_t* output_len) {
  // The exported C ABI returns ownership of the JSON buffer to JS/WASM callers. Native tests
  // also exercise this path, so buffers are tracked here and released through acrp_free_buffer().
  auto buffer = std::make_unique<std::byte[]>(json.size());
  std::memcpy(buffer.get(), json.data(), json.size());
  *output_ptr = reinterpret_cast<std::uint8_t*>(buffer.get());
  *output_len = static_cast<std::uint32_t>(json.size());
  g_owned_buffers.push_back(std::move(buffer));
  return 0;
}

}  // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE std::uint8_t* acrp_alloc_buffer(std::uint32_t len) {
  return reinterpret_cast<std::uint8_t*>(new std::byte[len]);
}

EMSCRIPTEN_KEEPALIVE void acrp_free_buffer(std::uint8_t* ptr) {
  if (ptr == nullptr) {
    return;
  }

  auto* const target = reinterpret_cast<std::byte*>(ptr);
  for (auto it = g_owned_buffers.begin(); it != g_owned_buffers.end(); ++it) {
    if (it->get() == target) {
      // Erasing the unique_ptr drops ownership and frees memory allocated in writeJsonResult().
      g_owned_buffers.erase(it);
      return;
    }
  }

  delete[] target;
}

EMSCRIPTEN_KEEPALIVE const char* acrp_last_error_message() { return g_last_error.c_str(); }

EMSCRIPTEN_KEEPALIVE std::uint32_t acrp_last_error_length() {
  return static_cast<std::uint32_t>(g_last_error.size());
}

EMSCRIPTEN_KEEPALIVE int acrp_inspect_json(const std::uint8_t* input_ptr, std::uint32_t input_len,
                                           std::uint8_t** output_ptr, std::uint32_t* output_len) {
  try {
    g_last_error.clear();
    const auto result =
        acrp::inspectReplay(std::span(reinterpret_cast<const std::byte*>(input_ptr), input_len));
    return writeJsonResult(toJson(result), output_ptr, output_len);
  } catch (const std::exception& error) {
    g_last_error = error.what();
    return 1;
  }
}

EMSCRIPTEN_KEEPALIVE int acrp_parse_car_json(const std::uint8_t* input_ptr, std::uint32_t input_len,
                                             std::uint32_t car_index, std::uint8_t** output_ptr,
                                             std::uint32_t* output_len) {
  try {
    g_last_error.clear();
    const auto result = acrp::parseCar(
        std::span(reinterpret_cast<const std::byte*>(input_ptr), input_len), car_index);
    return writeJsonResult(toJson(result), output_ptr, output_len);
  } catch (const std::exception& error) {
    g_last_error = error.what();
    return 1;
  }
}

}  // extern "C"

#ifndef ACRP_WASM_NO_MAIN
int main() { return 0; }
#endif
