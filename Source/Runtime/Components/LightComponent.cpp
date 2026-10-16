#include "EnginePCH.h"
#include "LightComponent.h"

#include "Engine/Scene.h"
#include "Engine/World.h"

#include <cmath>

ULightComponent::ULightComponent() {}
ULightComponent::~ULightComponent() {}
void ULightComponent::DrawDebug(FLineBatcher* LineBatcher) const {}

void ULightComponent::PostEditChangeProperty(const FProperty& Property) {
	Super::PostEditChangeProperty(Property);
	SetShadowMapResolution(mShadowMapResolution);
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

void ULightComponent::SetShadowDepthBias(float DepthBias) {
	mShadowDepthBias = std::max(DepthBias, 0.0f);
}

void ULightComponent::SetShadowSlopeScaledDepthBias(float SlopeScaledDepthBias) {
	mShadowSlopeScaledDepthBias = std::max(SlopeScaledDepthBias, 0.0f);
}

void ULightComponent::SetShadowNormalBias(float NormalBias) {
	mShadowNormalBias = std::max(NormalBias, 0.0f);
}

void ULightComponent::CreateRenderState()
{
	Super::CreateRenderState();
	GetWorld()->GetScene().AddLight(this);
}

void ULightComponent::DestroyRenderState()
{
	GetWorld()->GetScene().RemoveLight(this);
	Super::DestroyRenderState();
}
