# Horizontal Curvature, or Why we can't see High Hrothgar from White Gold Tower

*Technical Design Document & Agentic Implementation Blueprint (Pass 2)*

---

## 1. Project Overview & Objective

### 1.1 Objective

Implement a real-time, camera-relative distant horizon curvature shader module for **Skyrim (Special Edition / AE)** via the open-source **Community Shaders** framework.

### 1.2 Core Philosophy

* **Local-First & Modular:** Built cleanly as a feature module within the Community Shaders codebase.
* **No Physics Interruption:** Target *strictly* distant terrain and object LOD meshes ($\sim 1\text{ mile}+$ out), leaving active physics-enabled cells, actors, and local raycasts completely untouched.
* **Realistic Scale & Elevation Awareness:** Avoid cartoonish "fishbowl" or "tiny planet" artifacts by utilizing an adjustable inner radius deadzone and a **dynamic elevation-aware scaling modifier** that mimics real-world horizon physics ($\sqrt{2Rh}$) when viewing from high vantage points like High Hrothgar or lighthouses.

---

## 2. Mathematical Foundation

To achieve a natural horizon drop without distorting active gameplay cells or breaking high-altitude vistas, the vertex modification relies on a 2D horizontal distance-based quadratic drop-off with a dynamic cutoff radius:

$$\Delta Z = -k \cdot \max\left(0, \Vert{}\mathbf{p}_{\text{world}, xy} - \mathbf{p}_{\text{cam}, xy}\Vert{} - R_{\text{cutoff}}(h)\right)^2$$

Where:

* $\mathbf{p}_{\text{world}, xy}$: The 2D horizontal world position of the target vertex.
* $\mathbf{p}_{\text{cam}, xy}$: The active camera position in the $XY$ plane (ignoring elevation deltas during distance checks to prevent distortion).
* $R_{\text{cutoff}}(h)$: The dynamic safety deadzone radius adjusted by the observer's height ($h$).
* $k$ (`CurvatureStrength`): The scaling coefficient controlling the subtlety of the descent.

---

## 3. Architecture & Implementation Blueprint

### 3.1 C++ Backend: Constant Buffer (CBuffer) Injection

The C++ layer must intercept frame data from Community Shaders, package active camera coordinates, elevation data, and curvature configuration values, and bind them to the GPU constant buffer.

* **Struct Alignment & Padding:** DirectX 11 requires strict 16-byte boundary alignment. Structs must include explicit padding (`float _pad;`) to prevent silent data corruption or register offset bugs.
* **Parameters to Expose via CBuffer:**
* Camera position vector (`DirectX::XMFLOAT3` for $X, Y, Z$).
* Curvature coefficient ($k$).
* Base inner radius cutoff threshold ($R_{\text{cutoff}}$).
* Elevation scale factor (for high-altitude horizon extension).



### 3.2 HLSL Shaders: Vertex Transformation & Elevation Scaling

The transformation logic must be injected into the vertex shaders responsible for distant rendering (`TerrainLOD_VS.hlsl`, Object LOD, and Tree LOD passes).

#### Core HLSL Implementation Structure

```hlsl
cbuffer HorizonCurvatureBuffer : register(b12)
{
    float3 CameraPosition;       // World space camera position (X, Y, Z)
    float CurvatureStrength;     // Coefficient k
    float InnerRadiusCutoff;     // Base deadzone radius
    float ElevationScaleFactor;  // Multiplier for height-based horizon scaling
    float2 _pad;                 // 16-byte alignment padding
};

void ApplyHorizonCurvature(inout float4 worldPosition)
{
    // 1. Calculate dynamic cutoff based on camera elevation (mimicking sqrt(2Rh))
    float seaLevelZ = 0.0f; // Baseline Skyrim water/terrain level
    float cameraHeight = max(0.0f, CameraPosition.z - seaLevelZ);
    float dynamicCutoff = InnerRadiusCutoff + (ElevationScaleFactor * sqrt(cameraHeight));

    // 2. Compute strict 2D horizontal distance (ignoring Z delta)
    float2 delta = worldPosition.xy - CameraPosition.xy;
    float dist = length(delta);
    
    // 3. Apply quadratic drop-off past the dynamic deadzone boundary
    if (dist > dynamicCutoff)
    {
        float effectiveDist = dist - dynamicCutoff;
        // Smooth quadratic descent along the world Z-axis
        worldPosition.z -= CurvatureStrength * (effectiveDist * effectiveDist);
    }
}

```

### 3.3 In-Game UI Integration

Integrate with Community Shaders' existing ImGui framework (accessible via the `END` key menu) to expose live-adjustable sliders for:

* **Curvature Strength ($k$):** Fine-tunes the aggressiveness of the horizon drop.
* **Inner Radius Cutoff ($R_{\text{cutoff}}$):** Defines the base boundary where the transition begins.
* **Elevation Scale Factor:** Controls how much higher vantage points (e.g., High Hrothgar) push the horizon boundary outward to match physical line-of-sight expansion.

---

## 4. Known Technical Challenges & Mitigations

| Challenge | Root Cause | Mitigation Strategy |
| --- | --- | --- |
| **CPU Frustum Culling Mismatch** | CPU calculates bounding boxes ($AABBs$) using original unbent coordinates before GPU vertex displacement. | Maintain conservative, gentle $k$ coefficients to avoid extreme edge-of-screen vertex shifts that could trigger premature culling or pop-in. |
| **Boundary Tearing / Seams** | Sharp binary cutoff (`if (dist > cutoff)`) creates an instant derivative discontinuity across large contiguous meshes. | Utilize a smooth, continuous ramp function or smoothstep blend past the cutoff threshold rather than a hard step. |
| **High-Altitude Distortion** | Including camera $Z$-elevation directly in distance checks distorts perspective when looking down from high peaks. | Enforce strict 2D horizontal vector calculations (`XY` plane only) for distance, using elevation *only* to scale the deadzone threshold. |
| **Skybox / Fog Disconnect** | Lowering terrain reveals unaligned background pixels or gaps against the sky. | Pair vertex transformation with Community Shaders' atmospheric height fog so distant meshes blend seamlessly into the horizon haze. |

---

## 5. Testing & Calibration Workflow

1. **Build & Deploy:** Cross-compile the updated Community Shaders fork/module into a local test environment running via SKSE on CachyOS.
2. **Baseline Testing (Whiterun Tundra):**
* Fast-travel to the open plains outside Whiterun.
* Open the `END` menu and calibrate `InnerRadiusCutoff` until all immediate roads, watchtowers, and local flora are 100% locked and flat.


3. **High-Altitude Testing (High Hrothgar / Lighthouses):**
* Travel to a high elevation point.
* Fine-tune the **Elevation Scale Factor** until the expanded view feels open and natural, preventing the world from appearing artificially cramped or dished beneath your feet.


4. **Horizon Tuning:** Calibrate `CurvatureStrength` ($k$) until distant mountain ranges drop gracefully below the atmospheric haze line, achieving true planetary scale.
