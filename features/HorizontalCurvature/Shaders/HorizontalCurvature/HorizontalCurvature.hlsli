#ifndef __HORIZONTAL_CURVATURE_DEPENDENCY_HLSL__
#define __HORIZONTAL_CURVATURE_DEPENDENCY_HLSL__

// LOD world positions are camera-relative, so distance is measured directly in XY.
// Camera height comes from the per-frame CameraPosAdjust value, which is the
// camera-relative offset back to absolute world space.
#include "Common/SharedData.hlsli"

namespace HorizontalCurvature
{
	float ComputeDrop(float2 cameraRelativeXY)
	{
		const float cameraHeight = max(0.0f, FrameBuffer::CameraPosAdjust.z - SharedData::horizontalCurvatureSettings.SeaLevelOffset);
		const float dynamicCutoff = max(0.0f, SharedData::horizontalCurvatureSettings.InnerRadiusCutoff) +
		                            max(0.0f, SharedData::horizontalCurvatureSettings.ElevationScaleFactor) * sqrt(cameraHeight);
		const float distanceOverCutoff = max(0.0f, length(cameraRelativeXY) - dynamicCutoff);
		return max(0.0f, SharedData::horizontalCurvatureSettings.CurvatureStrength) * distanceOverCutoff * distanceOverCutoff;
	}

}

#endif  // __HORIZONTAL_CURVATURE_DEPENDENCY_HLSL__
