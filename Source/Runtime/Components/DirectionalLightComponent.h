#pragma once

#include "LightComponent.h"

class UDirectionalLightComponent : public ULightComponent {
	DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)
	REFLECT_START(UDirectionalLightComponent)
		InClass->AddProperty<decltype(ThisClass::mSourceAngle)>("SourceAngle", offsetof(ThisClass, mSourceAngle));
		InClass->AddProperty<decltype(ThisClass::mEnableCSM)>("bEnableCSM", offsetof(ThisClass, mEnableCSM));
		InClass->AddProperty<decltype(ThisClass::mCascadeCount)>("CascadeCount", offsetof(ThisClass, mCascadeCount));
		InClass->AddProperty<decltype(ThisClass::mMaxShadowDistance)>("MaxShadowDistance", offsetof(ThisClass, mMaxShadowDistance));
		InClass->AddProperty<decltype(ThisClass::mSplitLambda)>("SplitLambda", offsetof(ThisClass, mSplitLambda));
		InClass->AddProperty<decltype(ThisClass::mCascadeTransitionFraction)>("CascadeTransitionFraction", offsetof(ThisClass, mCascadeTransitionFraction));
	REFLECT_END()

public:
	UDirectionalLightComponent();
	virtual ~UDirectionalLightComponent() override;

public:
	virtual FLightSceneProxy* CreateSceneProxy() const override;
	virtual void PostEditChangeProperty(const FProperty& Property) override;
	virtual FLightShadowSettings GetShadowSettings() const override;
	virtual ELightType GetLightType() const override;
	float GetSourceAngle() const;
	void SetSourceAngle(float Angle);
	bool IsCSMEnabled() const;
	void SetEnableCSM(bool Enabled);
	uint32 GetCascadeCount() const;
	void SetCascadeCount(uint32 Count);
	float GetMaxShadowDistance() const;
	void SetMaxShadowDistance(float Distance);
	float GetSplitLambda() const;
	void SetSplitLambda(float Lambda);
	float GetCascadeTransitionFraction() const;
	void SetCascadeTransitionFraction(float Fraction);

private:
	float mSourceAngle{ DefaultShadowSourceAngle };
	bool mEnableCSM{};
	uint32 mCascadeCount{ 3 };
	float mMaxShadowDistance{ 50.0f };
	float mSplitLambda{ 0.85f };
	float mCascadeTransitionFraction{ 0.1f };
};
