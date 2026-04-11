import createDj1000Module from "./dj1000_wasm.mjs";

export const ExportSize = Object.freeze({
  small: 0,
  normal: 1,
  large: 2,
});

const FINALIZE_SESSION = typeof FinalizationRegistry === "function"
  ? new FinalizationRegistry(({ module, sessionPtr }) => {
      module._dj1000_wasm_session_free(sessionPtr);
    })
  : null;

function toUint8Array(input) {
  if (input instanceof Uint8Array) {
    return input;
  }
  if (input instanceof ArrayBuffer) {
    return new Uint8Array(input);
  }
  if (ArrayBuffer.isView(input)) {
    return new Uint8Array(input.buffer, input.byteOffset, input.byteLength);
  }
  throw new TypeError("input must be a Uint8Array, ArrayBuffer, or ArrayBuffer view");
}

function normalizeOptions(options = {}) {
  return {
    size: typeof options.size === "string" ? ExportSize[options.size] : (options.size ?? ExportSize.large),
    redBalance: options.redBalance ?? 100,
    greenBalance: options.greenBalance ?? 100,
    blueBalance: options.blueBalance ?? 100,
    contrast: options.contrast ?? 3,
    brightness: options.brightness ?? 3,
    vividness: options.vividness ?? 3,
    sharpness: options.sharpness ?? 3,
    hasSourceGain: options.sourceGain !== undefined && options.sourceGain !== null,
    sourceGain: options.sourceGain ?? 1.0,
  };
}

function takeErrorMessage(module, errorPtrPtr) {
  const errorPtr = module.HEAPU32[errorPtrPtr >>> 2];
  if (errorPtr === 0) {
    return null;
  }
  const message = module.UTF8ToString(errorPtr);
  module._dj1000_wasm_free_string(errorPtr);
  module.HEAPU32[errorPtrPtr >>> 2] = 0;
  return message;
}

function makeRenderResult(module, pixelsPtr, widthPtr, heightPtr, byteCountPtr) {
  const width = module.HEAP32[widthPtr >>> 2];
  const height = module.HEAP32[heightPtr >>> 2];
  const byteCount = module.HEAPU32[byteCountPtr >>> 2];
  const pixels = module.HEAPU8.slice(pixelsPtr, pixelsPtr + byteCount);
  module._dj1000_wasm_free_buffer(pixelsPtr);

  return {
    width,
    height,
    channels: 4,
    pixelFormat: "rgba",
    pixels,
  };
}

export async function createDj1000WasmConverter(moduleOptions = {}) {
  const module = await createDj1000Module(moduleOptions);

  function convertDatToRgba(input, options = {}) {
    const bytes = toUint8Array(input);
    const normalized = normalizeOptions(options);

    const inputPtr = module._malloc(bytes.byteLength);
    const widthPtr = module._malloc(4);
    const heightPtr = module._malloc(4);
    const byteCountPtr = module._malloc(4);
    const errorPtrPtr = module._malloc(4);

    module.HEAPU8.set(bytes, inputPtr);
    module.HEAPU32[widthPtr >>> 2] = 0;
    module.HEAPU32[heightPtr >>> 2] = 0;
    module.HEAPU32[byteCountPtr >>> 2] = 0;
    module.HEAPU32[errorPtrPtr >>> 2] = 0;

    let pixelsPtr = 0;
    try {
      pixelsPtr = module._dj1000_wasm_convert_dat_rgba(
        inputPtr,
        bytes.byteLength,
        normalized.size,
        normalized.redBalance,
        normalized.greenBalance,
        normalized.blueBalance,
        normalized.contrast,
        normalized.brightness,
        normalized.vividness,
        normalized.sharpness,
        normalized.hasSourceGain ? 1 : 0,
        normalized.sourceGain,
        widthPtr,
        heightPtr,
        byteCountPtr,
        errorPtrPtr,
      );

      if (pixelsPtr === 0) {
        throw new Error(takeErrorMessage(module, errorPtrPtr) ?? "dj1000 conversion failed");
      }

      const result = makeRenderResult(module, pixelsPtr, widthPtr, heightPtr, byteCountPtr);
      pixelsPtr = 0;
      return result;
    } finally {
      if (pixelsPtr !== 0) {
        module._dj1000_wasm_free_buffer(pixelsPtr);
      }
      module._free(errorPtrPtr);
      module._free(byteCountPtr);
      module._free(heightPtr);
      module._free(widthPtr);
      module._free(inputPtr);
    }
  }

  function openSession(input) {
    const bytes = toUint8Array(input);
    const inputPtr = module._malloc(bytes.byteLength);
    const errorPtrPtr = module._malloc(4);
    module.HEAPU8.set(bytes, inputPtr);
    module.HEAPU32[errorPtrPtr >>> 2] = 0;

    let sessionPtr = 0;
    try {
      sessionPtr = module._dj1000_wasm_session_open(
        inputPtr,
        bytes.byteLength,
        errorPtrPtr,
      );
      if (sessionPtr === 0) {
        throw new Error(takeErrorMessage(module, errorPtrPtr) ?? "dj1000 session open failed");
      }

      let closed = false;
      const sessionToken = {};

      const session = {
        renderToRgba(options = {}) {
          if (closed) {
            throw new Error("dj1000 session is already closed");
          }
          const normalized = normalizeOptions(options);
          const widthPtr = module._malloc(4);
          const heightPtr = module._malloc(4);
          const byteCountPtr = module._malloc(4);
          const renderErrorPtrPtr = module._malloc(4);
          module.HEAPU32[widthPtr >>> 2] = 0;
          module.HEAPU32[heightPtr >>> 2] = 0;
          module.HEAPU32[byteCountPtr >>> 2] = 0;
          module.HEAPU32[renderErrorPtrPtr >>> 2] = 0;

          let pixelsPtr = 0;
          try {
            pixelsPtr = module._dj1000_wasm_session_render_rgba(
              sessionPtr,
              normalized.size,
              normalized.redBalance,
              normalized.greenBalance,
              normalized.blueBalance,
              normalized.contrast,
              normalized.brightness,
              normalized.vividness,
              normalized.sharpness,
              normalized.hasSourceGain ? 1 : 0,
              normalized.sourceGain,
              widthPtr,
              heightPtr,
              byteCountPtr,
              renderErrorPtrPtr,
            );

            if (pixelsPtr === 0) {
              throw new Error(takeErrorMessage(module, renderErrorPtrPtr) ?? "dj1000 session render failed");
            }

            const result = makeRenderResult(module, pixelsPtr, widthPtr, heightPtr, byteCountPtr);
            pixelsPtr = 0;
            return result;
          } finally {
            if (pixelsPtr !== 0) {
              module._dj1000_wasm_free_buffer(pixelsPtr);
            }
            module._free(renderErrorPtrPtr);
            module._free(byteCountPtr);
            module._free(heightPtr);
            module._free(widthPtr);
          }
        },
        close() {
          if (closed) {
            return;
          }
          if (FINALIZE_SESSION !== null) {
            FINALIZE_SESSION.unregister(sessionToken);
          }
          module._dj1000_wasm_session_free(sessionPtr);
          closed = true;
          sessionPtr = 0;
        },
      };

      if (FINALIZE_SESSION !== null) {
        FINALIZE_SESSION.register(session, { module, sessionPtr }, sessionToken);
      }

      return session;
    } finally {
      module._free(errorPtrPtr);
      module._free(inputPtr);
    }
  }

  return {
    module,
    convertDatToRgba,
    openSession,
  };
}
