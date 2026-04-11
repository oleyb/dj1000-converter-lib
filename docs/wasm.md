# WebAssembly Build

## Why The WASM Target Lives Here

The WASM target is a build of the same converter core, not a separate product.

Keeping it in this repo means:

- native and WASM use the same reverse-engineered algorithm
- parity fixes land in one place
- DLL-backed verification remains anchored to the core implementation
- downstream browser apps can stay thin and depend on a packaged build of this repo

## Build Prerequisites

You only need Emscripten if you want the WASM target.

On macOS with Homebrew:

```bash
brew install emscripten
```

Typical flow:

```bash
emcmake cmake -S . -B build-wasm -G Ninja \
  -DDJ1000_BUILD_WASM=ON \
  -DDJ1000_BUILD_CLI=OFF \
  -DDJ1000_BUILD_TESTS=OFF
cmake --build build-wasm --target dj1000_wasm
```

Outputs:

- `build-wasm/native/dj1000_wasm.mjs`
- `build-wasm/native/dj1000_wasm.wasm`
- `build-wasm/native/dj1000_wasm_api.mjs`

The `.mjs` helper is copied next to the generated Emscripten output so it can be imported directly by a downstream web or Node project.

## Small Example Consumer

There is a tiny static browser-side consumer in [`examples/wasm-browser`](../examples/wasm-browser).

It demonstrates:

- loading the built WASM helper from outside the core library folder
- reading a local `.DAT` file in the browser
- opening a per-image session in a browser worker
- converting it to RGBA off the main thread
- updating the preview live while sliders move
- drawing the result into a `<canvas>`

Run it by:

```bash
python3 -m http.server 8000
```

Then open:

- `http://localhost:8000/examples/wasm-browser/`

## JS Helper API

The helper module lives at [`native/wasm/dj1000_wasm_api.mjs`](../native/wasm/dj1000_wasm_api.mjs) in source form and is copied into the build output.

Example:

```js
import { createDj1000WasmConverter } from "./dj1000_wasm_api.mjs";

const converter = await createDj1000WasmConverter();
const session = converter.openSession(datBytes);
const result = session.renderToRgba({
  size: "large",
  brightness: 6,
  vividness: 6,
});

console.log(result.width, result.height, result.pixels.length);
session.close();
```

Returned shape:

- `width`
- `height`
- `channels`
- `pixelFormat`
- `pixels` as a `Uint8Array` in RGBA order

The helper supports both styles:

- `convertDatToRgba(datBytes, options)` for direct one-shot conversion
- `openSession(datBytes)` plus `session.renderToRgba(options)` for GUI-style repeated editing

The session form is the better fit for editors and library views. It reuses the core library's per-image cache of slider-independent stages, so changing brightness, vividness, or color balance can rerender from a cheaper starting point than reopening the DAT every time.

## Option Mapping

The WASM helper intentionally mirrors the core library settings instead of inventing a new UI model.

Current defaults:

- `size: "large"`
- `redBalance: 100`
- `greenBalance: 100`
- `blueBalance: 100`
- `contrast: 3`
- `brightness: 3`
- `vividness: 3`
- `sharpness: 3`
- `sourceGain: undefined`

That keeps the WASM layer aligned with the native converter and leaves UI-specific slider translation to downstream apps.

## Lower-Level Embedding

If a downstream project wants to avoid the JS helper, it can bind against:

- [`native/include/dj1000/c_api.h`](../native/include/dj1000/c_api.h)

The WASM target exports a simple RGBA conversion bridge built on top of that C API:

- `_dj1000_wasm_convert_dat_rgba`
- `_dj1000_wasm_session_open`
- `_dj1000_wasm_session_render_rgba`
- `_dj1000_wasm_session_free`
- `_dj1000_wasm_free_buffer`
- `_dj1000_wasm_free_string`

## Relationship To DLL Verification

The WASM build does not change the verification model.

The authoritative parity workflow remains:

1. verify native behavior against the original `DsGraph.dll`
2. keep the WASM target as a thin build of that same native core

That way, browser-facing consumers inherit the same verified converter logic rather than a separate rewrite.
