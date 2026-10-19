#include "EnginePCH.h"
#include "SpotLightSceneProxy.h"

#include "Components/SpotLightComponent.h"
#include "Math/Transform.h"
#include "Render/ShadowView.h"

void FSpotLightSceneProxy::GetLightShaderParameters(FLightRenderParameters& Out) const {
	FPointLightSceneProxy::GetLightShaderParameters(Out);

	const USpotLightComponent* Spot{ static_cast<const USpotLightComponent*>(LightComponent) };
	FTransform Transform{};
	Transform.Rotation = Spot->GetWorldRotation();
	const float Outer{ FMath::Clamp(Spot->GetOuterConeAngle(), 0.0f, 89.0f) };
	const float Inner{ FMath::Clamp(Spot->GetInnerConeAngle(), 0.0f, Outer) };

	Out.Type = static_cast<uint32>(ELightType::Spot);
	Out.Direction = Transform.GetForward();
	Out.InnerConeCos = std::cos(FMath::DegreesToRadians(Inner));
	Out.OuterConeCos = std::cos(FMath::DegreesToRadians(Outer));
}

bool FSpotLightSceneProxy::BuildShadowView(FShadowView& OutView) const {
	const FLightShadowSettings Settings{ GetShadowSettings() };
	if (!IsVisible() || !Settings.mCastShadows) {
		return false;
	}

	const USpotLightComponent* Spot{ static_cast<const USpotLightComponent*>(LightComponent) };
	const FTransform LightTransform{ Spot->GetWorldRotation(), Spot->GetWorldLocation() };
	return BuildSpotShadowView(LightTransform, Spot->GetOuterConeAngle(), Spot->GetAttenuationRadius(), OutView);
}
