/**
 * Numeric IDs for every telemetry channel in a `LapTelemetryPackV1` pack.
 *
 * **ID ranges:**
 * - `1–25`   Core vehicle — position, velocity, driver inputs, engine state.
 *            Always present regardless of CSP availability.
 * - `32–47`  Wheel analytics — angular velocity, slip ratio, normalised slip, load.
 *            Four values per metric in FL / FR / RL / RR order.
 * - `64–95`  CSP extras — only present when {@link PACK_FLAGS.HAS_CSP} is set.
 *
 * **Type prefixes on JSDoc entries:**
 * - `f32` → `Float32Array` (4 bytes/sample)
 * - `u8`  → `Uint8Array`   (1 byte/sample, 0–255)
 * - `i8`  → `Int8Array`    (1 byte/sample, −128..127)
 *
 * @example
 * ```ts
 * const speed = view.channels.get(channelIds.speed_kmh) as Float32Array;
 * ```
 */
export const channelIds = {
  // ── Core vehicle ─────────────────────────────────────────────────────────
  /** f32  Cumulative distance along the racing line (m). Primary X axis for uPlot. */
  distance_m: 1,
  /** f32  Elapsed lap time (ms). Monotonically increasing within a lap. */
  time_ms: 2,
  /** f32  World-space X position (m). Used with pos_z for TrackMap. */
  pos_x: 3,
  /** f32  World-space Y (vertical) position (m). */
  pos_y: 4,
  /** f32  World-space Z position (m). Used with pos_x for TrackMap. */
  pos_z: 5,
  /** f32  Yaw rotation (rad). YXZ Euler Y component. Used to orient car arrow on TrackMap. */
  yaw_rad: 6,
  /** f32  Pitch rotation (rad). YXZ Euler X component. */
  rot_x_rad: 7,
  /** f32  Yaw rotation (rad). Duplicate of yaw_rad stored as part of the full rotation triple. */
  rot_y_rad: 8,
  /** f32  Roll rotation (rad). YXZ Euler Z component. */
  rot_z_rad: 9,
  /** f32  Speed (km/h). Derived: length(velocity) × 3.6. */
  speed_kmh: 10,
  /** f32  World-space X velocity (m/s). */
  vel_x_mps: 11,
  /** f32  World-space Y velocity (m/s). */
  vel_y_mps: 12,
  /** f32  World-space Z velocity (m/s). */
  vel_z_mps: 13,
  /** u8   Throttle pedal 0–255. */
  throttle_raw: 14,
  /** u8   Brake pedal 0–255. */
  brake_raw: 15,
  /** f32  Steering angle (degrees). Positive = right. */
  steer_deg: 16,
  /** i8   Gear: −1=reverse, 0=neutral, 1=first, 2=second … Derived: raw − 1. */
  gear: 17,
  /** f32  Engine RPM. */
  rpm: 18,
  /** u8   Coasting flag: 1 when throttle_raw == 0 && brake_raw == 0, else 0. */
  coast: 19,
  /** u8   Fuel level 0–255. */
  fuel_raw: 20,
  /** u8   Fuel consumed per lap 0–255. */
  fuel_per_lap_raw: 21,
  /** u8   Turbo boost 0–255. */
  boost_raw: 22,
  /** u8   Engine health 0–255. */
  engine_health_raw: 23,
  /** u8   1 when gearbox damage status bit (bit 9 of CarFrame.status) is set, else 0. */
  gearbox_being_damaged: 24,
  /** f32  Drivetrain speed (units match AC internal drivetrain output). */
  drivetrain_speed: 25,

  // ── Wheel analytics — FL / FR / RL / RR ──────────────────────────────────
  /** f32  Front-left wheel angular velocity (rad/s). */
  wheel_ang_vel_fl: 32,
  /** f32  Front-right wheel angular velocity (rad/s). */
  wheel_ang_vel_fr: 33,
  /** f32  Rear-left wheel angular velocity (rad/s). */
  wheel_ang_vel_rl: 34,
  /** f32  Rear-right wheel angular velocity (rad/s). */
  wheel_ang_vel_rr: 35,
  /** f32  Front-left slip ratio (dimensionless). */
  wheel_slip_ratio_fl: 36,
  /** f32  Front-right slip ratio (dimensionless). */
  wheel_slip_ratio_fr: 37,
  /** f32  Rear-left slip ratio (dimensionless). */
  wheel_slip_ratio_rl: 38,
  /** f32  Rear-right slip ratio (dimensionless). */
  wheel_slip_ratio_rr: 39,
  /** f32  Front-left normalised slip (dimensionless). */
  wheel_nd_slip_fl: 40,
  /** f32  Front-right normalised slip (dimensionless). */
  wheel_nd_slip_fr: 41,
  /** f32  Rear-left normalised slip (dimensionless). */
  wheel_nd_slip_rl: 42,
  /** f32  Rear-right normalised slip (dimensionless). */
  wheel_nd_slip_rr: 43,
  /** f32  Front-left wheel load (N). */
  wheel_load_fl_n: 44,
  /** f32  Front-right wheel load (N). */
  wheel_load_fr_n: 45,
  /** f32  Rear-left wheel load (N). */
  wheel_load_rl_n: 46,
  /** f32  Rear-right wheel load (N). */
  wheel_load_rr_n: 47,

  // ── CSP extras — only when PACK_FLAGS.HAS_CSP is set ─────────────────────
  /** u8   Clutch pedal 0–255. Sourced from CSP EXT_PERCAR v6/v7. */
  clutch_raw: 64,
} as const;

/** Union of all valid channel ID values. Use with {@link LapTelemetryPackView.channels}. */
export type ChannelId = (typeof channelIds)[keyof typeof channelIds];
