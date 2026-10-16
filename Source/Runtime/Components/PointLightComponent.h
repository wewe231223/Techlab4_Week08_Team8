#pragma once

#include "LocalLightComponent.h"

class UPointLightComponent : public ULocalLightComponent
{
	DECLARE_CLASS(UPointLightComponent, ULocalLightComponent)
	REFLECT_START(UPointLightComponent)
		PROPERTY(LightFalloffExponent)
	REFLECT_END()
public:
	UPointLightComponent();
	virtual ~UPointLightComponent() override;

	virtual FLightSceneProxy* CreateSceneProxy() const override;

	void DrawDebug(FLineBatcher* LineBatcher) const override;
	ELightType GetLightType() const override { return ELightType::Point; }
	// index 0~5: +X, -X, +Y, -Y, +Z, -Z 면
	FMatrix GetShadowViewPorjection(const uint32 ShadowMapResolution, FVector Center, float Radius, int index = 0) const override;

	float GetFalloff() const { return LightFalloffExponent; }
	void SetFalloff(float InFalloff) { LightFalloffExponent = InFalloff; }


protected:
	float LightFalloffExponent = 1.0f;
private:
	
};