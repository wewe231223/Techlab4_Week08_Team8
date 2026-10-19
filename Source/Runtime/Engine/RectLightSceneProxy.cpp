#include "EnginePCH.h"
#include "RectLightSceneProxy.h"

#include "Components/RectLightComponent.h"
#include "Math/Transform.h"
#include "Render/ShadowView.h"

void FRectLightSceneProxy::GetLightShaderParameters(FLightRenderParameters& Out) const {
	const URectLightComponent* RectLightComponent{ static_cast<const URectLightComponent*>(LightComponent) };
	const FLinearColor Color{ RectLightComponent->GetLightColor() };
	FTransform Transform{};
	Transform.Rotation = RectLightComponent->GetWorldRotation();

	Out = FLightRenderParameters{};
	Out.Type = static_cast<uint32>(ELightType::Rect);
	Out.Position = RectLightComponent->GetWorldLocation();
	Out.Radius = RectLightComponent->GetAttenuationRadius();
	Out.Color = FVector{ Color.R, Color.G, Color.B };
	Out.Intensity = RectLightComponent->GetIntensity();
	Out.FalloffExponent = 1.0f;
	Out.Direction = Transform.GetForward();
	Out.Right = Transform.GetRight();
	Out.HalfWidth = RectLightComponent->GetWidth() * 0.5f;
	Out.HalfHeight = RectLightComponent->GetHeight() * 0.5f;

	const float AngleRad{ FMath::DegreesToRadians(FMath::Clamp(RectLightComponent->GetBarnDoorAngle(), 0.0f, 90.0f)) };
	Out.BarnDoorCos = std::cos(AngleRad);
	Out.BarnDoorSin = std::sin(AngleRad);
	Out.BarnDoorLength = std::max(RectLightComponent->GetBarnDoorLength(), 0.0f);
}

bool FRectLightSceneProxy::BuildShadowView(FShadowView& OutView) const {
	const FLightShadowSettings Settings{ GetShadowSettings() };
	if (!IsVisible() || !Settings.mCastShadows) {
		return false;
	}

	const URectLightComponent* Rect{ static_cast<const URectLightComponent*>(LightComponent) };
	const FTransform LightTransform{ Rect->GetWorldRotation(), Rect->GetWorldLocation() };
	return BuildRectShadowView(LightTransform, Rect->GetWidth() * 0.5f, Rect->GetHeight() * 0.5f, Rect->GetAttenuationRadius(), OutView);
}
