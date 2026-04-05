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

/** Async factory that instantiates the compiled WebAssembly module. */
export type WasmModuleFactory = () => Promise<NativeModule>;

/**
 * Emscripten-generated module constructor type.
 * Accepts an optional `locateFile` hook for customising where the `.wasm` file is fetched from.
 */
export type EmscriptenModuleFactory = (moduleArg?: {
  locateFile?(path: string, scriptDirectory: string): string;
}) => Promise<NativeModule>;

/**
 * Wraps an Emscripten-generated module constructor into a {@link WasmModuleFactory}.
 *
 * @param moduleFactory - The raw Emscripten factory (default export of the generated JS glue).
 * @param options - Optional overrides, e.g. a custom `locateFile` to control `.wasm` URL resolution.
 * @returns A zero-argument factory that resolves to the native module.
 */
export function createEmscriptenModuleFactory(
  moduleFactory: EmscriptenModuleFactory,
  options: {
    locateFile?(path: string, scriptDirectory: string): string;
  } = {},
): WasmModuleFactory {
  return () => moduleFactory({ locateFile: options.locateFile });
}

/**
 * Instantiates the WebAssembly module using the provided factory.
 *
 * @param factory - A {@link WasmModuleFactory} returned by {@link createEmscriptenModuleFactory}.
 * @returns The loaded native module ready for use.
 */
export async function loadNativeModule(factory: WasmModuleFactory): Promise<NativeModule> {
  return factory();
}
