#include "EnginePCH.h"
#include "ShadowMap.h"

#include <algorithm>
#include <bit>
#include <cmath>

namespace {
	constexpr FShadowDepthBiasPreset ShadowDepthBiasPresets[ShadowDepthBiasPresetCount]{ { "None (0)", 0 }, { "Low (4194)", 4194 }, { "Medium (16777)", 16777 }, { "High (33554)", 33554 }, { "Very High (67109)", 67109 }, { "Low / 16 (262)", 262 }, { "Low / 8 (524)", 524 }, { "Low / 4 (1049)", 1049 }, { "Low / 2 (2097)", 2097 }, { "Low-Medium (8389)", 8389 }, { "Below Medium (12583)", 12583 }, { "Medium-High (25166)", 25166 }, { "High-Very High (50332)", 50332 } };
	constexpr FShadowSlopeScaledDepthBiasPreset ShadowSlopeScaledDepthBiasPresets[ShadowSlopeScaledDepthBiasPresetCount]{ { "None (0)", 0.0f }, { "Low (0.5)", 0.5f }, { "Medium (1)", 1.0f }, { "High (2)", 2.0f }, { "Very High (4)", 4.0f }, { "Low / 16 (0.03125)", 0.03125f }, { "Low / 8 (0.0625)", 0.0625f }, { "Low / 4 (0.125)", 0.125f }, { "Low / 2 (0.25)", 0.25f }, { "Low-Medium (0.625)", 0.625f }, { "Below Medium (0.75)", 0.75f }, { "Medium-High (1.5)", 1.5f }, { "High-Very High (3)", 3.0f } };
}

EShadowFilterMode NormalizeShadowFilterMode(int32 Mode) {
	return Mode >= 0 && Mode < static_cast<int32>(EShadowFilterMode::Count) ? static_cast<EShadowFilterMode>(Mode) : EShadowFilterMode::None;
}

EShadowDepthBiasPreset NormalizeShadowDepthBiasPreset(int32 Preset) {
	return Preset >= 0 && Preset < ShadowDepthBiasPresetCount ? static_cast<EShadowDepthBiasPreset>(Preset) : EShadowDepthBiasPreset::Medium;
}

EShadowSlopeScaledDepthBiasPreset NormalizeShadowSlopeScaledDepthBiasPreset(int32 Preset) {
	return Preset >= 0 && Preset < ShadowSlopeScaledDepthBiasPresetCount ? static_cast<EShadowSlopeScaledDepthBiasPreset>(Preset) : EShadowSlopeScaledDepthBiasPreset::Medium;
}

const FShadowDepthBiasPreset& GetShadowDepthBiasPreset(EShadowDepthBiasPreset Preset) {
	return ShadowDepthBiasPresets[static_cast<int32>(NormalizeShadowDepthBiasPreset(static_cast<int32>(Preset)))];
}

const FShadowSlopeScaledDepthBiasPreset& GetShadowSlopeScaledDepthBiasPreset(EShadowSlopeScaledDepthBiasPreset Preset) {
	return ShadowSlopeScaledDepthBiasPresets[static_cast<int32>(NormalizeShadowSlopeScaledDepthBiasPreset(static_cast<int32>(Preset)))];
}

int32 NormalizeShadowMapResolution(int32 Resolution) {
	const uint32 Size{ static_cast<uint32>(std::clamp(Resolution, MinShadowMapResolution, MaxShadowMapResolution)) };
	return static_cast<int32>(std::bit_ceil(Size));
}

float NormalizeShadowMinCasterDepth(float Depth) {
	return std::isfinite(Depth) ? std::max(Depth, 0.001f) : DefaultShadowMinCasterDepth;
}

float NormalizeShadowSourceRadius(float Radius) {
	return std::isfinite(Radius) ? std::clamp(Radius, 0.0f, MaxShadowSourceRadius) : DefaultShadowSourceRadius;
}

float NormalizeShadowSourceAngle(float Angle) {
	return std::isfinite(Angle) ? std::clamp(Angle, 0.0f, MaxShadowSourceAngle) : DefaultShadowSourceAngle;
}
