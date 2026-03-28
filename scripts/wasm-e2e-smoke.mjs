import {readFile} from 'node:fs/promises'
import {resolve} from 'node:path'

import createAcrpModule from '../cpp/build-wasm/acrp_wasm/acrp_wasm.js'
import {createEmscriptenModuleFactory, createReplayParser,} from '../packages/acreplay-wasm/dist/index.js'
import {readLapPack} from '../packages/telemetry-pack/dist/index.js'

const repoRoot = resolve(import.meta.dirname, '..')
const wasmPath = resolve(repoRoot, 'cpp/build-wasm/acrp_wasm/acrp_wasm.wasm')
const replayPath = resolve(repoRoot, 'cpp/tests/fixtures/example.acreplay')

const fileBytes = await readFile(replayPath)
const parser = await createReplayParser(
    createEmscriptenModuleFactory(createAcrpModule, {
      locateFile(path, scriptDirectory) {
        if (path.endsWith('.wasm')) {
          return wasmPath
        }
        return new URL(path, scriptDirectory).toString()
      },
    }),
)

const manifest = await parser.inspectReplay(
    fileBytes.buffer.slice(
        fileBytes.byteOffset,
        fileBytes.byteOffset + fileBytes.byteLength,
        ),
)
const parsedCar = await parser.parseCar(
    fileBytes.buffer.slice(
        fileBytes.byteOffset,
        fileBytes.byteOffset + fileBytes.byteLength,
        ),
    0,
)

if (parsedCar.lapPacks.length === 0) {
  throw new Error('Expected lap packs from wasm parser')
}

const firstLap = parsedCar.lapPacks[0]
const packView = readLapPack(
    firstLap.bytes.buffer.slice(
        firstLap.bytes.byteOffset,
        firstLap.bytes.byteOffset + firstLap.bytes.byteLength,
        ),
)

if (manifest.laps.length !== parsedCar.manifest.laps.length) {
  throw new Error('Manifest lap count mismatch between inspectReplay and parseCar')
}

if (packView.header.lapNumber !== firstLap.lapNumber) {
  throw new Error('Decoded lap pack header does not match lap number')
}

console.log(
    `wasm e2e ok: laps=${parsedCar.lapPacks.length} channels=${packView.header.channelCount} samples=${
        packView.header.sampleCount}`,
)
