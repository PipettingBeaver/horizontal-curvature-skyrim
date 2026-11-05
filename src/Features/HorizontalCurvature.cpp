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
	SeaLevelOffset,
	CutoffSoftness,
	CurvatureMode,
	TreeMode,
	DebugVertexMode,
	DebugVertexAmount,
	ApplyToAllLighting,
	ApplyTerrain,
	ApplyObjects,
	ApplyTrees,
	Raise)

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
	data.CurvatureMode = settings.CurvatureMode;
	data.TreeMode = settings.TreeMode;
	data.DebugVertexMode = settings.DebugVertexMode;
	data.Flags = (settings.ApplyToAllLighting ? 1u : 0u) |
	             (settings.ApplyTerrain ? 2u : 0u) |
	             (settings.ApplyObjects ? 4u : 0u) |
	             (settings.ApplyTrees ? 8u : 0u);
	data.DebugVertexAmount = settings.DebugVertexAmount;
	data.CutoffSoftness = settings.CutoffSoftness;
	data.Direction = settings.Raise ? -1.0f : 1.0f;
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

	ImGui::SliderFloat(T(TKEY("cutoff_softness"), "Cutoff Softness"), &settings.CutoffSoftness, 0.0f, 50000.0f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
	if (auto _tt = Util::HoverTooltipWrapper()) {
		ImGui::Text("%s", T(TKEY("cutoff_softness_tooltip"),
							  "Ramps the deadzone in over this distance instead of starting with a hard edge. 0 is the current hard cutoff."));
	}

	ImGui::Checkbox(T(TKEY("raise_curvature"), "Curvature Raise (invert)"), &settings.Raise);
	if (auto _tt = Util::HoverTooltipWrapper()) {
		ImGui::Text("%s", T(TKEY("raise_curvature_tooltip"),
							  "Raises affected geometry instead of lowering it. Useful for separating HC-displaced meshes from z-fighting or stale depth copies."));
	}

	ImGui::SeparatorText(T(TKEY("experimental"), "Experimental / Fallbacks"));

	static const char* curvatureModes[] = { "Parabolic", "Spherical (vertical)", "Spherical (XZ shrink)" };
	int curvatureMode = static_cast<int>(settings.CurvatureMode);
	if (ImGui::Combo(T(TKEY("curvature_mode"), "Curvature Mode"), &curvatureMode, curvatureModes, IM_ARRAYSIZE(curvatureModes)))
		settings.CurvatureMode = static_cast<uint32_t>(curvatureMode);
	if (auto _tt = Util::HoverTooltipWrapper()) {
		ImGui::Text("%s", T(TKEY("curvature_mode_tooltip"),
							  "Parabolic is the current drop. Spherical uses a circular arc; the full variant also pulls XZ toward the camera."));
	}

	static const char* treeModes[] = { "Per-vertex", "Base anchor" };
	int treeMode = static_cast<int>(settings.TreeMode);
	if (ImGui::Combo(T(TKEY("tree_mode"), "Tree Warp"), &treeMode, treeModes, IM_ARRAYSIZE(treeModes)))
		settings.TreeMode = static_cast<uint32_t>(treeMode);
	if (auto _tt = Util::HoverTooltipWrapper()) {
		ImGui::Text("%s", T(TKEY("tree_mode_tooltip"),
							  "Per-vertex bends each billboard. Base anchor translates the whole tree from its instance origin, preserving its shape."));
	}

	ImGui::Checkbox(T(TKEY("apply_terrain"), "Apply to Terrain LOD"), &settings.ApplyTerrain);
	ImGui::Checkbox(T(TKEY("apply_objects"), "Apply to Object LOD"), &settings.ApplyObjects);
	ImGui::Checkbox(T(TKEY("apply_trees"), "Apply to Tree LOD"), &settings.ApplyTrees);
	ImGui::Checkbox(T(TKEY("apply_all_lighting"), "Apply to All Lighting (test)"), &settings.ApplyToAllLighting);
	if (auto _tt = Util::HoverTooltipWrapper()) {
		ImGui::Text("%s", T(TKEY("apply_all_lighting_tooltip"),
							  "Bypasses the LOD guard for non-skinned Lighting shaders. Test-only: it will also warp full-detail geometry."));
	}

	static const char* debugVertexModes[] = { "None", "Clip X", "Clip Y", "World Z" };
	int debugVertexMode = static_cast<int>(settings.DebugVertexMode);
	if (ImGui::Combo(T(TKEY("debug_vertex_mode"), "Vertex Probe"), &debugVertexMode, debugVertexModes, IM_ARRAYSIZE(debugVertexModes)))
		settings.DebugVertexMode = static_cast<uint32_t>(debugVertexMode);
	if (auto _tt = Util::HoverTooltipWrapper()) {
		ImGui::Text("%s", T(TKEY("debug_vertex_mode_tooltip"),
							  "Temporary vertex-only diagnostic. Clip X/Y are screen-space; World Z is a fixed game-unit offset. Use to prove the VS path independently of the curvature math."));
	}

	if (ImGui::Button(T(TKEY("run_positive_control"), "Run Old Positive Control (Clip X)"))) {
		settings.Enable = true;
		settings.DebugVertexMode = static_cast<uint32_t>(DebugVertexMode::ClipX);
		settings.DebugVertexAmount = 0.25f;
	}
	if (auto _tt = Util::HoverTooltipWrapper()) {
		ImGui::Text("%s", T(TKEY("run_positive_control_tooltip"),
							  "Restores the temporary horizontal clip-X shift that visibly moved LOD earlier. If this moves nothing, the VS path or its binding is still broken; if it moves, the VS works and the curvature math/settings are the remaining suspect."));
	}

	if (settings.DebugVertexMode != 0) {
		ImGui::SliderFloat(T(TKEY("debug_vertex_amount"), "Vertex Probe Amount"), &settings.DebugVertexAmount, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text("%s", T(TKEY("debug_vertex_amount_tooltip"),
								  "Clip modes use this as a fraction of clip W. World Z multiplies it by 10,000 game units."));
		}
	}
}

#undef I18N_KEY_PREFIX
