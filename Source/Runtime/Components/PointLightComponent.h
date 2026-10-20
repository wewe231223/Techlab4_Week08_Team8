#pragma once

#include "LocalLightComponent.h"

class UPointLightComponent : public ULocalLightComponent
{
	DECLARE_CLASS(UPointLightComponent, ULocalLightComponent)
	REFLECT_START(UPointLightComponent)
		PROPERTY(LightFalloffExponent)
		PROPERTY(SourceRadius)
	REFLECT_END()
public:
	UPointLightComponent();
	virtual ~UPointLightComponent() override;

	virtual FLightSceneProxy* CreateSceneProxy() const override;
	virtual void PostEditChangeProperty(const FProperty& Property) override;
	virtual FLightShadowSettings GetShadowSettings() const override;
	float GetSourceRadius() const;
	void SetSourceRadius(float Radius);

	void DrawDebug(FLineBatcher* LineBatcher) const override;
	ELightType GetLightType() const override { return ELightType::Point; }

	float GetFalloff() const { return LightFalloffExponent; }
	void SetFalloff(float InFalloff) { LightFalloffExponent = InFalloff; }


protected:
	float LightFalloffExponent = 1.0f;
private:
	float SourceRadius{ DefaultShadowSourceRadius }; // Spot도 상속하는 월드 단위 발광면 반경.
};
