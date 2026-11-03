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

	float ComputeDrop(float2 cameraRelativeXY, float cameraZ)
	{
		if (SharedData::horizontalCurvatureSettings.Enable == 0)
			return 0.0f;

		const float cameraHeight = max(0.0f, cameraZ - SharedData::horizontalCurvatureSettings.SeaLevelOffset);
		const float dynamicCutoff = max(0.0f, SharedData::horizontalCurvatureSettings.InnerRadiusCutoff) +
		                            max(0.0f, SharedData::horizontalCurvatureSettings.ElevationScaleFactor) * sqrt(cameraHeight);
		const float distanceOverCutoff = max(0.0f, length(cameraRelativeXY) - dynamicCutoff);
		return max(0.0f, SharedData::horizontalCurvatureSettings.CurvatureStrength) * distanceOverCutoff * distanceOverCutoff;
	}

	float3 GetDebugColor(float2 cameraRelativeXY, float cameraZ, float hueOffset)
	{
		const float drop = ComputeDrop(cameraRelativeXY, cameraZ);
		const float t = saturate(drop / 5000.0f);
		return HsvToRgb(frac(SharedData::horizontalCurvatureSettings.DebugHue + hueOffset + t * 0.25f), 1.0f, 1.0f);
	}
}

#endif  // __HORIZONTAL_CURVATURE_DEPENDENCY_HLSL__
