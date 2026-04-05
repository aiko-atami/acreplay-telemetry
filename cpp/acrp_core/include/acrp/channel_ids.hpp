#pragma once

#include <cstdint>

namespace acrp {

// Stable channel IDs for LapTelemetryPackV1.
//
// ID ranges:
//   1–25   Core vehicle channels — position, velocity, driver inputs, engine state.
//          All present in every pack regardless of CSP availability.
//   32–47  Wheel analytics — angular velocity, slip ratio, normalised slip, and load.
//          Four values per metric in FL / FR / RL / RR wheel order.
//   64–95  CSP extras — only present when kPackFlagHasCsp is set.
//
// Type column refers to ChannelValueType:
//   f32  IEEE-754 single precision (4 bytes/sample)
//   u8   unsigned byte 0–255 (1 byte/sample)
//   i8   signed byte –128..127 (1 byte/sample)
//
// Derivation column describes how the value is computed from the raw CarFrame:
//   raw     copied verbatim from the source field
//   derived computed from one or more other fields
enum class ChannelId : std::uint16_t {
  // ── Core vehicle (1–25) ────────────────────────────────────────────────────
  distance_m = 1,          // f32  Cumulative distance along the racing line (m).
                            //      Derived: √(Δx²+Δz²) accumulated over XZ position samples.
  time_ms = 2,              // f32  Elapsed lap time (ms). Raw: CarFrame::currentLapTime.
  pos_x = 3,                // f32  World-space X position (m). Raw: CarFrame::position.x.
  pos_y = 4,                // f32  World-space Y (vertical) position (m). CarFrame::position.y.
  pos_z = 5,                // f32  World-space Z position (m). Raw: CarFrame::position.z.
  yaw_rad = 6,              // f32  Yaw rotation (rad). Raw: CarFrame::rotation.y (YXZ Euler Y).
                            //      Used by TrackMap to orient the car arrow.
  rot_x_rad = 7,            // f32  Pitch rotation (rad). Raw: CarFrame::rotation.x (YXZ Euler X).
  rot_y_rad = 8,            // f32  Yaw rotation (rad). Duplicate of yaw_rad for completeness.
  rot_z_rad = 9,            // f32  Roll rotation (rad). Raw: CarFrame::rotation.z (YXZ Euler Z).
  speed_kmh = 10,           // f32  Speed (km/h). Derived: length(velocity) × 3.6.
  vel_x_mps = 11,           // f32  World-space X velocity (m/s). CarFrame::velocity.x widened from f16.
  vel_y_mps = 12,           // f32  World-space Y velocity (m/s). CarFrame::velocity.y widened from f16.
  vel_z_mps = 13,           // f32  World-space Z velocity (m/s). CarFrame::velocity.z widened from f16.
  throttle_raw = 14,        // u8   Throttle pedal 0–255. Raw: CarFrame::gas.
  brake_raw = 15,           // u8   Brake pedal 0–255. Raw: CarFrame::brake.
  steer_deg = 16,           // f32  Steering angle (degrees). CarFrame::steerAngle widened from f16.
  gear = 17,                // i8   Gear: –1=reverse, 0=neutral, 1=first, etc.
                            //      Derived: CarFrame::gear − 1  (raw stores 0=reverse, 1=neutral…).
  rpm = 18,                 // f32  Engine RPM. CarFrame::rpm widened from f16.
  coast = 19,               // u8   Coasting flag: 1 when throttle_raw == 0 && brake_raw == 0, else 0.
  fuel_raw = 20,            // u8   Fuel level 0–255. Raw: CarFrame::fuel.
  fuel_per_lap_raw = 21,    // u8   Fuel used per lap 0–255. Raw: CarFrame::fuelPerLap.
  boost_raw = 22,           // u8   Turbo boost 0–255. Raw: CarFrame::boost.
  engine_health_raw = 23,   // u8   Engine health 0–255. Raw: CarFrame::engineHealth.
  gearbox_being_damaged = 24,  // u8  1 when status bit 9 is set, else 0.
  drivetrain_speed = 25,    // f32  Drivetrain speed. CarFrame::drivetrainSpeed widened from f16.

  // ── Wheel analytics (32–47) — FL/FR/RL/RR order ──────────────────────────
  wheel_ang_vel_fl = 32,    // f32  Front-left angular velocity (rad/s), widened from f16.
  wheel_ang_vel_fr = 33,    // f32  Front-right angular velocity (rad/s), widened from f16.
  wheel_ang_vel_rl = 34,    // f32  Rear-left angular velocity (rad/s), widened from f16.
  wheel_ang_vel_rr = 35,    // f32  Rear-right angular velocity (rad/s), widened from f16.
  wheel_slip_ratio_fl = 36, // f32  Front-left slip ratio (dimensionless), widened from f16.
  wheel_slip_ratio_fr = 37, // f32  Front-right slip ratio (dimensionless), widened from f16.
  wheel_slip_ratio_rl = 38, // f32  Rear-left slip ratio (dimensionless), widened from f16.
  wheel_slip_ratio_rr = 39, // f32  Rear-right slip ratio (dimensionless), widened from f16.
  wheel_nd_slip_fl = 40,    // f32  Front-left normalised slip (dimensionless), widened from f16.
  wheel_nd_slip_fr = 41,    // f32  Front-right normalised slip (dimensionless), widened from f16.
  wheel_nd_slip_rl = 42,    // f32  Rear-left normalised slip (dimensionless), widened from f16.
  wheel_nd_slip_rr = 43,    // f32  Rear-right normalised slip (dimensionless), widened from f16.
  wheel_load_fl_n = 44,     // f32  Front-left wheel load (N), widened from f16.
  wheel_load_fr_n = 45,     // f32  Front-right wheel load (N), widened from f16.
  wheel_load_rl_n = 46,     // f32  Rear-left wheel load (N), widened from f16.
  wheel_load_rr_n = 47,     // f32  Rear-right wheel load (N), widened from f16.

  // ── CSP extras (64–95) — only when kPackFlagHasCsp is set ────────────────
  clutch_raw = 64,          // u8   Clutch pedal 0–255. From CSP EXT_PERCAR CarFrameExtraV6/V7.
};

}  // namespace acrp
