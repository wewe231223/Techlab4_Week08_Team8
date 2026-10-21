#include "EnginePCH.h"
#include "LightComponent.h"

#include "Engine/Scene.h"
#include "Engine/World.h"

#include <algorithm>

ULightComponent::ULightComponent() {
}

ULightComponent::~ULightComponent() {
}

void ULightComponent::DrawDebug(FLineBatcher* LineBatcher) const {
}

void ULightComponent::PostEditChangeProperty(const FProperty& Property) {
	Super::PostEditChangeProperty(Property);
	SetShadowMapResolution(mShadowMapResolution);
	SetShadowDepthBiasPreset(GetShadowDepthBiasPreset());
	SetShadowSlopeScaledDepthBiasPreset(GetShadowSlopeScaledDepthBiasPreset());
	SetShadowNormalBias(mShadowNormalBias);
	SetMinCasterDepth(mMinCasterDepth);
}

bool ULightComponent::CastsShadows() const {
	return mCastShadows;
}

FLightShadowSettings ULightComponent::GetShadowSettings() const {
	FLightShadowSettings Settings{};
	Settings.mCastShadows = mCastShadows;
	Settings.mResolution = NormalizeShadowMapResolution(mShadowMapResolution);
	Settings.mDepthBiasPreset = GetShadowDepthBiasPreset();
	Settings.mSlopeScaledDepthBiasPreset = GetShadowSlopeScaledDepthBiasPreset();
	Settings.mNormalBias = std::max(mShadowNormalBias, 0.0f);
	Settings.mMinCasterDepth = GetMinCasterDepth();
	return Settings;
}

float ULightComponent::GetMinCasterDepth() const {
	return NormalizeShadowMinCasterDepth(mMinCasterDepth);
}

void ULightComponent::SetMinCasterDepth(float Depth) {
	mMinCasterDepth = NormalizeShadowMinCasterDepth(Depth);
}

EShadowDepthBiasPreset ULightComponent::GetShadowDepthBiasPreset() const {
	return NormalizeShadowDepthBiasPreset(mShadowDepthBiasPreset);
}

EShadowSlopeScaledDepthBiasPreset ULightComponent::GetShadowSlopeScaledDepthBiasPreset() const {
	return NormalizeShadowSlopeScaledDepthBiasPreset(mShadowSlopeScaledDepthBiasPreset);
}

void ULightComponent::SetCastShadows(bool CastShadows) {
	mCastShadows = CastShadows;
}

void ULightComponent::SetShadowMapResolution(int32 Resolution) {
	mShadowMapResolution = NormalizeShadowMapResolution(Resolution);
}

void ULightComponent::SetShadowDepthBiasPreset(EShadowDepthBiasPreset Preset) {
	mShadowDepthBiasPreset = static_cast<int32>(NormalizeShadowDepthBiasPreset(static_cast<int32>(Preset)));
}

void ULightComponent::SetShadowSlopeScaledDepthBiasPreset(EShadowSlopeScaledDepthBiasPreset Preset) {
	mShadowSlopeScaledDepthBiasPreset = static_cast<int32>(NormalizeShadowSlopeScaledDepthBiasPreset(static_cast<int32>(Preset)));
}

void ULightComponent::SetShadowNormalBias(float NormalBias) {
	mShadowNormalBias = std::max(NormalBias, 0.0f);
}

FLightSceneProxy* ULightComponent::CreateSceneProxy() const {
	return nullptr;
}

bool ULightComponent::ShouldCreateRenderState() const {
	return true;
}

void ULightComponent::CreateRenderState() {
	Super::CreateRenderState();
	GetWorld()->GetScene().AddLight(this);
}

void ULightComponent::DestroyRenderState() {
	GetWorld()->GetScene().RemoveLight(this);
	Super::DestroyRenderState();
}
