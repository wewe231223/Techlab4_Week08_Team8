#pragma once

#include "LightComponent.h"
#include <algorithm>

class UDirectionalLightComponent :public ULightComponent
{
	DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)
	REFLECT_START(UDirectionalLightComponent)
		PROPERTY(bEnableCSM)
		PROPERTY(CascadeCount)
		PROPERTY(MaxShadowDistance)
		PROPERTY(SplitLambda)
		PROPERTY(CascadeTransitionFraction)
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
	void SetSplitLambda(float InSplitLambda) { SplitLambda = std::clamp(InSplitLambda, 0.0f, 1.0f); }

	float GetCascadeTransitionFraction() const { return CascadeTransitionFraction; }
	void SetCascadeTransitionFraction(float InFraction) { CascadeTransitionFraction = std::clamp(InFraction, 0.0f, 0.3f); }

	virtual void PostEditChangeProperty(const FProperty& Property) override;

private:
	bool bEnableCSM = false;
	uint32 CascadeCount = 3;
	float MaxShadowDistance = 50.0f;
	float SplitLambda = 0.85f;
	float CascadeTransitionFraction = 0.1f;
};