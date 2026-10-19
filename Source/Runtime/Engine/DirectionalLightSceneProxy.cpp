#include "EnginePCH.h"
#include "DirectionalLightSceneProxy.h"

#include "Components/DirectionalLightComponent.h"
#include "Math/Transform.h"
#include "Render/ShadowView.h"

void FDirectionalLightSceneProxy::GetLightShaderParameters(FLightRenderParameters& Out) const {
	const UDirectionalLightComponent* Dir{ static_cast<const UDirectionalLightComponent*>(LightComponent) };

	FTransform Transform{};
	Transform.Rotation = Dir->GetWorldRotation();
	const FLinearColor Color{ Dir->GetLightColor() };

	Out = FLightRenderParameters{};
	Out.Type = static_cast<uint32>(ELightType::Directional);
	Out.Direction = Transform.GetForward();
	Out.Color = FVector{ Color.R, Color.G, Color.B };
	Out.Intensity = Dir->GetIntensity();
}

bool FDirectionalLightSceneProxy::BuildShadowView(const FBox& CasterBounds, FShadowView& OutView) const {
	const FLightShadowSettings Settings{ GetShadowSettings() };
	if (!IsVisible() || !Settings.mCastShadows) {
		return false;
	}

	const FTransform LightTransform{ LightComponent->GetWorldRotation(), LightComponent->GetWorldLocation() };
	return BuildDirectionalShadowView(LightTransform, CasterBounds, Settings.mResolution, OutView);
}
