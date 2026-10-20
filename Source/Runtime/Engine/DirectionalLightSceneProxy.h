#pragma once

#include "LightSceneProxy.h"
#include "Components/DirectionalLightComponent.h"
#include "Render/ShadowView.h"

struct FShadowView;
struct FBox;
//struct FShadowView;

class FDirectionalLightSceneProxy : public FLightSceneProxy
{
public:
	FDirectionalLightSceneProxy(const UDirectionalLightComponent* InComponent)
		:FLightSceneProxy(InComponent)
	{
		bEnableCSM = InComponent->IsCSMEnabled();
		CascadeCount = InComponent->GetCascadeCount();
		MaxShadowDistance = InComponent->GetMaxShadowDistance();
		SplitLambda = InComponent->GetSplitLambda();
	}
	using FLightSceneProxy::FLightSceneProxy;
	void GetLightShaderParameters(FLightRenderParameters& Out) const override;
	bool BuildShadowView(const FBox& CasterBounds, FShadowView& OutView) const;

	// 내부에서 CSM ShadowView 생성해서 넣어주기 
	// CSM 단계 조절 추가
	bool BuildShadowViews(const FBox& CasterBounds, FShadowView(&OutViews)[PointShadowFaceCount], const FShadowCameraContext& CamContext) const;

	bool IsCSMEnabled() const { return bEnableCSM; }
	int32 GetCascadeCount() const { return CascadeCount; }
	float GetMaxShadowDistance() const { return MaxShadowDistance; }
	float GetSplitLambda() const { return SplitLambda; }
private:
	bool bEnableCSM = false;
	int32 CascadeCount = 3;
	float MaxShadowDistance = 150.0f;
	float SplitLambda = 0.85f;
};
