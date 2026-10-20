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

void UDirectionalLightComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);

	if (Property.Name == "CascadeCount")
	{
		CascadeCount = std::clamp(CascadeCount, static_cast<uint32>(2), static_cast<uint32>(4));
	}
    else if (Property.Name == "SplitLambda")
    {
        SplitLambda = std::clamp(SplitLambda, 0.0f, 1.0f);
    }
    else if (Property.Name == "MaxShadowDistance")
    {
        MaxShadowDistance = std::max(MaxShadowDistance, 150.0f);
    }
}
