#pragma once

#include "LocalLightComponent.h"

class URectLightComponent :public ULocalLightComponent
{
	DECLARE_CLASS(URectLightComponent, ULocalLightComponent)
	REFLECT_START(URectLightComponent)
		PROPERTY(HalfWidth)
		PROPERTY(HalfHeight)
		PROPERTY(ShadowSourceRadius)
		PROPERTY(BarnDoorAngle)
		PROPERTY(BarnDoorLength)
	REFLECT_END()
public:
	URectLightComponent();
	virtual ~URectLightComponent() override;
	virtual FLightSceneProxy* CreateSceneProxy() const override;
	virtual void PostEditChangeProperty(const FProperty& Property) override;
	virtual FLightShadowSettings GetShadowSettings() const override;
	float GetShadowSourceRadius() const;
	void SetShadowSourceRadius(float Radius);

	ELightType GetLightType() const { return ELightType::Rect; }
	void DrawDebug(FLineBatcher* LineBatcher) const;

	// Getter
	float GetWidth() const { return HalfWidth * 2.0f; }
	float GetHeight() const { return HalfHeight * 2.0f; }

	void SetBarnDoorAngle(float InAngle) { BarnDoorAngle = std::max(0.0f, InAngle); }
	float GetBarnDoorAngle() const { return BarnDoorAngle; }

	void SetBarnDoorLength(float InLength) { BarnDoorLength = std::clamp(InLength, 0.0f, 90.0f); }
	float GetBarnDoorLength() const { return BarnDoorLength; }

private:
	float HalfWidth = 3.0f;
	float HalfHeight = 3.0f;
	float ShadowSourceRadius{ DefaultShadowSourceRadius }; // 긴 변 기준의 그림자 반경. 발광면 크기와 독립적으로 조절한다.
	float BarnDoorAngle = 88.0f;	// 도 단위
	float BarnDoorLength = 2.0f;	
};
