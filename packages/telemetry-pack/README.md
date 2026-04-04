# @aikotami/telemetry-pack

TypeScript reader and utility toolkit for the binary lap-pack format produced by the replay parser.

## What it does

- validates lap-pack buffers
- reads headers, channel descriptors, and typed channel arrays
- exposes helpers for nearest-sample lookup, delta-time calculations, and chart adapters

## Position in the pipeline

`lapPack bytes` -> `@aikotami/telemetry-pack` -> `channels / delta / chart data`
