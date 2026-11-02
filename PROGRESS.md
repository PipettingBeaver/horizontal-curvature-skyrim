# Horizontal Curvature — Progress

## Project

- Repo: `<REPO_ROOT>`
- Branch: `horizontal-curvature`
- Remote: `git@github.com:PipettingBeaver/horizontal-curvature-skyrim.git`
- CI: `.github/workflows/build-fork.yml` (Windows runner, full AIO build)
- Feature: `src/Features/HorizontalCurvature.*`, `features/HorizontalCurvature/`
- Shaders: `Lighting.hlsl` (terrain/object LOD), `DistantTree.hlsl` (tree LOD), settings in shared `b6` FeatureData

## Runtime setup

- Active game: `<GAME_MOUNT>/game/SkyrimSE-1.5.97`
  - Clean 1.5.97 data built from Steam depots
  - SKSE 1.5.97 files and Engine Fixes preloader present
- Previous Best of Both Worlds copy: `<GAME_MOUNT>/game/SkyrimSE-1.5.97-bobw`
- Steam install: `~/.local/share/Steam/steamapps/common/Skyrim Special Edition` (1.7.104)
- Depot staging: `~/.local/share/Steam/ubuntu12_32/steamapps/content/app_489830`
- MO2: `<MO2_INSTANCE>`, profile `horizon`
- Enabled mods: Community Shaders Dev, Address Library, SSE Engine Fixes, preloader
- All optional CS features disabled; HorizontalCurvature enabled at boot
- Clean install has no CC/AE content; the full mod list will need CC + BEES later

## Fixes applied

- Empty `horizon/skyrimprefs.ini` caused startup crashes; replaced with working INIs
- Engine Fixes preloader `d3dx9_42.dll` placed in game root
- Missing `Skyrim - Patch.bsa` restored from the 1.5.97 depot
- Best of Both Worlds data mix caused New Game / `coc` infinite loads; clean 1.5.97 data fixed cell loading
- CS shader failure `X4578: cbuffer bank 12 used more than once` fixed:
  - Lighting reads `CameraPosAdjust` through the existing `VS_PerFrame` b12 view
  - DistantTree passes `FrameBuffer::CameraPosAdjust` explicitly
- CS shader cache cleared after each shader edit

## Current diagnostics (uncommitted)

- `HorizontalCurvature.hlsli`: settings-based `ComputeDrop`
- `Lighting.hlsl`
  - `VS_PerFrame` exposes `CameraPosAdjust` at `c40` when not skinned
  - LOD apply: `ComputeDrop` plus temporary hardcoded `+5000` Z drop
  - Temporary clip-space X shift: `vsout.Position.x += 0.25 * w`
  - Debug tint by computed drop: blue = no drop, red = strong
- `DistantTree.hlsl`
  - LOD apply: `ComputeDrop` plus temporary hardcoded `+5000` Z drop
  - Temporary clip-space X shift
  - Debug tint by computed drop

## Findings

- `coc Riverwood` works on the clean 1.5.97 install
- Debug tint reaches LOD terrain, trees, and some close LOD objects (e.g. watchtower roofs)
- Close LOD objects show blue because they are inside the cutoff
- The 25% clip-space X shift moves LOD geometry and tracks camera rotation, proving the LOD vertex block executes
- Pending: the hardcoded +5000 world-Z test and whether settings/sliders reach the shader

## Next steps

1. Confirm whether the hardcoded Z shift moves LOD down as well as sideways
2. If world Z works, investigate why the `b6` settings do not appear to affect the color/displacement
3. Remove all temporary diagnostics
4. Add real UI:
   - Runtime `Enable Effect` toggle (no shader recompile)
   - Topographic debug-color toggle
   - Lower `InnerRadiusCutoff` minimum/default for exaggerated testing
5. Extend the `b6` struct with `Enable` + `DebugColor` and rebuild via CI
6. Re-enable only the needed CS features, then handle CC + BEES for the full mod list

## Launcher note

`cmd.exe /c launch-mo2.bat` stayed alive because the batch waited on MO2. The batch now uses:

```bat
start "" "%~dp0ModOrganizer.exe" %*
```

so the wrapper cmd exits immediately while MO2 continues under Wine.
