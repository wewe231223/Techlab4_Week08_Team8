#include "EnginePCH.h"
#include "LightComponent.h"

#include "Engine/Scene.h"
#include "Engine/World.h"

#include <cmath>

ULightComponent::ULightComponent() {
}

ULightComponent::~ULightComponent() {
}

void ULightComponent::DrawDebug(FLineBatcher* LineBatcher) const {
}

FLightSceneProxy* ULightComponent::CreateSceneProxy() const {
	return nullptr;
}

void ULightComponent::PostEditChangeProperty(const FProperty& Property) {
	Super::PostEditChangeProperty(Property);
	SetShadowMapResolution(mShadowMapResolution);
	SetShadowDistance(mShadowDistance);
	SetShadowDepthBias(mShadowDepthBias);
	SetShadowSlopeScaledDepthBias(mShadowSlopeScaledDepthBias);
	SetShadowNormalBias(mShadowNormalBias);
}

bool ULightComponent::CastsShadows() const {
	return mCastShadows;
}

FLightShadowSettings ULightComponent::GetShadowSettings() const {
	FLightShadowSettings Settings{
		.mCastShadows = mCastShadows,
		.mResolution = FMath::Clamp(mShadowMapResolution, MinShadowMapResolution, MaxShadowMapResolution),
		.mDistance = std::max(mShadowDistance, 0.01f),
		.mDepthBias = std::max(mShadowDepthBias, 0.0f),
		.mSlopeScaledDepthBias = std::max(mShadowSlopeScaledDepthBias, 0.0f),
		.mNormalBias = std::max(mShadowNormalBias, 0.0f)		
	};

	return Settings; 
}

void ULightComponent::SetCastShadows(bool CastShadows) {
	mCastShadows = CastShadows;
}

void ULightComponent::SetShadowMapResolution(int32 Resolution) {
	mShadowMapResolution = FMath::Clamp(Resolution, MinShadowMapResolution, MaxShadowMapResolution);
}

void ULightComponent::SetShadowDistance(float Distance) {
	mShadowDistance = SanitizeShadowValue(Distance, FLightShadowSettings{}.mDistance, 0.01f);
}

void ULightComponent::SetShadowDepthBias(float DepthBias) {
	mShadowDepthBias = SanitizeShadowValue(DepthBias, FLightShadowSettings{}.mDepthBias, 0.0f);
}

void ULightComponent::SetShadowSlopeScaledDepthBias(float SlopeScaledDepthBias) {
	mShadowSlopeScaledDepthBias = SanitizeShadowValue(SlopeScaledDepthBias, FLightShadowSettings{}.mSlopeScaledDepthBias, 0.0f);
}

void ULightComponent::SetShadowNormalBias(float NormalBias) {
	mShadowNormalBias = SanitizeShadowValue(NormalBias, FLightShadowSettings{}.mNormalBias, 0.0f);
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
