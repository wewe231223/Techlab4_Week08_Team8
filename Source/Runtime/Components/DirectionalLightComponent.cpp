#include "EnginePCH.h"
#include "DirectionalLightComponent.h"

#include "Engine/DirectionalLightSceneProxy.h"

UDirectionalLightComponent::UDirectionalLightComponent() {
	SetShadowMapResolution(DefaultDirectionalShadowMapResolution);
}

UDirectionalLightComponent::~UDirectionalLightComponent()
{
}

FLightSceneProxy* UDirectionalLightComponent::CreateSceneProxy() const
{
    return new FDirectionalLightSceneProxy(this);
}

void UDirectionalLightComponent::PostEditChangeProperty(const FProperty& Property) {
	Super::PostEditChangeProperty(Property);
	SetSourceAngle(SourceAngle);
}

FLightShadowSettings UDirectionalLightComponent::GetShadowSettings() const {
	FLightShadowSettings Settings{ Super::GetShadowSettings() };
	Settings.mSourceAngle = GetSourceAngle();
	return Settings;
}

float UDirectionalLightComponent::GetSourceAngle() const {
	return NormalizeShadowSourceAngle(SourceAngle);
}

void UDirectionalLightComponent::SetSourceAngle(float Angle) {
	SourceAngle = NormalizeShadowSourceAngle(Angle);
}
