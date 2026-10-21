#include "EnginePCH.h"
#include "DirectionalLightComponent.h"

#include "Core/StatOverlay.h"
#include "Engine/DirectionalLightSceneProxy.h"

#include <algorithm>
#include <cmath>

UDirectionalLightComponent::UDirectionalLightComponent() {
	SetShadowMapResolution(DefaultDirectionalShadowMapResolution);
}

UDirectionalLightComponent::~UDirectionalLightComponent() {
}

FLightSceneProxy* UDirectionalLightComponent::CreateSceneProxy() const {
	return new FDirectionalLightSceneProxy{ this };
}

void UDirectionalLightComponent::PostEditChangeProperty(const FProperty& Property) {
	Super::PostEditChangeProperty(Property);
	SetSourceAngle(mSourceAngle);
	SetCascadeCount(mCascadeCount);
	SetMaxShadowDistance(mMaxShadowDistance);
	SetSplitLambda(mSplitLambda);
	SetCascadeTransitionFraction(mCascadeTransitionFraction);
	SetEnableCSM(mEnableCSM);
}

FLightShadowSettings UDirectionalLightComponent::GetShadowSettings() const {
	FLightShadowSettings Settings{ Super::GetShadowSettings() };
	Settings.mSourceAngle = GetSourceAngle();
	return Settings;
}

ELightType UDirectionalLightComponent::GetLightType() const {
	return ELightType::Directional;
}

float UDirectionalLightComponent::GetSourceAngle() const {
	return NormalizeShadowSourceAngle(mSourceAngle);
}

void UDirectionalLightComponent::SetSourceAngle(float Angle) {
	mSourceAngle = NormalizeShadowSourceAngle(Angle);
}

bool UDirectionalLightComponent::IsCSMEnabled() const {
	return mEnableCSM;
}

void UDirectionalLightComponent::SetEnableCSM(bool Enabled) {
	mEnableCSM = Enabled;
	FCSMDebugSnapshot Snapshot{ FStatOverlay::GetCSMStats() };
	Snapshot.mEnabled = Enabled;
	Snapshot.mCascadeCount = Enabled ? mCascadeCount : 0;
	FStatOverlay::UpdateCSMStats(Snapshot);
}

uint32 UDirectionalLightComponent::GetCascadeCount() const {
	return std::clamp(mCascadeCount, 2u, 4u);
}

void UDirectionalLightComponent::SetCascadeCount(uint32 Count) {
	mCascadeCount = std::clamp(Count, 2u, 4u);
}

float UDirectionalLightComponent::GetMaxShadowDistance() const {
	return mMaxShadowDistance;
}

void UDirectionalLightComponent::SetMaxShadowDistance(float Distance) {
	mMaxShadowDistance = std::isfinite(Distance) ? std::max(Distance, 50.0f) : 50.0f;
}

float UDirectionalLightComponent::GetSplitLambda() const {
	return mSplitLambda;
}

void UDirectionalLightComponent::SetSplitLambda(float Lambda) {
	mSplitLambda = std::isfinite(Lambda) ? std::clamp(Lambda, 0.0f, 1.0f) : 0.85f;
}

float UDirectionalLightComponent::GetCascadeTransitionFraction() const {
	return mCascadeTransitionFraction;
}

void UDirectionalLightComponent::SetCascadeTransitionFraction(float Fraction) {
	mCascadeTransitionFraction = std::isfinite(Fraction) ? std::clamp(Fraction, 0.0f, 0.3f) : 0.1f;
}
