#include "EnginePCH.h"
#include "ShadowMap.h"

#include <algorithm>
#include <bit>

namespace {
	constexpr FShadowDepthBiasPreset ShadowDepthBiasPresets[ShadowDepthBiasPresetCount]{ { "None (0)", 0 }, { "Low (4194)", 4194 }, { "Medium (16777)", 16777 }, { "High (33554)", 33554 }, { "Very High (67109)", 67109 } };
	constexpr FShadowSlopeScaledDepthBiasPreset ShadowSlopeScaledDepthBiasPresets[ShadowSlopeScaledDepthBiasPresetCount]{ { "None (0)", 0.0f }, { "Low (0.5)", 0.5f }, { "Medium (1)", 1.0f }, { "High (2)", 2.0f }, { "Very High (4)", 4.0f } };
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

int32 NormalizeShadowAtlasResolution(int32 Resolution) {
	const uint32 Size{ static_cast<uint32>(std::clamp(Resolution, MinShadowMapResolution, MaxShadowAtlasResolution)) };
	return static_cast<int32>(std::bit_ceil(Size));
}
