#pragma once

#include "LightComponent.h"

class USkyLightComponent : public ULightComponent
{
	DECLARE_CLASS(USkyLightComponent, ULightComponent)
	REFLECT_START(USkyLightComponent)
	REFLECT_END()
public:
	USkyLightComponent();
	virtual ~USkyLightComponent() override;

	ELightType GetLightType() const override { return ELightType::Sky; }
	// 앰비언트라 그림자가 없다 (프록시도 만들지 않으므로 호출되지 않음)
	FMatrix GetShadowViewPorjection(const uint32 ShadowMapResolution, FVector Center, float Radius, int index = 0) const override { return FMatrix::Identity; }
	
protected:
private:
};