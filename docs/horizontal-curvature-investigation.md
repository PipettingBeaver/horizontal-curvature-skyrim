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
- Both BSTriShape pieces are owned by the same NiNode under the BSFadeNode.
- The moss/transparent piece has a `NiAlphaProperty`; the opaque base does not.

This supports the depth/pass explanation: the moss is a separate alpha/decal
shape rendered through the transparency/decal pass, while the opaque base is
subject to the static z-prepass. The base can be moved in the color pass but
culled by the unwarped depth prepass; the transparent decal remains visible.

It does not indicate a separate shader class or skinned base. A watchtower NIF
would be a useful second sample, but only if it shows a different shader type or
skin setup.

## Pending diagnostic: skip depth and shadow masks

`Main_RenderDepth` is temporarily skipped while Horizontal Curvature is loaded
and enabled. This tests the depth-prepass hypothesis directly:

- If the grey layer disappears and opaque geometry remains visible → the
  z-prepass is the blocker; it must be warped, skipped, or bypassed.
- If nothing improves → the grey layer is another engine pass.

## LOD terrain result (latest)

With `ApplyToAllLighting = OFF`, lower curvature strength, and cutoff `0`,
distant terrain LOD visibly descends. The grey layer remains.

This confirms the LOD terrain color path can work without the
`ApplyToAllLighting` fallback. The remaining grey layer is therefore a separate
pass/overlay, not the terrain color geometry itself. Next isolation:
`ApplyTerrain` vs `ApplyObjects` vs `ApplyTrees`, plus z-prepass/shadow-mask
output. The shadow-mask skip CI is pending.
With `ApplyTerrain = OFF`, the terrain effect stops and the grey layer also
disappears, so the grey is tied to the terrain-LOD path. `ApplyObjects` and
`ApplyTrees` also show grey; `ToggleZPrepassOutput` hides the grey and shows
only the sparse prepass geometry (LOD terrain and transparent outlines), while
ordinary opaque objects are absent. A `Curvature Raise` switch has been added
for diagnostics: it inverts the vertical displacement so HC-affected meshes can
be distinguished from static/z-fighting copies.

## Raise test result

`Curvature Raise` works: distant terrain visibly rises instead of lowering.
The grey layer stays at the original height. Close/full-detail objects do not
rise with `ApplyToAllLighting = OFF`; that is expected because they are outside
the LOD path. Trees/grass classifications in pass colors are unreliable because
some are full-detail, alpha, or separate shader types.

`ToggleZPrepassOutput` hides the grey layer. The temporarily skipped
`Main_RenderDepth` and `Main_RenderShadowmasks` paths did not remove it. The
remaining grey is therefore another engine depth/prepass or base representation,
not the custom Lighting color path.

Next step: identify the actual z-prepass/depth pass with FrameAnnotations or
RenderDoc, then warp/disable that pass. More per-type in-game toggles are not
useful.

## Community Shaders z-prepass reference (important)

Upstream Community Shaders' **Terrain Blending** feature uses a different
`Main_RenderDepth` hook from the one in this fork's `FrameAnnotations`:

- Terrain Blending: `REL::RelocationID(35560, 36559)` — identifies the actual
  z-prepass versus shadow depth.
- Our diagnostic skip used `REL::RelocationID(100421, 107139)`, which is a
  different depth call. That explains why skipping it did not remove the grey.

Terrain Blending also hooks:

- `BSBatchRenderer__RenderPassImmediately` at
  `REL::RelocationID(100852, 107642)`, to manipulate depth writes, depth test,
  and alpha blending per render pass.
- It renders a separate terrain depth target and blends it with the main depth
  via a compute shader.

Other relevant references:

- Community Shaders VR z-fighting fix commit `746a41f`: adjusts
  `LODLANDSCAPE`/`LODLANDNOISE` view-space Z for distant mountains.
- DynDOLOD: SS/VR terrain LOD is lowered in the distance by a baked shader to
  counter z-fighting with object LOD; object/tree LOD does not get the same
  lowering automatically.
- Unity Curved World / No Rest for the Wicked: apply the same transform to all
  coherent passes and rework culling/lighting for the curved world.

Next code direction: replace the ineffective `Main_RenderDepth` skip with the
actual z-prepass hook used by Terrain Blending, or use
`BSBatchRenderer_RenderPassImmediately` to manipulate only the affected depth
passes. This should explain why nearby full-detail geometry is unaffected and
distance LOD leaves a grey depth/weather shell.

## Build 0.4.0 result

The real z-prepass skip was tested and did not remove the grey layer; the
temporary skip has been removed for the next build.

## Build 0.5.0 scope

- Remove the failed z-prepass skip.
- Add granular Lighting apply switches:
  LOD landscape, LOD land noise, LOD objects, LOD objects HD,
  full-detail landscape, full-detail objects, skinned.
- Keep Terrain/Objects group masters, Tree LOD, All Lighting, and Raise.
- Grass/Water/Sky actual warping is deferred because those shaders use
  different vertex spaces; they are not exposed as dead switches yet.
- Feature version `0.5.0`.

## Build 0.4.0 result

Build `2fef14f` / feature `0.4.0` was deployed and loaded. It contains only the
focused real-z-prepass test; it does not contain the new granular toggles,
Grass/Water/Sky hooks, or probe slider.

Result with HC enabled: distant terrain turned grey and did not visibly move.
Skipping the `35560/36559 + 0x395` call is therefore not a usable fix; the call
is likely genuinely needed for depth coverage. The next approach must warp or
selectively manage that depth pass, not skip the whole call.

The existing ApplyAll/Raise controls should still be present in 0.3.0+ builds;
if the menu shows only the three apply checkboxes, scroll the feature page.

## Z-prepass output and full-detail transparent walls

`ToggleZPrepassOutput` is an engine debug view: it displays the z-prepass output
instead of the normal color/fog composition. It does not disable the z-prepass
or change HC. That is why the grey layer disappears while the view is on and
only depth/prepass geometry is visible.

Observed with the transparent Whiterun wall:

- `ApplyToAllLighting = OFF`: the wall does not move and has no grey layer.
- `ApplyToAllLighting = ON`: the wall moves, and still no separate grey layer is
  created.

This indicates the transparent wall is full-detail geometry reached only by the
all-Lighting branch. Moving full-detail alpha/decal geometry is not itself what
creates the grey shell. The grey remains associated with the terrain-LOD/depth
path.

## Update: grey applies to moved opaque geometry generally

Latest observations:

- With `ApplyToAllLighting = ON`, rocks right next to the player also move and
  become grey.
- Walls do not move under `ApplyToAllLighting`; `ApplyObjects` only covers LOD
  object draws, not these full-detail walls.
- `ToggleZPrepassOutput` makes the full walls invisible or entirely grey.
- The transparent wall/moss overlay can move without producing the grey shell,
  but the opaque wall/base does not move at all.

Revised conclusion: the grey is not unique to terrain. It appears on any opaque
geometry moved by HC while its depth/prepass remains at the old position.
Transparent/decal overlays do not show it the same way. The earlier
terrain-specific result was because `ApplyTerrain=OFF` stopped the only moving
geometry in that test.

The wall is a separate unresolved issue: its shader/path is apparently not being
moved by `ApplyObjects` or `ApplyToAllLighting`. It needs Active Shaders /
Shader Blocking identification. The transparent piece that moves may be only
the moss/decal overlay.

Next in-game check:

1. With `ApplyToAllLighting=ON`, open Active Shaders and compare the nearby rock
   and wall entries.
2. Block the wall's Lighting VS/PS and see whether only the transparent overlay
   disappears.
3. Confirm whether the base wall is absent from the custom shader list or uses a
   different shader/path.
4. Use `ToggleZPrepassOutput` as a view only.

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


## Grey static layer: consolidated findings and test matrix

### What the evidence says

- The grey layer is generated while Horizontal Curvature is active; disabling
  the feature removes it.
- Shader blocking: only `Lighting:Vertex:HORIZONTAL_CURVATURE VC`
  (descriptor `0x1`) affects it. No Lighting **Pixel** row removes it.
- `ApplyTerrain = OFF` stops the terrain curvature and also makes the grey layer
  disappear. The grey is tied to the terrain-LOD path.
- `Curvature Raise` lifts terrain correctly, but the grey stays at the original
  height.
- `ToggleZPrepassOutput` hides the grey and shows sparse prepass geometry:
  distant LOD terrain and transparent outlines; ordinary opaque objects are
  absent.
- Weather (`sw 0010199f`) changes the grey layer's color.
- Shadows off, TAA off, `bLodZPrepass=0`,
  `bEnableFrontToBackPrepass=0`, and `bDisableZPrepassOutput=1` did not remove
  it.
- A b6-independent hardcoded clip-X shift did not move the grey layer or
  full-detail trees. It did move some transparent/moss overlays.
- The NIF base and moss are both `BSLightingShaderProperty`/Default and share a
  NiNode. The moss piece has `Decal | Dynamic_Decal` and `NiAlphaProperty`; the
  base does not.

Best current explanation: the custom Lighting color pass moves, but the
engine depth/z-prepass or a second depth/fog representation stays at the old
position. Opaque moved LOD fails the old depth test and disappears; transparent
decal/alpha overlays survive because they do not depend on that depth the same
way. The grey shell is the old depth/fog/weather result, not a duplicate of the
custom-warped mesh.

### Important hook correction

Upstream Terrain Blending identifies the real z-prepass with:

```text
REL::RelocationID(35560, 36559)
```

Our diagnostic skip used `FrameAnnotations`' `Main_RenderDepth` at:

```text
REL::RelocationID(100421, 107139)
```

That is a different depth call. This is why skipping `Main_RenderDepth` and
`Main_RenderShadowmasks` did not affect the grey.

Terrain Blending also uses `BSBatchRenderer__RenderPassImmediately`
(`REL::RelocationID(100852, 107642)`) to manipulate depth writes, depth test,
and alpha blending per pass, and renders a separate terrain depth target that
it blends with the main depth via a compute shader.

### In-game matrix

Run each row from a safe state unless noted. Record terrain, objects, trees,
grass, grey, and z-fighting separately.

| Test | Settings | Result to look for |
| --- | --- | --- |
| A | `ApplyTerrain=OFF`, others OFF, DebugHue normal | No terrain movement; grey gone. Confirms terrain association |
| B | `ApplyTerrain=ON`, Objects/Trees OFF, cutoff 0, low strength | LOD terrain moves; grey should remain at old height |
| C | Same as B + `Raise=ON` | Terrain rises; grey independent |
| D | Same as B + `DebugHue=1.0` | Moving terrain colored; grey uncolored = no color PS |
| E | Same as B + `ToggleZPrepassOutput` | Grey hidden; sparse prepass geometry only |
| F | Same as B + weather change (`sw 0010199f`) | Grey color changes = fog/weather/depth |
| G | `ApplyObjects=OFF` / `ApplyTrees=OFF` individually | Which object/tree category is present in the grey/z-fight |
| H | Enable **Terrain Blending** (if compatible) | Does its depth handling change the grey? Record before/after |
| I | Correct z-prepass hook test (future build) | Grey gone / opaque restored = z-prepass confirmed |
| J | `BSBatchRenderer__RenderPassImmediately` depth-write test (future build) | Isolates the pass without skipping the whole prepass |

### Future diagnostic build

1. Remove the temporary `Main_RenderDepth`/`Main_RenderShadowmasks` skips from
   `FrameAnnotations.cpp`.
2. Add a test using the real z-prepass hook (`35560/36559`) or
   `BSBatchRenderer__RenderPassImmediately`.
3. Keep `Raise`.
4. Add granular apply toggles:
   LOD landscape, LOD land noise, LOD objects, LOD objects HD,
   full-detail landscape, full-detail objects, skinned, tree LOD,
   grass, water, sky.
5. Add a pass-level probe slider, not just a pixel-shader color probe:
   use `BSBatchRenderer__RenderPassImmediately`/render flags to tint or log
   depth, shadowmask, terrain, opaque, alpha, and shader-class passes.
6. Bump to `0.4.0` and run CI.

### Rollback / safe baseline

Current deployed `26917ab3` contains the temporary skip code. It is safe only
while HC is disabled.

Safe settings already applied:

```json
"Disable at Boot": { "HorizontalCurvature": true },
"Horizontal Curvature": { "Enable": false },
"Advanced": { "Developer Mode": false, "Frame Annotations": false }
```

HCTest profile test settings removed:

```ini
bLodZPrepass=0
bDisableZPrepassOutput=1
bEnableFrontToBackPrepass=0
bDeferredShadows=0
bShadowsOnGrass=0
```

`bUseTAA=0` remains from the original profile.

To recover normal rendering:

1. Keep `HorizontalCurvature` disabled at boot.
2. Launch `launch-hctest.bat`.
3. If still broken, clear `ShaderCache/Lighting` and `ShaderCache/DistantTree`,
   then relaunch.
4. Do not re-enable HC in `26917ab3`; its diagnostic skips can crash.

### Build / hook history

| Commit | Meaning |
| --- | --- |
| `e2fcc2c1` | lab controls + first VS b6 binding |
| `7fb44411` | per-draw b6 in `SetVertexShader` (too early) |
| `0c0cdeba` | b6 after `BeginTechnique` (still too early) |
| `e81fe677` | b6 in `Lighting SetupGeometry`; no fix for grey |
| `7962bc30` | skip `FrameAnnotations` `Main_RenderDepth` (wrong hook) |
| `ab69d420` | compile/include fix for skip test |
| `6296b6c3` | skip `Main_RenderShadowmasks` (no effect) |
| `26917ab3` | Raise + skips; current deployed, unsafe with HC on |

### Rule for future changes

- Keep the current safe baseline until a clean build without skips is deployed.
- Do not mix shader source edits with a deployed DLL whose FeatureData layout
  differs.
- Always clear at least `ShaderCache/Lighting` and `ShaderCache/DistantTree`
  after editing those shader files/includes.
- Use the correct z-prepass hook, not the 100421 `Main_RenderDepth` call.

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
