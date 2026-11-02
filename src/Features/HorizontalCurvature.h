#pragma once

#include "Feature.h"

/**
 * @brief Bends distant LOD terrain, objects and trees below a camera-relative horizon.
 *
 * The displacement is applied only to LOD permutations in Lighting.hlsl and to
 * DistantTree.hlsl, so loaded cells, actors, physics and local raycasts are untouched.
 */
struct HorizontalCurvature : Feature
{
	virtual inline std::string GetName() override { return "Horizontal Curvature"; }
	virtual std::string GetDisplayName() override { return T("feature.horizontal_curvature.name", "Horizontal Curvature"); }
	virtual inline std::string GetShortName() override { return "HorizontalCurvature"; }
	virtual inline std::string_view GetShaderDefineName() override { return "HORIZONTAL_CURVATURE"; }
	virtual bool HasShaderDefine(RE::BSShader::Type a_type) override
	{
		return a_type == RE::BSShader::Type::Lighting || a_type == RE::BSShader::Type::DistantTree;
	}
	virtual std::string_view GetCategory() const override { return FeatureCategories::kLandscapeAndTextures; }

	virtual std::pair<std::string, std::vector<std::string>> GetFeatureSummary() override
	{
		return { T("feature.horizontal_curvature.description", "Bends distant terrain, object and tree LOD below a camera-relative horizon."),
			{ T("feature.horizontal_curvature.key_feature_1", "Leaves loaded cells and gameplay geometry untouched"),
				T("feature.horizontal_curvature.key_feature_2", "Adjustable deadzone radius and curvature strength"),
				T("feature.horizontal_curvature.key_feature_3", "Elevation-aware horizon extension for high vantage points") } };
	}

	struct Settings
	{
		bool Enable = true;                 ///< Runtime effect switch; does not recompile shaders.
		bool DebugColor = true;             ///< Tint LOD by the computed curvature.
		float DebugHue = 0.35f;             ///< Base hue of the debug tint (0-1).
		float CurvatureStrength = 5.0e-6f;  ///< Quadratic drop coefficient k.
		float InnerRadiusCutoff = 1000.0f;  ///< Game units from the camera before displacement begins.
		float ElevationScaleFactor = 0.0f;  ///< Extends the deadzone by this factor times sqrt(camera height).
		float SeaLevelOffset = 0.0f;        ///< World-space Z treated as sea level for the elevation term.
	};
	Settings settings;

	/** @brief Feature buffer data matching HorizontalCurvatureSettings in SharedData.hlsli. */
	struct alignas(16) PerFrame
	{
		float CurvatureStrength;
		float InnerRadiusCutoff;
		float ElevationScaleFactor;
		float SeaLevelOffset;
		uint32_t Enable;
		uint32_t DebugColor;
		float DebugHue;
		float pad0;
	};
	static_assert(sizeof(PerFrame) % 16 == 0, "HorizontalCurvature::PerFrame must be 16-byte aligned");

	PerFrame GetCommonBufferData() const;

	virtual void RestoreDefaultSettings() override;
	virtual void LoadSettings(json& o_json) override;
	virtual void SaveSettings(json& o_json) override;
	virtual void DrawSettings() override;
};
