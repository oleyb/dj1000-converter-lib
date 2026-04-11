# Mitsubishi DJ-1000 / UMAX PhotoRun Converter

Native reverse-engineered converter for the UMAX PhotoRun / Mitsubishi DJ-1000 `.DAT` image format.

This repository is intended to be the source of truth for:

- the standalone converter CLI
- the reusable native library that other applications depend on
- the reverse-engineering notes and historical documentation
- the verification tooling that compares native behavior against the original Windows DLL

It is not intended to contain the final desktop GUI apps. Those should live in separate repositories and consume this project as a dependency.

## Goals

- Preserve faithful conversion behavior, including the original software's quirks.
- Provide a clean native library API for other projects.
- Provide a standalone CLI for scripting, batch conversion, and debugging.
- Keep the reverse-engineering process transparent and well documented.
- Preserve the ability to verify native changes against the original `DsGraph.dll`.

## Recommended Repo Boundaries

Keep this repository as the converter core.

What belongs here:

- C++ library code in [`native/include`](native/include) and [`native/src`](native/src)
- the CLI in [`native/src/main.cpp`](native/src/main.cpp)
- native tests
- DLL-verification tools in [`tools`](tools)
- reverse-engineering and contributor documentation in [`docs`](docs)

What should be separate repositories:

- macOS desktop app
- Windows desktop app
- browser/web UI

Those projects should depend on this repository's library package rather than duplicating converter logic.

## WASM Recommendation

The WebAssembly build target should live in this repository.

Reasoning:

- it is the same converter core, not a different product
- parity tests belong next to the implementation they verify
- bugs fixed in native code should immediately benefit native and WASM builds
- it avoids a second repo drifting from the core algorithm

What should stay separate is the browser application itself. A future web app repo can depend on a WASM package built from this project.

## Public Library Surface

The intended downstream entry point is [`converter.hpp`](native/include/dj1000/converter.hpp).

It exposes:

- `dj1000::ConvertOptions`
- `dj1000::ConvertDebugState`
- `dj1000::ConvertedImage`
- `dj1000::convert_dat_to_bgr(...)`
- `dj1000::write_bmp(...)`

That keeps downstream apps away from the internal pipeline stages.

## Build

Configure from the repo root:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Build options:

- `-DDJ1000_BUILD_CLI=ON|OFF`
- `-DDJ1000_BUILD_TESTS=ON|OFF`
- `-DDJ1000_INSTALL=ON|OFF`

## Install / Consume As A Dependency

The native CMake project now exports an installable package:

```bash
cmake -S . -B build -G Ninja -DCMAKE_INSTALL_PREFIX=/tmp/dj1000-install
cmake --build build
cmake --install build
```

Downstream CMake projects can then use:

```cmake
find_package(dj1000 CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE dj1000::dj1000)
```

## Verification Against The Original DLL

This repo deliberately keeps the verification workflow, but it does not need to redistribute proprietary runtime files.

Verification tooling lives in [`tools`](tools), including:

- [`verify_native_reference_corpus.py`](tools/verify_native_reference_corpus.py)
- [`trace_default_large_export_sample.py`](tools/trace_default_large_export_sample.py)
- [`trace_default_normal_export_sample.py`](tools/trace_default_normal_export_sample.py)
- [`Dj1000DllHarness.cs`](tools/Dj1000DllHarness.cs)

Contributors who want DLL-backed verification should provide their own local copy of the original DLL and a working Wine environment. The step-by-step workflow is documented in [DLL Verification](docs/dll-verification.md) and summarized in [CONTRIBUTING.md](CONTRIBUTING.md).

The reference-corpus verifier expects a local dataset path:

```bash
python3 tools/verify_native_reference_corpus.py --dataset-root /path/to/MDSC
```

## Documentation

- [Repo Architecture](docs/repo-architecture.md)
- [Development Setup](docs/development.md)
- [DLL Verification](docs/dll-verification.md)
- [Reverse Engineering Notes](docs/reverse-engineering.md)
- [Native Rewrite Plan](docs/native-rewrite-plan.md)

## Current Verification Status

The native converter currently matches the local MDSC reference corpus exactly:

- `14 / 14` known BMP references pass
- this includes the known edited exports, not just untouched defaults

## License

This repository is licensed under the Apache License 2.0. See [LICENSE](LICENSE).

## Before Publishing Publicly

There are still a few repo-level decisions worth making explicitly:

- decide whether to publish any sample fixtures, and which ones are safe to redistribute
- decide which sample assets, if any, are safe to include directly in the public repo
