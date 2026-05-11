# DLL Verification

DLL-backed verification is still available, but it is an opt-in maintainer workflow, not a normal build prerequisite.

That means:

- the native library and test suite should work without any proprietary files
- the original `DsGraph.dll` is only needed when you want to compare native behavior against the legacy converter directly
- this repo does not need to redistribute the DLL

## Is It Easy To Do By Hand?

Yes, once the prerequisites are in place.

The manual workflow is now straightforward enough for contributors, but it is still more involved than running the normal native tests because it depends on:

- a local copy of the original `DsGraph.dll`
- Wine
- a Wine prefix that can run the C# harness used to call into the DLL

The normal native test suite is the easy path. DLL-backed tracing is the deeper parity-check path.

## Prerequisites

Build the native tools first:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Then provide:

- the original `DsGraph.dll`
- a working `wine` binary
- a dataset folder containing matched `.DAT` and `.BMP` files if you want corpus checks

## Environment Variables

The verification helpers can now be driven without machine-specific edits.

Supported overrides:

- `DJ1000_WINE`: path to the `wine` executable if it is not on `PATH`
- `DJ1000_WINEPREFIX`: Wine prefix used for the harness and DLL calls
- `DJ1000_WINEPREFIX_TEMPLATE`: optional prefix template to copy on first run
- `DJ1000_DLL_PATH`: path to the original `DsGraph.dll`
- `DJ1000_HARNESS_SOURCE`: optional override for `tools/Dj1000DllHarness.cs`
- `DJ1000_HARNESS_EXE`: optional override for the compiled harness location
- `DJ1000_REFERENCE_DATASET`: dataset root for the reference corpus verifier

Example:

```bash
export DJ1000_DLL_PATH=/path/to/DsGraph.dll
export DJ1000_WINE=/opt/homebrew/bin/wine
export DJ1000_WINEPREFIX=$HOME/.local/share/dj1000-wineprefix
export DJ1000_REFERENCE_DATASET=/path/to/MDSC
```

## One End-To-End DLL Check

This is a good manual smoke test for a known edited export:

```bash
python3 tools/trace_default_large_export_sample.py \
  "$DJ1000_REFERENCE_DATASET/MDSC0010.DAT" \
  --brightness 6 \
  --vividness 6 \
  --probe-top-level-callsite
```

Expected result:

- a stream of `OK ...` stage checks
- a final success line indicating the trace matched through the final large-export BMP

Artifacts are written under `tmp_debug/default_large_export_trace` by default, or another directory if you pass `--scratch`.

## Native Corpus Check

This does not call the DLL. It verifies the native exporter against the known local BMP corpus:

```bash
python3 tools/verify_native_reference_corpus.py --dataset-root "$DJ1000_REFERENCE_DATASET"
```

Expected result:

- one `OK ...` line per BMP
- a final `verified 14 corpus BMPs`

## Notes About The Harness

The DLL tools compile and run [`tools/Dj1000DllHarness.cs`](../tools/Dj1000DllHarness.cs) inside Wine.

That means the Wine environment needs to provide a working C# compiler at the path the harness expects:

- `C:\windows\Microsoft.NET\Framework\v4.0.30319\csc.exe`

In practice, that usually means using a Wine setup with Wine Mono or .NET installed in the prefix.

## When To Use Which Workflow

Use the native tests when:

- you are changing internal implementation details
- you want fast feedback
- you are not touching behavior-sensitive code

Use DLL-backed verification when:

- you are changing image math or stage ordering
- you are touching code tied to a known reverse-engineered DLL stage
- you want to prove parity against the original converter rather than only against the local BMP corpus
