#pragma once

#include "LightComponent.h"

class UDirectionalLightComponent :public ULightComponent
{
	DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)
	REFLECT_START(UDirectionalLightComponent)
		PROPERTY(SourceAngle)
	REFLECT_END()
public:
	UDirectionalLightComponent();
	virtual ~UDirectionalLightComponent() override;

	virtual class FLightSceneProxy* CreateSceneProxy() const override;
	virtual void PostEditChangeProperty(const FProperty& Property) override;
	virtual FLightShadowSettings GetShadowSettings() const override;
	float GetSourceAngle() const;
	void SetSourceAngle(float Angle);

	ELightType GetLightType() const override { return ELightType::Directional; }


private:
	float SourceAngle{ DefaultShadowSourceAngle }; // 발광원 전체 각지름(도). 그림자의 부드러움을 결정한다.
};
