# Horizontal Curvature — Investigation Log

Status as of 2026-11-04. This log records the in-game reports, diagnostic
settings, shader keys, and conclusions. The architecture is in
`docs/horizontal-curvature-architecture.md`; the original design intent is in
`design.md`.

## Current build state

- Deployed before the latest fix: `7fb44411` (per-draw `b6` binding inside
  `SetVertexShader`).
- Latest committed fix: `0c0cdeba` — bind `b6` after
  `BSShader::BeginTechnique` returns.
- CI for `0c0cdeba`: run `41816433069` — shader validation and C++/AIO build
  both **success**. Not yet deployed at the time of this entry.
- Feature version: `0.2.0`.
- Test profile: MO2 `HorizontalCurvatureTest`.
- Test settings: `SettingsUser.json.bak.hctest`.

## Confirmed root causes, in order

### 1. Stale shader cache

Live shader edits were ignored because the game loaded old compiled shaders
from `overwrite/ShaderCache`. The log said `Using disk cache`; the deployed
files had newer code but the game ran the old `.vso/.pso`.

- Prefer deleting only `ShaderCache/Lighting` and `ShaderCache/DistantTree`.
- Full cache clear only for shared includes, feature-set changes, or when
  selective invalidation fails.
- `Skip Unchanged Shaders` only compares the top-level `.hlsl` mtime; a changed
  `.hlsli` needs the including `.hlsl` touched or the affected cache dirs
  deleted.

### 2. `FeatureData` b6 not bound to the vertex stage

- PS/CS received `FeatureData`; VS did not.
- PS debug tint and sliders worked; VS read `Enable = 0`, so `ComputeDrop`
  returned zero.
- `src/VanillaBinding.cpp` now binds b6 to VS.

### 3. Depth/prepass still unwarped

Shader blocking isolated the static grey layer:

```text
Type: Lighting
Class: Vertex
Descriptor: 0x1
Key: Lighting:Vertex:HORIZONTAL_CURVATURE VC
Cache Path: Data/ShaderCache/Lighting/1_D6A832E5.vso
```

- Blocking this VS makes the grey layer partially disappear and hides or
  inverts the lowered/debug-color element.
- **No Lighting Pixel shader removes the grey layer.** Blocking each
  `Lighting:Pixel:...` entry leaves it.
- Therefore the grey layer is a **vertex-only Lighting depth/prepass path**,
  not a Lighting color pixel pass.
- Binding b6 inside `BSShader__BeginTechnique_SetVertexShader` was too early;
  the engine resets VS constants afterwards. `0c0cdeba` binds b6 **after**
  `BSShader::BeginTechnique` returns.

## Shader-blocking observations

Colored distant mountains:

```text
Type: Lighting
Class: Pixel
Descriptor: 0xF100011
Key: Lighting:Pixel:DEFERRED DO_ALPHA_TEST HORIZONTAL_CURVATURE LODOBJECTSHD VC
Cache Path: Data/ShaderCache/Lighting/F100011_7B164BE8.pso
```

```text
Type: Lighting
Class: Pixel
Descriptor: 0xF108011
Key: Lighting:Pixel:DEFERRED DO_ALPHA_TEST HORIZONTAL_CURVATURE LODOBJECTSHD PROJECTED_UV VC
Cache Path: Data/ShaderCache/Lighting/F108011_E72D3340.pso
```

These select the color on different distant object-LOD mountains. They are not
the grey layer.

Tree LOD appears magenta in the distance during the pass-color diagnostic:

```text
Type: DistantTree
Class: Vertex
Descriptor: 0x10000
Key: DistantTree:Vertex:DO_ALPHA_TEST HORIZONTAL_CURVATURE
Cache Path: Data/ShaderCache/DistantTree/10000_4E16F74B.vso
```

`DistantTree` does not use the Lighting pass-color diagnostic; this is the
normal tree debug tint at `DebugHue = 1.0`.

## Pass-color diagnostic

Temporary shader diagnostic: when `DebugHue >= 0.99`, Lighting PS output is
replaced with a family color:

- Green — `LODLANDNOISE` / `LODLANDSCAPE`
- Cyan — `LODOBJECTS` / `LODOBJECTSHD`
- Blue — `LANDSCAPE` (full-detail landscape)
- Orange — `SKINNED`
- Yellow — other Lighting
- Deferred variants are darkened to 45%
- Grey — no Lighting color PS; depth/prepass

Report: the moving ground/LOD is yellow/blue and colored. The static remaining
layer stays grey. This is consistent with the depth/prepass conclusion.

## Latest visual diagnosis

Reports after the `e81fe677` deployment:

- The grey layer still does not move with cutoff softness.
- Changing weather (`sw 0010199f`) changes the grey layer's color.
- `ToggleZPrepassOutput` shows green distant ground, yellow outlines around
  rocks/Whiterun overlays, and basal water; most opaque objects are absent.
- `tfc` toward Whiterun reveals z-fighting between the grey/black weather layer
  and geometry that is not the moss/transparency overlay.
- Only transparent/moss-style Lighting overlays appear to be moved by HC;
  opaque walls/buildings mostly disappear from view after the displacement.
- Maximum curvature appears to move only ~32 units of visible geometry.

The consistent explanation is a depth-prepass mismatch, not a duplicate mesh:

1. The color/opaque vertices move downward with HC.
2. The depth/prepass still contains their old positions.
3. The moved opaque pixels fail the depth test and disappear.
4. Alpha-tested/transparent overlays that do not use the static depth remain
   visible and are the parts that appear to move.
5. The old depth is shaded with fog/weather, producing the grey/black layer and
   z-fighting. Weather changes its color.
6. No Pixel shader removes it; blocking the shared Lighting VS does, because it
   removes the geometry that creates the mismatch.

The ~32-unit visible movement is probably only the near/alpha overlay; the
actual opaque drop is larger but hidden behind the old depth.

Next diagnostic: hardcode a temporary depth/all-Lighting offset to prove whether
the depth pass is using the custom VS at all. If it is, the remaining fix is to
bind/warp the z-prepass; if not, disable or bypass the engine z-prepass for HC.

## Hardcoded Lighting VS control (result)

A temporary b6-independent clip-X shift was added to every non-skinned Lighting
VS. Results:

- The grey/weather layer stayed still.
- Full-detail trees stayed still.
- Opaque rock/wall base textures did not move with the shift; only
  transparent/moss overlays appeared to move.
- The base geometry often disappeared or looked broken.

Conclusion: the grey layer and opaque depth-culling are not being moved by the
custom Lighting VS. The grey layer is the engine depth/fog/weather result, not a
second copy of the custom-warped mesh.

## NIF findings (dirtcliffs01moss.nif)

`Static Mesh Improvement Mod/meshes/landscape/dirtcliffs/dirtcliffs01moss.nif`,
the moss overlay vs the top ground/base:

- Both use `BSLightingShaderProperty`.
- Both have `Skyrim Shader Type = Default`.
- Both are non-skinned in the visible data.
- Both have `Vertex_Alpha`, shadows, `Own_Emit`, `Remappable_Textures`,
  `ZBuffer_Test`, and `ZBuffer_Write`.
- The moss overlay additionally has `Decal` and `Dynamic_Decal` in Shader
  Flags 1, and `No_Fade` in Shader Flags 2.
- The base has neither `Decal` nor `Dynamic_Decal`.

This supports the depth/pass explanation: the moss is a separate decal/dynamic
decal shape rendered through a different pass, while the opaque base is subject
to the static z-prepass. The base can be moved in the color pass but culled by
the unwarped depth prepass; the decal remains visible.

It does not indicate a separate shader class or skinned base. A watchtower NIF
would be a useful second sample, but only if it shows a different shader type or
skin setup.

## Pending diagnostic: skip the z-prepass

`Main_RenderDepth` is temporarily skipped while Horizontal Curvature is loaded
and enabled. This tests the depth-prepass hypothesis directly:

- If the grey layer disappears and opaque geometry remains visible → the
  z-prepass is the blocker; it must be warped, skipped, or bypassed.
- If nothing improves → the grey layer is another engine pass.

## Runtime settings observed

Test profile `HorizontalCurvatureTest/skyrimprefs.ini`:

```ini
bUseTAA=0
bLodZPrepass=0
bEnableFrontToBackPrepass=0
bDeferredShadows=0
bShadowsOnGrass=0
```

The grey layer remained with all of these set, after the `SetVertexShader`
(`7fb44411`), `BeginTechnique` (`0c0cdeba`), and `SetupGeometry` (`e81fe677`)
b6 binding attempts. `bDisableZPrepassOutput=1` was also tried without removing
it.

`bSAOEnable`, `bScreenSpaceReflectionEnabled`, `bIBLFEnable`, and
`bVolumetricLightingEnable` remain enabled. They have not yet been isolated.

## Test history / observations

- Debug hue, cutoff, strength, color updated → PS settings and b6 work.
- Initial geometry did not move → stale cache and missing VS b6.
- Positive control (clip-X) and World-Z probe moved geometry → VS executes
  when the diagnostic code runs.
- `Apply to All Lighting` moved full-detail objects but produced grey/duplicate
  appearance; that was a Lighting/depth pass mismatch, not a second copy of the
  mesh.
- `CutoffSoftness` appeared to “enable” the effect in one test, but the sample
  was contaminated by `ApplyToAllLighting`, `DebugVertexMode = ClipX`,
  `CutoffSoftness = 19280`, and `SeaLevelOffset = -473`.
- Shader blocking: only `Lighting:Vertex:HORIZONTAL_CURVATURE VC` affects the
  grey layer; no Pixel entry does.
- Performance Overlay reported `Lighting` class; the “12 Lighting shaders” are
  12 Lighting draw calls, not 12 color shader programs.

## Test setup and commands

- Launch: MO2 instance `<MO2_INSTANCE>`,
  profile `HorizontalCurvatureTest`; use `launch-hctest.bat`.
- Full baseline: `SettingsUser.json.bak.fullfeatures` + `launch-full.bat`.
- Shader cache:
  `<MO2_INSTANCE>/overwrite/ShaderCache`.
- Log:
  `.../compatdata/489830/pfx/drive_c/users/steamuser/Documents/My Games/Skyrim Special Edition/SKSE/CommunityShaders.log`.
- Developer Mode: CS menu → Settings → Advanced.
- Shader blocking: Advanced → Enable Shader Blocking; Active Shaders table.
- Performance Overlay: F10 by default.

## Next steps

1. Deploy `0c0cdeba`, clear `ShaderCache/Lighting`, launch HCTest.
2. With `DebugHue = 1.0`, check whether the static grey depth layer is gone or
   moves with the colored layer.
3. If grey remains:
   - Try `bDisableZPrepassOutput=1` in the profile.
   - Isolate whether it is the LOD or full-detail z-prepass.
   - If necessary, hardcode a temporary depth-pass offset to prove the depth VS
     path, then implement the correct b6/depth binding.
4. If grey is gone, set `DebugHue = 0.35`, disable test probes, and validate the
   real curvature:
   - LOD-only with `ApplyToAllLighting = OFF`.
   - `ApplyToAllLighting = ON` + cutoff for landscape/full-detail.
   - Test parabolic, spherical vertical, and spherical full.
   - Test tree per-vertex vs base anchor.
   - Tune cutoff, softness, elevation, sea level.
5. Only after the visual result is clean, remove the temporary pass-color
   diagnostic and ship-gate the debug controls.

## Related files

- `docs/horizontal-curvature-architecture.md`
- `design.md`
- `PROGRESS.md`
- `RESUME.md`
- `AGENTS.md`
- `.claude/CLAUDE.md`
- `src/VanillaBinding.cpp`
- `src/Hooks.cpp`
- `package/Shaders/Lighting.hlsl`
- `features/HorizontalCurvature/Shaders/HorizontalCurvature/HorizontalCurvature.hlsli`
