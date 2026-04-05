type NativeModule = {
  HEAPU8: Uint8Array;
  _acrp_alloc_buffer(size: number): number;
  _acrp_free_buffer(ptr: number): void;
  _acrp_last_error_message(): number;
  _acrp_last_error_length(): number;
  _acrp_inspect_json(
    inputPtr: number,
    inputLen: number,
    outputPtrPtr: number,
    outputLenPtr: number,
  ): number;
  _acrp_parse_car_json(
    inputPtr: number,
    inputLen: number,
    carIndex: number,
    outputPtrPtr: number,
    outputLenPtr: number,
  ): number;
  getValue(ptr: number, type: "*" | "i32"): number;
  UTF8ToString(ptr: number, len: number): string;
};

export type WasmModuleFactory = () => Promise<NativeModule>;

export type EmscriptenModuleFactory = (moduleArg?: {
  locateFile?(path: string, scriptDirectory: string): string;
}) => Promise<NativeModule>;

export function createEmscriptenModuleFactory(
  moduleFactory: EmscriptenModuleFactory,
  options: {
    locateFile?(path: string, scriptDirectory: string): string;
  } = {},
): WasmModuleFactory {
  return () => moduleFactory({ locateFile: options.locateFile });
}

export async function loadNativeModule(factory: WasmModuleFactory): Promise<NativeModule> {
  return factory();
}
