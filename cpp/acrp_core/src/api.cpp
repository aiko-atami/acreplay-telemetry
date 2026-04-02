#include "acrp/api.hpp"

#if ACRP_HAS_ZLIB
#include <zlib.h>
#endif

#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <streambuf>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "acrp/csv_export.hpp"
#include "acrp/pack_format.hpp"
#include "acrp/replay_types.hpp"
#include "acrp/status.hpp"

namespace acrp {

namespace {

// Reverse-engineered layout of a version-16 `.acreplay` file as used by the current parser:
// 1. uint32 version
// 2. ReplayHeader payload: double recordingInterval, 3 length-prefixed strings, 4 uint32 values
// 3. replay-global frame data, skipped as `(4 + numTrackObjects * 12) * numFrames` bytes
// 4. `numCars` car sections:
//    - CarHeader payload: 5 length-prefixed strings, then numFrames + numWings
//    - one unresolved 20-byte block before the first CarFrame
//    - `numFrames` CarFrame payloads with unresolved gaps between frames
//    - `numWings * 4` bytes after each frame, plus another unresolved 20-byte block before each
//      non-final frame
//    - after the last frame, `uint32 count` followed by `count * 8` trailing bytes
// 5. optional CSP footer at EOF: `"__AC_SHADERS_PATCH_v1__" + uint32 offset + uint32 version`
//
// Confirmed by parser behavior: the parse order and skip sizes above are required to walk the
// real fixture correctly. Unknown / unresolved: several skipped blocks are still treated as opaque.
constexpr std::string_view kCsvExtension = "csv";
constexpr std::string_view kCspFooter = "__AC_SHADERS_PATCH_v1__";
constexpr std::string_view kDriverNameIni = "DRIVER_NAME=";
constexpr std::string_view kExtPerCar = "EXT_PERCAR";
// Zero means the tag version exists in the wild or legacy code, but this repo does not yet know a
// trustworthy fixed record size for one decompressed frame of that version.
constexpr std::array<std::uint32_t, 7> kExtPerCarBytesPerFrame = {
    0, 0, 0, 0, 0, 108, 108,
};

// CSP EXT_PERCAR v6/v7 payload. The overall record size is confirmed by fixture-backed parsing,
// but most field semantics are still unresolved, so the raw layout is preserved verbatim.
//
// Inferred from legacy parser / fixture behavior:
// - `wipers` is 0..4
// - `handbrake` and `clutch` are 0..255 pedal-style values
// - `status & 0b0111` behaves like turn-signal state
// - `(status >> 3) & 0x1` behaves like low-beam state
//
// Unknown / unresolved:
// - the remaining `status` bits and most scalar fields are not yet decoded with confidence
struct CarFrameExtraV6 {
  std::uint32_t i;
  acrp::HalfFloat h;
  acrp::HalfFloat h2;
  acrp::HalfFloat h3;
  acrp::HalfFloat h4;
  float f;
  acrp::HalfFloat h5;
  acrp::HalfFloat h6;
  acrp::HalfFloat h7;
  acrp::HalfFloat h8;
  acrp::HalfFloat h9;
  acrp::HalfFloat h10;
  acrp::HalfFloat h11;
  acrp::HalfFloat h12;
  float f2;
  acrp::HalfFloat h13;
  acrp::HalfFloat h14;
  std::uint32_t i2;
  acrp::HalfFloat h15;
  acrp::HalfFloat h16;
  acrp::HalfFloat h17;
  acrp::HalfFloat h18;
  float f3;
  acrp::HalfFloat h19;
  acrp::HalfFloat h20;
  std::uint32_t i3;
  acrp::HalfFloat h21;
  acrp::HalfFloat h22;
  acrp::HalfFloat h23;
  acrp::HalfFloat h24;
  float f4;
  acrp::HalfFloat h25;
  acrp::HalfFloat h26;
  std::uint32_t i4;
  std::uint32_t i5;
  std::uint8_t b;
  std::uint8_t wipers;
  std::uint16_t status;
  std::uint8_t handbrake;
  std::uint8_t b2;
  std::uint8_t b3;
  std::uint8_t b4;
  acrp::HalfFloat h27;
  std::uint8_t clutch;
  std::uint8_t b5;
  std::uint32_t i6;
  acrp::HalfFloat h28;
  acrp::HalfFloat h29;

  static constexpr const char* label =
      "h0,h1,h2,h3,h4,f0,h5,h6,h7,h8,h9,h10,h11,h12,f1,h13,h14,i2,h15,h16,h17,h18,"
      "f2,h19,h20,i3,h21,h22,h23,h24,f3,h25,h26,i4,i5,b,wipers,turnSignals,lowBeams,"
      "handbrake,b2,b3,b4,h27,clutch,b5,i6,h28,h29";

  void outputData(std::ostream& out) const {
    out << i << ',' << h << ',' << h2 << ',' << h3 << ',' << h4 << ',' << f << ',' << h5 << ','
        << h6 << ',' << h7 << ',' << h8 << ',' << h9 << ',' << h10 << ',' << h11 << ',' << h12
        << ',' << f2 << ',' << h13 << ',' << h14 << ',' << i2 << ',' << h15 << ',' << h16 << ','
        << h17 << ',' << h18 << ',' << f3 << ',' << h19 << ',' << h20 << ',' << i3 << ',' << h21
        << ',' << h22 << ',' << h23 << ',' << h24 << ',' << f4 << ',' << h25 << ',' << h26 << ','
        << i4 << ',' << i5 << ',' << +b << ',' << +wipers << ',' << (status & 0b0111) << ','
        << ((status >> 3) & 0x1) << ',' << +handbrake << ',' << +b2 << ',' << +b3 << ',' << +b4
        << ',' << h27 << ',' << +clutch << ',' << +b5 << ',' << i6 << ',' << h28 << ',' << h29;
  }
};

using CarFrameExtraV7 = CarFrameExtraV6;

struct ParsedCarData {
  CarHeader header;
  std::vector<CarFrame> frames;
  std::optional<std::vector<CarFrameExtraV6>> extraFramesV6;
  std::optional<std::vector<CarFrameExtraV7>> extraFramesV7;
};

struct ParsedReplayData {
  ReplayHeader header;
  std::vector<std::string> cspDriverNames;
  std::vector<ParsedCarData> cars;
};

struct LapSlice {
  std::size_t beginFrame{};
  std::size_t endFrame{};
  std::uint16_t lapNumber{};
  std::uint32_t lapTimeMs{};
  bool isComplete{};
  bool isValid{};
};

struct ChannelPayload {
  std::uint16_t channelId{};
  ChannelValueType valueType{ChannelValueType::f32};
  std::vector<std::byte> bytes;
};

struct DriverNamesSection {
  std::uint32_t offset{};
  std::vector<std::string>::size_type numDrivers{};
};

class SpanStreamBuffer final : public std::streambuf {
 public:
  SpanStreamBuffer(char* data, std::size_t size) : begin_(data), end_(data + size) {
    setg(data, data, data + size);
  }

 protected:
  pos_type seekoff(off_type off, std::ios_base::seekdir dir,
                   std::ios_base::openmode which) override {
    if ((which & std::ios_base::in) == 0) {
      return {static_cast<off_type>(-1)};
    }

    char* next = nullptr;
    switch (dir) {
      case std::ios_base::beg:
        next = begin_ + off;
        break;
      case std::ios_base::cur:
        next = gptr() + off;
        break;
      case std::ios_base::end:
        next = end_ + off;
        break;
      default:
        return {static_cast<off_type>(-1)};
    }

    if (next < begin_ || next > end_) {
      return {static_cast<off_type>(-1)};
    }

    setg(begin_, next, end_);
    return {static_cast<off_type>(next - begin_)};
  }

  pos_type seekpos(pos_type pos, std::ios_base::openmode which) override {
    return seekoff(static_cast<off_type>(pos), std::ios_base::beg, which);
  }

 private:
  char* begin_;
  char* end_;
};

class SpanInputStream final : public std::istream {
 public:
  explicit SpanInputStream(std::span<const std::byte> bytes)
      : std::istream(nullptr),
        buffer_(reinterpret_cast<char*>(const_cast<std::byte*>(bytes.data())), bytes.size()) {
    rdbuf(&buffer_);
  }

  explicit SpanInputStream(std::span<const std::uint8_t> bytes)
      : std::istream(nullptr),
        buffer_(reinterpret_cast<char*>(const_cast<std::uint8_t*>(bytes.data())), bytes.size()) {
    rdbuf(&buffer_);
  }

 private:
  SpanStreamBuffer buffer_;
};

class Sha256 {
 public:
  void update(std::span<const std::byte> bytes) {
    for (const auto value : bytes) {
      buffer_[bufferSize_++] = std::to_integer<std::uint8_t>(value);
      bitLength_ += 8;
      if (bufferSize_ == 64) {
        transform(buffer_);
        bufferSize_ = 0;
      }
    }
  }

  [[nodiscard]] std::string finish() {
    buffer_[bufferSize_++] = 0x80;
    if (bufferSize_ > 56) {
      while (bufferSize_ < 64) {
        buffer_[bufferSize_++] = 0;
      }
      transform(buffer_);
      bufferSize_ = 0;
    }
    while (bufferSize_ < 56) {
      buffer_[bufferSize_++] = 0;
    }
    for (int shift = 56; shift >= 0; shift -= 8) {
      buffer_[bufferSize_++] = static_cast<std::uint8_t>(bitLength_ >> shift);
    }
    transform(buffer_);

    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.resize(64);
    for (std::size_t i = 0; i < state_.size(); ++i) {
      const auto base = i * 8;
      const auto word = state_[i];
      out[base + 0] = kHex[(word >> 28) & 0xF];
      out[base + 1] = kHex[(word >> 24) & 0xF];
      out[base + 2] = kHex[(word >> 20) & 0xF];
      out[base + 3] = kHex[(word >> 16) & 0xF];
      out[base + 4] = kHex[(word >> 12) & 0xF];
      out[base + 5] = kHex[(word >> 8) & 0xF];
      out[base + 6] = kHex[(word >> 4) & 0xF];
      out[base + 7] = kHex[word & 0xF];
    }
    return out;
  }

 private:
  static constexpr std::array<std::uint32_t, 64> kTable = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
      0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
      0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
      0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
      0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
      0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
      0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
      0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
      0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
      0xc67178f2,
  };

  static constexpr std::uint32_t rotr(std::uint32_t value, int shift) {
    return (value >> shift) | (value << (32 - shift));
  }

  void transform(const std::array<std::uint8_t, 64>& chunk) {
    std::array<std::uint32_t, 64> w{};
    for (std::size_t i = 0; i < 16; ++i) {
      const auto j = i * 4;
      w[i] = (static_cast<std::uint32_t>(chunk[j]) << 24) |
             (static_cast<std::uint32_t>(chunk[j + 1]) << 16) |
             (static_cast<std::uint32_t>(chunk[j + 2]) << 8) |
             static_cast<std::uint32_t>(chunk[j + 3]);
    }
    for (std::size_t i = 16; i < 64; ++i) {
      const auto s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      const auto s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    auto a = state_[0];
    auto b = state_[1];
    auto c = state_[2];
    auto d = state_[3];
    auto e = state_[4];
    auto f = state_[5];
    auto g = state_[6];
    auto h = state_[7];

    for (std::size_t i = 0; i < 64; ++i) {
      const auto s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
      const auto ch = (e & f) ^ (~e & g);
      const auto temp1 = h + s1 + ch + kTable[i] + w[i];
      const auto s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
      const auto maj = (a & b) ^ (a & c) ^ (b & c);
      const auto temp2 = s0 + maj;

      h = g;
      g = f;
      f = e;
      e = d + temp1;
      d = c;
      c = b;
      b = a;
      a = temp1 + temp2;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
  }

  std::array<std::uint32_t, 8> state_ = {
      0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
      0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
  };
  std::array<std::uint8_t, 64> buffer_{};
  std::size_t bufferSize_{};
  std::uint64_t bitLength_{};
};

ParseError fail(StatusCode code, std::string_view message) { return {code, message}; }

template <typename T>
T readValue(std::istream& in) {
  T value{};
  in.read(reinterpret_cast<char*>(&value), static_cast<std::streamsize>(sizeof(value)));
  if (!in) {
    throw fail(StatusCode::invalid_format, "Unexpected end of replay stream");
  }
  return value;
}

template <>
std::string readValue<std::string>(std::istream& in) {
  // Strings in the replay stream are encoded as little-endian uint32 byte length followed by the
  // raw bytes with no trailing terminator.
  const auto size = readValue<std::uint32_t>(in);
  std::string value(size, '\0');
  in.read(value.data(), static_cast<std::streamsize>(size));
  if (!in) {
    throw fail(StatusCode::invalid_format, "Unexpected end of replay string");
  }
  return value;
}

template <typename T>
void readValueArray(std::istream& in, std::uint32_t count, T* out) {
  in.read(reinterpret_cast<char*>(out), static_cast<std::streamsize>(sizeof(T) * count));
  if (!in) {
    throw fail(StatusCode::invalid_format, "Unexpected end of replay array");
  }
}

std::string readString(std::istream& in, std::uint32_t size) {
  std::string value(size, '\0');
  in.read(value.data(), static_cast<std::streamsize>(size));
  if (!in) {
    throw fail(StatusCode::invalid_format, "Unexpected end of replay string");
  }
  return value;
}

std::optional<std::uint32_t> getCspDataOffset(std::istream& in, std::size_t fileSize) {
  if (fileSize < (kCspFooter.size() + 8)) {
    return std::nullopt;
  }
  // CSP appends its own footer at EOF instead of changing the base replay layout. The footer
  // points back to an auxiliary metadata block elsewhere in the file.
  const auto originalPos = in.tellg();
  in.seekg(-static_cast<std::streamoff>(kCspFooter.size()) - 8, std::ios_base::end);
  const auto footer = readString(in, static_cast<std::uint32_t>(kCspFooter.size()));
  if (footer != kCspFooter) {
    in.seekg(originalPos, std::ios_base::beg);
    return std::nullopt;
  }

  const auto offset = readValue<std::uint32_t>(in);
  const auto version = readValue<std::uint32_t>(in);
  in.seekg(originalPos, std::ios_base::beg);
  if (version == 1) {
    return offset;
  }
  return std::nullopt;
}

std::vector<std::string> getDriverNames(std::istream& in, DriverNamesSection section) {
  const auto originalPos = in.tellg();
  in.seekg(static_cast<std::streamoff>(section.offset), std::ios_base::beg);

  std::vector<std::string> names(section.numDrivers);
  // The CSP metadata block contains several length-prefixed strings before a larger INI-like blob.
  // Driver names are currently sourced from `DRIVER_NAME=` entries inside that blob.
  while (true) {
    const auto len = readValue<std::uint32_t>(in);
    if (len > 255) {
      in.seekg(-4, std::ios_base::cur);
      break;
    }
    in.seekg(static_cast<std::streamoff>(len), std::ios_base::cur);
  }

  const auto ini = readValue<std::string>(in);
  std::size_t index = 0;
  std::size_t start = ini.find(kDriverNameIni);
  while (start != std::string::npos && index < names.size()) {
    start += kDriverNameIni.size();
    const auto end = ini.find('\n', start);
    auto name = ini.substr(start, end - start);
    if (!name.empty() && name.front() == '\'' && name.back() == '\'') {
      name = name.substr(1, name.size() - 2);
    }
    names[index++] = name;
    start = ini.find(kDriverNameIni, start);
  }

  in.seekg(originalPos, std::ios_base::beg);
  return names;
}

void skipTrackObjects(std::istream& in, const ReplayHeader& header) {
  // This replay-global section is confirmed by the legacy parser's stride calculations, but the
  // current core does not surface it because the downstream telemetry path only needs per-car data.
  const auto bytesPerFrame =
      std::streamoff{4} + (static_cast<std::streamoff>(header.numTrackObjects) * 12);
  const auto skipBytes = bytesPerFrame * static_cast<std::streamoff>(header.numFrames);
  in.seekg(skipBytes, std::ios_base::cur);
}

std::string computeSha256(std::span<const std::byte> bytes) {
  Sha256 sha;
  sha.update(bytes);
  return sha.finish();
}

void getUniquePath(std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) {
    return;
  }
  const auto extension = path.extension().string();
  const auto filename = path.stem().string() + " (";
  unsigned int index = 2;
  while (std::filesystem::exists(path)) {
    std::string candidate = filename;
    candidate += std::to_string(index);
    candidate += ')';
    candidate += extension;
    path.replace_filename(candidate);
    ++index;
  }
}

void outputCarFrame(std::ostream& out, const CarFrame& frame) {
  const auto originalPrecision = static_cast<int>(out.precision());
  out << std::setprecision(std::numeric_limits<float>::max_digits10) << frame.position.x << ','
      << frame.position.y << ',' << frame.position.z << ','
      << std::setprecision(std::numeric_limits<float>::max_digits10) << frame.rotation.x << ','
      << frame.rotation.y << ',' << frame.rotation.z << ',' << frame.velocity.x << ','
      << frame.velocity.y << ',' << frame.velocity.z << ','
      << std::setprecision(std::numeric_limits<float>::max_digits10)
      << frame.wheelStaticPosition[0].x << ',' << frame.wheelStaticPosition[0].y << ','
      << frame.wheelStaticPosition[0].z << ',' << frame.wheelStaticPosition[1].x << ','
      << frame.wheelStaticPosition[1].y << ',' << frame.wheelStaticPosition[1].z << ','
      << frame.wheelStaticPosition[2].x << ',' << frame.wheelStaticPosition[2].y << ','
      << frame.wheelStaticPosition[2].z << ',' << frame.wheelStaticPosition[3].x << ','
      << frame.wheelStaticPosition[3].y << ',' << frame.wheelStaticPosition[3].z << ','
      << std::setprecision(std::numeric_limits<float>::max_digits10)
      << frame.wheelStaticRotation[0].x << ',' << frame.wheelStaticRotation[0].y << ','
      << frame.wheelStaticRotation[0].z << ',' << frame.wheelStaticRotation[1].x << ','
      << frame.wheelStaticRotation[1].y << ',' << frame.wheelStaticRotation[1].z << ','
      << frame.wheelStaticRotation[2].x << ',' << frame.wheelStaticRotation[2].y << ','
      << frame.wheelStaticRotation[2].z << ',' << frame.wheelStaticRotation[3].x << ','
      << frame.wheelStaticRotation[3].y << ',' << frame.wheelStaticRotation[3].z << ','
      << std::setprecision(std::numeric_limits<float>::max_digits10) << frame.wheelPosition[0].x
      << ',' << frame.wheelPosition[0].y << ',' << frame.wheelPosition[0].z << ','
      << frame.wheelPosition[1].x << ',' << frame.wheelPosition[1].y << ','
      << frame.wheelPosition[1].z << ',' << frame.wheelPosition[2].x << ','
      << frame.wheelPosition[2].y << ',' << frame.wheelPosition[2].z << ','
      << frame.wheelPosition[3].x << ',' << frame.wheelPosition[3].y << ','
      << frame.wheelPosition[3].z << ','
      << std::setprecision(std::numeric_limits<float>::max_digits10) << frame.wheelRotation[0].x
      << ',' << frame.wheelRotation[0].y << ',' << frame.wheelRotation[0].z << ','
      << frame.wheelRotation[1].x << ',' << frame.wheelRotation[1].y << ','
      << frame.wheelRotation[1].z << ',' << frame.wheelRotation[2].x << ','
      << frame.wheelRotation[2].y << ',' << frame.wheelRotation[2].z << ','
      << frame.wheelRotation[3].x << ',' << frame.wheelRotation[3].y << ','
      << frame.wheelRotation[3].z << ',' << frame.wheelAngularVelocity[0] << ','
      << frame.wheelAngularVelocity[1] << ',' << frame.wheelAngularVelocity[2] << ','
      << frame.wheelAngularVelocity[3] << ',' << frame.slipAngle[0] << ',' << frame.slipAngle[1]
      << ',' << frame.slipAngle[2] << ',' << frame.slipAngle[3] << ',' << frame.slipRatio[0] << ','
      << frame.slipRatio[1] << ',' << frame.slipRatio[2] << ',' << frame.slipRatio[3] << ','
      << frame.ndSlip[0] << ',' << frame.ndSlip[1] << ',' << frame.ndSlip[2] << ','
      << frame.ndSlip[3] << ',' << frame.load[0] << ',' << frame.load[1] << ',' << frame.load[2]
      << ',' << frame.load[3] << ',' << +frame.tireDirt[0] << ',' << +frame.tireDirt[1] << ','
      << +frame.tireDirt[2] << ',' << +frame.tireDirt[3] << ',' << frame.steerAngle << ','
      << frame.bodyworkNoise << ',' << frame.drivetrainSpeed << ',' << +frame.currentLap << ','
      << frame.currentLapTime << ',' << frame.lastLapTime << ',' << frame.bestLapTime << ','
      << +frame.fuel << ',' << +frame.fuelPerLap << ',' << frame.rpm << ',' << +frame.gear << ','
      << +frame.gas << ',' << +frame.brake << ',' << +frame.boost << ','
      << +frame.damageFrontDeformation << ',' << +frame.damageFront << ',' << +frame.damageRear
      << ',' << +frame.damageLeft << ',' << +frame.damageRight << ','
      << std::setprecision(originalPrecision);

  out << static_cast<bool>((frame.status >> 12) & 0x1) << ','
      << static_cast<bool>((frame.status >> 3) & 0x1) << ','
      << static_cast<unsigned int>(static_cast<std::uint8_t>((frame.status >> 4) & 0b0011)) << ','
      << +frame.engineHealth << ',' << static_cast<bool>((frame.status >> 9) & 0x1) << ','
      << +frame.dirt;
}

void appendU8(std::vector<std::byte>& out, std::uint8_t value) {
  out.push_back(static_cast<std::byte>(value));
}

void appendI8(std::vector<std::byte>& out, std::int8_t value) {
  out.push_back(static_cast<std::byte>(std::bit_cast<std::uint8_t>(value)));
}

void appendU16Le(std::vector<std::byte>& out, std::uint16_t value) {
  appendU8(out, static_cast<std::uint8_t>(value & 0xFFu));
  appendU8(out, static_cast<std::uint8_t>((value >> 8) & 0xFFu));
}

void appendU32Le(std::vector<std::byte>& out, std::uint32_t value) {
  appendU8(out, static_cast<std::uint8_t>(value & 0xFFu));
  appendU8(out, static_cast<std::uint8_t>((value >> 8) & 0xFFu));
  appendU8(out, static_cast<std::uint8_t>((value >> 16) & 0xFFu));
  appendU8(out, static_cast<std::uint8_t>((value >> 24) & 0xFFu));
}

void appendF32Le(std::vector<std::byte>& out, float value) {
  appendU32Le(out, std::bit_cast<std::uint32_t>(value));
}

std::vector<LapSlice> segmentLaps(const ParsedCarData& car) {
  std::vector<LapSlice> laps;
  if (car.frames.empty()) {
    return laps;
  }

  std::size_t segmentStart = 0;
  for (std::size_t frameIndex = 1; frameIndex < car.frames.size(); ++frameIndex) {
    if (car.frames[frameIndex].currentLap == car.frames[frameIndex - 1].currentLap) {
      continue;
    }

    // Lap boundaries are inferred from the parsed telemetry, not marked explicitly in the replay.
    // The current heuristic treats a lap change in `currentLap` as the boundary and only marks the
    // first segment complete when the replay starts exactly at lap time 0.
    const bool isInitialFullLap = segmentStart == 0 && car.frames[segmentStart].currentLapTime == 0;
    const auto lastFrameIndex = frameIndex - 1;
    laps.push_back(LapSlice{
        .beginFrame = segmentStart,
        .endFrame = frameIndex,
        .lapNumber = static_cast<std::uint16_t>(car.frames[segmentStart].currentLap + 1),
        .lapTimeMs = car.frames[lastFrameIndex].currentLapTime,
        .isComplete = segmentStart == 0 ? isInitialFullLap : true,
        .isValid = true,
    });
    segmentStart = frameIndex;
  }

  const auto lastFrameIndex = car.frames.size() - 1;
  laps.push_back(LapSlice{
      .beginFrame = segmentStart,
      .endFrame = car.frames.size(),
      .lapNumber = static_cast<std::uint16_t>(car.frames[segmentStart].currentLap + 1),
      .lapTimeMs = car.frames[lastFrameIndex].currentLapTime,
      .isComplete = false,
      .isValid = true,
  });

  return laps;
}

std::vector<std::byte> buildLapPack(const ParsedCarData& car, const LapSlice& lap,
                                    std::uint32_t recordingIntervalMs) {
  static constexpr std::uint16_t kPackVersion = 1;
  static constexpr std::uint16_t kPackHeaderBytes = 36;
  static constexpr std::uint16_t kPackDescriptorBytes = 16;
  static constexpr std::uint16_t kFlagHasCsp = 1u << 0;
  static constexpr std::uint16_t kFlagIsComplete = 1u << 1;
  static constexpr std::uint16_t kFlagIsValid = 1u << 2;

  const auto sampleCount = static_cast<std::uint32_t>(lap.endFrame - lap.beginFrame);
  std::vector<float> distance;
  distance.reserve(sampleCount);

  float cumulativeDistance = 0.0f;
  for (std::size_t frameIndex = lap.beginFrame; frameIndex < lap.endFrame; ++frameIndex) {
    if (frameIndex == lap.beginFrame) {
      distance.push_back(0.0f);
      continue;
    }
    const auto& prev = car.frames[frameIndex - 1];
    const auto& current = car.frames[frameIndex];
    const auto dx = current.position.x - prev.position.x;
    const auto dz = current.position.z - prev.position.z;
    cumulativeDistance += std::sqrt((dx * dx) + (dz * dz));
    distance.push_back(cumulativeDistance);
  }

  std::vector<ChannelPayload> channels;

  const auto addFloatChannel = [&](std::uint16_t channelId, const auto& valueAtFrame) {
    ChannelPayload channel{
        .channelId = channelId,
        .valueType = ChannelValueType::f32,
        .bytes = {},
    };
    channel.bytes.reserve(sampleCount * sizeof(float));
    for (std::size_t frameIndex = lap.beginFrame; frameIndex < lap.endFrame; ++frameIndex) {
      appendF32Le(channel.bytes, valueAtFrame(frameIndex));
    }
    channels.push_back(std::move(channel));
  };

  const auto addU8Channel = [&](std::uint16_t channelId, const auto& valueAtFrame) {
    ChannelPayload channel{
        .channelId = channelId,
        .valueType = ChannelValueType::u8,
        .bytes = {},
    };
    channel.bytes.reserve(sampleCount);
    for (std::size_t frameIndex = lap.beginFrame; frameIndex < lap.endFrame; ++frameIndex) {
      appendU8(channel.bytes, valueAtFrame(frameIndex));
    }
    channels.push_back(std::move(channel));
  };

  const auto addI8Channel = [&](std::uint16_t channelId, const auto& valueAtFrame) {
    ChannelPayload channel{
        .channelId = channelId,
        .valueType = ChannelValueType::i8,
        .bytes = {},
    };
    channel.bytes.reserve(sampleCount);
    for (std::size_t frameIndex = lap.beginFrame; frameIndex < lap.endFrame; ++frameIndex) {
      appendI8(channel.bytes, valueAtFrame(frameIndex));
    }
    channels.push_back(std::move(channel));
  };

  addFloatChannel(1, [&](std::size_t frameIndex) { return distance[frameIndex - lap.beginFrame]; });
  addFloatChannel(2, [&](std::size_t frameIndex) {
    return static_cast<float>(car.frames[frameIndex].currentLapTime);
  });
  addFloatChannel(3, [&](std::size_t frameIndex) { return car.frames[frameIndex].position.x; });
  addFloatChannel(4, [&](std::size_t frameIndex) { return car.frames[frameIndex].position.y; });
  addFloatChannel(5, [&](std::size_t frameIndex) { return car.frames[frameIndex].position.z; });
  addFloatChannel(6, [&](std::size_t frameIndex) {
    return static_cast<float>(car.frames[frameIndex].rotation.y);
  });
  addFloatChannel(7, [&](std::size_t frameIndex) {
    return static_cast<float>(car.frames[frameIndex].rotation.x);
  });
  addFloatChannel(8, [&](std::size_t frameIndex) {
    return static_cast<float>(car.frames[frameIndex].rotation.y);
  });
  addFloatChannel(9, [&](std::size_t frameIndex) {
    return static_cast<float>(car.frames[frameIndex].rotation.z);
  });
  addFloatChannel(10, [&](std::size_t frameIndex) {
    const auto& velocity = car.frames[frameIndex].velocity;
    const auto vx = static_cast<float>(velocity.x);
    const auto vy = static_cast<float>(velocity.y);
    const auto vz = static_cast<float>(velocity.z);
    return std::sqrt((vx * vx) + (vy * vy) + (vz * vz)) * 3.6f;
  });
  addFloatChannel(11, [&](std::size_t frameIndex) {
    return static_cast<float>(car.frames[frameIndex].velocity.x);
  });
  addFloatChannel(12, [&](std::size_t frameIndex) {
    return static_cast<float>(car.frames[frameIndex].velocity.y);
  });
  addFloatChannel(13, [&](std::size_t frameIndex) {
    return static_cast<float>(car.frames[frameIndex].velocity.z);
  });
  addU8Channel(14, [&](std::size_t frameIndex) { return car.frames[frameIndex].gas; });
  addU8Channel(15, [&](std::size_t frameIndex) { return car.frames[frameIndex].brake; });
  addFloatChannel(16, [&](std::size_t frameIndex) {
    return static_cast<float>(car.frames[frameIndex].steerAngle);
  });
  addI8Channel(17, [&](std::size_t frameIndex) {
    return static_cast<std::int8_t>(static_cast<int>(car.frames[frameIndex].gear) - 1);
  });
  addFloatChannel(
      18, [&](std::size_t frameIndex) { return static_cast<float>(car.frames[frameIndex].rpm); });
  addU8Channel(19, [&](std::size_t frameIndex) {
    const auto& frame = car.frames[frameIndex];
    return static_cast<std::uint8_t>(frame.gas == 0 && frame.brake == 0 ? 1 : 0);
  });
  addU8Channel(20, [&](std::size_t frameIndex) { return car.frames[frameIndex].fuel; });
  addU8Channel(21, [&](std::size_t frameIndex) { return car.frames[frameIndex].fuelPerLap; });
  addU8Channel(22, [&](std::size_t frameIndex) { return car.frames[frameIndex].boost; });
  addU8Channel(23, [&](std::size_t frameIndex) { return car.frames[frameIndex].engineHealth; });
  addU8Channel(24, [&](std::size_t frameIndex) {
    return static_cast<std::uint8_t>((car.frames[frameIndex].status >> 9) & 0x1);
  });
  addFloatChannel(25, [&](std::size_t frameIndex) {
    return static_cast<float>(car.frames[frameIndex].drivetrainSpeed);
  });
  addFloatChannel(32, [&](std::size_t frameIndex) {
    return static_cast<float>(car.frames[frameIndex].wheelAngularVelocity[0]);
  });
  addFloatChannel(33, [&](std::size_t frameIndex) {
    return static_cast<float>(car.frames[frameIndex].wheelAngularVelocity[1]);
  });
  addFloatChannel(34, [&](std::size_t frameIndex) {
    return static_cast<float>(car.frames[frameIndex].wheelAngularVelocity[2]);
  });
  addFloatChannel(35, [&](std::size_t frameIndex) {
    return static_cast<float>(car.frames[frameIndex].wheelAngularVelocity[3]);
  });
  addFloatChannel(36, [&](std::size_t frameIndex) {
    return static_cast<float>(car.frames[frameIndex].slipRatio[0]);
  });
  addFloatChannel(37, [&](std::size_t frameIndex) {
    return static_cast<float>(car.frames[frameIndex].slipRatio[1]);
  });
  addFloatChannel(38, [&](std::size_t frameIndex) {
    return static_cast<float>(car.frames[frameIndex].slipRatio[2]);
  });
  addFloatChannel(39, [&](std::size_t frameIndex) {
    return static_cast<float>(car.frames[frameIndex].slipRatio[3]);
  });
  addFloatChannel(40, [&](std::size_t frameIndex) {
    return static_cast<float>(car.frames[frameIndex].ndSlip[0]);
  });
  addFloatChannel(41, [&](std::size_t frameIndex) {
    return static_cast<float>(car.frames[frameIndex].ndSlip[1]);
  });
  addFloatChannel(42, [&](std::size_t frameIndex) {
    return static_cast<float>(car.frames[frameIndex].ndSlip[2]);
  });
  addFloatChannel(43, [&](std::size_t frameIndex) {
    return static_cast<float>(car.frames[frameIndex].ndSlip[3]);
  });
  addFloatChannel(44, [&](std::size_t frameIndex) {
    return static_cast<float>(car.frames[frameIndex].load[0]);
  });
  addFloatChannel(45, [&](std::size_t frameIndex) {
    return static_cast<float>(car.frames[frameIndex].load[1]);
  });
  addFloatChannel(46, [&](std::size_t frameIndex) {
    return static_cast<float>(car.frames[frameIndex].load[2]);
  });
  addFloatChannel(47, [&](std::size_t frameIndex) {
    return static_cast<float>(car.frames[frameIndex].load[3]);
  });

  bool hasCsp = false;
  if (car.extraFramesV6.has_value()) {
    hasCsp = true;
    addU8Channel(64,
                 [&](std::size_t frameIndex) { return (*car.extraFramesV6)[frameIndex].clutch; });
  } else if (car.extraFramesV7.has_value()) {
    hasCsp = true;
    addU8Channel(64,
                 [&](std::size_t frameIndex) { return (*car.extraFramesV7)[frameIndex].clutch; });
  }

  std::uint16_t flags = 0;
  if (hasCsp) {
    flags |= kFlagHasCsp;
  }
  if (lap.isComplete) {
    flags |= kFlagIsComplete;
  }
  if (lap.isValid) {
    flags |= kFlagIsValid;
  }

  const auto descriptorsOffset = static_cast<std::uint32_t>(kPackHeaderBytes);
  const auto dataOffset =
      descriptorsOffset + static_cast<std::uint32_t>(channels.size() * kPackDescriptorBytes);

  std::vector<std::byte> result;
  result.reserve(static_cast<std::size_t>(dataOffset) +
                 (channels.size() * static_cast<std::size_t>(sampleCount) * std::size_t{4}));

  appendU8(result, static_cast<std::uint8_t>('A'));
  appendU8(result, static_cast<std::uint8_t>('C'));
  appendU8(result, static_cast<std::uint8_t>('T'));
  appendU8(result, static_cast<std::uint8_t>('L'));
  appendU16Le(result, kPackVersion);
  appendU16Le(result, kPackHeaderBytes);
  appendU16Le(result, kPackDescriptorBytes);
  appendU16Le(result, static_cast<std::uint16_t>(channels.size()));
  appendU32Le(result, sampleCount);
  appendU16Le(result, lap.lapNumber);
  appendU16Le(result, flags);
  appendU32Le(result, lap.lapTimeMs);
  appendU32Le(result, recordingIntervalMs * 1000u);
  appendU32Le(result, descriptorsOffset);
  appendU32Le(result, dataOffset);

  std::uint32_t runningOffset = 0;
  for (const auto& channel : channels) {
    // Descriptors are laid out first and point into one tightly-packed data block. This keeps the
    // wasm/TS reader simple because every channel can be reconstructed from header + descriptor
    // only.
    appendU16Le(result, channel.channelId);
    appendU8(result, static_cast<std::uint8_t>(channel.valueType));
    appendU8(result, 0);
    appendU32Le(result, sampleCount);
    appendU32Le(result, runningOffset);
    appendU32Le(result, static_cast<std::uint32_t>(channel.bytes.size()));
    runningOffset += static_cast<std::uint32_t>(channel.bytes.size());
  }

  for (const auto& channel : channels) {
    result.insert(result.end(), channel.bytes.begin(), channel.bytes.end());
  }

  return result;
}

constexpr std::string_view kCarFrameLabel =
    "frame,"
    "position.x,position.y,position.z,"
    "rotation.x,rotation.y,rotation.z,"
    "velocity.x,velocity.y,velocity.z,"
    "wheelFL.staticPosition.x,wheelFL.staticPosition.y,wheelFL.staticPosition.z,"
    "wheelFR.staticPosition.x,wheelFR.staticPosition.y,wheelFR.staticPosition.z,"
    "wheelRL.staticPosition.x,wheelRL.staticPosition.y,wheelRL.staticPosition.z,"
    "wheelRR.staticPosition.x,wheelRR.staticPosition.y,wheelRR.staticPosition.z,"
    "wheelFL.staticRotation.x,wheelFL.staticRotation.y,wheelFL.staticRotation.z,"
    "wheelFR.staticRotation.x,wheelFR.staticRotation.y,wheelFR.staticRotation.z,"
    "wheelRL.staticRotation.x,wheelRL.staticRotation.y,wheelRL.staticRotation.z,"
    "wheelRR.staticRotation.x,wheelRR.staticRotation.y,wheelRR.staticRotation.z,"
    "wheelFL.position.x,wheelFL.position.y,wheelFL.position.z,"
    "wheelFR.position.x,wheelFR.position.y,wheelFR.position.z,"
    "wheelRL.position.x,wheelRL.position.y,wheelRL.position.z,"
    "wheelRR.position.x,wheelRR.position.y,wheelRR.position.z,"
    "wheelFL.rotation.x,wheelFL.rotation.y,wheelFL.rotation.z,"
    "wheelFR.rotation.x,wheelFR.rotation.y,wheelFR.rotation.z,"
    "wheelRL.rotation.x,wheelRL.rotation.y,wheelRL.rotation.z,"
    "wheelRR.rotation.x,wheelRR.rotation.y,wheelRR.rotation.z,"
    "wheelFL.angularVelocity,wheelFR.angularVelocity,wheelRL.angularVelocity,wheelRR."
    "angularVelocity,"
    "wheelFL.slipAngle,wheelFR.slipAngle,wheelRL.slipAngle,wheelRR.slipAngle,"
    "wheelFL.slipRatio,wheelFR.slipRatio,wheelRL.slipRatio,wheelRR.slipRatio,"
    "wheelFL.ndSlip,wheelFR.ndSlip,wheelRL.ndSlip,wheelRR.ndSlip,"
    "wheelFL.load,wheelFR.load,wheelRL.load,wheelRR.load,"
    "wheelFL.dirt,wheelFR.dirt,wheelRL.dirt,wheelRR.dirt,"
    "steerAngle,bodyworkNoise,drivetrainSpeed,"
    "currentLap,currentLapTime,lastLapTime,bestLapTime,"
    "fuel,fuelPerLap,rpm,gear,gas,brake,boost,"
    "damageFrontDeformation,damageFront,damageRear,damageLeft,damageRight,"
    "lights,horn,cameraDir,engineHealth,gearboxBeingDamaged,dirt";

ParsedReplayData parseReplayData(std::span<const std::byte> replayBytes) {
  if (replayBytes.empty()) {
    throw fail(StatusCode::invalid_argument, "Replay buffer is empty");
  }

  // Parsing follows the on-disk section order directly. This function is effectively the most
  // up-to-date specification of the base `.acreplay` layout in the repo.
  SpanInputStream in(replayBytes);

  const auto version = readValue<std::uint32_t>(in);
  if (version != 16) {
    throw fail(StatusCode::unsupported_version, "Only version 16 .acreplay files are supported");
  }

  ParsedReplayData replay{};
  replay.header = ReplayHeader{
      .version = version,
      .recordingInterval = readValue<double>(in),
      .weather = readValue<std::string>(in),
      .track = readValue<std::string>(in),
      .trackConfig = readValue<std::string>(in),
      .numCars = readValue<std::uint32_t>(in),
      .currentRecordingIndex = readValue<std::uint32_t>(in),
      .numFrames = readValue<std::uint32_t>(in),
      .numTrackObjects = readValue<std::uint32_t>(in),
  };

  const auto cspOffset = getCspDataOffset(in, replayBytes.size());
  if (cspOffset.has_value()) {
    replay.cspDriverNames = getDriverNames(in, DriverNamesSection{
                                                   .offset = *cspOffset,
                                                   .numDrivers = replay.header.numCars,
                                               });
  }

  skipTrackObjects(in, replay.header);
  replay.cars.reserve(replay.header.numCars);

  for (std::uint32_t carIndex = 0; carIndex < replay.header.numCars; ++carIndex) {
    ParsedCarData car{};
    car.header = CarHeader{
        .carId = readValue<std::string>(in),
        .driverName = readValue<std::string>(in),
        .nationCode = readValue<std::string>(in),
        .driverTeam = readValue<std::string>(in),
        .carSkinId = readValue<std::string>(in),
        .numFrames = readValue<std::uint32_t>(in),
        .numWings = readValue<std::uint32_t>(in),
    };

    // Confirmed by parser behavior: each car block contains an unresolved 20-byte prefix before
    // the first CarFrame. Between non-final frames there is another unresolved 20-byte block plus
    // `numWings * 4` bytes. After the last frame only the wing bytes remain, followed by a
    // trailing `(count * 8)` opaque block.
    in.seekg(20, std::ios_base::cur);
    car.frames.resize(car.header.numFrames);
    const auto wingBytes = static_cast<std::streamoff>(car.header.numWings) * 4;
    for (std::uint32_t frameIndex = 0; frameIndex < car.header.numFrames; ++frameIndex) {
      car.frames[frameIndex] = readValue<CarFrame>(in);
      if (frameIndex + 1 < car.header.numFrames) {
        in.seekg(std::streamoff{20} + wingBytes, std::ios_base::cur);
      } else {
        in.seekg(wingBytes, std::ios_base::cur);
        const auto count = readValue<std::uint32_t>(in);
        if (count > 0) {
          in.seekg(static_cast<std::streamoff>(count) * 8, std::ios_base::cur);
        }
      }
    }

    if (cspOffset.has_value()) {
#if ACRP_HAS_ZLIB
      const auto originalPos = in.tellg();
      in.seekg(static_cast<std::streamoff>(*cspOffset), std::ios_base::beg);

      int extraVersion = -1;
      std::uint32_t bytesPerFrame = 0;
      // CSP stores one tagged compressed blob per car, for example `EXT_PERCAR_v6:0`.
      // Only v6/v7 record sizes are currently known well enough to deserialize.
      while (true) {
        const auto len = readValue<std::uint32_t>(in);
        const auto resetPos = in.tellg();
        if (readString(in, static_cast<std::uint32_t>(kExtPerCar.size())) == kExtPerCar) {
          in.seekg(-static_cast<std::streamoff>(kExtPerCar.size()), std::ios_base::cur);
          const auto tag = readString(in, len);
          const auto versionIndex = tag.find("_v");
          const auto separatorIndex = tag.find(':');
          if (versionIndex == std::string::npos || separatorIndex == std::string::npos) {
            break;
          }

          std::uint32_t extraCarIndex = 0;
          std::from_chars(tag.data() + versionIndex + 2, tag.data() + separatorIndex, extraVersion);
          std::from_chars(tag.data() + separatorIndex + 1, tag.data() + tag.size(), extraCarIndex);
          if (extraVersion > 0 &&
              static_cast<std::size_t>(extraVersion) <= kExtPerCarBytesPerFrame.size()) {
            bytesPerFrame = kExtPerCarBytesPerFrame[static_cast<std::size_t>(extraVersion - 1)];
          }
          if (carIndex == extraCarIndex) {
            break;
          }
        }
        in.seekg(resetPos + static_cast<std::streamoff>(len), std::ios_base::beg);
      }

      if (extraVersion >= 0 && bytesPerFrame > 0) {
        const auto compressedSize = readValue<std::uint32_t>(in);
        std::vector<std::uint8_t> compressedData(compressedSize);
        readValueArray(in, compressedSize, compressedData.data());

        // CSP stores per-frame extras as one compressed block per car. We only surface the pieces
        // needed by pack v1, but we keep the raw structs because most field mappings are still
        // inferred from legacy behavior rather than fully confirmed.
        auto uncompressedSize =
            static_cast<uLongf>(bytesPerFrame) * static_cast<uLongf>(replay.header.numFrames);
        std::vector<std::uint8_t> uncompressedData(uncompressedSize);
        const auto status = uncompress(uncompressedData.data(), &uncompressedSize,
                                       compressedData.data(), compressedSize);
        if (status == Z_OK) {
          SpanInputStream extra(std::span<const std::uint8_t>(
              uncompressedData.data(), static_cast<std::size_t>(uncompressedSize)));

          if (extraVersion == 6) {
            car.extraFramesV6.emplace(car.header.numFrames);
            readValueArray(extra, car.header.numFrames, car.extraFramesV6->data());
          } else if (extraVersion == 7) {
            car.extraFramesV7.emplace(car.header.numFrames);
            readValueArray(extra, car.header.numFrames, car.extraFramesV7->data());
          }
        }
      }

      in.seekg(originalPos, std::ios_base::beg);
#else
      (void)cspOffset;
#endif
    }

    replay.cars.push_back(std::move(car));
  }

  return replay;
}

ReplayManifestV1 buildManifest(const ParsedReplayData& replay, const std::string& sourceSha256,
                               std::uint32_t carIndex) {
  if (carIndex >= replay.cars.size()) {
    throw fail(StatusCode::invalid_argument, "Car index is out of range");
  }

  const auto& car = replay.cars[carIndex];
  ReplayManifestV1 manifest{};
  manifest.sourceSha256 = sourceSha256;
  manifest.track = replay.header.track;
  manifest.trackConfig = replay.header.trackConfig;
  manifest.carIndex = carIndex;
  manifest.driverName = car.header.driverName;
  manifest.recordingIntervalMs =
      static_cast<std::uint32_t>(std::lround(replay.header.recordingInterval));

  for (const auto& lap : segmentLaps(car)) {
    manifest.laps.push_back(LapManifestV1{
        .lapNumber = lap.lapNumber,
        .lapTimeMs = lap.lapTimeMs,
        .sampleCount = static_cast<std::uint32_t>(lap.endFrame - lap.beginFrame),
        .isComplete = lap.isComplete,
        .isValid = lap.isValid,
    });
  }

  return manifest;
}

void exportCarCsv(const ParsedReplayData& replay, const ParsedCarData& car, std::uint32_t carIndex,
                  const CsvExportOptions& options) {
  std::filesystem::path outPath(options.outputPath);
  const auto inputStem =
      options.inputStem.empty() ? std::string_view("replay") : std::string_view(options.inputStem);
  if (!outPath.has_filename()) {
    if (options.driverName.empty() && replay.header.numCars > 1) {
      outPath.replace_filename(std::string(inputStem) + "_" + car.header.driverName + "." +
                               std::string(kCsvExtension));
    } else {
      outPath.replace_filename(std::string(inputStem) + "." + std::string(kCsvExtension));
    }
    getUniquePath(outPath);
  } else {
    const auto hadExtension = outPath.has_extension();
    if (options.driverName.empty() && replay.header.numCars > 1) {
      outPath.replace_filename(outPath.stem().string() + "_" + car.header.driverName +
                               outPath.extension().string());
    }
    if (!hadExtension) {
      outPath.concat("." + std::string(kCsvExtension));
    }
  }

  std::ofstream out(outPath);
  out << "# numFrames " << car.header.numFrames << "\n# recordingInterval "
      << replay.header.recordingInterval << '\n'
      << kCarFrameLabel;

  bool wroteExtra = false;
  if (car.extraFramesV6.has_value()) {
    wroteExtra = true;
    out << ',' << CarFrameExtraV6::label << '\n';
    for (std::uint32_t frameIndex = 0; frameIndex < car.header.numFrames; ++frameIndex) {
      out << frameIndex << ',';
      outputCarFrame(out, car.frames[frameIndex]);
      out << ',';
      (*car.extraFramesV6)[frameIndex].outputData(out);
      out << '\n';
    }
  } else if (car.extraFramesV7.has_value()) {
    wroteExtra = true;
    out << ',' << CarFrameExtraV7::label << '\n';
    for (std::uint32_t frameIndex = 0; frameIndex < car.header.numFrames; ++frameIndex) {
      out << frameIndex << ',';
      outputCarFrame(out, car.frames[frameIndex]);
      out << ',';
      (*car.extraFramesV7)[frameIndex].outputData(out);
      out << '\n';
    }
  }

  if (!wroteExtra) {
    out << '\n';
    for (std::uint32_t frameIndex = 0; frameIndex < car.header.numFrames; ++frameIndex) {
      out << frameIndex << ',';
      outputCarFrame(out, car.frames[frameIndex]);
      out << '\n';
    }
  }

  std::cout << outPath.string() << "\nDone!\n";
  (void)carIndex;
}

}  // namespace

ParseError::ParseError(StatusCode code, std::string_view message)
    : std::runtime_error(std::string(message)), code_(code) {}

StatusCode ParseError::code() const noexcept { return code_; }

ReplayManifestV1 inspectReplay(std::span<const std::byte> replayBytes) {
  const auto replay = parseReplayData(replayBytes);
  if (replay.cars.empty()) {
    throw fail(StatusCode::invalid_format, "Replay does not contain any cars");
  }
  return buildManifest(replay, computeSha256(replayBytes), 0);
}

ParsedCarResult parseCar(std::span<const std::byte> replayBytes, std::uint32_t carIndex) {
  const auto replay = parseReplayData(replayBytes);
  if (carIndex >= replay.cars.size()) {
    throw fail(StatusCode::invalid_argument, "Car index is out of range");
  }
  const auto recordingIntervalMs =
      static_cast<std::uint32_t>(std::lround(replay.header.recordingInterval));
  std::vector<ParsedLapPack> lapPacks;
  for (const auto& lap : segmentLaps(replay.cars[carIndex])) {
    lapPacks.push_back(ParsedLapPack{
        .lapNumber = lap.lapNumber,
        .bytes = buildLapPack(replay.cars[carIndex], lap, recordingIntervalMs),
    });
  }
  return ParsedCarResult{
      .manifest = buildManifest(replay, computeSha256(replayBytes), carIndex),
      .lapPacks = std::move(lapPacks),
  };
}

void exportCsvFiles(std::span<const std::byte> replayBytes, const CsvExportOptions& options) {
  const auto replay = parseReplayData(replayBytes);
  for (std::uint32_t carIndex = 0; carIndex < replay.cars.size(); ++carIndex) {
    const auto& car = replay.cars[carIndex];
    if (!options.driverName.empty() && car.header.driverName != options.driverName) {
      continue;
    }
    exportCarCsv(replay, car, carIndex, options);
  }
}

}  // namespace acrp
