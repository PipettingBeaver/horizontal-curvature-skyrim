#include "HorizontalCurvature.h"

#include "I18n/I18n.h"

#define I18N_KEY_PREFIX "feature.horizontal_curvature."

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	HorizontalCurvature::Settings,
	Enable,
	DebugColor,
	DebugHue,
	CurvatureStrength,
	InnerRadiusCutoff,
	ElevationScaleFactor,
	SeaLevelOffset)

HorizontalCurvature::PerFrame HorizontalCurvature::GetCommonBufferData() const
{
	PerFrame data{};
	data.CurvatureStrength = settings.CurvatureStrength;
	data.InnerRadiusCutoff = settings.InnerRadiusCutoff;
	data.ElevationScaleFactor = settings.ElevationScaleFactor;
	data.SeaLevelOffset = settings.SeaLevelOffset;
	data.Enable = settings.Enable ? 1u : 0u;
	data.DebugColor = settings.DebugColor ? 1u : 0u;
	data.DebugHue = settings.DebugHue;
	return data;
}

void HorizontalCurvature::RestoreDefaultSettings()
{
	settings = {};
}

void HorizontalCurvature::LoadSettings(json& o_json)
{
	settings = o_json;
}

void HorizontalCurvature::SaveSettings(json& o_json)
{
	o_json = settings;
}

void HorizontalCurvature::DrawSettings()
{
	ImGui::Checkbox(T(TKEY("enable_effect"), "Enable Effect"), &settings.Enable);
	if (auto _tt = Util::HoverTooltipWrapper()) {
		ImGui::Text("%s", T(TKEY("enable_effect_tooltip"),
							  "Toggles the displacement without recompiling shaders. Useful for A/B comparison."));
	}

	ImGui::Checkbox(T(TKEY("debug_colors"), "Debug Colors"), &settings.DebugColor);
	if (auto _tt = Util::HoverTooltipWrapper()) {
		ImGui::Text("%s", T(TKEY("debug_colors_tooltip"),
							  "Tints affected LOD geometry by the computed curvature strength."));
	}

	if (settings.DebugColor) {
		ImGui::SliderFloat(T(TKEY("debug_hue"), "Debug Hue"), &settings.DebugHue, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text("%s", T(TKEY("debug_hue_tooltip"),
								  "Base hue of the debug tint. Moving this is an immediate check that settings reach the shader."));
		}
	}

	ImGui::SliderFloat(T(TKEY("curvature_strength"), "Curvature Strength"), &settings.CurvatureStrength, 0.0f, 5.0e-6f, "%.2e", ImGuiSliderFlags_AlwaysClamp);
	if (auto _tt = Util::HoverTooltipWrapper()) {
		ImGui::Text("%s", T(TKEY("curvature_strength_tooltip"),
							  "Quadratic drop coefficient. Larger values bend the horizon down sooner. Real-world curvature is about 1.1e-9; this is intentionally exaggerated."));
	}

	ImGui::SliderFloat(T(TKEY("inner_radius_cutoff"), "Inner Radius Cutoff"), &settings.InnerRadiusCutoff, 0.0f, 300000.0f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
	if (auto _tt = Util::HoverTooltipWrapper()) {
		ImGui::Text("%s", T(TKEY("inner_radius_cutoff_tooltip"),
							  "Game units from the camera where the world stays flat. Values below the LOD transition are only for testing; they cause visible seams."));
	}

	ImGui::SliderFloat(T(TKEY("elevation_scale_factor"), "Elevation Scale Factor"), &settings.ElevationScaleFactor, 0.0f, 30000.0f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
	if (auto _tt = Util::HoverTooltipWrapper()) {
		ImGui::Text("%s", T(TKEY("elevation_scale_factor_tooltip"),
							  "Pushes the deadzone outward as the camera rises, using sqrt(height). Raise this when viewing from High Hrothgar or a lighthouse."));
	}

	ImGui::SliderFloat(T(TKEY("sea_level_offset"), "Sea Level Offset"), &settings.SeaLevelOffset, -20000.0f, 20000.0f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
	if (auto _tt = Util::HoverTooltipWrapper()) {
		ImGui::Text("%s", T(TKEY("sea_level_offset_tooltip"),
							  "World Z treated as sea level for the elevation term. Adjust for worldspaces whose baseline height is not zero."));
	}
}

#undef I18N_KEY_PREFIX
