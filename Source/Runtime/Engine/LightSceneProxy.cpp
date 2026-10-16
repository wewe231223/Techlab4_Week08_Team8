#include "EnginePCH.h"
#include "LightSceneProxy.h"

FLightSceneProxy::FLightSceneProxy(const ULightComponent* InLightComponent)
	: LightComponent{ InLightComponent } {
}

const ULightComponent* FLightSceneProxy::GetLightComponent() const {
	return LightComponent;
}

bool FLightSceneProxy::IsVisible() const {
	return LightComponent->IsVisible();
}

FLightShadowSettings FLightSceneProxy::GetShadowSettings() const {
	return LightComponent->GetShadowSettings();
}
