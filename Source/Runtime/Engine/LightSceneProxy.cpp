#include "EnginePCH.h"
#include "LightSceneProxy.h"

FLightShadowSettings FLightSceneProxy::GetShadowSettings() const {
	return LightComponent->GetShadowSettings();
}
