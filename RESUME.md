# Horizontal Curvature — Resume / Research / Test Plan

This file is the single resume point for the next session. It includes the current
state, the unfinished test, cross-engine research, implementation tricks, and a
full test plan.

## 1. Project and CI

- Repo: `<REPO_ROOT>`
- Branch: `horizontal-curvature`
- Remote: `https://github.com/PipettingBeaver/horizontal-curvature-skyrim.git`
  (HTTPS with `gh` credential helper; the SSH key needs unlocking again)
- Latest pushed commit: `615568bd feat(horizontal-curvature): add debug controls`
- CI workflow: `.github/workflows/build-fork.yml`
- CI runs on GitHub-hosted Windows runners. **The local machine does not need to stay on.**
- Failed CI run: `41418566747`
  - C++ and AIO package build succeeded.
  - Shader validation failed on `DistantTree` depth permutations:
    `DistantTree:Pixel:10001` and `Pixel:1` reported
    `X3018: invalid subscript 'WorldPosition'`.
  - Cause: the debug-color block was outside the `RENDER_DEPTH` guard, but the
    depth PS input has `Depth`, not `WorldPosition`.
  - Fix: wrap the debug block in `!defined(RENDER_DEPTH)` (and guard the Lighting
    debug block too).
  - Added per-LOD debug hue offsets: terrain hue +0.0, object +0.33, tree +0.66.
  - Fork CI now uses the faster `PR` preset for the C++/AIO build; shader
    validation remains `ALL`.
- New CI run will start on the next push. Do not push another commit while it
  is in progress; the workflow concurrency cancels the older run.

### Download / deploy when CI finishes

```bash
PATH="$HOME/.local/bin:$PATH"
gh run watch 41418566747 -R PipettingBeaver/horizontal-curvature-skyrim

gh run download 41418566747 -R PipettingBeaver/horizontal-curvature-skyrim \
  -n dist-artifacts -D /tmp/opencode/cs-dist-new

<REPO_ROOT>/tools/deploy-aio.sh \
  /tmp/opencode/cs-dist-new/CommunityShaders_AIO-*.7z

rm -rf "<MO2_INSTANCE>/overwrite/ShaderCache"
```

Confirm the new build in `CommunityShaders.log`:

```text
Community Shaders 1.9.0 | branch: horizontal-curvature | describe: 615568b...
```

The feature `.ini` version is still `0-1-0`; the build hash is the reliable identifier.

## 2. Game / MO2 state

- Active clean 1.5.97 install:
  `<GAME_MOUNT>/game/SkyrimSE-1.5.97`
- Previous Best of Both Worlds copy:
  `<GAME_MOUNT>/game/SkyrimSE-1.5.97-bobw`
- Steam install: `~/.local/share/Steam/steamapps/common/Skyrim Special Edition` (1.7.104)
- Steam 1.5.97 depots downloaded under:
  `~/.local/share/Steam/ubuntu12_32/steamapps/content/app_489830`
- MO2: `<MO2_INSTANCE>`, profile `horizon`
- Enabled: Community Shaders Dev, Address Library, SSE Engine Fixes + preloader
- All optional CS features are disabled; `HorizontalCurvature` is enabled at boot.
- The clean install has **no CC/AE content**. The full 1,096-mod `StableList` will
  need CC content + BEES (and a compatible 1.5.97/1.7 data setup) later.

### Important install facts

- `Skyrim - Patch.bsa` is mandatory for 1.5.97 and is present in the clean install.
- Engine Fixes preloader `d3dx9_42.dll` must be in the game root.
- `Skyrim.ccc` is disabled in the clean install.
- The clean install was built because the Best of Both Worlds data mix caused
  New Game / `coc` infinite loads.
- The launcher batch now uses
  `start "" "%~dp0ModOrganizer.exe" %*`
  so the wrapper `cmd.exe` exits. The current session predates this change.
  If MO2 closes when the wrapper exits next time, restore `launch-mo2.bat.bak`.
- `coc` from the main menu worked on the clean install (`coc Riverwood`).

## 3. What is actually deployed

The **deployed mod folder is still the old temporary build** until the new AIO is
deployed. Do not mix new shaders with the old DLL: the new C++ `PerFrame` is 32
bytes (8 floats), while the old DLL writes 16 bytes (4 floats).

Repo commit `615568bd` contains the proper implementation:

- `src/Features/HorizontalCurvature.h/.cpp`
- `features/HorizontalCurvature/Shaders/HorizontalCurvature/HorizontalCurvature.hlsli`
- `package/Shaders/Common/SharedData.hlsli`
- `package/Shaders/Lighting.hlsl`
- `package/Shaders/DistantTree.hlsl`

Deployed mod path:

`<MO2_INSTANCE>/mods/Community Shaders Dev`

## 4. Root causes already fixed

- Empty `horizon/skyrimprefs.ini` caused startup crashes.
- Engine Fixes preloader was inside a mod instead of the game root.
- Missing `Skyrim - Patch.bsa`.
- Best of Both Worlds cell loading was broken; clean 1.5.97 data fixed it.
- Four CS shader failures:
  `X4578: cbuffer bank 12 used more than once`
  - Lighting now reads `CameraPosAdjust` through the existing `VS_PerFrame` b12 view.
  - DistantTree passes `FrameBuffer::CameraPosAdjust` explicitly.
- The `cmd.exe` launcher wrapper was fixed via `start`.

## 5. New controls in 615568bd

- **Enable Effect**: runtime switch; shader returns no drop when off.
- **Debug Colors**: tint affected LOD.
- **Debug Hue**: hue slider. Moving it is the positive test that `b6` settings
  reach the shader.
- **Inner Radius Cutoff**: minimum `0`, default `1000` for exaggerated testing.
- Debug tint is driven by the shared `b6` buffer, not hardcoded values.
- Temporary hardcoded Z/NDC diagnostics are removed from the repo version.

## 6. Current unknowns / decision tree

### First: does Debug Hue work?

- Hue changes -> `b6` settings reach the shader.
- Hue does not change -> inspect:
  - `FeatureBuffer.cpp` order and `GetCommonBufferData()`
  - `SharedData.hlsli` struct layout/size
  - C++ `PerFrame` field order/size
  - whether the feature is loaded and the shader cache was cleared
  - whether the new AIO was actually deployed and the build hash changed

### If hue works but terrain/mountain still does not move

1. Use per-LOD-type debug colors to identify the mountain:
   - terrain LOD: `LODLANDSCAPE` / `LODLANDNOISE`
   - object LOD: `LODOBJECTS` / `LODOBJECTSHD`
   - tree LOD: `DistantTree`
   - full-detail cells: no debug tint
2. Check the CS log for shader compile errors for Lighting LOD.
3. Add a temporary clip-space shift (X/Y) to the Lighting LOD VS only:
   - If it moves, the VS block executes; the world-space recompute is wrong.
   - If it does not move, the mountain is not using that VS permutation.
4. Compare Lighting vs DistantTree:
   - Lighting uses `ViewProj`; DistantTree uses `FrameBuffer::CameraViewProj`.
   - Verify `worldPosition` is camera-relative and that `viewPos = mul(ViewProj, worldPosition)`
     is the correct recompute.
5. Check the engine's built-in terrain LOD lowering:
   - `LodLandscape::AdjustLodLandscapeVertexPositionMS`
   - the `vsout.Position.z` LOD nudge
   - object/tree LOD does not receive the same offset automatically.
6. Check terrain LOD mesh resolution/quads. Large LOD triangles cannot show a
   smooth curve; higher-resolution xLODGen LOD or tessellation may be needed.
7. Check culling/pop-in; curved vertices may leave the original AABB frustum.

### If both move but trees/terrain float or shear

- Apply tree warp at the **base/anchor** for a rigid translation, not per-vertex.
- Sample the same `f` on terrain at the tree base.
- Coarse terrain triangle chords underestimate/overestimate the exact warp at the
  tree base. Mitigate with a small sink/overshoot, higher LOD resolution, or
  terrain chord correction.
- Check the engine's baked terrain-LOD lowering.
- Check billboard base offsets vs actual tree origin.

### If FOV/rotation changes the result

- The world warp must be in **unrotated camera-relative space**.
- Remove screen-space/NDC debug effects before judging.
- A world-space drop is FOV-independent; only its apparent pixel size changes.
- If the relationship still changes, suspect a different matrix/space in one shader.

## 7. Cross-engine research

### Minecraft / Bliss Shader / Distant Horizons

- Curvature should be applied in **unrotated view/camera-relative space**.
  Applying after camera rotation causes unnatural warping when rotating.
- Bliss `PLANET_CURVATURE` applies the same function to **all solid, translucent,
  Distant Horizons LOD, and shadow** vertex shaders:
  ```glsl
  float curvature = length(worldpos) / (16 * 8);
  worldpos.y -= curvature * curvature * CURVATURE_AMOUNT;
  ```
  Its `worldpos` is camera-relative. Everything that should stay coherent uses
  the same function; objects/VFX without it float.
- Distant Horizons uses true spherical curvature:
  ```glsl
  float localRadius = earthRadius + vertexY;
  float phi = length(vertexWorldPos.xz) / localRadius;
  vertexWorldPos.y += (cos(phi) - 1.0) * localRadius;
  vertexWorldPos.xz *= sin(phi) / phi;
  ```
  `earthRadius = 6371000 / ratio`; ratio 100 ≈ 63.7 km radius.
- When curvature is enabled DH limits LOD quad width
  (`MAX_QUAD_WIDTH_FOR_EARTH_CURVATURE`) so large flat quads are subdivided.
- DH disables curvature for ratio values between -1 and 1; 0 means flat.
- DH curvature is a visual shader effect; it does not move blocks physically.

### Unity Curved World

- Per-material vertex transformation with a shared pivot (usually camera).
- Global parameters are set once; all materials must use the curved shaders.
- Objects/VFX not using the shader float when the camera moves.
- Culling must be adjusted; curved geometry can move into/out of the original frustum.
- Mesh vertex density matters; a low-vertex plane does not bend smoothly.
- Parabola is cheaper but distorts tall objects and constrains camera placement;
  spiral/spherical variants are used for 360° observation.

### Floating props / terrain LOD

- Near field: props on slopes float if their flat base is seated on one sample;
  fix by conforming the base or sinking it slightly.
- Far field: coarse terrain chords sag below the exact height; props sampled at
  exact height float. Fix by lifting terrain vertices by chord excess or sinking props.
- This is directly relevant to tree LOD vs coarse terrain LOD under a quadratic warp.

### Skyrim-specific

- Skyrim is mechanically flat; there is no native curvature.
- DynDOLOD notes that terrain LOD is lowered in the distance by a baked shader
  to counter z-fighting with object LOD.
  `LodLandscape::AdjustLodLandscapeVertexPositionMS` and the `vsout.Position.z`
  nudge are visible in `Lighting.hlsl`.
- Object/tree LOD does not automatically get the same lowering, which can
  decouple terrain and trees even before our warp.
- Terrain LOD is generated as `.btr`; object LOD `.bto`; tree LOD uses DistantTree.
- DynDOLOD does not create/update terrain LOD; use xLODGen for terrain LOD.

## 8. Implementation tricks / checklist

1. Keep warp in camera-relative, unrotated world space.
2. Apply the same function to all relevant LOD passes (terrain, object, tree,
   and shadows if they overlap).
3. Do not use per-object center scaling; use a shared height field.
4. For trees, use the base/anchor for rigid translation; sample the same function
   on terrain at the base.
5. For terrain/object LOD, per-vertex warp is correct, but coarse triangles need
   higher LOD resolution or tessellation.
6. Try true spherical vs parabolic:
   - parabola: `y -= k * max(0, d - R)^2`
   - sphere: DH formula above; pulls XZ inward too.
7. Account for the engine's baked terrain LOD lowering.
8. Remove screen-space/NDC debug before judging FOV/rotation.
9. Use per-LOD-type debug colors/hue to identify what is actually warped.
10. Arbitrary sub-mesh selection is not possible with a pure height field; use
    shader/pass/object-type selection or vertex masks.
11. Culling: test pop-in; consider conservative bounds/camera culling.
12. Motion vectors/TAA: warp previous positions too; test ghosting.
13. Water/sky: separate passes; fog/horizon handling.

## 9. Full test plan

### Transform / space

- Fixed camera, rotate 360° at constant FOV.
- Translate camera only, no rotation.
- FOV 60 / 90 / 120 at the same position and rotation.
- Aspect ratio 16:9 and 21:9.
- Camera heights: sea level, Whiterun tundra, High Hrothgar, lighthouse.
- Compare world-space drop vs screen-space appearance.

### LOD relationship

- Debug-color terrain LOD, object LOD, tree LOD, full-detail separately.
- Multi-part mountains; city LOD; watchtower roofs vs walls.
- Tree base vs canopy on a slope.
- Billboard tree LOD vs ultra-tree/object LOD.
- Full-detail/LOD boundary; cutoff below/at/above the transition.
- Vanilla LOD vs xLODGen/DynDOLOD-generated LOD.
- Test engine’s terrain LOD lowering on/off where possible.

### Selection / scope

- Include/exclude whole LOD categories.
- Exclude object types (buildings, rocks, water, trees).
- Confirm full-detail cells, actors, physics and local raycasts are untouched.
- Test merged mountain+building LOD; document that sub-mesh exclusion needs masks.

### Visual correctness

- Cracks/seams between LOD tiles and LOD levels.
- Z-fighting, popping, culling.
- Floating trees, terrain skirts, buried trees.
- Water horizon, sky/fog disconnect, distant mountain silhouettes.
- TAA on/off, motion vectors/ghosting, motion blur.
- First-person/third-person, world map, interiors.
- Other worldspaces: Solstheim, Forgotten Vale, Blackreach (no LOD).
- Weather/time of day, dynamic resolution, HDR, upscaling.

### Performance / regression

- Frame time with effect on/off at low/high `k`.
- Shader compile/cache behavior when toggling features.
- `Enable Effect` off is a strict no-op.
- Shader cache invalidation when shaders change.

### Synthetic / automated

- A test grid mesh with markers at known coordinates.
- A tall pole/flag on a hill to compare tree base vs terrain.
- RenderDoc A/B capture with effect on/off at identical camera.
- Screenshot comparisons across FOV/camera height/LOD types.
- Debug colors per pass + hue positive test.

### Content / integration

- Clean 1.5.97 base first.
- Then CC content + BEES and the full `StableList`.
- Re-enable only needed CS features; re-check shader cache.
- SE/AE targeting (the CI build is ALL).

## 10. Parameters / variables

- `CurvatureStrength` (k): currently default `5e-6`, max `5e-6` in UI.
- `InnerRadiusCutoff`: testing `0`/`1000`; production should exceed the
  full-detail/LOD transition.
- `ElevationScaleFactor`: horizon extension vs camera height.
- `SeaLevelOffset`: worldspace baseline.
- `DebugHue`, `DebugColor`.
- FOV, camera height, worldspace, LOD distances:
  `fBlockLevel0Distance`, `fBlockLevel1Distance`, `fBlockMaximumDistance`,
  `fTreeLoadDistance`, `fSplitDistanceMult`.
- TAA/upscaling/dynamic resolution/HDR.
- Vanilla vs generated LOD; CC/BEES.

## 11. Cleanup before shipping

- Remove or ship-gate debug hue/colors.
- Decide parabola vs true sphere; document.
- Decide tree anchor vs per-vertex warp.
- Bump the feature `.ini` version after validation.
- Update `design.md`/README with final behavior and constraints.
- Re-enable desired CS features and re-test.
- Test full mod list/CC/BEES and performance.
- Commit/push final changes; don’t push while CI is in progress.

## 12. Links

- Minecraft World Curvature / spherical view-space discussion:
  https://www.minecraftforum.net/forums/mapping-and-modding-java-edition/minecraft-mods/1272365-1-3-1-glsl-shaders-dof-bump-mapping-waving-wheat?page=246
- Distant Horizons source:
  https://github.com/GTNewHorizons/DistantHorizons
- Bliss Shader:
  https://github.com/X0nk/Bliss-Shader
- Unity Curved World docs:
  https://amazing-assets.gitbook.io/curved-world/
- Curved World floating VFX discussion:
  https://discussions.unity.com/t/i-need-help-with-logic-for-a-game-jam-feature/865631
- Floating props / terrain chord study:
  https://csarko.sh/research/floating-props-on-procedural-terrain
- DynDOLOD terrain LOD lowering note:
  https://dyndolod.info/Help/Terrain-LOD-and-Water-LOD
