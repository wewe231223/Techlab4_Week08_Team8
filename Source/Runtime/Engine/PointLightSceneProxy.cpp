#include "EnginePCH.h"
#include "PointLightSceneProxy.h"

#include "Components/PointLightComponent.h"
#include "Math/Transform.h"
#include "Render/ShadowView.h"

void FPointLightSceneProxy::GetLightShaderParameters(FLightRenderParameters& Out) const {
	const UPointLightComponent* PointLightComponent{ static_cast<const UPointLightComponent*>(LightComponent) };
	const FLinearColor Color{ PointLightComponent->GetLightColor() };

	Out = FLightRenderParameters{};
	Out.Type = static_cast<uint32>(ELightType::Point);
	Out.Position = PointLightComponent->GetWorldLocation();
	Out.Radius = PointLightComponent->GetAttenuationRadius();
	Out.Color = FVector{ Color.R, Color.G, Color.B };
	Out.Intensity = PointLightComponent->GetIntensity();
	Out.FalloffExponent = PointLightComponent->GetFalloff();
}

bool FPointLightSceneProxy::BuildShadowViews(FShadowView (&OutViews)[PointShadowFaceCount]) const {
	if (!IsVisible() || !GetShadowSettings().mCastShadows || LightComponent->GetLightType() != ELightType::Point) {
		return false;
	}
	const UPointLightComponent* Point{ static_cast<const UPointLightComponent*>(LightComponent) };
	const FTransform LightTransform{ Point->GetWorldRotation(), Point->GetWorldLocation() };
	return BuildPointShadowViews(LightTransform, Point->GetAttenuationRadius(), OutViews);
}
