# Contributing

This project is meant to be friendly both to human developers and to tooling-assisted contributors. The goal is not just to preserve a working converter, but to preserve how it works and how we know it is correct.

## What Matters Most

- keep the native converter faithful to the original software
- prefer small, testable changes
- document any new DLL findings in the repo
- preserve the ability to verify native changes against the original converter

## Local Development Setup

Required:

- CMake 3.20+
- a C++20 compiler
- Python 3.11+ or similar modern Python
- Ninja is recommended

Recommended macOS setup:

```bash
brew install cmake ninja python
```

Optional WASM toolchain on macOS:

```bash
brew install emscripten
```

Build and test:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Optional WASM build:

```bash
emcmake cmake -S . -B build-wasm -G Ninja \
  -DDJ1000_BUILD_WASM=ON \
  -DDJ1000_BUILD_CLI=OFF \
  -DDJ1000_BUILD_TESTS=OFF
cmake --build build-wasm --target dj1000_wasm
```

The WASM-specific workflow is documented in [docs/wasm.md](docs/wasm.md).

## Optional Legacy Verification Setup

DLL-backed verification is optional, but important.

To use it, you need:

- a local copy of the original `DsGraph.dll`
- a working Wine environment
- a Wine environment capable of compiling/running the C# harness used by [`Dj1000DllHarness.cs`](tools/Dj1000DllHarness.cs)

This repo should not require proprietary files to build the native library or run its normal unit tests. The legacy DLL should remain an opt-in verification path.

The concrete manual workflow is documented in [docs/dll-verification.md](docs/dll-verification.md).

## Useful Verification Commands

Native tests:

```bash
ctest --test-dir build --output-on-failure
```

Reference corpus verification:

```bash
python3 tools/verify_native_reference_corpus.py --dataset-root /path/to/MDSC
```

Stage-by-stage tracing against the original DLL, when configured:

```bash
python3 tools/trace_default_large_export_sample.py /path/to/MDSC0010.DAT --vividness 6 --probe-top-level-callsite
python3 tools/trace_default_normal_export_sample.py /path/to/MDSC0008.DAT --probe-top-level-callsite
```

## Documentation Expectations

When you learn something new about the original converter, update the docs in the same change whenever possible:

- [`docs/reverse-engineering.md`](docs/reverse-engineering.md) for confirmed findings
- [`docs/native-rewrite-plan.md`](docs/native-rewrite-plan.md) for milestones and remaining work
- [`docs/repo-architecture.md`](docs/repo-architecture.md) when repo boundaries or public surfaces change
- [`docs/development.md`](docs/development.md) when setup or workflow changes
- [`docs/wasm.md`](docs/wasm.md) when WASM build or embedding details change

## Repository Philosophy

This repository is the converter core, not the final GUI product.

That means:

- core library and CLI belong here
- native tests and verification tooling belong here
- reverse-engineering notes belong here
- desktop apps and browser UIs should live elsewhere and depend on this repo

## Binding Direction

The repo now has two intended public surfaces:

- a small C++ API in [`native/include/dj1000/converter.hpp`](native/include/dj1000/converter.hpp)
- a stable C API in [`native/include/dj1000/c_api.h`](native/include/dj1000/c_api.h)

That makes it easier to:

- embed the converter in desktop apps
- bind into other languages
- expose a WASM-friendly boundary

Changes to those headers should be treated more carefully than internal stage-helper changes.
