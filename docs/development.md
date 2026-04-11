# Development Setup

## Toolchain

Required:

- CMake 3.20+
- C++20 compiler
- Python 3

Recommended:

- Ninja
- Git
- Emscripten for the optional WASM target
- Wine for legacy DLL verification

## macOS Setup

Example with Homebrew:

```bash
brew install cmake ninja python
```

Optional WASM toolchain on macOS with Homebrew:

```bash
brew install emscripten
```

## Configure And Build

From the repo root:

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

## Configure And Build The WASM Target

The WASM target is optional and lives in the same repo so it shares the exact converter core.

Example with Emscripten:

```bash
emcmake cmake -S . -B build-wasm -G Ninja \
  -DDJ1000_BUILD_WASM=ON \
  -DDJ1000_BUILD_CLI=OFF \
  -DDJ1000_BUILD_TESTS=OFF
cmake --build build-wasm --target dj1000_wasm
```

That produces:

- `build-wasm/native/dj1000_wasm.mjs`
- `build-wasm/native/dj1000_wasm.wasm`
- `build-wasm/native/dj1000_wasm_api.mjs`

The helper module wraps the low-level export and gives downstream browser/Node code a `convertDatToRgba(...)` entrypoint. See [WebAssembly Build](wasm.md) for usage details.

## Run Tests

```bash
ctest --test-dir build --output-on-failure
```

## Install The Package

```bash
cmake -S . -B build -G Ninja -DCMAKE_INSTALL_PREFIX=/tmp/dj1000-install
cmake --build build
cmake --install build
```

## Use As A CMake Dependency

After installation:

```cmake
find_package(dj1000 CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE dj1000::dj1000)
```

## Repo Layout

- [`native/include`](../native/include): public headers
- [`native/src`](../native/src): converter implementation
- [`native/wasm`](../native/wasm): WASM-facing JS helper layer
- [`native/tests`](../native/tests): fast native tests
- [`tools`](../tools): verification and tracing tools
- [`docs`](.): reverse-engineering and contributor docs

## DLL Verification Setup

DLL-backed verification is optional and local-only.

You need:

- a local copy of the original `DsGraph.dll`
- a working Wine environment
- the harness sources in [`tools/Dj1000DllHarness.cs`](../tools/Dj1000DllHarness.cs)

The full manual workflow is documented in [DLL Verification](dll-verification.md).

Useful commands:

```bash
python3 tools/verify_native_reference_corpus.py --dataset-root /path/to/MDSC
python3 tools/trace_default_large_export_sample.py /path/to/MDSC0010.DAT --vividness 6 --probe-top-level-callsite
python3 tools/trace_default_normal_export_sample.py /path/to/MDSC0008.DAT --probe-top-level-callsite
```

## What Must Stay Easy

This repo should remain easy for a new contributor to do four things:

1. build the native library and CLI
2. run the fast native test suite
3. understand the repo boundaries and public API
4. opt into DLL-backed verification if they want deeper parity checks

If a change makes those harder, the docs should be updated in the same change.
