# Repo Architecture

## Intended Shape

This repository should stay focused on the converter core.

Primary responsibilities:

- native converter library
- standalone CLI
- reverse-engineering notes
- verification against the original DLL
- developer documentation

## Downstream Project Boundaries

Separate repositories should consume this project rather than reimplementing it.

Recommended downstream repos:

- `dj1000-macos-app`
- `dj1000-windows-app`
- `dj1000-web-app`

Those projects should be thin shells around this converter:

- UI and file picking there
- conversion logic here
- algorithm parity tests here

## Why The GUIs Should Be Separate

- keeps the converter easier to reuse
- keeps platform UI concerns from cluttering the core
- allows different release cadences for library vs apps
- makes historical/reverse-engineering documentation easier to preserve in one place

## Why WASM Should Stay In This Repo

The WebAssembly target is still the converter core.

It belongs here because:

- the algorithm source of truth should stay in one repository
- native and WASM parity should share the same tests
- reverse-engineering notes should not be split across repos
- downstream web UI repos should not need to own the low-level build of the converter itself

Recommended split:

- WASM build target and bindings in this repo
- browser application and deployment in a separate repo

## Internal Layers

Recommended mental model:

1. `converter.hpp`
   Thin public library API for consumers.
2. export pipelines
   `small`, `normal`, and `large` paths, plus shared helpers.
3. internal stage helpers
   reverse-engineered pieces such as `0x3890`, `0x3060`, `0x4810`, and so on.
4. verification tools
   Python and C# harnesses that compare native outputs against the original DLL.
5. documentation
   historical notes, milestones, and contributor guidance.

## Public API Direction

Current downstream entry point:

- [`native/include/dj1000/converter.hpp`](../native/include/dj1000/converter.hpp)
- [`native/include/dj1000/c_api.h`](../native/include/dj1000/c_api.h)

Current direction:

- keep a small high-level C++ API
- keep a stable C API for broader embedding and WASM/language bindings
- expose pipeline choice at the top level rather than exposing reverse-engineered stage details
- support both direct one-shot conversion and per-image session workflows
- let per-image sessions cache slider-independent pipeline work so editor-style apps can rerender efficiently
- keep the WASM target as a thin build/binding layer over the same native library

That top-level pipeline choice currently means:

- `legacy`: the DLL-faithful reverse-engineered converter path
- `modern`: a separate high-resolution render path focused on denoise, cleaner tone mapping, and better resizing rather than PhotoRun parity

The public API should not expose individual reverse-engineered stage helpers unless there is a strong reason.

## Verification Principle

There are two kinds of correctness in this project:

- native tests that validate the reconstructed logic directly
- DLL-backed tests that validate parity against the original software

Both are important:

- native tests are fast and always runnable
- DLL-backed tests protect against faithful-looking but incorrect rewrites

## Publishing Principle

This repository should be publishable without shipping proprietary runtime files.

That means:

- do not require `DsGraph.dll` to build the library or run the normal test suite
- keep DLL verification opt-in
- document how to add proprietary files locally for verification without making them part of the public repo
