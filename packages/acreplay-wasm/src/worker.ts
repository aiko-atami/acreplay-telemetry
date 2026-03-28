import {createReplayParser} from './index.js'
import type {WasmModuleFactory} from './module.js'

type WorkerRequest =|{
  id: number;
  type: 'init'
}
|{
  id: number;
  type: 'inspectReplay';
  fileBytes: ArrayBuffer
}
|{
  id: number;
  type: 'parseCar';
  fileBytes: ArrayBuffer;
  carIndex: number
}

let parserPromise: ReturnType<typeof createReplayParser>|null = null

export function attachWorkerHandler(factory: WasmModuleFactory): void {
  self.onmessage = async (event: MessageEvent<WorkerRequest>) => {
      const message = event.data

      try {
        if (message.type === 'init') {
        parserPromise = createReplayParser(factory)
        await parserPromise
        self.postMessage({id: message.id, ok: true})
        return
      }

      if (parserPromise == null) {
        throw new Error('Worker is not initialized')
      }

      const parser = await parserPromise
      if (message.type === 'inspectReplay') {
        const result = await parser.inspectReplay(message.fileBytes)
        self.postMessage({id: message.id, ok: true, result})
        return
      }

      const result = await parser.parseCar(message.fileBytes, message.carIndex)
      self.postMessage({id: message.id, ok: true, result})
    } catch (error) {
      self.postMessage({
        id: message.id,
        ok: false,
        error: error instanceof Error ? error.message : String(error),
      })
    }
  }
}
