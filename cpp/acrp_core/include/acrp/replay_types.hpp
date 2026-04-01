#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <ostream>
#include <string>

namespace acrp {

// Replay samples use IEEE-754 binary16 values in several dense structs. We store the raw 16-bit
// payload and widen on demand instead of relying on compiler-specific half-float types.
struct HalfFloat {
  std::uint16_t bits{};

  operator float() const noexcept {
    const auto sign = (bits & 0x8000u) != 0 ? -1.0f : 1.0f;
    const auto exponent = static_cast<std::uint16_t>((bits >> 10) & 0x1Fu);
    const auto fraction = static_cast<std::uint16_t>(bits & 0x03FFu);

    if (exponent == 0) {
      if (fraction == 0) {
        return sign * 0.0f;
      }
      return sign * std::ldexp(static_cast<float>(fraction), -24);
    }

    if (exponent == 0x1Fu) {
      if (fraction == 0) {
        return sign * INFINITY;
      }
      return NAN;
    }

    return sign * std::ldexp(1.0f + static_cast<float>(fraction) / 1024.0f,
                             static_cast<int>(exponent) - 15);
  }
};

inline std::ostream& operator<<(std::ostream& out, HalfFloat value) {
  out << static_cast<float>(value);
  return out;
}

template <typename T>
struct Vec3 {
  T x;
  T y;
  T z;
};

template <typename T>
struct VecYXZ {
  T y;
  T x;
  T z;
};

// Parsed directly from the replay stream after the version field. Strings are encoded as
// uint32 byte length + raw bytes.
struct ReplayHeader {
  std::uint32_t version;
  double recordingInterval;
  std::string weather;
  std::string track;
  std::string trackConfig;
  std::uint32_t numCars;
  std::uint32_t currentRecordingIndex;
  std::uint32_t numFrames;
  std::uint32_t numTrackObjects;
};

// Per-car metadata prefix before the frame payloads for that car.
struct CarHeader {
  std::string carId;
  std::string driverName;
  std::string nationCode;
  std::string driverTeam;
  std::string carSkinId;
  std::uint32_t numFrames;
  std::uint32_t numWings;
};

// One decoded car frame from the base version-16 replay stream.
//
// Confirmed by parser behavior:
// - position / velocity use world-space XYZ values
// - rotation stores YXZ Euler components as binary16
// - lap times are milliseconds
// - currentLap is zero-based (0 = lap 1)
//
// Inferred from the legacy parser / fixture behavior:
// - several uint8 values are pedal, fuel, damage, and dirt telemetry
// - `status` bit 3 behaves like horn, bits 4..5 like camera direction, bit 9 like gearbox damage,
//   and bit 12 like exterior lights
//
// Unknown / unresolved:
// - the on-disk frame includes small padding gaps around the half-float groups
// - `unknown` and `unknown2` are preserved because their semantics are not yet decoded
// - `status` is a packed bitfield with only a subset of bits currently understood
struct CarFrame {
  // Car body position in world coordinates.
  Vec3<float> position;
  // Car body rotation in YXZ order, stored as binary16.
  VecYXZ<HalfFloat> rotation;

  // The on-disk layout appears to contain a small unresolved gap before the wheel sub-structures.
  std::array<Vec3<float>, 4> wheelStaticPosition;
  std::array<VecYXZ<HalfFloat>, 4> wheelStaticRotation;
  std::array<Vec3<float>, 4> wheelPosition;
  std::array<VecYXZ<HalfFloat>, 4> wheelRotation;
  // Linear velocity in m/s.
  Vec3<HalfFloat> velocity;
  // Engine RPM.
  HalfFloat rpm;
  std::array<HalfFloat, 4> wheelAngularVelocity;
  // Usually zero in the known fixture.
  std::array<HalfFloat, 4> slipAngle;
  std::array<HalfFloat, 4> slipRatio;
  std::array<HalfFloat, 4> ndSlip;
  // Wheel load in Newtons.
  std::array<HalfFloat, 4> load;
  // Steering input in degrees.
  HalfFloat steerAngle;
  HalfFloat bodyworkNoise;
  HalfFloat drivetrainSpeed;
  // Milliseconds elapsed in the current lap.
  std::uint32_t currentLapTime;
  std::uint32_t lastLapTime;
  std::uint32_t bestLapTime;

  // The on-disk layout appears to contain another small unresolved gap before the u8 telemetry.
  std::uint8_t fuel;
  std::uint8_t fuelPerLap;
  std::uint8_t gear;  // 0 = reverse, 1 = neutral, 2 = first gear, etc.
  std::array<std::uint8_t, 4> tireDirt;
  std::uint8_t damageFrontDeformation;
  std::uint8_t damageRear;
  std::uint8_t damageLeft;
  std::uint8_t damageRight;
  std::uint8_t damageFront;
  std::uint8_t gas;
  std::uint8_t brake;
  // Zero-based lap index.
  std::uint8_t currentLap;
  // Unknown / unresolved, usually zero in known fixtures.
  std::uint8_t unknown;
  // Packed bitfield. Inferred bits: 3 = horn, 4..5 = camera direction, 9 = gearbox damage,
  // 12 = lights. The remaining bits are still unresolved.
  std::uint16_t status;
  // Unknown / unresolved, possibly structural padding.
  std::uint16_t unknown2;
  std::uint8_t dirt;
  std::uint8_t engineHealth;
  std::uint8_t boost;
};

}  // namespace acrp
