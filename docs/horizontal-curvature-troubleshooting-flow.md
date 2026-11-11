# Horizontal Curvature — In-Game Troubleshooting Flow

Date: 2026-11-10
Use with the `HorizontalCurvatureTest` profile and `launch-hctest.bat`.
The goal is to identify **who writes the grey layer**, not to tune curvature.

---

## 0. Preconditions

- Deployed build: `ea9930e6` AIO + the working-tree `DistantTree.hlsl`,
  `Lighting.hlsl`, and `Utility.hlsl`.
- `launch-hctest.bat` applies `SettingsUser.json.bak.hctest`.
- Profile: `HorizontalCurvatureTest`. Loaded: Horizontal Curvature, Developer
  Tools (RenderDoc), Performance Overlay. Everything else is disabled at boot;
  Terrain Blending is **off** in this profile.
- `General → Post Processing Mode = Vanilla`, `Use FileWatcher = true`.
- Host RenderDoc UI: `sudo pacman -S renderdoc` (`qrenderdoc`), if not installed.
- Start the game, then `coc Whiterun` (or `coc Riverwood`), wait for the actual
  world (not the main menu), turn `tfc`/`tcl` off, set clear weather with
  `sw 0010199f`, and stand still. Pick a view with distant terrain/object/tree
  LOD.
- Record the `describe:` hash from
  `CommunityShaders.log` and the shader-cache state before testing.

### Baseline settings (backup state)

| Control | Value |
| --- | --- |
| Enable | off |
| DebugColor | off |
| CurvatureStrength | `0` |
| InnerRadiusCutoff | `0` |
| ElevationScaleFactor | `0` |
| CutoffSoftness | `0` |
| ApplyToAllLighting | off |
| ApplyTerrain / ApplyObjects / ApplyTrees | on |
| DebugVertexMode | none |
| Raise | off |
| Advanced → Shader Defines | empty |
| Developer Mode | on |
| RenderDoc Capture | enabled (F12) |

Capture directory: `Data/SKSE/Plugins/CommunityShaders/Captures` — under MO2 this
is `overwrite/SKSE/Plugins/CommunityShaders/Captures`.

---

## 1. Flowchart

```text
Start (HC off, no probes)
  |
  |-- grey visible with HC off? --yes--> not an HC problem: stop, fix the profile
  |                               \no
  v
Step 1: Probe A = Lighting/DistantTree clip-X (Enable on, DebugVertexMode=ClipX)
  |
  |-- LOD moves AND grey moves/no shell --> Lighting/DistantTree depth+color are coherent
  |                                         for that type -> go to Step 4 (math)
  |
  |-- LOD moves, grey stays at old spot ---> producer is NOT the Lighting/DistantTree
  |                                         color VS -> go to Step 2
  |
  \-- nothing moves -----------------------> b6/Enable not reaching the VS or gated
                                            -> go to Step 3 (RenderDoc, inspect b6)
Step 2: Probe B = Utility depth clip-X (HC_PROBE, HC off, restart)
  |
  |-- grey shifts/moves with Utility depth --> H4 confirmed: Utility owns the
  |                                           depth/G-buffer path -> go to Step 4
  |
  |-- nothing changes -----------------------> Utility is not the producer
  |                                           -> go to Step 3
  |
  \-- world unreadable ---------------------> narrow the probe gate and repeat
Step 3: RenderDoc (baseline vs probe, same camera/weather)
  |
  |-- grey pixel has a color-writing draw? --yes--> inspect that draw's VS,
  |  |                                              b6 slot, Enable value
  |  \-- no draw wrote color -----------------------> hole/background: moved color
  |                                                  is depth-rejected; find the
  |                                                  depth writer
  v
Step 4: identify the pass and fix that pass (not the math)
```

---

## 2. Step 0 — baseline

1. Launch with HC off and no probes.
2. Confirm the grey is absent and the world renders normally.
3. If the grey is present with HC off, stop: the problem is the profile/build, not
   HC.

Record: build hash, weather, camera, whether the grey is present.

---

## 3. Step 1 — Probe A: Lighting/DistantTree clip-X

In-game:

- `Enable = on`
- `DebugVertexMode = Clip X`
- `DebugVertexAmount = 0.25`
- everything else neutral.

This is b6-driven and bypasses the per-type gating. It moves the Lighting and
DistantTree color VS (and their depth permutations **if** `b6` is bound there).

Watch terrain LOD, object LOD, and tree LOD separately.

### Outcomes

| Observation | Interpretation | Next |
| --- | --- | --- |
| LOD moves and the grey moves with it (or no grey shell) | Lighting/DistantTree depth and color are coherent for that type; the pipeline is not the blocker | Step 4: tune the math/coverage |
| LOD moves in color, grey stays at the old position | The grey producer is **not** the Lighting/DistantTree color VS | Step 2 |
| Nothing moves | `b6`/`Enable` is not reaching the VS, or the probe is not active | Step 3, inspect `b6`; check build/cache/feature state |
| Only trees misbehave | DistantTree depth fix or `CameraViewProj` binding issue | Step 3, inspect the DistantTree depth draw |

Reset: `Enable = off`, `DebugVertexMode = none`.

---

## 4. Step 2 — Probe B: Utility depth clip-X

This is a compile-time probe added to `package/Shaders/Utility.hlsl`. It is gated
to `HC_PROBE && RENDER_DEPTH && !RENDER_SHADOWMAP && (LOD_LANDSCAPE || LOD_OBJECT
|| TREE_ANIM)` and shifts `vsout.PositionCS.x += 0.25 * w`.

In-game:

1. Set `Enable = off`, `DebugVertexMode = none`.
2. Set `Advanced → Shader Defines = HC_PROBE`.
3. Restart the game. If the Utility shaders still load from disk, delete
   `overwrite/ShaderCache/Utility` and restart again.

Because HC is off, Lighting/DistantTree do not move. Only the Utility depth-pass
LOD/tree geometry shifts.

### Outcomes

| Observation | Interpretation | Next |
| --- | --- | --- |
| The grey shifts/moves with the Utility depth | `Utility` owns the depth/G-buffer path (Claude's H4) | Step 4: fix Utility |
| Nothing changes; the grey stays | Utility is not the producer | Step 3: RenderDoc |
| The world becomes unreadable | The probe hit more than LOD | Narrow the gate to `LOD_LANDSCAPE` only, then `LOD_OBJECT` only, then `TREE_ANIM` only, and repeat |

Reset: `Shader Defines = ""`, restart; clear `ShaderCache/Utility` if needed.

---

## 5. Step 3 — RenderDoc

Capture two frames at the same camera, weather, and position:

1. Baseline: HC off, no probes.
2. Probe: whichever probe is under investigation.

Press F12 (or use the Developer Tools card) to capture. Open the `.rdc` in
`qrenderdoc`.

For a grey pixel:

- **Pixel history:** which event wrote the grey color?
  - Utility draw? Lighting draw? Fullscreen/composite? No draw at all?
- If a draw wrote it:
  - What VS is bound?
  - Is `b6` bound, and does `Enable` read 1 or 0?
  - Does the shader include `HC_PROBE`?
- If no draw wrote it:
  - It is a hole/background. The moved color geometry is being depth-rejected.
  - Inspect the depth buffer at that pixel and the event that wrote depth.

For the same terrain/rock vertex, compare `SV_Position.z` in the depth draw and
the color draw. If they differ, the two paths are using different positions.

Also inspect:

- the z-prepass event,
- Utility G-buffer/depth fills,
- the deferred composite,
- any Terrain Blending pass (not loaded in hctest; check again in the full
  profile).

---

## 6. Step 4 — interpret and fix

| Observation | Most likely cause | Fix |
| --- | --- | --- |
| Grey moves with Probe A | Lighting/DistantTree depth is coherent; remaining issue is math/coverage | tune the curvature math |
| Grey stays with Probe A, moves with Probe B | Utility depth path is unwarped | apply HC in Utility's `RENDER_DEPTH` LOD/tree path |
| Grey stays with both A and B | another pass (G-buffer/composite/other) | RenderDoc identifies it; fix that pass |
| Grey stays, RenderDoc shows no color draw at the grey pixel | moved color fails depth; depth writer is elsewhere | find and warp the depth writer |
| Grey stays, `b6 = 0` at the draw | binding failure | bind `b6` at that draw (late, after the engine resets constants) |
| Trees only: grey/z-fighting | DistantTree depth path or `CameraViewProj` binding | inspect the DistantTree depth draw in RenderDoc |

If **Utility** is the producer, the real fix is not the clip-X probe. Compute the
world position in Utility's `RENDER_DEPTH` path (`positionWS = mul(World,
positionMS)` for the non-skinned path), apply the HC drop, and use
`CameraViewProj * positionWS`. Keep the `LodLandscape::AdjustLodLandscapeVertexPositionMS/CS`
ordering in mind. For skinned/tree paths, use the matching world position.

If **Lighting/DistantTree** is the producer with `b6 = 0`, fix the binding at that
draw. Do not add more shader toggles.

---

## 7. Reset / restore

- Set `Advanced → Shader Defines = ""`, `Enable = off`,
  `DebugVertexMode = none`; restart.
- Or run `launch-full.bat` to restore the full baseline.
- `HC_PROBE` and the Utility probe block are diagnostic only; remove them before
  shipping.

---

## 8. Evidence to record

Use the template in `RESUME.md` §7. At minimum:

```text
Date / build describe:
Shader cache state:
Profile / weather / camera:
Probe (A/B/off) and all HC settings:
Terrain LOD result:
Object LOD result:
Tree LOD result:
Grey layer:
Z-fighting:
Shader keys observed:
RenderDoc: event, VS, b6 value, depth vs SV_Position.z:
Interpretation:
Next single variable:
```
