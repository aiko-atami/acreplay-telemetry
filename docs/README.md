# Project Docs

This repository currently contains four layers around Assetto Corsa `.acreplay` files:

- `cpp/` is the native workspace root for CMake, compiler settings, and tests.
- `cpp/acrp_core/` is the new C++ core library used by every modern target.
- `cpp/acrp_cli/` and `cpp/acrp_wasm/` are thin wrappers over the same core.
- `packages/telemetry-pack/` and `packages/acreplay-wasm/` are the TypeScript consumers for the wasm and telemetry-pack formats.

Recommended reading order:

1. [overview.md](overview.md)
2. [architecture.md](architecture.md)
3. [build-and-test.md](build-and-test.md)
4. [data-formats.md](data-formats.md)

Tooling note:

- `mise.toml` in the repo root is the shared toolchain config for this mixed C++ / wasm / TypeScript workspace.
