# Horizontal Curvature — Critic Brief

Date: 2026-11-10, updated 2026-11-11 after the first RenderDoc capture session
Purpose: a self-contained packet for an independent model (Claude) to critique the
current diagnosis and plan. You are the critic. Your job is to find the weakest
inference, the missing experiment, and any claim that is unfalsifiable or
contaminated by test state. Do not simply agree with the plan.

---

## 0. What changed in this session (2026-11-10 → 2026-11-11)

- `package/Shaders/DistantTree.hlsl`: HC now runs before the `RENDER_DEPTH`
  split and the depth branch uses the warped world position. Locally validated
  (9 variants, 0 warnings/errors).
- `package/Shaders/Lighting.hlsl`: pre-existing working-tree change validated
  locally (535 variants, 0 warnings/errors).
- `package/Shaders/Utility.hlsl`: added the temporary `HC_PROBE` depth-only
  clip-X block for the Utility LOD/depth path. Validated with `HC_PROBE`
  (446 variants, 0 warnings/errors).
- Deployed the `ea9930e6` AIO (safe, no skips) and copied the edited shaders into
  `mods/Community Shaders Dev/Shaders/`; deleted the relevant shader-cache
  directories.
- No C++ changes; the deployed DLL is still `ea9930e6`.
- Local shader validation was set up (hlslkit + Wine + the installed `fxc.exe`).
- RenderDoc now works: the settings block had been written under the short name
  `"DeveloperTools"` instead of the feature display name `"Developer Tools"`, so
  capture was silently disabled. Fixed; the bundled 1.43 DLL loads and F12
  captures. Captures are in
  `overwrite/SKSE/Plugins/CommunityShaders/Captures`, ~650 MB each.
- First in-game observations are in §5.1.
- A runtime preset cycler and an `HC_IDENTITY` positive-control pass are proposed
  in §7.1. Neither is implemented yet.
- No CI run: November usage is ~2,141 billed Windows minutes and the remaining
  quota is unconfirmed.

---

## 1. What the feature is trying to do

Bend distant terrain LOD, object LOD, and tree LOD downward in the vertex shader
to suggest a planet-sized horizon. Loaded cells, actors, physics, water, and sky
are out of scope. The effect is visual only.

Repo: `HorizontalCurvatureSkyrim`, a Community Shaders fork. The feature is
`HORIZONTAL_CURVATURE`, compiled into `Lighting.hlsl` and `DistantTree.hlsl`.
Settings live in a shared `b6` FeatureData buffer.

---

## 2. The symptom

When HC moves opaque LOD geometry, a static grey/black weather-tinted shell
remains at the original position. Transparent/decal/moss overlays move and stay
visible; opaque geometry often disappears or is replaced by the grey shell.
Changing weather changes the grey's color. No Lighting Pixel shader removes it;
blocking the shared Lighting Vertex shader does.

The current belief: this is a **depth / deferred G-buffer coherence** problem —
the color pass uses warped vertices, but the pass that writes depth or the
G-buffer still uses unwarped ones, so the moved pixels fail depth and the
background/fog shows through at the old silhouette.

---

## 3. What is actually proven

- `HORIZONTAL_CURVATURE` is compiled into Lighting VS and DistantTree VS/PS
  (temporary `#error` diagnostics).
- `FeatureData` is `b6`; the VS binding was missing at one point and was fixed
  (`src/VanillaBinding.cpp:16-18`).
- A temporary **clip-space X shift** visibly moved LOD geometry. This proves the
  VS executes for the LOD color path. It is screen-space and is not curvature.
- A stale `overwrite/ShaderCache` was a real false negative; CS can log
  `Using disk cache` and run old shaders.
- **DistantTree applied HC only in the color branch.** The block sat inside the
  `#else` of `#ifdef RENDER_DEPTH`, so the depth permutation never warped tree
  vertices. This is a real code bug and was fixed in the working tree on
  2026-11-10.
- **Lighting applies HC outside the `RENDER_DEPTH` guard**
  (`package/Shaders/Lighting.hlsl:194-240`). Therefore the Lighting depth
  permutation should warp too, *if* `b6` is bound at that draw.
- **Lighting LOD defines are not missing.** `src/ShaderCache.cpp:180-183`
  delegates to the engine's vanilla define function; the LOD technique selects
  `LODLANDSCAPE` / `LODLANDNOISE` / `LODOBJECTS` / `LODOBJECTSHD`. The earlier
  claim that "the LOD type defines are not present in the VS permutations" is
  probably a contaminated test.
- `Grass Collision` is the working precedent for ordering: it applies
  displacement to `msPosition` **before** the `#ifdef RENDER_DEPTH` split, so
  depth and color use the same position (`package/Shaders/RunGrass.hlsl:195-207`).

---

## 4. What is reported but not independently verified

- The grey layer is the background/fog composited where moved opaque geometry
  fails the *unwarped* depth test.
- Skipping the real z-prepass (`REL::RelocationID(35560, 36559) + 0x395`) did not
  remove the grey.
- `ToggleZPrepassOutput` hides the grey and shows only sparse prepass geometry.
- "Only ~32 units of visible movement" — likely the alpha/decal overlay, not the
  opaque drop.
- `ApplyToAllLighting=ON` moves full-detail geometry and produces grey.

These are hypotheses derived from in-game observation, often with several
controls changed at once. Treat them as weak evidence.

---

## 5. The contradiction that matters

A **b6-independent hardcoded clip-X shift did not move the grey layer or
full-detail trees**, while the clip-X positive control moved LOD.

Therefore "the custom VS executes" is proven for the LOD color path, not for the
depth/G-buffer path. The current plan must resolve whether the failing pass uses
the custom VS at all before changing more shader code.

---

## 5.1 New evidence from the first RenderDoc session (2026-11-11)

Eight F12 captures were taken at a fixed camera with the current build. The last
four are intended as the `#0`–`#3` sequence (baseline, Real HC, Clip X, reset);
the earlier four are preliminary. The key capture is **Real HC** (world-Z drop,
`DebugColor` off), which is the only one that shows the grey shell.

Observed in-game with the current build:

- **Clip X (`DebugVertexMode = ClipX`, amount 0.25):** distant terrain LOD and
  the **transparent outlines** of object LOD (e.g., Whiterun wall outlines) move.
  The opaque object LOD/wall bodies do not visibly move. No grey shell appears
  in this mode.
- **DebugColors:** only the distant terrain LOD changes color. Object LOD/walls
  and trees do not.
- **Real HC (world-Z drop, `DebugColor` off):** the grey layer appears on the
  distant terrain/LOD surfaces. Clip X does not reproduce it.

Working interpretation:

- The opaque object LOD is likely **not using the Lighting
  `HORIZONTAL_CURVATURE` color/VS path**; it may be Utility/G-buffer or
  full-detail geometry. The transparent outlines are an alpha/decal overlay that
  does use the HC path. This is consistent with H4 and with the earlier NIF
  finding (moss/decal overlay moves; opaque base does not).
- `DebugColors` tinting only terrain is consistent with the debug tint living in
  the Lighting PS LOD block. If object LOD's visible color is produced by
  Utility/G-buffer/composite, it will not tint.
- Clip X not producing the grey is expected, not a fix: Clip X shifts the color
  and depth permutations together (when `b6` is bound), so they stay coherent.
  The world-Z HC is the one that exposes a depth-path mismatch.
- Caveat: the walls may be full-detail at the tested distance. With
  `ApplyToAllLighting` off, full-detail is deliberately excluded, so "not moving"
  would be expected. The identity pass in §7.1 is designed to distinguish
  full-detail from a different LOD shader.

The Real HC capture still needs the decisive RenderDoc analysis:

1. Pixel-history a grey pixel: which event wrote the grey color?
   - Utility draw → H4 confirmed.
   - Lighting draw → inspect `b6` and `Enable`.
   - No color draw → hole/background; find the depth writer.
2. Find the opaque Whiterun wall draw. Is it `Lighting`, `Utility`, or something
   else? Does its VS/PS include HC? This explains the missing movement/tint.
3. Same vertex: compare `SV_Position.z` in the depth draw and the color draw.

---

## 6. Hypotheses, ranked

| # | Hypothesis | Evidence for | Evidence against | How to falsify |
| --- | --- | --- | --- | --- |
| H1 | Depth / G-buffer pass writes unwarped positions while color writes warped ones | Grey at old silhouette; no PS removes it; blocking shared Lighting VS affects it; `ToggleZPrepassOutput` hides it; DistantTree had a literal `RENDER_DEPTH` guard bug | Skipping the real z-prepass did not remove it | RenderDoc: compare `SV_Position.z` and depth-buffer value for the same vertex/pixel in depth, G-buffer, and color passes |
| H2 | `b6` is unbound at the depth / G-buffer draw, so `Enable` reads 0 there | Binding "too early" history; per-draw resets; depth draw is a different path | `BSLightingShader_SetupGeometry` now binds after `func()` | Same RenderDoc capture: inspect the `b6` slot contents at that draw |
| H3 | The grey is the background/fog showing through depth-failed pixels, not a second mesh | Weather changes its color; z-prepass output view hides it; transparent overlays survive | Not directly measured | Read the depth buffer at a grey pixel; compare with the moved VS output |
| H4 | The Lighting depth/G-buffer draw uses a different shader (or the vanilla shader) entirely | Clip X moves only terrain + transparent outlines; DebugColors only tints terrain; opaque object LOD does not move/tint; would explain why binding timing did not help | No direct shader-key evidence yet for the wall draw; walls may be full-detail | RenderDoc identify the wall draw's shader; identity pass (§7.1) |
| H5 | The LOD define logic falls through in the VS permutation | One in-game report says per-type terrain switch does nothing | Code says defines come from the engine's vanilla function; `ApplyToAllLighting` works | Dump the actual compiled defines/keys for a LOD draw |
| H6 | The math is wrong | User sees little movement | Clip-X and WorldZ probes move geometry; the math is simple and validated by compilation | Constant-drop probe; compare predicted vs measured drop |
| H7 | Stale cache / feature boot state still contaminates tests | Past false negatives | Cache is now cleared selectively; feature boot state is known | Record `describe:` + cache state for every run |
| H8 | Opaque object LOD is full-detail (not LOD) at the tested distance, so it is correctly excluded | DebugColors does not tint it; Clip X does not move it; `ApplyToAllLighting` is off | User describes it as distant LOD; not confirmed | Identity pass + RenderDoc wall draw shader; enable `ApplyToAllLighting` as a control |

---

## 7. The proposed next experiment (critique this)

**Phase 0, one build, no tuning:**

1. Fix DistantTree so HC runs before the `RENDER_DEPTH` split and the depth branch
   uses the warped world position. **Already applied** in the working tree.
2. Deploy the existing `ea9930e6` AIO (C++ unchanged; no CI build needed) and copy
   the locally validated shaders into the mod.
3. Run a single-variable coherence probe: `DebugVertexMode = World Z`,
   `DebugVertexAmount = 0.2` (2000 units), `CurvatureStrength = 0`,
   `DebugColor = off`, `ApplyToAllLighting = off`, all other controls neutral.
4. Observe terrain LOD, object LOD, and tree LOD separately:
   - one coherent surface, no grey → the depth/G-buffer path sees `b6` and the
     warped VS; coherence is solved for that type.
   - moves in color, grey shell at old position → the depth/G-buffer path is not
     using the warped position; find out why.
   - nothing moves → `b6`/`Enable` is not reaching the VS in this configuration.
5. RenderDoc capture: same camera/weather, HC off vs probe on. For the same draw,
   compare the depth-prepass, G-buffer, and color passes: shader key, `b6` slot,
   `SV_Position.z`, depth-buffer value. Pixel history on a grey pixel and a moved
   pixel.

**Definition of done:** a hardcoded world-space drop on LOD terrain, object LOD,
and tree LOD produces one coherent surface with no grey shell and no z-fighting
from a fixed camera, and RenderDoc confirms the depth/G-buffer and color draws use
the same warped position.

---

## 7.1 Proposed test harness (critique this)

Two ideas were proposed after the first capture session. Neither is implemented.

### A. Runtime preset cycler

- Add a `TestPreset` index and a preset table to `HorizontalCurvature` (Baseline,
  RealHC, ClipX, WorldZ, DebugColors, ApplyAll, Raise, ...).
- Bind **Shift+F12** to “cycle preset + capture”. `RenderDoc::HandleCaptureHotkey`
  returns false when Ctrl/Shift/Alt is held, so Shift+F12 is free and does not
  collide with the existing F12 capture.
- On cycle: log the full settings, call
  `renderDoc.SetPendingCaptureComments(presetName)`, then
  `renderDoc.CaptureNow()`.
- Add a “Cycle test preset” button and current-preset display in the feature UI.
- Only `b6` uniforms can change at runtime. `Shader Defines` / `HC_PROBE` /
  `HC_IDENTITY` require a recompile and restart.
- Cost: a small C++ change and a DLL build (CI quota or the local Windows build).

### B. Identity control (“debug color of everything”)

- Add an `HC_IDENTITY` define (set via `Advanced → Shader Defines = HC_IDENTITY`,
  restart).
- Lighting outputs flat **red**; DistantTree flat **green**; Utility (LOD/G-buffer)
  flat **blue**.
- One frame then shows which shader draws each part: opaque wall body,
  transparent outline, terrain LOD, trees. This is the positive identity control.
- Caveat: Utility is also used for shadow/depth/G-buffer fills; gate the tint to
  `LOD_LANDSCAPE`, `LOD_OBJECT`, `TREE_ANIM`, `RENDER_NORMAL` /
  `RENDER_BASE_TEXTURE`, and verify it survives the deferred composite.
- Cost: shader-only + restart; no DLL build.

### Questions for the critic on the harness

- Does the §5.1 evidence confirm H4, or is H8 (full-detail) equally likely? What
  single measurement distinguishes them?
- Is a flat-color identity pass a valid positive control under the deferred
  renderer, or can it lie (albedo tint overwritten, shadows tinted, composite
  neutralizes it)? How would you validate the control itself?
- Is the runtime cycler worth a C++ build given the CI quota, or is manual preset
  + RenderDoc comment enough?
- What should the preset table contain, and which presets should auto-capture?
- Captures are ~650 MB each; what capture policy avoids filling the disk?

---

## 8. Questions for the critic

Be specific and adversarial:

1. Which inference in §3–§5 is weakest, and what would you measure instead?
2. Is H1 actually distinguishable from H3 with the proposed probe? If not, design
   the measurement that separates "unwarped depth" from "background showing
   through".
3. The z-prepass skip did not remove the grey. Does that falsify H1, or only the
   specific skip location? What does it imply about the G-buffer path?
4. Why would `BSLightingShader_SetupGeometry` binding `b6` after `func()` fail to
   cover the depth draw? What else resets VS constants between setup and draw?
5. Is the DistantTree fix sufficient, or does changing the depth branch from
   `WorldViewProj * finalModelPosition` to `CameraViewProj * worldPosition`
   introduce a matrix mismatch for shadows, cubemaps, or first-person? How would
   you verify?
6. Which single test most reduces uncertainty per unit of effort?
7. What claim in the investigation log is unfalsifiable or likely contaminated?
8. If you had to bet on the root cause, what is it, and what result would change
   your mind?
9. Does the §5.1 observation (Clip X moves only terrain + transparent outlines;
   DebugColors only tints terrain) confirm H4, or is full-detail gating (H8) an
   equally good explanation? What measurement separates them?
10. Is the `HC_IDENTITY` flat-color pass a trustworthy positive control under the
    deferred renderer? Could the G-buffer/composite make it lie?
11. Is the Shift+F12 preset cycler worth a DLL build, or should the preset table
    stay manual?
12. What is the single most informative analysis of the existing Real HC capture?

---

## 9. Constraints and current environment

- **No CI builds.** The repo is private; November usage is ~2,141 billed Windows
  minutes. The last full Fork Build cost 159.1 billed minutes. The plan's remaining
  quota is unknown (the `gh` token lacks the `user` scope). Do not propose a CI run
  unless it is the only option.
- **Local shader validation works.** `hlslkit` + Wine + the installed `fxc.exe`.
  `DistantTree.hlsl` (9 variants), `Lighting.hlsl` (535 variants), and
  `Utility.hlsl` with `HC_PROBE` (446 variants) all passed with 0 warnings/errors
  on 2026-11-10/11.
- **RenderDoc works in-process.** The bundled 1.43 DLL loads and F12 captures.
  The settings block must be under the feature display name `"Developer Tools"`,
  not the short name `"DeveloperTools"`; the latter silently disables capture.
  Captures are ~650 MB each in
  `overwrite/SKSE/Plugins/CommunityShaders/Captures`; eight already exist
  (~5.1 GB).
- **Local C++ build is not set up.** The Windows install at `<WINDOWS_MOUNT>` has
  VS 2022 BuildTools, Windows SDK 10.0.26100, MSBuild, and `fxc`. The practical
  path is booting Windows and building with `ALL-VS2022` (or a local
  `PR-VS2022` preset). No VM is configured; the host has 15 GB RAM.
- **Deployed state:** `ea9930e6` AIO (`CommunityShaders.dll` 35,377,664 bytes,
  2026-11-05) + edited `DistantTree.hlsl`, `Lighting.hlsl`, and `Utility.hlsl`
  copied into `mods/Community Shaders Dev/Shaders/`.
  `overwrite/ShaderCache/Lighting`, `DistantTree`, and `Utility` were deleted.
  Test profile: `HorizontalCurvatureTest`, `launch-hctest.bat`. Active settings:
  `Developer Tools → RenderDoc` enabled, `Shader Defines` empty, HC neutral/off.
- **Do not add runtime toggles.** The problem is not a missing control.
- **Do not change the math** (mode, cutoff, elevation, tree anchor) until the
  coherence probe passes.

---

## 10. Files to read

| Path | Why |
| --- | --- |
| `RESUME.md` | the current execution brief and Phase 0 plan |
| `docs/horizontal-curvature-investigation.md` | chronological in-game reports, shader keys, contradictions |
| `docs/horizontal-curvature-architecture.md` | implemented architecture and data flow |
| `design.md` | original intent and math |
| `package/Shaders/Lighting.hlsl:180-250` | HC VS block, LOD gating, depth guard |
| `package/Shaders/DistantTree.hlsl:55-125` | HC before/after the `RENDER_DEPTH` split |
| `package/Shaders/Utility.hlsl:118-175` | Utility depth path, LOD nudge, `HC_PROBE` |
| `package/Shaders/RunGrass.hlsl:180-235` | working precedent: displace before the depth/color split |
| `features/Grass Collision/Shaders/GrassCollision/GrassCollision.hlsli:102-174` | model-space displacement, previous position, stretch clamp |
| `src/Hooks.cpp:160-280` | `BeginTechnique` / `SetupGeometry` `b6` binding |
| `src/VanillaBinding.cpp:8-25` | shared constant-buffer binding |
| `src/Deferred.cpp:217-340` | z-prepass, G-buffer start, depth copy, composite |
| `src/Features/TerrainBlending.cpp:259-420` | precedent for a separate depth target and `RenderPassImmediately` |
| `src/Features/RenderDoc.cpp:741-760` | capture hotkey (F12/PrintScreen only when unmodified) |
| `src/Features/RenderDoc.h:43-57` | capture, comments, and capture-path API |
| `src/Menu.cpp:1240-1260` | feature hotkey dispatch point |
| `docs/horizontal-curvature-troubleshooting-flow.md` | in-game flow and decision tree |
| `docs/horizontal-curvature-shader-keys.md` | active shader-key observations |

---

## 11. One-paragraph ask

Read the files above. Then answer §8 directly, including the new questions about
the §5.1 capture evidence and the §7.1 harness. Rank the hypotheses by evidence,
name the weakest inference, and propose the single most informative experiment
(with acceptance criteria) that fits the no-CI constraint. If the Phase 0 probe
or the identity pass is the wrong experiment, say so and replace it. Do not
restate the plan; critique it.
