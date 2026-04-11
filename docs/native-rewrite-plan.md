# Native Rewrite Plan

## Goal

Replace the Wine/DLL bridge with a native converter that can run on both macOS and Windows while preserving the original PhotoRun output as closely as possible.

## Current native baseline

The new native workspace lives under [`native/`](../native).

It currently contains:

  - a C++ library for:
  - `.DAT` parsing
  - trailer metadata extraction
  - exact `TransToIndexView` reproduction
  - exact reimplementation of the geometry-stage resample-LUT helper at `0x10001210`
  - exact reimplementation of the currently isolated impulse-seed slice at the head of `0x10005eb0`
    - exact reimplementation of the pre-geometry float-plane normalization helper at `0x10001e00`
    - exact reimplementation of the float-to-byte truncation stage that feeds geometry
    - exact reimplementation of the large-path geometry helper at `0x10001920`
    - exact reimplementation of the non-large row helper at `0x10001000`
    - exact reimplementation of the non-large wrapper at `0x100016a0`
    - exact reimplementation of the deterministic copy-out stage at the tail of `0x100013c0`
    - an exact composed normal/export `source -> normalize -> quantize -> non-large geometry` path
    - exact reimplementation of the post-geometry double-plane prepare helper at `0x10002410`
    - exact reimplementation of the post-geometry delta filter helper at `0x100021a0`
    - exact reimplementation of the post-geometry center-driven side-plane scale helper at `0x10002c40`
    - an exact composed normal/export `source -> normalize -> quantize -> non-large geometry -> post-geometry prepare/filter` path
    - simple 24-bit BMP writing
- a small CLI:
  - `dj1000 info INPUT.DAT`
  - `dj1000 index-bmp INPUT.DAT OUTPUT.bmp`
  - `dj1000 dump-resample-lut WIDTH OUTPUT.bin`
  - `dj1000 nonlarge-stage-plane PREVIEW_FLAG CHANNEL PLANE0.bin PLANE1.bin PLANE2.bin OUTPUT.bin`
  - `dj1000 source-seed-stage PREVIEW_FLAG INPUT.bin OUTPUT_PREFIX`
  - `dj1000 large-stage-plane INPUT.bin OUTPUT.bin`
  - `dj1000 normalize-stage PREVIEW_FLAG PLANE0.f32 PLANE1.f32 PLANE2.f32 OUTPUT_PREFIX`
  - `dj1000 quantize-stage PLANE0.f32 PLANE1.f32 PLANE2.f32 OUTPUT_PREFIX`
  - `dj1000 nonlarge-source-stage PREVIEW_FLAG INPUT.bin OUTPUT_PREFIX`
  - `dj1000 post-geometry-prepare WIDTH HEIGHT PLANE0.bin PLANE1.bin PLANE2.bin OUTPUT_PREFIX`
  - `dj1000 post-geometry-center-scale WIDTH HEIGHT DELTA0.f64 CENTER.f64 DELTA2.f64 OUTPUT_PREFIX`
  - `dj1000 post-geometry-filter WIDTH HEIGHT DELTA0.f64 DELTA2.f64 OUTPUT_PREFIX`
  - `dj1000 nonlarge-post-geometry-source PREVIEW_FLAG INPUT.bin OUTPUT_PREFIX`
- unit tests and DLL-backed parity scripts that now cover:
  - preview/index reorder
  - resample LUT generation
  - the isolated source-stage impulse seed
  - the full normal/export source stage
  - the combined normal/export source -> normalize block
  - the combined normal/export source -> normalize -> quantize -> non-large geometry block
  - the isolated post-geometry prepare helper
  - the isolated post-geometry center-scale helper
  - the isolated post-geometry filter helper
  - the combined normal/export source -> normalize -> quantize -> non-large geometry -> post-geometry prepare/filter block
  - large-path vertical geometry
  - non-large row resampling
  - non-large geometry wrapping
  - pre-geometry float-plane normalization

## Why start here

`TransToIndexView` is one part of the original DLL we already understand completely, so it gives us a native code path we can trust while we build out the harder full-resolution pipeline.

## Proposed reverse-engineering stages

1. Lock down fixtures.
   - Use the current reference corpus to generate machine-readable expected outputs.
   - Separate untouched exports from slider-adjusted exports.

2. Reconstruct the container and mode logic.
   - Keep the `.DAT` layout, trailer parsing, and mode selection exact.
   - Preserve the original size-selection behavior for `320x240` and `504x378`.

3. Reconstruct the geometry lookup / resample helpers.
   - Lift self-contained helpers like `0x10001210`, `0x10001290`, and `0x10001340`.
   - Verify them directly against the original DLL before moving on.

4. Reconstruct the raw decode and intermediate planes.
   - Focus first on the path that produces the three `504x384` byte planes before geometry/crop.
   - Add instrumentation against the original DLL wherever possible.

5. Reconstruct geometry/crop.
   - Reimplement the `0x100013c0`-style output stage for small and large exports.
   - Validate exact dimensions and edge behavior against the original BMPs.

6. Reconstruct tone/color controls.
   - Map brightness, vividness, contrast, sharpness, and color balance onto the native pipeline.
   - Validate per-image slider cases against the known edited references.

7. Replace the GUI backend.
   - Once native large/small export parity is good enough, swap the GUI preview/export path from Wine to the native core.

## Verification strategy

- Treat the current DLL-backed output as the ground truth oracle.
- Use exact SHA-256 matches for BMP fixtures whenever possible.
- Add stage-level tests as individual helpers become understood.
- Keep native components usable from the command line so they can be diffed independently of the GUI.

## Immediate next tasks

- Keep the newly lifted `0x10001210` helper verified against the DLL with the parity script.
- Keep the newly lifted `0x10001e00` helper verified against the DLL with the parity script.
- Keep the new native `0x10001920` large-path helper verified against the DLL with the parity script.
- Keep the new native mode-0 `0x10001000` row helper verified against the DLL with the parity script.
- Keep the new native `0x100016a0` per-plane wrapper verified against the DLL with the full-stage parity script.
- Keep the new impulse-exact `0x10005eb0` source-seed helper verified against the DLL with the parity script.
- Keep the new bright vertical gate slice from `0x10005eb0` verified against the DLL on the flat / vertical-step cases where later source-stage passes stay inert.
- Keep the new native source-center stage verified against the DLL on the flat / rowcode / checker / hstep / vstep corpus in both modes.
- Keep the now-complete normal/export source stage verified against the DLL with [`verify_native_source_stage.py`](../tools/verify_native_source_stage.py).
- Keep the new combined normal/export source -> normalize block verified with [`verify_native_pregeometry_pipeline.py`](../tools/verify_native_pregeometry_pipeline.py).
- Keep the new combined normal/export source -> normalize -> quantize -> non-large geometry block verified with [`verify_native_nonlarge_source_pipeline.py`](../tools/verify_native_nonlarge_source_pipeline.py).
- Keep the new isolated `0x10002410` post-geometry prepare helper verified with [`verify_native_post_geometry_prepare.py`](../tools/verify_native_post_geometry_prepare.py).
- Keep the new isolated `0x10002c40` post-geometry center-scale helper verified with [`verify_native_post_geometry_center_scale.py`](../tools/verify_native_post_geometry_center_scale.py).
- Keep the new isolated `0x100021a0` post-geometry filter helper verified with [`verify_native_post_geometry_filter.py`](../tools/verify_native_post_geometry_filter.py).
- Keep the new combined normal/export source -> normalize -> quantize -> non-large geometry -> post-geometry prepare/filter block verified with [`verify_native_nonlarge_post_geometry_pipeline.py`](../tools/verify_native_nonlarge_post_geometry_pipeline.py).
- Keep the default `320 x 240` end-to-end path locked against fresh DLL-backed exports:
  - `MDSC0001.DAT`, `MDSC0008.DAT`, and `MDSC0009.DAT` now match exactly with the current native default path
  - the plain `normal-export-bmp` command now uses the DLL-backed call-site values directly:
    - crop top: `3`
    - `0x10003060 param0 = 16`
    - `0x10003060 param1 = 40`
    - `0x10003060 scalar = 30`
    - `0x10003060 threshold = 20`
- Keep the newly corrected `0x10001cc0` source-gain helper behavior documented:
  - the region average is integer-divided before the ratio is formed
  - max-gain cases hid this bug; the non-saturated `MDSC0009.DAT` case exposed it immediately
- Keep the newly corrected `0x10004810` behavior documented:
  - the helper uses flat-buffer overlap semantics in both its horizontal and vertical repair passes
  - the left-edge normal-export mismatch came from clipping those overlapping writes too aggressively in native
- Circle back to the preview-only `128 x 64` tail/context quirk after the export-critical pipeline keeps moving:
  - `preview.rowcode` differs only in two quantized tail bytes
  - `preview.gradient` and `preview.hstep` still show a few larger last-row/context mismatches in plane `0`
- Move on from the now-verified default normal-export path into the remaining open areas:
  - preview parity
  - the full large / `504 x 378` export path
  - non-default slider-controlled native exports once the default call sites stay stable
- Treat the open-interval normalization rule `20.0 < plane1 < 127.0` as locked down for future ports of `0x10001e00`.

## New large-path tracing status

- Keep the new top-level large-export trace mode in [`Dj1000DllHarness.cs`](../tools/Dj1000DllHarness.cs) available while the remaining `504 x 378` drift is open.
- Treat these top-level large-path facts as locked down:
  - `0x100021a0` is part of the real default large path, between `0x10002410` and `0x10004810`
  - `0x10003890 level = 3`
  - `0x10003060 param0 = 16`
  - `0x10003060 param1 = 40`
  - `0x10003060 param2 = 160`
  - `0x10003060 param3 = 10`
  - `0x10003060 threshold = 20`
  - `0x10004450 scalar = clamp(source_gain, 1.0, 1.5)` still matches the top-level call path
- Do not yet rewrite the native large path to feed the raw top-level `0x10003060` traced slot directly into the current helper implementation.
  - The current standalone helper signature / interpretation is not yet aligned well enough for that direct substitution.
- Treat the large-path center branch as effectively solved for the traced default case:
  - top-level `3060.pre.plane0.i32` matches the reconstructed native `edge.i32`
  - top-level `3060.pre.plane1.f64` matches the reconstructed native `2c40.center.f64`
- Keep the native large path wired as:
  - `geometry -> 0x2410 -> 0x21a0 -> 0x4810 -> 0x3600 -> 0x2a00 -> 0x2c40 -> 0x2dd0 -> 0x3890 -> 0x3060 -> 0x4450 -> RGB/post-RGB`
- Treat the broad large pipeline as mostly solved now:
  - exact large default export is now verified for `MDSC0001`, `MDSC0004`, `MDSC0005`, `MDSC0007`, `MDSC0008`, `MDSC0009`, `MDSC0011`, and `MDSC0016`
  - `0x10004810` is no longer the main remaining blocker
- Keep the corrected `0x10004810` structure documented:
  - horizontal repair rows: `0 .. height - 3`
  - vertical temp-buffer source rows: full source height
  - vertical repair rows: `0 .. height - 2`
- Focus the remaining large-path work on the low-gain upstream branch instead:
  - the upstream low-gain source-stage bug is now fixed:
    - the ratio path in the source center/source stage triggers when `abs(left - right) >= 2.0`
    - that correction closes `MDSC0015.DAT` and `MDSC0017.DAT` through geometry and the whole post-geometry chain up to `0x10003890`
  - keep the low-gain top-level call-site values locked down:
    - `MDSC0015.DAT`
      - `source_gain = 1.24054054054054`
      - `0x10003060 param1 = 24`
      - `0x10003060 threshold = 23`
      - `0x10003060 scalar = 1.24054054054054`
      - `0x10004450 scalar = 1.24054054054054`
    - `MDSC0017.DAT`
      - `source_gain = 1.02`
      - `0x10003060 param1 = 20`
      - `0x10003060 threshold = 25`
      - `0x10003060 scalar = 1.02`
      - `0x10004450 scalar = 1.02`
  - the remaining large default-export blocker is now very narrow:
    - native `0x10003060`
    - only on the low-gain real-image cases
    - current residuals:
      - `MDSC0015.DAT`: `60` samples, max diff `13.967195675675725`
      - `MDSC0017.DAT`: `197` samples, max diff `28.06117389473684`

## Current end-to-end export status

- Treat the large default-export path as closed for the current untouched local corpus:
  - `MDSC0001`, `MDSC0004`, `MDSC0005`, `MDSC0007`, `MDSC0008`, `MDSC0009`, `MDSC0011`, `MDSC0015`, `MDSC0016`, and `MDSC0017` now all match exactly.
- Treat the small default-export path as a separate locked call-site variant of the non-large pipeline:
  - `export_mode = 0`
  - `0x10003060 param0 = 8`
  - `0x10003060 param1 = 40`
  - `0x10003060 threshold = 20`
  - that path now matches `MDSC0008.DAT -> mdsc0008s.bmp` and `MDSC0009.DAT -> mdsc0009s.bmp` exactly
- The native reference corpus is now exact for `14 / 14` known local BMP references.
  - the edited cases are closed too:
    - `mdsc0006.bmp` with brightness `6`
    - `mdsc0010.bmp` with brightness `6` plus vividness `6`
- Keep the `0x10003890` regression pinned down:
  - the vividness side-plane path needs the grouped `((plane0 * ratio) + plane1) * scale` evaluation
  - the intermediate multiply/add must round before the final scale
  - a numerically “better” compensated helper is not DLL-faithful here
- Keep the new edited-export trace and verification tooling available:
  - [`tools/trace_default_large_export_sample.py`](../tools/trace_default_large_export_sample.py) now accepts slider overrides
  - [`tools/verify_native_reference_corpus.py`](../tools/verify_native_reference_corpus.py) now verifies the full MDSC local corpus end to end
- The next truly new native milestone is no longer slider or default export fidelity.
  - focus next on preview parity, on replacing the GUI/export front end’s Wine dependency with the native core, or on extending the same trace discipline to color-balance-edited reference cases if more samples appear
