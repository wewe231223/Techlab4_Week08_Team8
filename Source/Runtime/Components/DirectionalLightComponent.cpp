#include "EnginePCH.h"
#include "DirectionalLightComponent.h"

#include "Engine/DirectionalLightSceneProxy.h"
#include "Core/StatOverlay.h"

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

void UDirectionalLightComponent::SetEnableCSM(bool bInEnable)
{
	bEnableCSM = bInEnable;
	FCSMDebugSnapshot Snap{};
	Snap.bEnabled = bInEnable;
	Snap.CascadeCount = bInEnable ? CascadeCount : 0;
	FStatOverlay::UpdateCSMStats(Snap);
}

void UDirectionalLightComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);

	if (Property.Name == "bEnableCSM")
	{
		FCSMDebugSnapshot Snap{};
		Snap.bEnabled = bEnableCSM;
		Snap.CascadeCount = bEnableCSM ? CascadeCount : 0;
		FStatOverlay::UpdateCSMStats(Snap);
	}
	else if (Property.Name == "CascadeCount")
	{
		CascadeCount = std::clamp(CascadeCount, static_cast<uint32>(2), static_cast<uint32>(4));
		FCSMDebugSnapshot Snap = FStatOverlay::GetCSMStats();
		Snap.CascadeCount = CascadeCount;
		FStatOverlay::UpdateCSMStats(Snap);
	}
    else if (Property.Name == "SplitLambda")
    {
        SplitLambda = std::clamp(SplitLambda, 0.0f, 1.0f);
    }
    else if (Property.Name == "MaxShadowDistance")
    {
        MaxShadowDistance = std::max(MaxShadowDistance, 50.0f);
    }
    else if (Property.Name == "CascadeTransitionFraction")
    {
        CascadeTransitionFraction = std::clamp(CascadeTransitionFraction, 0.0f, 0.3f);
    }
}
