# WASM Browser Example

This folder is a tiny browser-side consumer of the `dj1000` WebAssembly build.

It is intentionally small:

- no framework
- no bundler
- no repo-specific magic beyond importing the built WASM helper

## Build The WASM Target

From the repo root:

```bash
emcmake cmake -S . -B build-wasm -G Ninja \
  -DDJ1000_BUILD_WASM=ON \
  -DDJ1000_BUILD_CLI=OFF \
  -DDJ1000_BUILD_TESTS=OFF
EM_CACHE="$PWD/build-wasm/.emcache" cmake --build build-wasm --target dj1000_wasm
```

## Serve The Repo Root

From the repo root:

```bash
python3 -m http.server 8000
```

Then open:

- `http://localhost:8000/examples/wasm-browser/`

## What It Demonstrates

- loading the built WASM module from a separate consumer directory
- reading a local `.DAT` file in the browser
- opening a per-image WASM session from uploaded DAT bytes
- calling `session.renderToRgba(...)` inside a module worker
- live slider updates while conversion runs off the main UI thread
- drawing the returned RGBA pixels into a `<canvas>`

For the lower-level build and API details, see [WebAssembly Build](../../docs/wasm.md).
