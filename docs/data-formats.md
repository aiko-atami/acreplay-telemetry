# Data Formats

## `.acreplay` version 16

The native parser in [`cpp/acrp_core/src/api.cpp`](../cpp/acrp_core/src/api.cpp) is the source of truth for the reverse-engineered replay layout.

Stream order currently documented and implemented:

- `uint32 version`
- `ReplayHeader`
  `double recordingInterval`, 3 strings (`uint32 byte_length + raw bytes`), then 4 `uint32`
- replay-global frame section
  skipped as `(4 + numTrackObjects * 12) * numFrames` bytes
- `numCars` car sections
  5 strings, then `numFrames`, `numWings`
- per-car frame payload
  one opaque 20-byte block before the first `CarFrame`
- inter-frame padding
  for non-final frames: opaque 20-byte block plus `numWings * 4` bytes
- post-car trailer
  after the final frame: `numWings * 4`, then `uint32 count`, then `count * 8` opaque bytes
- optional CSP footer
  `__AC_SHADERS_PATCH_v1__ + uint32 offset + uint32 version` at EOF

Confidence markers used in code comments:

- Confirmed by parser behavior
  parse order and skip sizes needed to read the known fixture correctly
- Inferred from legacy parser / fixture behavior
  semantic guesses for bitfields and CSP extras
- Unknown / unresolved
  opaque padding blocks and fields whose meaning is still not decoded

Important inferred bitfields now preserved in the new codebase:

- base `CarFrame.status`
  bit 3 horn, bits 4..5 camera direction, bit 9 gearbox damage, bit 12 lights
- CSP `EXT_PERCAR` v6/v7 `status`
  bits 0..2 turn-signal state, bit 3 low beams

Only `EXT_PERCAR` v6 and v7 currently have known fixed `bytesPerFrame` values in the parser.

## ReplayManifestV1

`ReplayManifestV1` is the summary returned by the core API and wasm wrapper.

Fields:

- `sourceSha256`
  SHA-256 of the original replay bytes.
- `track`
- `trackConfig`
- `carIndex`
- `driverName`
- `recordingIntervalMs`
- `laps[]`

Each `LapManifestV1` contains:

- `lapNumber`
- `lapTimeMs`
- `sampleCount`
- `isComplete`
- `isValid`

## LapTelemetryPackV1

One lap equals one binary blob.

Header layout:

```c
PackHeaderV1 {
  char magic[4];              // "ACTL"
  uint16_t version;           // 1
  uint16_t header_bytes;      // 36
  uint16_t descriptor_bytes;  // 16
  uint16_t channel_count;
  uint32_t sample_count;
  uint16_t lap_number;
  uint16_t flags;             // bit0 has_csp, bit1 is_complete, bit2 is_valid
  uint32_t lap_time_ms;
  uint32_t recording_interval_us;
  uint32_t descriptors_offset;
  uint32_t data_offset;
}
```

Descriptor layout:

```c
ChannelDescriptorV1 {
  uint16_t channel_id;
  uint8_t value_type;         // 1=f32, 2=u8, 3=i8
  uint8_t reserved;
  uint32_t sample_count;
  uint32_t byte_offset;       // relative to data_offset
  uint32_t byte_length;
}
```

## Channel ranges

- `1..31`
  core vehicle channels
- `32..63`
  wheel analytics
- `64..95`
  CSP extras

Current v1 channels implemented in the core:

- `1 distance_m`
- `2 time_ms`
- `3 pos_x`
- `4 pos_y`
- `5 pos_z`
- `6 yaw_rad`
- `7 rot_x_rad`
- `8 rot_y_rad`
- `9 rot_z_rad`
- `10 speed_kmh`
- `11 vel_x_mps`
- `12 vel_y_mps`
- `13 vel_z_mps`
- `14 throttle_raw`
- `15 brake_raw`
- `16 steer_deg`
- `17 gear`
- `18 rpm`
- `19 coast`
- `20 fuel_raw`
- `21 fuel_per_lap_raw`
- `22 boost_raw`
- `23 engine_health_raw`
- `24 gearbox_being_damaged`
- `25 drivetrain_speed`
- `32..35 wheel_ang_vel_*`
- `36..39 wheel_slip_ratio_*`
- `40..43 wheel_nd_slip_*`
- `44..47 wheel_load_*_n`
- `64 clutch_raw`

## CSV export

CSV is still supported for backward compatibility.

- Export happens through `acrp::exportCsvFiles(...)`.
- CLI target `acrp_cli` uses this path.
- Blender importer still expects CSV, not `LapTelemetryPackV1`.
