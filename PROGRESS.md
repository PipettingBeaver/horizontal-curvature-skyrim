# Horizontal Curvature — Progress

## Project

- Repo: `<REPO_ROOT>`
- Branch: `horizontal-curvature`
- Remote: `git@github.com:PipettingBeaver/horizontal-curvature-skyrim.git` (SSH)
- CI: `.github/workflows/build-fork.yml` (GitHub-hosted Windows runner)
- Latest pushed commit: `0c0cdeba fix(horizontal-curvature): bind b6 after setup`
- Latest CI run: `41816433069` — **success** (shader validation and C++/AIO); latest fix not yet deployed
- Prior deployed lab build: `7fb44411` (`41803166595`)
- Investigation log: `docs/horizontal-curvature-investigation.md`
- Previous CI run: `41418566747` — failed only in shader validation
  - C++/AIO build job succeeded
  - `DistantTree` depth permutations (`Pixel:10001`, `Pixel:1`) used
    `input.WorldPosition` in the debug block, but `RENDER_DEPTH` output has
    `Depth`, not `WorldPosition`
  - Fixed by guarding depth-pass debug code with `!defined(RENDER_DEPTH)`
  - Added per-LOD debug hue offsets (terrain 0.0, object 0.33, tree 0.66)
  - Fork CI switched from `ALL` to `PR` preset for faster iteration
  - New CI run `41556965617` passed; AIO deployed and shader cache cleared
- Feature: `src/Features/HorizontalCurvature.*`, `features/HorizontalCurvature/`
- Shaders: `Lighting.hlsl` (terrain/object LOD), `DistantTree.hlsl` (tree LOD), shared settings in `b6` FeatureData
- **Root cause of the long "no effect" run:** a stale
  `overwrite/ShaderCache` kept loading old compiled shaders. CS logged
  `Using disk cache` while live HLSL edits were ignored. For iteration, delete
  only `ShaderCache/Lighting` and `ShaderCache/DistantTree` (or enable
  `Use FileWatcher`); do a full cache clear only for shared includes, feature-set
  changes, or when selective invalidation still fails. `Skip Unchanged Shaders`
  only checks top-level `.hlsl` mtimes, so touch/delete after `.hlsli` edits.
- Architecture is documented in `docs/horizontal-curvature-architecture.md`.

## Runtime setup

- Active game: `<GAME_MOUNT>/game/SkyrimSE-1.5.97`
  - Clean 1.5.97 data built from Steam depots
  - SKSE 1.5.97 and Engine Fixes preloader present
- Previous Best of Both Worlds copy: `<GAME_MOUNT>/game/SkyrimSE-1.5.97-bobw`
- Steam install: `~/.local/share/Steam/steamapps/common/Skyrim Special Edition` (1.7.104)
- Depot staging: `~/.local/share/Steam/ubuntu12_32/steamapps/content/app_489830`
- MO2: `<MO2_INSTANCE>`, profile `horizon`
- Enabled: Community Shaders Dev, Address Library, SSE Engine Fixes + preloader
- Clean install has no CC/AE content; full mod list will need CC + BEES later

## Fixed so far

- Empty `horizon/skyrimprefs.ini` caused startup crashes
- Engine Fixes preloader `d3dx9_42.dll` must live in the game root
- Missing `Skyrim - Patch.bsa` restored from the 1.5.97 depot
- Best of Both Worlds 1.7 data + 1.5.97 exe caused New Game/`coc` infinite loads
  - Fixed by building a clean 1.5.97 install from Steam depots
- CS shader failures `X4578: cbuffer bank 12 used more than once`
  - Lighting reads `CameraPosAdjust` through the existing `VS_PerFrame` b12 view
  - DistantTree passes `FrameBuffer::CameraPosAdjust` explicitly
- Launcher `cmd.exe /c launch-mo2.bat` persisted because the batch waited on MO2
  - Changed to `start "" "%~dp0ModOrganizer.exe" %*`; applies next MO2 launch

## New controls in commit 615568bd

- Runtime **Enable Effect** toggle (no shader recompile)
- **Debug Colors** toggle
- **Debug Hue** slider
  - Moving it must change LOD tint if `b6` settings reach the shader
- `Inner Radius Cutoff` minimum lowered to `0`, default `1000` for exaggerated testing
- Debug tint now driven by `b6`, not hardcoded values
- Temporary hardcoded Z/NDC diagnostics removed

## Current deployment / state (2026-11-03)

- CI AIO `846a2f4b` deployed; build hash `describe: 846a2f4`.
- Proper shaders restored; no hardcoded Z/NDC and no `#error`.
- Feature enabled at boot (`Disable at Boot: HorizontalCurvature = false`).
- **Fast test profile created:** MO2 `HorizontalCurvatureTest` (copy of
  `horizon`) plus `SettingsUser.json.bak.hctest`.
- The reduced settings disable all optional CS features except
  `HorizontalCurvature`, set `Post Processing Mode = Vanilla`, enable
  `Use FileWatcher`, and use the test values: Enable on, DebugColor on,
  DebugHue 0.35, k `5e-6`, cutoff `0`, elevation `0`, sea level `0`.
- Instance launchers: `launch-hctest.bat` (reduced settings + test profile) and
  `launch-full.bat` (full settings + `horizon`). CS settings are shared across
  MO2 profiles, so the launchers switch both.
- Fallback if the all-off `Vanilla` profile looks wrong:
  `SettingsUser.json.bak.hctest-pipeline` keeps `PostProcessing`,
  `LinearLighting`, and `HDRDisplay` while still disabling the heavy features.
- Full baseline remains in `SettingsUser.json.bak.fullfeatures`.
- Reduced profile was launched at 00:31 and compiled (4,098 cache files).
- **Root cause confirmed:** `FeatureData` b6 was bound to PS/CS but not VS.
  `Lighting`/`DistantTree` VS read `Enable = 0`; PS debug tint and sliders still
  worked. `src/VanillaBinding.cpp` now binds b6 to VS.
- **Second root cause confirmed:** shader blocking isolated the static grey
  layer to the vertex-only `Lighting:Vertex:HORIZONTAL_CURVATURE VC` pass;
  no Lighting Pixel shader removes it. This is the depth/prepass path.
  `BSShader::BeginTechnique` and `BSLightingShader_SetupGeometry` now bind b6 as
  late as possible; the latter runs immediately before the draw.
- The current deployed DLL only has the `7fb44411` fix; `0c0cdeba` passed CI but
  is not deployed yet.
- Current in-game report with the pass-color build: moving LOD/full-detail
  Lighting is colored (yellow/blue/green); the remaining static layer is grey.
- A hardcoded b6-independent clip-X control did not move the grey layer or
  full-detail trees. This confirms the grey is the engine depth/fog result, not
  custom-warped geometry.
- `Main_RenderDepth` was skipped in build `ab69d420`; the grey layer remained.
- Next temporary diagnostic: skip `Main_RenderShadowmasks` as well.
- **Lab build added in the same pending batch:** feature version `0.2.0`,
  `PerFrame` is 64 bytes, and the new runtime controls are:
  `CurvatureMode` (parabolic/spherical vertical/spherical full), `TreeMode`
  (per-vertex/base anchor), `ApplyToAllLighting`, per-type terrain/object/tree
  toggles, `DebugVertexMode` (clip X/Y/world Z), `DebugVertexAmount`, and
  `CutoffSoftness`.
- These are b6 uniforms; changing them in-game requires no recompile. The
  `VanillaBinding.cpp` fix and shader lab code must ship as one CI AIO; do not
  mix the new 64-byte shaders with the old 32-byte DLL.

### Confirmed diagnostics

- A temporary `#error` proved `HORIZONTAL_CURVATURE` is present in **Lighting
  vertex** shaders and in **DistantTree vertex and pixel** shaders.
- A temporary **clip-space horizontal X shift** visibly moved LOD geometry.
  That proved the vertex path executes, but it is screen-space and changes with
  camera rotation/FOV; it is not the curvature implementation.
- The later vertical Y/world-Z diagnostics were not conclusive: they were
  affected by stale cache, disabled-at-boot state, and/or no world load. Do not
  cite them as evidence that vertex displacement is unavailable.
- CS can run old compiled shaders even after the files change. Prefer deleting
  only `ShaderCache/Lighting` and `ShaderCache/DistantTree` after editing those
  shaders; full-clear only for shared includes, feature-set changes, or when
  selective invalidation fails.

## Next test sequence

1. Commit/push the per-draw VS `b6` binding fix; let CI build/deploy.
2. Launch via `launch-hctest.bat`, select `HorizontalCurvatureTest`, and let the
   reduced shader set compile.
3. `coc Whiterun`; wait until the actual world is visible, not the main menu.
4. Keep `DebugHue = 1.0` for the pass-color build. The static grey depth layer
   should disappear or match the colored moving layer. Then set `DebugHue = 0.35`
   and inspect terrain/object/tree LOD with cutoff `0`, k `5e-6`.
   - All move -> validate and tune production cutoff/elevation.
   - Trees only -> Lighting LOD VS path/permutation or space issue.
   - Nothing -> verify feature loaded, settings, cache, and shader errors.
5. Toggle **Enable Effect** and confirm no recompile is needed.
6. Use the lab controls in this order:
   - **Run Old Positive Control (Clip X)**: sets the historical horizontal
     `0.25 * w` shift. If this moves nothing, the VS path or b6 binding is still
     broken; if it moves, curvature math/settings are the remaining suspect.
   - `ApplyToAllLighting`: if LOD still does not move but full-detail does, the
     problem is the LOD permutation, not the binding.
   - `CurvatureMode = Spherical vertical`, then `Spherical full`.
   - `TreeMode = Base anchor` if trees shear/float.
   - Per-type toggles to isolate terrain/object/tree behavior.
   - `CutoffSoftness` to test the LOD-boundary seam.

## Testing variables for next time

- **Settings path:** Debug Hue positive test; check `b6` layout/offset/size if it fails.
- **Vertex path:** terrain LOD vs object LOD vs tree LOD vs full-detail cells; only LOD should move.
- **World space:** camera-relative distance, `worldPosition`, `ViewProj`/`CameraViewProj`; verify moving/rotating camera does not change the actual drop.
- **FOV/projection:** world-space drop should be FOV-independent; apparent screen displacement changes with FOV. The old NDC debug shift was screen-space and rotation-dependent; it is removed.
- **LOD boundary:** cutoff must exceed the full-detail/LOD transition in production to avoid seams; `0/1000` is only for testing.
- **LOD types:** vanilla LOD vs generated DynDOLOD/xLODGen LOD; large references; terrain skirts.
- **Visuals:** cracks between LOD tiles, z-fighting, popping, normals/lighting, distant trees floating, water horizon, sky/fog disconnect.
- **Runtime:** TAA on/off, motion vectors/ghosting, first/third person, world map, interiors, different worldspaces, weather/time of day, dynamic resolution, HDR.
- **Performance:** LOD vertex cost, CPU culling, shader compile/cache, VRAM.
- **Content:** clean 1.5.97 install has no CC; full `StableList` needs CC content + BEES and a 1.5.97/1.7-compatible base setup.
- **Cleanup:** remove or ship-gate debug hue/colors after validation; bump feature `.ini` version; update docs; re-enable only needed CS features.

## CI / session notes

- Builds run on GitHub-hosted Windows runners; the local computer does **not** need to stay on.
- The artifact is retained by GitHub; download it later with `gh run download`.
- Do not push another commit on `horizontal-curvature` while this run is in progress; the workflow's concurrency cancels the older run.
- The pending lab build bumps the feature `.ini` to `0.2.0`; the build hash in `CommunityShaders.log` remains the reliable DLL identifier.

## Research notes: other curvature implementations

### Minecraft shaders / Distant Horizons

- Minecraft “World Curvature” shaders apply curvature in **unrotated view/camera-relative space**, not after camera rotation. Rotating the camera must not change the world-space warp; screen-space/NDC debug shifts do, which explains previous FOV/rotation weirdness.
- Distant Horizons uses **true spherical geometry**, not just a Z drop:
  ```glsl
  float localRadius = earthRadius + vertexY;
  float phi = length(vertexWorldPos.xz) / localRadius;
  vertexWorldPos.y += (cos(phi) - 1.0) * localRadius;
  vertexWorldPos.xz *= sin(phi) / phi;
  ```
  `earthRadius = 6371000 / ratio`; ratio 100 ≈ 63.7 km radius. This drops Y and pulls XZ inward.
- When DH curvature is enabled it limits LOD quad width (`MAX_QUAD_WIDTH_FOR_EARTH_CURVATURE`) so large LOD quads are subdivided. Coarse geometry cannot represent curvature smoothly.
- Bliss shader’s `PLANET_CURVATURE` applies the same parabola to **all solid, translucent, Distant Horizons LOD, and shadow** vertex shaders:
  ```glsl
  float curvature = length(worldpos) / (16 * 8);
  worldpos.y -= curvature * curvature * CURVATURE_AMOUNT;
  ```
  It is camera-relative and applies to everything that must stay coherent; objects not using the shader float.
- Skyrim/DynDOLOD note: terrain LOD is already lowered in the distance by a baked shader (`LodLandscape::AdjustLodLandscapeVertexPositionMS` and the `vsout.Position.z` nudge) to reduce z-fighting with object LOD. That built-in offset can make terrain and object/tree LOD decouple independently of our warp.

### Unity Curved World / Curved World-like shaders

- Curved World uses per-material vertex transformations with a shared **pivot/camera point**; global parameters updated once.
- Objects/VFX not using the curved shader appear to float when the camera moves. Every world object/pass that must stay coherent needs the same transformation.
- Culling must be adjusted; curved geometry can move into/out of the original frustum. Unity versions provide a camera-culling helper.
- Enough mesh vertices are required for smooth bends; a 4-vertex ground plane never bends. This mirrors coarse Skyrim terrain LOD.

### Floating props on procedural terrain

- Near field: props on slopes float if their flat base is seated on one height sample. Fix by conforming the base or sinking it.
- Far field: coarse terrain chords sag below the exact height field; props using the exact height float. Fix by lifting terrain vertices by the chord excess or sinking props. Curvature adds a similar low-frequency chord error on coarse terrain LOD.
- For trees, apply the warp at the **tree base/anchor** to keep the tree rigid and planted; sample the same function on the terrain.

### Practical implications for Skyrim

- Keep the warp in camera-relative, unrotated world space; world-space drop should be FOV-independent.
- Apply the same function to terrain LOD, object LOD, and tree LOD; any pass left out will decouple.
- Terrain LOD uses large triangles; consider xLODGen higher-resolution LOD or runtime tessellation, and/or accept LOD-only approximation.
- For tree LOD, anchor-based rigid translation is likely better than per-billboard-vertex warping; match the terrain at the anchor.
- Check the engine’s built-in terrain-LOD lowering; it may need compensation for object/tree LOD.
- True spherical warp is an alternative to the current only-Z parabolic drop; it pulls XZ inward and may improve horizon/relationship behaviour at the cost of more distortion and gaps.
- Arbitrary sub-mesh selection is not possible with a pure height field; use shader/pass/object-type selection or per-vertex masks. Merged mountain+building LOD cannot be separated without a mask.

### Links

- Minecraft World Curvature / spherical view-space discussion: https://www.minecraftforum.net/forums/mapping-and-modding-java-edition/minecraft-mods/1272365-1-3-1-glsl-shaders-dof-bump-mapping-waving-wheat?page=246
- Distant Horizons curvature source: https://github.com/GTNewHorizons/DistantHorizons (terrain shader + `earthCurveRatio` + quad width)
- Bliss `PLANET_CURVATURE`: https://github.com/X0nk/Bliss-Shader
- Unity Curved World docs: https://amazing-assets.gitbook.io/curved-world/
- Curved World floating VFX discussion: https://discussions.unity.com/t/i-need-help-with-logic-for-a-game-jam-feature/865631
- Floating props / terrain chord study: https://csarko.sh/research/floating-props-on-procedural-terrain
- DynDOLOD terrain LOD lowering note: https://dyndolod.info/Help/Terrain-LOD-and-Water-LOD
