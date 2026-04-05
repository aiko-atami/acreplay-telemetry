/**
 * Validates the structure of a raw `LapTelemetryPackV1` binary buffer.
 *
 * Checks magic bytes (`"ACTL"`), version (`1`), fixed header/descriptor sizes,
 * descriptor-table offset alignment, and that the data block fits within the buffer.
 *
 * @param buffer - Raw `ArrayBuffer` to validate.
 * @throws {Error} With a descriptive message if any structural invariant is violated.
 */
export function validateLapPack(buffer: ArrayBuffer): void {
  const view = new DataView(buffer);
  if (buffer.byteLength < 36) {
    throw new Error("LapTelemetryPackV1 buffer is too small");
  }

  const magic = String.fromCharCode(
    view.getUint8(0),
    view.getUint8(1),
    view.getUint8(2),
    view.getUint8(3),
  );
  if (magic !== "ACTL") {
    throw new Error(`Unexpected pack magic: ${magic}`);
  }

  const version = view.getUint16(4, true);
  if (version !== 1) {
    throw new Error(`Unsupported pack version: ${version}`);
  }

  const headerBytes = view.getUint16(6, true);
  const descriptorBytes = view.getUint16(8, true);
  const channelCount = view.getUint16(10, true);
  const descriptorsOffset = view.getUint32(28, true);
  const dataOffset = view.getUint32(32, true);

  if (headerBytes !== 36) {
    throw new Error(`Unexpected header size: ${headerBytes}`);
  }
  if (descriptorBytes !== 16) {
    throw new Error(`Unexpected descriptor size: ${descriptorBytes}`);
  }
  if (descriptorsOffset !== headerBytes) {
    throw new Error("Descriptor table does not start after header");
  }

  const descriptorTableBytes = channelCount * descriptorBytes;
  if (dataOffset !== descriptorsOffset + descriptorTableBytes) {
    throw new Error("Data block offset does not match descriptor table size");
  }
  if (dataOffset > buffer.byteLength) {
    throw new Error("Data block offset exceeds buffer length");
  }
}
