# Horizontal Curvature — Resume Brief (Phase 0/1)

Written 2026-11-10 for a fresh agent (Sonnet). This file **supersedes** the prior
resume. The chronological history, shader keys, and past in-game reports remain
in `docs/horizontal-curvature-investigation.md`; treat those reports as
hypotheses, not facts.

Your job is not to add more toggles. It is to answer one question with evidence:

> **Does the pass that writes depth / the deferred G-buffer use the same warped
> vertex position as the pass that writes color?**

Until that is answered, every visual test is ambiguous.

---

## 0. Operating rules (read first)

1. **One variable per run.** Do not combine `ApplyToAllLighting`, `DebugHue`,
   `Raise`, strength, cutoff, and skip flags in one test. Past conclusions are
   contaminated by exactly that.
2. **Record the build hash and the cache state.** A result without
   `describe:` and a statement about `ShaderCache` is not evidence.
3. **If a result contradicts the code, the test state is wrong until proven
   otherwise.** Re-read the shader and C++ before inventing a new theory.
4. **Prefer numbers to impressions.** Shader key, constant-buffer value,
   `SV_Position.z`, depth-buffer value, world-space displacement.
5. **Do not add runtime toggles.** The remaining problem is not a missing
   control.
6. **Do not ship or test with the `Main_RenderDepth` / `Main_RenderShadowmasks`
   skip code.** It is unsafe with HC enabled. See §2.

---

## 1. Current state and safe baseline

- Repo: `<REPO_ROOT>`
- Branch: `horizontal-curvature`
- Remote: `git@github.com:PipettingBeaver/horizontal-curvature-skyrim.git`
  (passphrase-protected SSH; `gh` HTTPS works for downloads/runs but lacks the
  `workflow` scope)
- HEAD at the time of writing: `ea9930e6 feat(horizontal-curvature): add apply switches`
- Feature version: `0-5-0` (`features/HorizontalCurvature/Shaders/Features/HorizontalCurvature.ini`)
- CI: `.github/workflows/build-fork.yml` (GitHub-hosted Windows runner; local
  machine does not need to stay on). **CI quota is nearly exhausted** (see §6);
  do not run a Fork Build without checking billing first.
- **Currently deployed (2026-11-10):** the `ea9930e6` AIO (safe; no debug skips)
  plus the working-tree `DistantTree.hlsl` and `Lighting.hlsl` copied into
  `mods/Community Shaders Dev/Shaders/`. `overwrite/ShaderCache/Lighting` and
  `DistantTree` were deleted. The old `26917ab3` skip build is no longer
  deployed.
- In the reduced hctest profile HC is loaded at boot but the runtime `Enable` is
  false; normal rendering is restored. Do not re-enable HC on a build containing
  the old skips.
- **Utility probe:** `package/Shaders/Utility.hlsl` now has an `HC_PROBE`
  clip-X block gated to `RENDER_DEPTH` + LOD/tree. It is copied to the deployed
  mod and validated with `HC_PROBE` (446 variants, 0 warnings/errors).
- **Test settings:** `SettingsUser.json.bak.hctest` now enables Developer
  Tools/RenderDoc, keeps `Shader Defines` empty, and leaves HC neutral/off.
  `launch-hctest.bat` applies it. The in-game flow is
  `docs/horizontal-curvature-troubleshooting-flow.md`.
- **Critic packet:** `docs/horizontal-curvature-critic-brief.md`.

### Safe settings to restore normal play

```json
"Disable at Boot": { "HorizontalCurvature": true },
"Horizontal Curvature": { "Enable": false },
"Advanced": { "Developer Mode": false, "Frame Annotations": false }
```

---

## 2. Ground truth

### Verified in code

- `HORIZONTAL_CURVATURE` is compiled into Lighting VS and DistantTree VS/PS.
- `FeatureData` is register `b6`; the VS binding was a real past bug
  (`src/VanillaBinding.cpp:16-18`).
- **DistantTree applied HC only in the non-depth branch.** In
  `package/Shaders/DistantTree.hlsl:69-107`, the HC block was inside the `#else`
  of `#ifdef RENDER_DEPTH`, so the depth permutation never warped tree vertices.
  **Fixed in the working tree on 2026-11-10**: the HC block now runs before the
  split and the depth branch uses the warped world position. The edited shader
  passes local validation (9 variants, 0 warnings/errors); the C++ is unchanged
  from the deployed `ea9930e6` DLL.
- **Lighting applies HC outside the `RENDER_DEPTH` guard.** In
  `package/Shaders/Lighting.hlsl:194-240`, the block runs for the depth
  permutation too, *if* `b6` is bound at that draw. So the remaining Lighting
  grey is either a `b6` binding failure at the depth/G-buffer draw, or a
  different shader/path being used there.
- **Lighting LOD defines are not missing.** `src/ShaderCache.cpp:180-183`
  delegates Lighting defines to the engine's vanilla function; the LOD
  technique selects `LODLANDSCAPE` / `LODLANDNOISE` / `LODOBJECTS` /
  `LODOBJECTSHD`. The investigation log's claim that "the LOD type defines are
  not present in the VS permutations" is almost certainly a contaminated test,
  not a code fact.
- `Grass Collision` shows the correct pattern: it applies displacement to
  `msPosition` **before** the `#ifdef RENDER_DEPTH` split, so depth and color use
  the same position (`package/Shaders/RunGrass.hlsl:195-207`). Use that ordering
  as the template.

### Reported but not independently verified

- The grey layer is the background/fog composited where moved opaque geometry
  fails the *unwarped* depth test.
- Skipping the real z-prepass (`REL::RelocationID(35560, 36559) + 0x395`) did not
  remove the grey.
- `ToggleZPrepassOutput` hides the grey and shows only sparse prepass geometry.
- "Only ~32 units of visible movement" — likely the alpha/decal overlay, not the
  opaque drop.
- `ApplyToAllLighting=ON` moves full-detail geometry and produces grey.

### The contradiction that matters

A b6-independent hardcoded clip-X shift did **not** move the grey layer or
full-detail trees, while the clip-X positive control moved LOD. So "the custom
VS executes" is narrower than previously stated: it is proven for the LOD color
path, not for the depth/G-buffer path. Resolve this before tuning the math.

---

## 3. Phase 0 — the decisive experiment

**Goal:** prove whether the depth/G-buffer and color paths agree. Do not tune
curvature, cutoff, elevation, or modes in this phase.

### 3.1 The DistantTree depth fix (already applied)

`package/Shaders/DistantTree.hlsl` now computes the HC displacement **before**
the `RENDER_DEPTH` split, and the depth branch uses the warped world position.
The applied shape is:

```hlsl
float4 finalModelPosition = float4(input.InstanceData1.xyz + adjustedModelPosition.xyz, 1.0);
float4 viewPosition = mul(WorldViewProj, finalModelPosition);

float4 worldPosition = mul(World, finalModelPosition);
float4 previousWorldPosition = mul(PreviousWorld, finalModelPosition);
bool hcApplied = false;

#if defined(HORIZONTAL_CURVATURE)
    // HC block on worldPosition / previousWorldPosition.
    // hcApplied = true only when a displacement or an explicit probe offset occurs.
#endif

if (hcApplied)
    viewPosition = mul(FrameBuffer::CameraViewProj, worldPosition);

// clip-space debug offsets (modes 1/2) are applied here, before the split

#ifdef RENDER_DEPTH
    vsout.Depth.xy = viewPosition.zw;
    vsout.Depth.zw = input.InstanceData2.zw;
#else
    vsout.WorldPosition = worldPosition;
    vsout.PreviousWorldPosition = previousWorldPosition;
    vsout.ViewPosition = viewPosition;
#endif

vsout.Position = viewPosition;
```

Notes:

- The working-tree change is not committed or deployed; build it as part of
  Phase 0.
- `hcApplied` is true only when the displacement is non-zero or a debug probe is
  active, so an enabled-but-zero-drop tree keeps the original `WorldViewProj`
  path.
- `previousWorldPosition` is needed only for motion vectors in the color pass;
  the depth permutation computes it but does not use it.
- **Risk:** the depth pass may not bind `FrameBuffer::CameraViewProj`. Verify in
  RenderDoc. If the matrices differ, the depth tree may shift even for a zero
  drop; in that case use `CameraViewProj` only for the warped case and keep
  `WorldViewProj` for the unwarped case.
- This fix is necessary but not sufficient: if `b6` is unbound in the depth pass,
  `Enable` still reads 0 and `hcApplied` stays false.

### 3.2 Probe A — Lighting/DistantTree clip-X (runtime, no new code)

The authoritative in-game flow is
`docs/horizontal-curvature-troubleshooting-flow.md`. Probe A is the existing
`DebugVertexMode = Clip X` test; it is b6-driven and bypasses the per-type
gating.

Deploy the clean build with the DistantTree fix. In the in-game UI, set exactly:

| Setting | Value |
| --- | --- |
| Enable | on |
| DebugColor | **off** |
| CurvatureStrength | `0` |
| InnerRadiusCutoff | `0` |
| ElevationScaleFactor | `0` |
| CutoffSoftness | `0` |
| ApplyToAllLighting | **off** |
| ApplyTerrain / ApplyObjects / ApplyTrees | on |
| All per-LOD and full-detail flags | leave defaults |
| **DebugVertexMode** | **Clip X** |
| **DebugVertexAmount** | **0.25** |
| Raise | off |

`DebugVertexMode != 0` forces the HC vertex block regardless of the per-type
flags (`Lighting.hlsl:215-216`; `DistantTree.hlsl:79-80`). That is intentional:
this is a producer-identification probe, not a per-type selection test. World Z
at `0.2` is the alternative coherence sanity check; it is not the main test.

Stand still in the Whiterun tundra with clear weather (`sw 0010199f`) and the
camera at a fixed position/rotation. Record:

- **Terrain LOD:** does the surface move as one piece, or does a grey shell stay
  behind?
- **Object LOD (rocks/mountains):** same question.
- **Tree LOD:** same question. The DistantTree fix should make this coherent; if
  it does not, `b6` is not bound in the DistantTree depth pass.

Interpretation:

| Terrain/object result | Tentative conclusion |
| --- | --- |
| Moves as one piece, no grey | Lighting/DistantTree depth and color are coherent for that type. Tune the math. |
| Moves in color, grey shell at old position | The grey producer is **not** the Lighting/DistantTree color VS. Go to Probe B. |
| Nothing moves | `b6`/`Enable` is not reaching the VS at all in this configuration; check the build, cache, and feature boot state; then use RenderDoc. |

Do **not** change other controls during this run.

### 3.3 Probe B — Utility depth clip-X (shader-only, restart required)

`package/Shaders/Utility.hlsl` now has a temporary `HC_PROBE` block gated to
`RENDER_DEPTH && !RENDER_SHADOWMAP && (LOD_LANDSCAPE || LOD_OBJECT || TREE_ANIM)`.
It shifts `vsout.PositionCS.x += 0.25 * w`, so it moves only the Utility
depth-pass LOD/tree geometry. The block is validated with `HC_PROBE`
(446 variants, 0 warnings/errors).

In-game:

1. Set `Enable = off`, `DebugVertexMode = none`.
2. Set `Advanced → Shader Defines = HC_PROBE`.
3. Restart the game. If the Utility shaders still load from disk, delete
   `overwrite/ShaderCache/Utility` and restart again.

Because HC is off, Lighting/DistantTree do not move. Outcomes:

- Grey shifts/moves with the Utility depth → Utility owns the depth/G-buffer path
  (Claude's H4). Fix Utility;
- Nothing changes → Utility is not the producer. Go to RenderDoc;
- The world becomes unreadable → narrow the gate (`LOD_LANDSCAPE` only, then
  `LOD_OBJECT`, then `TREE_ANIM`) and repeat.

Reset: `Shader Defines = ""`, restart; clear `ShaderCache/Utility` if needed.

### 3.4 RenderDoc capture (the number that settles it)

Capture two frames at the same camera/weather: one with HC off, one with the
probe under investigation on. Use the Renderdoc developer feature or attach
RenderDoc to the process.

For the same terrain/rock draw, compare the depth-prepass draw, the deferred
G-buffer draw, and the color draw:

- Which VS is bound? (shader key/name)
- Is `b6` bound at draw time? What does `Enable` read as?
- What is `SV_Position.z` at the same vertex?
- What is the depth-buffer value at the same pixel?
- Use pixel history on one grey pixel and one moved pixel.

Write the answers down. "It looks grey" is not a finding; "the depth draw used
VS X with b6 = 0 while the color draw used VS Y with b6 = 1" is.

If RenderDoc shows the depth/G-buffer draw is not using the custom VS at all,
do **not** keep editing the shader. Hook the pass that actually writes it
(`BSBatchRenderer::RenderPassImmediately` is already hooked by
`TerrainBlending`, `InteriorSun`, and `LightLimitFix`; follow one of those
patterns) or fall back to a Terrain-Blending-style separate depth target.

---

## 4. Phase 1 — fix the failing pass, then stop

Only after Phase 0 tells you which pass is wrong:

- **If `b6` is unbound at the depth/G-buffer draw:** bind it as late as possible
  at that actual draw. `src/Hooks.cpp:252-266` binds in
  `BSLightingShader_SetupGeometry` after `func()`; confirm that hook is on the
  failing path. `BSShader_BeginTechnique` alone is too early.
- **If the depth pass uses a different shader:** hook the pass or render a
  dedicated warped depth target (see `features/Terrain Blending/` —
  `DepthBlend.hlsl` and `TerrainBlending.cpp:259-420`).
- **If the depth is correct but the composite is wrong:** the grey is the
  deferred composite/background, and the fix belongs in the G-buffer/composite
  path, not in the VS.

**Definition of done for Phase 1:** a constant world-Z probe on LOD terrain,
object LOD, and tree LOD produces one coherent surface with **no grey shell and
no z-fighting** from a fixed camera. Transparent/moss overlays may still differ;
note them separately.

---

## 5. Later phases (gated on Phase 1)

Do not start these until Phase 1 passes.

1. **Math reintroduction:** parabolic only, hue off, one LOD type at a time.
   Then spherical vertical, then spherical full (XZ contraction). Keep the
   constant probe available as a control.
2. **Trees and motion:** base-anchor translation, grass-style sample clamp
   (`lerp` toward the instance origin, as in
   `features/Grass Collision/Shaders/GrassCollision/GrassCollision.hlsli:138-174`),
   previous-position displacement, TAA ghosting check.
3. **Production tuning:** cutoff/elevation/sea level, LOD seam, culling,
   full-feature baseline.
4. **Cleanup:** remove/gate debug hue and probes, bump the feature `.ini` only if
   the settings struct changed, update `design.md`/`architecture.md`.

Estimate for Phases 0–4: **4–7 focused sessions** (low confidence; the
calibration tool has no reference class, n=0). The main uncertainty is whether
the depth pass will accept the custom VS at all.

---

## 6. Build / deploy / test loop

### CI quota (2026-11-10)

- The repo is private, so Actions minutes are billed (Windows runners are 2x).
- November usage so far: ~2,141 billed minutes. The last full Fork Build
  (`ea9930e6`, run `42017811683`) cost **159.1 billed minutes** (95.8 C++ + 63.2
  shader).
- The plan's remaining quota is unknown (the `gh` token lacks the `user` scope).
  Check `https://github.com/settings/billing` → Actions, or:
  `gh auth refresh -h github.com -s user` then
  `gh api /users/PipettingBeaver/settings/billing/actions`.
- **Do not run a Fork Build unless the quota is confirmed.**
- `gh run download <run-id>` costs 0 minutes. The `ea9930e6` artifact is still
  available (58.7 MB, not expired).

### Local shader validation (working 2026-11-10)

`hlslkit` + Wine + the installed `fxc.exe` compiles the CI shader matrix locally:

- `DistantTree.hlsl`: 9 variants, **0 warnings/errors** (~8 s).
- `Lighting.hlsl`: 535 variants, **0 warnings/errors** (~29 s).

Setup in `/tmp/opencode` (disposable): `hlslkit-venv`, the `fxc-wine` wrapper,
and an `aio/Shaders` copy of `package/Shaders` + `features/*/Shaders`. The
wrapper translates `/Fo` and `/I` paths to Wine `Z:` paths. Recreate it if
`/tmp` is cleared. Use this for the fast loop; no CI.

```bash
# Download the CI artifact (no Actions minutes)
PATH="$HOME/.local/bin:$PATH"
gh run download <run-id> -R PipettingBeaver/horizontal-curvature-skyrim \
  -n dist-artifacts -D /tmp/opencode/cs-dist-new

# Deploy
<REPO_ROOT>/tools/deploy-aio.sh \
  /tmp/opencode/cs-dist-new/CommunityShaders_AIO-*.7z

# Invalidate only what changed
cache="<MO2_INSTANCE>/overwrite/ShaderCache"
rm -rf "$cache/Lighting" "$cache/DistantTree"
```

- After editing a `.hlsli` include, either touch the including `.hlsl` files or
  delete their cache directories. `Skip Unchanged Shaders` only compares
  top-level `.hlsl` mtimes.
- Full `rm -rf overwrite/ShaderCache` only for shared includes, feature-set
  changes, or a stubborn stale cache.
- Confirm the DLL in `CommunityShaders.log`:
  `Community Shaders 1.9.0 | branch: horizontal-curvature | describe: <hash>`.
- Launch: MO2 instance `<MO2_INSTANCE>`,
  profile `HorizontalCurvatureTest`, `launch-hctest.bat`. Settings backup
  `SettingsUser.json.bak.hctest`.
- Restore normal play with `launch-full.bat` and
  `SettingsUser.json.bak.fullfeatures`.
- Log: `.../compatdata/489830/pfx/drive_c/users/steamuser/Documents/My Games/Skyrim Special Edition/SKSE/CommunityShaders.log`.
- Developer Mode: CS menu → Settings → Advanced. Shader blocking: Advanced →
  Enable Shader Blocking → Active Shaders table. Performance Overlay: F10.

---

## 7. Evidence template (use for every meaningful run)

```text
Date:
Build / describe:
Shader cache cleared:
Profile / settings backup:
Feature boot state:
Camera / worldspace / weather:
HC controls (all values):
Apply flags:
DebugHue / DebugColor:
Strength / cutoff / softness / raise:
Probe / vertex mode:
Terrain LOD result:
Object LOD result:
Tree LOD result:
Grey shell:
Z-fighting:
Shader keys observed:
b6 bound at depth draw? (RenderDoc):
Depth value vs VS SV_Position.z:
Interpretation:
Next single variable:
```

---

## 8. Stop conditions / what not to do

- Do not run another `ApplyToAllLighting` + `DebugHue` matrix. It cannot
  distinguish a depth mismatch from a color-path mismatch.
- Do not add new settings or UI controls to "isolate" the grey.
- Do not change the curvature math, cutoff, or modes until Phase 1 passes.
- Do not trust a launch that says `Using disk cache` after a shader edit.
- Do not test with the `Main_RenderDepth` / `Main_RenderShadowmasks` skips
  present.
- Do not conclude "the LOD defines are missing" without dumping the actual
  compiled defines/keys.
- Do not treat the clip-space X shift as curvature; it is screen-space and
  rotation-dependent.
- Do not edit `design.md` conclusions to match a test that has not passed.

---

## 9. Open questions

1. **What is the "Open Shaders grass effect" the user means?** I verified that
   the current `open-shaders` feature list has no curvature/space-bending
   feature; the grass effect there is `Grass Collision`. If a different pack is
   meant, get the exact name/link before borrowing its approach.
2. **Is the grey the background/fog or a second representation?** Phase 0's
   depth-vs-VS comparison answers this.
3. **Does the depth pass bind `FrameBuffer::CameraViewProj`?** If not, the
   DistantTree fix needs the fallback described in §3.1.
4. **What is the actual user-visible goal for full-detail objects?** The design
   keeps full-detail cells out of scope. "Full level of detail objects" should
   mean all LOD tiers, not loaded-cell geometry. Confirm before expanding scope.

---

## 10. Definition of done for this milestone

> A hardcoded world-space drop applied to LOD terrain, object LOD, and tree LOD
> produces one coherent surface with no grey shell and no z-fighting from a
> fixed camera, and RenderDoc confirms the depth/G-buffer and color draws use the
> same warped vertex position.

Record the estimate/actual pair when this milestone closes. Keep full-detail
cells, water, and sky out of scope for this milestone.
