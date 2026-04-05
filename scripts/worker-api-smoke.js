import {access, readFile} from 'node:fs/promises'
import {resolve} from 'node:path'
import {Worker} from 'node:worker_threads'
import {pathToFileURL} from 'node:url'

import {createReplayWorkerClient} from '../packages/acreplay-wasm/dist/index.js'
import {readLapPack} from '../packages/actelemetry/dist/index.js'

const repoRoot = resolve(import.meta.dirname, '..')
const replayPath = resolve(repoRoot, 'cpp/tests/fixtures/example.acreplay')
const workerPath = resolve(repoRoot, 'scripts/worker-api-smoke-worker.js')
const wasmJsPath = resolve(repoRoot, 'cpp/build/wasm/acrp_wasm/acrp_wasm.js')
const wasmBinPath = resolve(repoRoot, 'cpp/build/wasm/acrp_wasm/acrp_wasm.wasm')

function toArrayBuffer(buffer) {
  return buffer.buffer.slice(buffer.byteOffset, buffer.byteOffset + buffer.byteLength)
}

function adaptNodeWorker(worker) {
  const handlers = new Map()

  return {
    addEventListener(type, listener) {
      let wrappedListener = listener

      if (type === 'message') {
        wrappedListener = (data) => listener({data})
      }

      handlers.set(listener, wrappedListener)
      worker.on(type, wrappedListener)
    },
    removeEventListener(type, listener) {
      const wrappedListener = handlers.get(listener)
      if (wrappedListener == null) {
        return
      }

      handlers.delete(listener)
      worker.off(type, wrappedListener)
    },
    postMessage(value, transferList) {
      worker.postMessage(value, transferList)
    },
    terminate() {
      void worker.terminate()
    },
  }
}

async function assertBuildArtifactsExist() {
  try {
    await Promise.all([access(wasmJsPath), access(wasmBinPath), access(replayPath)])
  } catch {
    throw new Error(
      'worker smoke test requires prebuilt wasm artifacts. Run `mise run build:wasm` once, then rerun `mise run smoke:worker`.',
    )
  }
}

await assertBuildArtifactsExist()
const replayFile = await readFile(replayPath)
const inspectBytes = toArrayBuffer(replayFile)
const parseBytes = toArrayBuffer(replayFile)

const worker = new Worker(pathToFileURL(workerPath), {type: 'module'})
const parser = createReplayWorkerClient(adaptNodeWorker(worker))

try {
  await parser.init()

  const manifest = await parser.inspectReplay(inspectBytes)
  if (inspectBytes.byteLength !== 0) {
    throw new Error('Expected inspectReplay buffer to be detached after transfer')
  }

  const parsedCar = await parser.parseCar(parseBytes, 0)
  if (parseBytes.byteLength !== 0) {
    throw new Error('Expected parseCar buffer to be detached after transfer')
  }

  if (parsedCar.lapPacks.length === 0) {
    throw new Error('Expected lap packs from worker parser')
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
    `worker api smoke ok: laps=${parsedCar.lapPacks.length} channels=${packView.header.channelCount} samples=${packView.header.sampleCount}`,
  )
} finally {
  parser.terminate()
}
