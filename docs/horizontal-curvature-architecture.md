# Horizontal Curvature — Architecture

Status: implementation in progress. Deployed build is `846a2f4b`/feature `0.1.0`;
the pending lab build is feature `0.2.0` with the VS `b6` fix and runtime
fallback matrix.
This document describes what is actually implemented, not the original design.
It is authoritative over the conceptual details in `design.md`.

## 1. Purpose and scope

Bend distant terrain LOD, object LOD, and tree LOD downward to suggest a
planet-sized horizon. Full-detail cells, actors, physics, local raycasts, water,
and the skybox are intentionally out of scope for the first version.

The effect is visual only. It does not move collision, AI, or world-space data.

## 2. Runtime topology

- Game: Skyrim SE 1.5.97 under Proton on CachyOS.
- Mod manager: MO2 instance `<MO2_INSTANCE>`,
  profile `horizon`.
- Active game: `<GAME_MOUNT>/game/SkyrimSE-1.5.97`.
- Active CS mod: `<MO2_INSTANCE>/mods/Community Shaders Dev`.
- CS settings: MO2 overwrite
  `<MO2_INSTANCE>/overwrite/SKSE/Plugins/CommunityShaders/SettingsUser.json`.
- Shader cache: MO2 overwrite
  `<MO2_INSTANCE>/overwrite/ShaderCache`.

The CI build is produced on a GitHub-hosted Windows runner. The local machine
only downloads/deploys the AIO and runs the game.

## 3. Data flow

```
HorizontalCurvature::Settings
        |
        | GetCommonBufferData()
        v
HorizontalCurvature::PerFrame  (64 bytes, 16-byte aligned)
        |
        | CS FeatureBuffer -> FeatureData
        v
SharedData::horizontalCurvatureSettings  : register(b6)
        |
        +--> Lighting.hlsl VS/PS
        +--> DistantTree.hlsl VS/PS
```

### Why `b6` and not `b12`

`Lighting.hlsl` and `DistantTree.hlsl` already use register bank `b12` for
per-frame data (`VS_PerFrame` / `CameraPosAdjust`). A second `b12` declaration
caused the CI shader failure:

```text
X4578: cbuffer bank 12 used more than once
```

The feature settings therefore live in the shared `FeatureData` buffer at `b6`.
The shaders read the absolute camera Z from their existing per-frame bank and
pass it into `ComputeDrop`; the HLSL helper does not declare another cbuffer.

### Vertex-stage binding bug

`FeatureData` was originally bound only to the pixel and compute stages
(`PSSetConstantBuffers` / `CSSetConstantBuffers`), not the vertex stage. The
Lighting and DistantTree vertex shaders therefore read an unbound `b6` and saw
`Enable = 0`, so `ComputeDrop` returned zero. The pixel debug tint still worked
because `b6` was bound there.

The fix in `src/VanillaBinding.cpp` is:

```cpp
context->VSSetConstantBuffers(6, 1, &buffers[2]);
```

This is pending a new DLL build. Before it lands, any VS-side HC settings test
is expected to show no geometry movement while the debug tint still responds.

### C++ layout

`src/Features/HorizontalCurvature.h`:

```cpp
struct alignas(16) PerFrame
{
    float CurvatureStrength;
    float InnerRadiusCutoff;
    float ElevationScaleFactor;
    float SeaLevelOffset;
    uint32_t Enable;
    uint32_t DebugColor;
    float DebugHue;
    float pad0;
    uint32_t CurvatureMode;
    uint32_t TreeMode;
    uint32_t DebugVertexMode;
    uint32_t Flags;
    float DebugVertexAmount;
    float CutoffSoftness;
    float Direction;
    float pad2;
};
```

`Flags` bits: `1` apply to all Lighting (test), `2` terrain, `4` objects,
`8` trees. The matching HLSL struct is in
`package/Shaders/Common/SharedData.hlsli` under
`HorizontalCurvatureSettings`. Keep field order and padding identical.

## 4. Shader integration

### Lighting (terrain and object LOD)

File: `package/Shaders/Lighting.hlsl`.

- Include guard: `#if defined(HORIZONTAL_CURVATURE)`.
- Vertex path: non-skinned LOD permutations (`LODLANDNOISE`, `LODLANDSCAPE`,
  `LODOBJECTS`, `LODOBJECTSHD`). `ApplyToAllLighting` additionally enables the
  non-LOD branches for testing.
- Mutate both `worldPosition.z` and `previousWorldPosition.z`, then recompute
  the clip position:

```hlsl
worldPosition.z -= HorizontalCurvature::ComputeDrop(worldPosition.xy, CameraPosAdjust.z);
previousWorldPosition.z -= HorizontalCurvature::ComputeDrop(previousWorldPosition.xy, CameraPosAdjust.z);
viewPos = mul(ViewProj, worldPosition);
```

- In `SphericalFull` mode, camera-relative XY is also multiplied by
  `sin(phi)/phi`; `TreeMode = BaseAnchor` skips that for trees.
- The engine's own terrain-LOD z nudge runs after this block:
  `vsout.Position.z += min(1, 1e-4 * max(0, viewPos.z - 70000)) * 0.5;`
  Object and tree LOD do not receive that nudge automatically.
- Pixel debug tint is guarded by `!defined(RENDER_DEPTH)` and by the runtime
  `Enable` and `DebugColor` settings. Terrain hue offset is `0.0`, object `0.33`.

### DistantTree (tree LOD)

File: `package/Shaders/DistantTree.hlsl`.

- Include guard: `#if defined(HORIZONTAL_CURVATURE)`.
- Vertex path runs before the `RENDER_DEPTH` split. The depth permutation now
  applies the same displacement; the color permutation still carries
  `WorldPosition`/`PreviousWorldPosition`.
- Mutate `worldPosition` and `previousWorldPosition`, then recompute with
  `FrameBuffer::CameraViewProj`.
- `TreeMode = BaseAnchor` samples the drop at the instance origin and translates
  the whole billboard; per-vertex mode warps each vertex.
- Pixel debug tint is guarded by `!defined(RENDER_DEPTH)` and runtime settings.
  Tree hue offset is `0.66`.

### Position space

LOD world positions are already camera-relative. Horizontal distance is therefore
`length(worldPosition.xy)`; do not subtract the camera XY again. The absolute
camera Z comes from the per-frame bank.

This is deliberately **not** screen space or NDC. A clip-space shift is useful
only as a diagnostic and changes with camera rotation/FOV.

### Runtime lab matrix

All of these are b6 uniforms and can be changed in-game without recompiling:

| Control | Values | Purpose |
| --- | --- | --- |
| `CurvatureMode` | Parabolic, Spherical vertical, Spherical full | Compare quadratic drop against a circular arc, with/without XZ contraction |
| `TreeMode` | Per-vertex, Base anchor | Test billboard shear vs rigid tree translation |
| `ApplyToAllLighting` | on/off | Bypass the LOD guard for a definitive VS-path/permutation test |
| `ApplyTerrain` | on/off | Isolate LOD landscape |
| `ApplyObjects` | on/off | Isolate LOD objects |
| `ApplyTrees` | on/off | Isolate DistantTree |
| `DebugVertexMode` | none, clip X, clip Y, world Z | Prove the VS path independently of curvature math |
| Old positive control | button: clip-X `0.25 * w` | Re-run the historical horizontal shift that visibly moved LOD |
| `DebugHue = 1.0` | treats Lighting passes | Green/cyan = LOD, blue = full-detail landscape, orange = skinned, yellow = other; deferred darkened; grey = no color PS |
| Per-draw VS `b6` bind | always | Ensures the depth prepass sees the same HC settings as the color pass |
| `CutoffSoftness` | game units | Smooth the hard cutoff to test seams |
| `Raise` / `Direction` | on/off | Invert vertical displacement: raise instead of lower, for z-fighting diagnostics |
| `CurvatureStrength`, `InnerRadiusCutoff`, `ElevationScaleFactor`, `SeaLevelOffset` | sliders | Existing tuning/debug |

The current shader still applies the HC VS block only to non-skinned Lighting
permutations by default. `ApplyToAllLighting` is test-only and will move
full-detail geometry too.

## 5. Curvature math

`features/HorizontalCurvature/Shaders/HorizontalCurvature/HorizontalCurvature.hlsli`:

```hlsl
float cameraHeight = max(0, cameraZ - SeaLevelOffset);
float dynamicCutoff = max(0, InnerRadiusCutoff) +
                      max(0, ElevationScaleFactor) * sqrt(cameraHeight);
float d = max(0, length(cameraRelativeXY) - dynamicCutoff);
if (CutoffSoftness > 0)
    d *= smoothstep(0, 1, saturate(d / CutoffSoftness));

// Parabolic (default)
drop = max(0, CurvatureStrength) * d * d;
```

- `CurvatureStrength` is the quadratic coefficient `k`.
- `InnerRadiusCutoff` is a flat deadzone in game units.
- `ElevationScaleFactor` extends the deadzone with camera height.
- `SeaLevelOffset` sets the baseline for the elevation term.
- `CutoffSoftness` smooths the deadzone edge.
- `SphericalVertical` uses `R = 1/(2k)` and `drop = R - sqrt(R^2 - d^2)`.
- `SphericalFull` also multiplies camera-relative XY by `sin(phi)/phi`.
- The default drop is along world Z only; use the sphere modes for XZ
  contraction.

## 6. Settings and UI

`src/Features/HorizontalCurvature.cpp` exposes:

- `Enable` — runtime switch; no shader recompile.
- `DebugColor`, `DebugHue` — debug tint by computed drop.
- `CurvatureStrength`, `InnerRadiusCutoff`, `ElevationScaleFactor`,
  `SeaLevelOffset`, `CutoffSoftness`.
- `CurvatureMode`, `TreeMode`, `DebugVertexMode`, `DebugVertexAmount`.
- `ApplyToAllLighting`, `ApplyTerrain`, `ApplyObjects`, `ApplyTrees`.

Defaults: `k = 5e-6`, cutoff `1000`, elevation `0`, sea level `0`,
debug color on, parabolic mode, per-vertex trees, all LOD types on,
softness `0`, vertex probe off. Testing uses cutoff `0` to make the drop
obvious.

Feature metadata:

- Short name: `HorizontalCurvature`.
- Shader define: `HORIZONTAL_CURVATURE`.
- Category: landscape/textures.
- Stage: Alpha CORE, so it is disabled by default until enabled.
- `HasShaderDefine` returns true for `Lighting` and `DistantTree`.

## 7. Feature enablement has two layers

1. **Boot enablement** (`Disable at Boot` in `SettingsUser.json`). If true, the
   feature does not load, `HORIZONTAL_CURVATURE` is not defined, and the menu
   entry may not be available.
2. **Runtime `Enable` setting** inside a loaded feature. This only changes the
   `b6` value; it does not add/remove the shader define.

The `#error` diagnostics only fire when the feature is enabled at boot.

## 8. Build, deploy, and shader cache

- CI: `.github/workflows/build-fork.yml`; `PR` preset for C++/AIO, full shader
  validation matrix.
- Successful run: `41556965617`; commit `846a2f4b`; build hash in
  `CommunityShaders.log` is `describe: 846a2f4`.
- Deploy: `tools/deploy-aio.sh <AIO.7z>`.
- **Invalidate only what changed** for fast iteration. `Enable Disk Cache` and
  `Skip Unchanged Shaders` are on; they rebuild a shader only when its top-level
  `.hlsl` mtime is newer than the cached `.bin`. `Skip Unchanged Shaders` does
  not check `.hlsli` includes, so after editing an include either touch the
  including `.hlsl` files or delete their cache directories.

```bash
cache="<MO2_INSTANCE>/overwrite/ShaderCache"
rm -rf "$cache/Lighting" "$cache/DistantTree"
```

`Use FileWatcher` can also hot-reload changed shaders and track include
dependencies, but it is off by default. Use a full cache clear only for shared
includes (for example `SharedData.hlsli`), feature-set changes, or when
selective invalidation still loads a stale shader.

CS may log `Using disk cache` and load old compiled shaders even when the `.hlsl`
files have changed. This was the main false negative in development: the pixel
debug tint came from an older build while live vertex edits appeared to do
nothing.

- The feature `.ini` version does not identify the shader build. Use the
  `describe:` hash for the DLL and file contents/mtime for live shader edits.

### Fast test profile

- MO2 profile: `HorizontalCurvatureTest`, a copy of `horizon` (same minimal mod
  list).
- Settings backup: `SettingsUser.json.bak.hctest`; it disables every optional CS
  feature except `HorizontalCurvature`, sets Post Processing Mode to `Vanilla`,
  and keeps the test values (cutoff `0`, k `5e-6`, debug tint on).
- With no optional features, CS only replaces the necessary core shaders and the
  HC permutations. This is the closest useful equivalent of the CS FAQ's
  "minimal setup": only CS and the feature under test.
- Restore normal play with `SettingsUser.json.bak.fullfeatures` and profile
  `horizon`. CS settings live in the MO2 overwrite, not in the MO2 profile, so
  the settings backup must be switched separately.
- Instance launchers switch both together:
  - `launch-hctest.bat` -> reduced settings + `HorizontalCurvatureTest`.
  - `launch-full.bat` -> full settings + `horizon`.
- `SettingsUser.json.bak.hctest-pipeline` is a fallback if the all-off `Vanilla`
  profile looks wrong: it keeps `PostProcessing`, `LinearLighting`, and
  `HDRDisplay` and uses `PostProcessing` mode, while still disabling the heavy
  optional features.
- The first launch after a feature-set change may still need a full compile; do
  not clear the cache again after that. Subsequent shader edits use selective
  invalidation.

## 9. Confirmed diagnostics

- The user-visible split (debug tint and sliders work, geometry does not) is
  explained by the missing VS `b6` binding above: the pixel stage sees settings,
  the vertex stage sees zero.
- A second binding issue affected the depth prepass: `Renderer::ResetState` binds
  feature data once, but some prepass draws reset VS constants afterwards. The
  colored Lighting passes moved while the depth prepass stayed at the old
  positions, leaving a grey static layer. Shader blocking isolated it to the
  vertex-only `Lighting:Vertex:HORIZONTAL_CURVATURE VC` pass; no Pixel shader
  removed it. `BSShader::BeginTechnique` now binds `b6` after the engine returns,
  which is later than the original `SetVertexShader` binding.
- A temporary `#error` at the top of the feature blocks proved:
  - `HORIZONTAL_CURVATURE` is defined in **Lighting vertex** shaders.
  - It is defined in **DistantTree vertex and pixel** shaders.
- A temporary **clip-space horizontal X shift** visibly moved LOD geometry.
  That proved the vertex block executes, but it was screen-space and rotated
  with the camera. It was not curvature and was removed.
- The corresponding vertical clip-space Y and world-Z diagnostic tests were not
  conclusive because the feature was disabled at boot, the world was not
  loaded, and/or the shader cache was stale. Do not cite them as evidence that
  vertex displacement is unavailable.
- The current proper shaders have no hardcoded diagnostics.

## 10. Known limitations and risks

- **Stale cache**: the highest-cost source of false test results. Clear it.
- **Permutation gating**: only specific Lighting LOD defines are modified.
  Removing the LOD guard and using a runtime cutoff is the way to test all
  Lighting geometry, at extra vertex cost.
- **Engine terrain nudge**: terrain LOD already receives an engine Z nudge;
  terrain and object/tree LOD can decouple even before this feature.
- **Coarse LOD**: large terrain triangles cannot show a smooth curve. Higher
  resolution xLODGen terrain LOD or tessellation may be required.
- **Trees**: per-vertex warping can shear billboards. Anchor-based rigid
  translation is the preferred future improvement.
- **Culling**: GPU displacement is not reflected in CPU AABB culling; extreme
  drops can pop at screen edges.
- **Motion vectors / TAA**: previous positions are warped too; test ghosting.
- **Full-detail leakage**: the LOD guard is what keeps gameplay geometry
  untouched; keep it for shipping.
- **Feature boot state**: if the menu entry is missing, check
  `Disable at Boot` in `SettingsUser.json` before suspecting the build.

## 11. Recommended next step: a single “lab” build

Compile all test variants once and select at runtime with `b6` fields:

- curvature mode: parabolic / vertical spherical
- tree warp: per-vertex / base anchor
- debug view: off / hue / per-type / strength / distance
- apply to terrain / objects / trees
- `ApplyToAllLighting` (with runtime cutoff) for permutation tests

This avoids repeated shader recompiles. Any change that adds/removes a shader
`#define` or edits HLSL still requires a cache clear and recompile.

## 12. File map

| Path | Role |
| --- | --- |
| `src/Features/HorizontalCurvature.h` | settings, `PerFrame`, feature metadata |
| `src/Features/HorizontalCurvature.cpp` | settings load/save, UI, buffer fill |
| `features/HorizontalCurvature/Shaders/HorizontalCurvature/HorizontalCurvature.hlsli` | drop math, HSV debug color |
| `package/Shaders/Common/SharedData.hlsli` | `b6` FeatureData entry |
| `package/Shaders/Lighting.hlsl` | terrain/object LOD VS/PS hooks |
| `package/Shaders/DistantTree.hlsl` | tree LOD VS/PS hooks |
| `.github/workflows/build-fork.yml` | CI build and validation |
| `tools/deploy-aio.sh` | AIO deployment |
| `design.md` | original design intent |
| `PROGRESS.md`, `RESUME.md` | live status and test plan |

## 13. Related documents

- `design.md` — original mathematical/design blueprint.
- `PROGRESS.md` — current status and research.
- `RESUME.md` — full resume/test plan for the next session.
- `docs/horizontal-curvature-investigation.md` — chronological in-game reports, shader-blocking keys, and test conditions.
