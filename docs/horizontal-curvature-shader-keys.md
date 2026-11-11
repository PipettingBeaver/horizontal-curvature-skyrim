# Horizontal Curvature — Shader Key Log

Purpose: record the exact shader keys observed with Active Shaders / Shader
Blocking, so results can be compared across builds and test conditions.

Use `CurvatureStrength = 0` first to map a key to the object it draws. Then use a
low nonzero strength (`5e-7` to `1e-6`) and block the same key again.

Blocking a Pixel shader often makes geometry black; that is a missing-color
fallback, not evidence about the grey layer. The grey layer itself has no color
pixel stage in the observations so far, so prioritize **Vertex** blocking when
trying to remove it.

## Current keys

| Type | Class | Descriptor | Key | Observed with block | Notes |
| --- | --- | --- | --- | --- | --- |
| Lighting | Vertex | `0x1` | `Lighting:Vertex:HORIZONTAL_CURVATURE VC` | Nearby full-detail rocks become normal-map colors; does not visibly change curvature response | Shared non-skinned Lighting VS; too broad for object-specific diagnosis |
| Lighting | Vertex | `0xC000003` | `Lighting:Vertex:HORIZONTAL_CURVATURE SKINNED TREE_ANIM VC` | Trees stop flickering | Skinned tree path |
| Lighting | Vertex | `0x8001` | `Lighting:Vertex:HORIZONTAL_CURVATURE PROJECTED_UV VC` | Distant rocks show normal-map colors | Projected-UV full-detail/object path |
| Lighting | Pixel | `0x11` | `Lighting:Pixel:DEFERRED HORIZONTAL_CURVATURE VC` | Full-detail objects turn black; at nonzero strength they disappear | Color path for full-detail objects; depth/occlusion remains the likely disappearance mechanism |
| Lighting | Pixel | `0x12000015` | `Lighting:Pixel:DEFERRED HORIZONTAL_CURVATURE LODLANDNOISE LODLANDSCAPE MODELSPACENORMALS VC` | Distant terrain past Whiterun turns black but still moves upward | Terrain LOD color path; movement is VS, color loss is PS |

## Test record template

Add one row per meaningful block test:

```text
Date:
Build / describe:
Apply switches:
Strength / Raise / cutoff / softness:
Type:
Class:
Descriptor:
Key:
Cache path:
Object tested:
At strength 0:
At nonzero strength:
Grey layer:
Z-fighting:
Interpretation:
```

## Next keys to target

1. The full-detail **Vertex** key that removes the grey layer, not just the
   normal-map fallback.
2. The terrain-LOD **Vertex** key that moves the distant terrain.
3. The skinned-tree **Vertex** key with strength 0 and nonzero, comparing the
   grey copy.
4. Any LOD object HD keys seen in Active Shaders.

Do not use `0x1` alone to conclude full-detail behavior; it is shared by many
draws. Use it only as a control.
