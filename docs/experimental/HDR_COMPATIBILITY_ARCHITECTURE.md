# HDR Compatibility Architecture

## Status and Scope

**2026-10-09 update:** Current configuration follows the [manual-chain requirements](todos/20261009-hdr-manual-chain-adjustment-TODO.md) and [implementation handoff](design/20261009-hdr-manual-chain-implementation.md). Users manually assemble the independent HDR→SDR, SDR→HDR and RTX HDR components. The group UI, automatic planning, generated ownership and method-switching API have been removed. Each node retains its explicit color domain and pair source. Only migrated trial chains may retain internal capture intent when inference alone would change their execution contract. The profile switch below is retired and cannot enable HDR; the remaining sections preserve protocol research rather than describe the current UI.

This document is the design and implementation baseline for a self-maintained HDR-compatible Magpie fork. It records decisions reached from capture experiments and protocol research, together with the currently implemented generic work surface. Existing SDR behavior remains the compatibility baseline and remains semantically unchanged while HDR compatibility is disabled.

The evidence record is [HDR_PROTOCOL_EVIDENCE.md](HDR_PROTOCOL_EVIDENCE.md) and its machine-readable companion [HDR_PROTOCOL_EVIDENCE.json](HDR_PROTOCOL_EVIDENCE.json).

## User Configuration Switch

HDR compatibility is controlled by one profile-level option in the user's normal/general configuration page. The persisted configuration key is:

```json
{
  "enableHdrCompatibility": false
}
```

The runtime representation is an independent `ScalingFlags::EnableHdrCompatibility` bit with an `IsHdrCompatibilityEnabled()` accessor. This option controls the global capture-to-presentation HDR path; it is not a capture-method choice and it is not an effect-specific protocol selector.

When enabled, the selected capture method remains unchanged and the first post-capture stage decodes the source color representation, normalizes exposure, and produces the canonical HDR working surface. When disabled, capture output enters the existing SDR effect and presentation path without the HDR front end or HDR boundary adapters. Effect-specific `R8`/`R10`/`FP16` selectors remain separate settings and are evaluated only after this global switch is enabled.

## Goal

The HDR path lets every effect participate in one predictable pipeline while allowing each backend to select the least lossy verified protocol it actually accepts.

```text
HDR capture
  -> decode and exposure normalization
  -> canonical HDR working surface
  -> per-effect protocol adapter
  -> existing effect backend
  -> per-effect protocol adapter
  -> canonical HDR working surface
  -> presentation compatibility encoding
```

The canonical inter-effect storage format is:

```text
DXGI_FORMAT_R16G16B16A16_FLOAT
```

`R16G16B16A16_FLOAT` defines storage precision only. It does not define color meaning. Every HDR frame therefore carries a color description.

```cpp
struct ColorDescription {
    DXGI_COLOR_SPACE_TYPE dxgiColorSpace;
    ColorPrimaries primaries;
    TransferFunction transfer;
    ColorRange range;
    float referenceWhiteNits;
    float displayPeakNits;
    bool isSceneReferred;
    bool isPreExposed;
    bool isInferred;
    float preExposure;
    HdrMetadata metadata;
};

struct HdrFrame {
    ID3D11Texture2D* texture; // R16G16B16A16_FLOAT in canonical stages
    ColorDescription color;
};
```

The working surface normally contains linear RGB in preserved source primaries. Primary conversion is explicit when a backend requires it. Texture format alone never proves Rec.709, Rec.2020, scRGB, PQ, or HLG semantics.

## Non-Goals

- Do not rewrite the existing SDR effect chain while HDR compatibility is disabled.
- Do not force every native backend, HLSL effect, or model to process FP16 internally. FP16 is the canonical inter-effect work surface; an effect boundary may select R8, R10, or FP16 when that route is declared and verified.
- Do not claim HDR support because a shader happens to compile with a floating-point texture.
- Do not replace the user's capture-method selection with a separate HDR-only capture method.
- Do not label a lossy SDR round trip as native HDR preservation.

## User-Facing Control

Add one global general setting:

```text
HDR compatibility: Off / On
```

`Off` retains current capture, effect, backend handoff, and presentation behavior. `On` enables format-aware capture normalization and per-effect adapters. Effects expose a protocol selector only when more than one usable path has evidence; single-path effects remain selector-free.

The current generic boundary implementation creates an effect-local input/output surface in the format declared by the existing effect description, performs the selected adapter operation around the effect, and returns a canonical FP16 surface to the next effect. Routes with no concrete evidence use the structured SDR-compatible fallback and emit diagnostics. When HDR compatibility is enabled, the shared publication ring and presenter use canonical FP16/scRGB surfaces, so the terminal path retains HDR values through frontend composition. When HDR compatibility is disabled, the existing R8 publication and presenter path remains active.

## Capture Contract

Every existing capture method retains its selection and window-acquisition behavior. HDR handling begins after it produces a texture.

```text
selected capture method
  -> captured texture + source DXGI format + source color-space metadata
  -> HDR decode / source transfer handling / exposure normalization
  -> canonical RGBA16F HdrFrame
```

The capture stage preserves source format and DXGI color-space data, decodes known PQ/HLG/scRGB/SDR transfers, preserves available primaries and metadata, and produces a visually correct HDR frame before the first effect. This prevents HDR code values from being sampled as SDR values and clipping highlights before any effect executes.

For Graphics Capture, HDR mode requests an `R16G16B16A16_FLOAT` frame pool and treats the returned surface as linear scRGB. The other existing capture methods retain their established 8-bit BGRA capture surfaces; those surfaces are interpreted as inferred sRGB display-referred values because the capture operation has already selected that representation. Display PQ metadata supplies peak-luminance normalization and does not redefine the stored 8-bit code values.

`R10G10B10A2_UNORM` alone is not an HDR declaration. The associated color-space contract determines whether it is HDR10/PQ or another representation.

## Canonical Effect Boundary

Every non-terminal effect follows this shape while HDR compatibility is enabled:

```text
canonical RGBA16F HdrFrame
  -> adapter input conversion
  -> backend-specific input texture(s)
  -> existing HLSL/native effect
  -> backend-specific output texture(s)
  -> adapter output conversion
  -> canonical RGBA16F HdrFrame
```

The backend only sees resources allowed by its protocol. The rest of the pipeline only sees the canonical handoff surface. This isolates texture allocation, format conversion, color conversion, normalizers, alpha policy, and metadata handling from the effect implementation.

```cpp
enum class EffectColorProfile {
    DirectFP16,
    BoundedHDR,
    SDRCompatible,
    Unknown,
};

struct EffectColorProtocol {
    std::string id;
    EffectColorProfile profile;
    ColorSurfaceContract input;
    ColorSurfaceContract output;
    EffectColorAdapter adapter;
    EvidenceLevel evidence;
    uint8_t qualityRank;
    uint8_t performanceRank;
    bool userSelectable;
    bool isPresentationTerminal;
};
```

`isPresentationTerminal` is topology information, not a fifth color profile. Frame-generation paths often produce swap-chain frames and end a render branch instead of returning a normal inter-effect texture.

## Adapter Profiles

### Protocol selection at the effect boundary

The canonical work surface and the backend protocol are separate concepts. Every HDR-compatible chain hands effects a canonical `R16G16B16A16_FLOAT` frame between adapters, while each concrete effect may expose more than one backend protocol option when evidence supports it.

Each option records:

```text
effectId
optionId
inputFormat
outputFormat
inputColorModel
outputColorModel
range
transferFunction
alphaMode
evidenceLevel
hdrNative
adapterProfile
```

`hdrNative` means that the selected representation carries the effect's HDR input semantics directly. It does not mean that every format accepted by the backend is HDR-native. A floating-point option may be HDR-native, an `R10G10B10A2_UNORM` option may be a bounded HDR10/PQ route, and an `R8G8B8A8_UNORM` option normally requires an SDR-compatible HDR adapter.

The selector is shown only when the concrete effect has more than one reliable protocol option. The default order in HDR mode is: verified HDR-native FP16, verified HDR-native R10/HDR10, verified bounded floating-point, then SDR-compatible R8. In SDR mode the existing default and existing path remain unchanged.

### DirectFP16

Use this profile when the upstream API or a verified reference path accepts a suitable linear floating-point HDR resource.

```text
RGBA16F canonical frame
  -> required primaries / transfer conversion only
  -> FP16-capable backend
  -> required output conversion only
  -> RGBA16F canonical frame
```

No SDR tone mapping or U8 quantization belongs in this path. Typical candidates are DLSS SR, FSR2, FSR3 SR, FSR4, NIS, and XeSS SR. CAS has a reference FP16 path and remains conditional until local validation confirms the exact Magpie backend path.

### BoundedHDR

Use this profile when a backend accepts a bounded numeric domain, needs a display-specific HDR representation, or has an experimentally validated HDR normalizer.

```text
RGBA16F canonical frame
  -> reversible bounded-domain encoding E()
  -> backend
  -> inverse-domain decoding E^-1()
  -> RGBA16F canonical frame
```

The encoding stores the curve identifier, reference white, normalization scale, peak/headroom, source transfer assumption, and inverse curve. A generic implementation must not hard-code one scale for all effects.

DLSSNR uses this profile based on local experiments:

```text
HDR -> bounded HDR normalization -> DLSSNR -> inverse normalization -> HDR
```

The normalizer preserves the tested HDR luminance relationships while placing model input in its bounded domain. It does not imply that DLSSNR has an officially published native HDR texture contract.

### SDRCompatible

Use this profile when the documented backend interface is SDR, UNORM, or U8-only.

```text
RGBA16F canonical frame
  -> primary conversion if required
  -> HDR-to-SDR appearance mapping
  -> SDR transfer encoding
  -> quantize to backend format
  -> backend
  -> dequantize / SDR decode
  -> SDR-to-HDR reconstruction policy
  -> RGBA16F canonical frame
```

This path is compatibility-oriented. The SDR appearance mapping is not mathematically lossless, so output reconstruction is an approximation. The adapter keeps the color transform, tone-mapping function, quantizer, and reconstruction policy together as one named protocol.

For U8 RGBA/BGRA backends, channel encoding normally is:

```text
u8 = round(saturate(sdrEncoded) * 255)
```

RTX Video VSR and RTX Video Denoiser use interleaved BGRA or RGBA U8 GPU buffers. The `0..255` domain is a storage-code boundary, not a linear HDR luminance domain.

### Unknown

Use this profile when source evidence does not establish an upstream mapping or a texture/color contract. HDR mode selects an adapter only after a local test. The catalog retains the evidence gap so a later experiment can promote the entry deliberately.

## Presentation-Terminal Effects

Frame generation often binds to backbuffer and swap-chain resources. Model it as a terminal branch:

```text
canonical RGBA16F HdrFrame
  -> presentation-specific encoder
  -> frame-generation backend
  -> proxy swap chain / presenter
```

XeSS FG has the clearest known contract:

```text
RGBA16F canonical HDR
  -> PQ / BT.2100 encoding
  -> R10G10B10A2_UNORM
  -> XeSS FG
  -> proxy swap chain presentation
```

XeSS FG requires HDR10 / BT.2100 with `R10G10B10A2_UNORM` for its documented HDR path and does not accept FP16 HDR or scRGB. The backbuffer, HUD-less texture, and UI texture agree on format, dimensions, and color space. The renderer prevents normal post-effects from being scheduled after a presentation-terminal backend unless a verified re-capture path exists.

## Resource and Synchronization Model

The HDR path allocates resources from declared surface contracts instead of a global R8 assumption.

1. Keep one canonical FP16 texture pool per active render size.
2. Reuse canonical ping-pong surfaces for ordinary chains where dependencies permit.
3. Allocate protocol-specific scratch textures only around the effect that needs them.
4. Cache adapter pipelines by source contract, destination contract, dimensions, and shader parameters.
5. Preserve SRV/UAV/RTV state transitions at adapter boundaries so native effects keep their resource-state expectations.
6. Keep alpha semantics explicit. Effects that discard alpha write a defined value, normally `1.0`, before returning to the canonical chain.
7. Keep HDR metadata separate from texture metadata. A copy or shared handle does not preserve semantic color state on its own.

Performance policy: FP16 stays at effect boundaries, while every backend uses the least lossy verified internal format it accepts. Neural filters and frame generation dominate the cost in most cases. Adapter allocations and conversions still need pooling and fusion because they occur every frame.

## Effect Selection and Configuration

Each descriptor declares candidate protocols, evidence level, input/output contract, auxiliary resource needs, terminal state, and adapter parameters.

```text
Effect name
  - Default protocol
  - Candidate protocol list
  - Evidence level
  - Input/output format contract
  - Required auxiliary resources
  - Presentation-terminal flag
  - Adapter parameters
```

Examples:

- XeSS SR defaults to DirectFP16; a lower-bandwidth `R11G11B10_FLOAT` internal option belongs behind later verification and only when alpha is irrelevant.
- RTX Video VSR exposes an SDR/U8 path until a higher-precision public contract exists.
- DLSSNR exposes its tested bounded-HDR normalizer parameters after the initial defaults are stable.
- XeSS FG is bound to its HDR10 `R10G10B10A2_UNORM` terminal protocol and has no FP16 selector.

## Validation Plan

The user performs visual/game tests. The implementation records diagnostics sufficient to identify a wrong adapter choice.

### Capture validation

- Compare no-effect HDR output with the source window.
- Confirm highlight detail before the first effect.
- Confirm SDR windows remain unchanged while the global option is off.
- Log source DXGI format, DXGI color space, decode path, and canonical color description.

### Per-effect validation

- Verify input and output resources match the selected profile.
- Test dark detail, saturated red, UI white, specular highlights, and low-light gradients.
- Check alpha behavior on overlays and composition paths.
- Record GPU time for adapter input, backend, and adapter output separately.
- For SDR-compatible effects, compare against a no-effect HDR reference and record reconstruction loss.

### Regression validation

- HDR option off: unchanged SDR pipeline.
- HDR option on with no effects: correct capture and presentation.
- One effect at a time: contract and visual validation.
- Mixed chain: canonical FP16 handoff between non-terminal effects.
- Terminal frame generation: no invalid post-effect after presentation handoff.

## Delivery Order

1. Add format-aware capture normalization and `HdrFrame` while retaining the disabled SDR branch.
2. Add adapter infrastructure and diagnostics without changing individual effect backends.
3. Implement DLSSNR as the bounded-HDR vertical slice.
4. Add verified DirectFP16 effects: NIS, XeSS SR, DLSS SR, and FSR2/FSR3 SR/FSR4 where auxiliary inputs are valid.
5. Add SDR-compatible adapters: RTX Video VSR, RTX Video Denoiser, FSR1, xBRZ, NVIDIA Optical Flow, and SMAA.
6. Add terminal frame generation: XeSS FG, then FSR3 FG and DLSS FG after their external temporal contracts are resolved.
7. Promote conditional and unknown entries one by one through local texture-contract tests.

## Decision Record

- Canonical handoff: `DXGI_FORMAT_R16G16B16A16_FLOAT` plus explicit `ColorDescription`.
- SDR behavior: preserved while HDR compatibility is off.
- Capture: every selected capture method feeds one HDR normalization stage.
- Backend precision: selected per effect protocol, never globally forced.
- DLSSNR: bounded-HDR normalization with inverse normalization, based on local experiments.
- RTX Video VSR/Denoiser: SDR/U8 adapters with explicit `0..255` quantization.
- XeSS FG: HDR10 `R10G10B10A2_UNORM` terminal protocol outside the ordinary FP16 chain.
- Evidence policy: public API contracts, reference implementations, GitHub experiments, and local experiments remain separate evidence levels.
