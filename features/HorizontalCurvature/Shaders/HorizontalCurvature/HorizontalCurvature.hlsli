#ifndef __HORIZONTAL_CURVATURE_DEPENDENCY_HLSL__
#define __HORIZONTAL_CURVATURE_DEPENDENCY_HLSL__

// LOD world positions are camera-relative, so distance is measured directly in XY.
// The caller supplies the absolute camera Z from its own per-frame bank, avoiding
// a second use of register b12 in shaders which already declare a b12 cbuffer.
#include "Common/SharedData.hlsli"

namespace HorizontalCurvature
{
	float3 HsvToRgb(float h, float s, float v)
	{
		float3 p = abs(frac(h + float3(1.0f, 2.0f / 3.0f, 1.0f / 3.0f)) * 6.0f - 3.0f);
		return v * lerp(float3(1.0f, 1.0f, 1.0f), saturate(p - 1.0f), s);
	}

	// Flags matching HorizontalCurvature::PerFrame::Flags.
	#define HC_FLAG_APPLY_ALL_LIGHTING 1u
	#define HC_FLAG_APPLY_TERRAIN 2u
	#define HC_FLAG_APPLY_OBJECTS 4u
	#define HC_FLAG_APPLY_TREES 8u
	#define HC_FLAG_APPLY_LOD_LANDSCAPE 16u
	#define HC_FLAG_APPLY_LOD_LANDNOISE 32u
	#define HC_FLAG_APPLY_LOD_OBJECTS 64u
	#define HC_FLAG_APPLY_LOD_OBJECTSHD 128u
	#define HC_FLAG_APPLY_FULL_LANDSCAPE 256u
	#define HC_FLAG_APPLY_FULL_OBJECTS 512u
	#define HC_FLAG_APPLY_SKINNED 1024u

	float GetDynamicCutoff(float cameraZ)
	{
		const float cameraHeight = max(0.0f, cameraZ - SharedData::horizontalCurvatureSettings.SeaLevelOffset);
		return max(0.0f, SharedData::horizontalCurvatureSettings.InnerRadiusCutoff) +
		       max(0.0f, SharedData::horizontalCurvatureSettings.ElevationScaleFactor) * sqrt(cameraHeight);
	}

	float GetDistanceOverCutoff(float2 cameraRelativeXY, float cameraZ)
	{
		float over = max(0.0f, length(cameraRelativeXY) - GetDynamicCutoff(cameraZ));
		const float softness = max(0.0f, SharedData::horizontalCurvatureSettings.CutoffSoftness);
		if (softness > 0.0f)
			over *= smoothstep(0.0f, 1.0f, saturate(over / softness));
		return over;
	}

	float GetCurvatureRadius()
	{
		const float k = max(0.0f, SharedData::horizontalCurvatureSettings.CurvatureStrength);
		return k > 1e-12f ? 0.5f / k : 1e12f;
	}

	float ComputeDrop(float2 cameraRelativeXY, float cameraZ)
	{
		if (SharedData::horizontalCurvatureSettings.Enable == 0)
			return 0.0f;

		const float over = GetDistanceOverCutoff(cameraRelativeXY, cameraZ);
		const float k = max(0.0f, SharedData::horizontalCurvatureSettings.CurvatureStrength);
		if (over <= 0.0f || k <= 0.0f)
			return 0.0f;

		const uint mode = SharedData::horizontalCurvatureSettings.CurvatureMode;
		float drop;
		if (mode == 1) {
			const float radius = GetCurvatureRadius();
			drop = radius - sqrt(max(0.0f, radius * radius - over * over));
		} else if (mode == 2) {
			const float radius = GetCurvatureRadius();
			const float phi = min(over / radius, 1.5f);
			drop = (1.0f - cos(phi)) * radius;
		} else {
			drop = k * over * over;
		}

		return (SharedData::horizontalCurvatureSettings.Direction < 0.0f ? -1.0f : 1.0f) * drop;
	}

	float ComputeXZScale(float2 cameraRelativeXY, float cameraZ)
	{
		if (SharedData::horizontalCurvatureSettings.Enable == 0 ||
		    SharedData::horizontalCurvatureSettings.CurvatureMode != 2)
			return 1.0f;

		const float over = GetDistanceOverCutoff(cameraRelativeXY, cameraZ);
		if (over <= 0.0f)
			return 1.0f;

		const float radius = GetCurvatureRadius();
		const float phi = min(over / radius, 1.5f);
		return phi > 1e-6f ? sin(phi) / phi : 1.0f;
	}

	float3 GetDebugColor(float2 cameraRelativeXY, float cameraZ, float hueOffset)
	{
		const float drop = ComputeDrop(cameraRelativeXY, cameraZ);
		const float t = saturate(abs(drop) / 5000.0f);
		return HsvToRgb(frac(SharedData::horizontalCurvatureSettings.DebugHue + hueOffset + t * 0.25f), 1.0f, 1.0f);
	}
}

#endif  // __HORIZONTAL_CURVATURE_DEPENDENCY_HLSL__
