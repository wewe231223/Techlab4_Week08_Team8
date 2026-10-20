#pragma once

#include "LightComponent.h"

class UDirectionalLightComponent :public ULightComponent
{
	DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)
	REFLECT_START(UDirectionalLightComponent)
		PROPERTY(bEnableCSM)
		PROPERTY(CascadeCount)
		PROPERTY(MaxShadowDistance)
		PROPERTY(SplitLambda)
	REFLECT_END()
public:
	UDirectionalLightComponent();
	virtual ~UDirectionalLightComponent() override;

	virtual class FLightSceneProxy* CreateSceneProxy() const override;

	ELightType GetLightType() const override { return ELightType::Directional; }

	bool IsCSMEnabled() const { return bEnableCSM; }
	void SetEnableCSM(bool bInEnable) { bEnableCSM = bInEnable; }

	uint32 GetCascadeCount() const { return CascadeCount; }
	void SetCascadeCount(uint32 InCascadeCount) { CascadeCount = InCascadeCount; }

	float GetMaxShadowDistance() const { return MaxShadowDistance; }
	void SetMaxShadowDistance(float InMaxShadowDistance) { MaxShadowDistance = InMaxShadowDistance; }

	float GetSplitLambda() const { return SplitLambda; }
	void SetSplitLambda(float InSplitLambda) { SplitLambda = InSplitLambda; }

	virtual void PostEditChangeProperty(const FProperty& Property) override;

private:
	bool bEnableCSM = false;
	uint32 CascadeCount = 3;
	float MaxShadowDistance = 150.0f;
	float SplitLambda = 0.85f;
};