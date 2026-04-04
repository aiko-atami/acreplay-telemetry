import type {ParsedCarResultV1, ReplayManifestV1} from './types.js'

export type ReplayWorkerRequest =
  | {
      id: number
      type: 'init'
    }
  | {
      id: number
      type: 'inspectReplay'
      fileBytes: ArrayBuffer
    }
  | {
      id: number
      type: 'parseCar'
      fileBytes: ArrayBuffer
      carIndex: number
    }

export type ReplayWorkerSuccessResponse =
  | {
      id: number
      ok: true
      type: 'init'
    }
  | {
      id: number
      ok: true
      type: 'inspectReplay'
      result: ReplayManifestV1
    }
  | {
      id: number
      ok: true
      type: 'parseCar'
      result: ParsedCarResultV1
    }

export type ReplayWorkerErrorResponse = {
  id: number
  ok: false
  type: ReplayWorkerRequest['type']
  error: string
}

export type ReplayWorkerResponse =
  | ReplayWorkerSuccessResponse
  | ReplayWorkerErrorResponse
