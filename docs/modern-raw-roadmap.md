# Modern Raw Roadmap

## Goal

Add a modern-only raw pipeline that is free to improve on PhotoRun's original image processing choices while keeping the legacy path byte-for-byte faithful to the original `DsGraph.dll`.

This roadmap is about the sensor-decoding boundary, not just later RGB post-processing.

## Non-Negotiable Boundary

- `legacy` stays frozen as the compatibility path.
- Any new raw calibration, demosaicing, denoise, color, or DNG work must live behind the modern pipeline selection.
- DLL-backed parity tests remain the gate for the legacy pipeline.

## What We Know About The DAT Payload

- The `.DAT` file is always `0x20000` bytes.
- The first `0x1f600` bytes are the image payload.
- The last `13` bytes are trailer metadata.
- The current native and DLL-faithful source stage consumes the payload as a normal-mode raw buffer with:
  - stride `512`
  - active width `504`
  - active rows `244`
- The full raw payload geometry implied by `0x1f600` bytes at `512` stride is `512 x 251`, which suggests there are additional non-active rows beyond the source stage's `244` active rows.
- Early corpus probes with the modern raw-stats tool suggest those inactive margins are not uniform:
  - the right margin is `8` columns wide, but on real sample files the trailing `6` columns are often near-zero padding
  - the bottom margin is `7` rows tall, but the trailing `5` rows can be near-zero while only the first few carry non-zero structure
  - that means the inactive region likely mixes useful overscan-like data with plain padding, so modern calibration should not lump the whole margin together
- Follow-up probes suggest the leading inactive lines are not all "dark reference" either:
  - the first right-margin column is strongly correlated with the last active even-parity column, which looks more like edge smear or deferred readout structure than optical black
  - the first two bottom rows are strongly correlated with the last two active rows in alternating parity, which again points to image-correlated structure rather than pure black reference
  - the remaining middle lines can be very low amplitude before the hard-zero tail begins, so the modern path should distinguish:
    - structured smear
    - low-signal dark-reference candidates
    - hard-zero padding
  - low-signal reference lines are not automatically good row/column-bias references; very sparse lines can still be useful for black-level estimation while being too thin to trust for column-noise modeling
- The downstream large export path later reconstructs a `504 x 378` image, but the earliest raw stage starts from `504 x 244` active samples.

## What The Sensor Datasheet Now Suggests

The LC9997M datasheet is a much stronger clue than the old Bayer-like inference:

- the sensor uses `Cy-G-Ye-W` mosaic complementary color filters
- the repeating `2 x 2` site order is `A C / D E`
- the datasheet's own sensitivity formulas derive color signals from that complementary tile:
  - `R = E - D`
  - `B = A - C`
  - `G = (C + D) / 2`

That means the modern path should no longer treat the DAT payload as "probably Bayer." The more faithful modern starting point is a complementary-color sensor model with a locked `A/C/D/E` site pattern, while the legacy DLL-faithful path remains untouched.

## Where The Legacy Decoder Likely Leaves Quality On The Table

### 1. Black level and masked-pixel use

The legacy path currently treats the first `504 x 244` active samples as the useful source region and mostly ignores the fact that the raw stride is `512`.

That means we should explicitly test whether the extra columns or border behavior carry:

- optical black reference
- row bias reference
- masked calibration pixels
- fixed-pattern noise clues

If they do, modern decoding should use them for black subtraction and row/column correction before any demosaic step.

### 2. Fixed-pattern and row noise

The DLL has several hand-tuned thresholds and row/column parity rules, which is typical of an era when raw processing often baked sensor cleanup into ad hoc stages.

A modern path should explicitly model:

- row bias
- column bias
- hot / dead pixels
- salt-and-pepper outliers
- low-frequency pattern noise

before demosaicing.

### 3. Gain and white level

The legacy path computes source gain from an integer-averaged region and then truncates the writeback. That is correct for parity with PhotoRun, but it is not a modern raw-calibration strategy.

Modern decoding should independently estimate:

- black level
- white / clip level
- usable highlight headroom
- per-channel gain or white balance anchors

from the raw mosaic itself.

### 4. Demosaicing

The legacy source stage is effectively a custom heuristic demosaic-plus-repair block. It is clever, but it is still a 1990s hand-tuned reconstruction method.

For the modern pipeline, the main opportunity is to replace that with a better raw reconstruction strategy.

## Recommended Modern Sensor Pipeline

### Phase 1: Build a linear sensor frame

Create a modern-only internal object such as:

```cpp
struct SensorFrame {
    int full_width;
    int full_height;
    int active_x;
    int active_y;
    int active_width;
    int active_height;
    int stride;
    int cfa_repeat_x;
    int cfa_repeat_y;
    std::array<int, 4> cfa_pattern;
    float black_level[4];
    float white_level[4];
    std::vector<uint16_t> mosaic;
};
```

This should be the new modern boundary. The modern RGB renderer and DNG export should both start from the same `SensorFrame`.

### Phase 2: Calibrate the raw mosaic before demosaic

Recommended order:

1. subtract black level
2. detect and repair bad pixels
3. estimate row / column bias and remove fixed-pattern noise
4. normalize to a stable white level
5. optionally apply a very light raw-domain denoise if the noise model warrants it

### Phase 3: Use a modern demosaic strategy

Practical recommendation:

- default modern demosaic: start with a strong classical Bayer demosaic approach
- noise-aware fallback: use a more conservative algorithm for noisy files
- keep the demosaic choice pluggable per file or per pipeline preset

Good practical candidates:

- AMaZE-like detail-first demosaic for cleaner files
- LMMSE- or IGV-like demosaic for noisier files
- RCD-style detail reconstruction as another strong candidate for Bayer data

Research/experimental candidates:

- joint demosaicing + denoising methods
- self-supervised or per-image neural approaches

Recommended project stance:

- use a deterministic classical method first
- keep learned / deep methods as an optional research track, not the first shipping dependency

That gives us the biggest quality gain without making the converter fragile, GPU-bound, or model-distribution heavy.

### Phase 4: Color science after demosaic

Modern color handling should be separated from raw reconstruction.

Recommended order:

1. demosaic to linear camera RGB
2. estimate or apply white balance
3. apply a camera-to-XYZ or camera-to-sRGB transform
4. do highlight handling in linear space
5. tone-map for display output
6. apply output sharpening and final chroma cleanup

This is also where a future calibrated camera profile should plug in.

## DNG Output

Yes, DNG output makes sense for the modern pipeline.

Current status:

- first-pass DNG export is implemented as a dependency-light classic TIFF/DNG writer built from the modern `SensorFrame`
- it is good enough for inspection and early interoperability testing
- it is not yet the final word on CFA orientation, color matrices, or every optional metadata tag we may eventually want

Why:

- it preserves a reusable raw interpretation boundary
- it lets advanced users open the files in Lightroom, Adobe Camera Raw, RawTherapee, darktable, and other raw tools
- it cleanly separates "decode the weird DAT container" from "render a final RGB image"

Recommended DNG scope:

- export a linear CFA DNG from the modern `SensorFrame`
- include the active area and CFA pattern
- include black and white levels
- include as-shot neutral / white-balance metadata when available
- include a placeholder camera profile first, then improve calibration later

## Third-Party Library Recommendations

### Best fit for DNG writing

Primary recommendation:

- Adobe DNG SDK

Why:

- it is the reference SDK for reading and writing DNG
- it is intended to help applications add DNG support
- Adobe publishes the specification and a patent license for compliant implementations

Fallback:

- write DNG directly with `libtiff` once the required tags are fully pinned down

That fallback is attractive for a small dependency surface, but the Adobe SDK is the faster path if we want robust DNG support sooner.

### Best fit for raw reconstruction

Primary recommendation:

- keep DAT unpacking and sensor calibration in our own code
- implement or port a modern Bayer demosaic strategy inside the modern pipeline

Why not make LibRaw the core decoder:

- LibRaw is excellent for known camera raw formats
- our DAT container is custom, and we already understand its payload boundary better than a generic raw loader would
- we still need our own unpacking, calibration, and camera characterization layer

LibRaw is still a useful reference for raw data structures and processing flow, but not the right place to hide DAT-specific sensor interpretation.

## Recommended Research Order

### High-confidence work first

1. measure masked / inactive columns and border rows across a corpus
2. estimate black level and row/column bias from the raw payload itself
3. lock the CFA orientation and active area for the modern path
4. create a raw `SensorFrame`
5. add DNG export from `SensorFrame`
6. compare two or three classical demosaic methods on real DATs

### Experimental work second

1. test joint demosaicing-and-denoising approaches on a small corpus
2. evaluate whether per-image self-supervised methods are worth the runtime cost
3. only then consider shipping a learned path as an optional "modern-ai" preset

## Concrete Next Implementation Steps

1. Add a `modern_raw_frame.hpp/.cpp` layer that exposes:
   - full payload dimensions
   - active area
   - raw mosaic samples
   - inferred CFA pattern
   - black / white levels
   Status:
   - done for raw dimensions, active area, inactive-line classification, and black-level calibration
2. Add diagnostics that measure:
   - per-column averages
   - per-row averages
   - inactive-column distributions
   - inactive-line nonzero counts
   - inactive-line correlation with the adjacent active boundary
   - hot-pixel candidates
   Status:
   - done for per-line means, nonzero counts, and active-boundary correlation
3. Add a modern-only CLI command such as:
   - `dj1000 modern-dump-raw-stats INPUT.DAT`
   Status:
   - done
4. Split inactive lines into:
   - image-correlated structured smear
   - dark-reference candidates
   - hard-zero padding
   Status:
   - done
5. Add a `modern_sensor_frame.hpp/.cpp` boundary that stores:
   - calibrated black levels
   - estimated white levels
   - normalized linear mosaic values
   - CFA metadata for future DNG / demosaic work
   Status:
   - done
6. Add a modern-only DNG export command once `SensorFrame` is stable:
   - `dj1000 modern-export-dng INPUT.DAT OUTPUT.dng`
   Status:
   - done for a first-pass dependency-light DNG writer
7. Use the stabilized `SensorFrame` as the entry point for modern demosaic experiments.
   Status:
   - done for the first modern renderer rewrite; `modern-export-bmp` now demosaics from `SensorFrame` instead of leaning on the legacy pregeometry/large-resample chain
8. Keep all of this outside the legacy pipeline and its parity tests.

## Best Next Steps

1. Lock the red/blue CFA orientation with stronger evidence than the current provisional mapping.
2. Improve the DNG metadata:
   - measured color matrices
   - better white-level modeling
   - any missing compatibility tags that real raw processors care about
3. Compare two or three stronger demosaic strategies from the same `SensorFrame` boundary:
   - current directional green + smoothed chroma baseline
   - a stronger classical edge-aware method
   - a denoise-aware variant
4. Decide whether to keep the current dependency-light DNG writer or move to a heavier writer stack such as the Adobe SDK or `libtiff`.

## External References

- Adobe Digital Negative overview and specification resources:
  - <https://helpx.adobe.com/in/camera-raw/digital-negative.html>
- Adobe DNG SDK:
  - <https://helpx.adobe.com/security/products/dng-sdk.html>
- LibRaw API overview:
  - <https://www.libraw.org/docs/API-overview.html>
- RawTherapee demosaicing overview:
  - <https://rawpedia.rawtherapee.com/Demosaicing>
- RawTherapee noise-reduction guidance:
  - <https://rawpedia.rawtherapee.com/Noise_Reduction>
- Recent joint demosaicing / denoising references:
  - Microsoft Research, 2011: <https://www.microsoft.com/en-us/research/publication/noise-suppression-in-low-light-images-through-joint-denoising-and-demosaicing/>
  - Microsoft Research, 2014: <https://www.microsoft.com/en-us/research/publication/joint-demosaicing-and-denoising-via-learned-non-parametric-random-fields/>
  - Li et al., 2024 (Sony): <https://www.sony.com/en/SonyInfo/technology/publications/joint-demosaicing-and-denoising-with-double-deep-image-priors/index.html>
- Raw-noise modeling reference:
  - Microsoft Research, 2020: <https://www.microsoft.com/en-us/research/publication/a-physics-based-noise-formation-model-for-extreme-low-light-raw-denoising/>
