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
	using FLightSceneProxy::FLightSceneProxy;
	void GetLightShaderParameters(FLightRenderParameters& Out) const override;
	bool BuildShadowView(const FBox& CasterBounds, FShadowView& OutView) const;

	// 내부에서 CSM ShadowView 생성해서 넣어주기 
	// CSM 단계 조절 추가
	bool BuildShadowViews(const FBox& CasterBounds, FShadowView(&OutViews)[PointShadowFaceCount], const FShadowCameraContext& CamContext) const;

	bool IsCSMEnabled() const 
	{ 
		auto* Dir = static_cast<const UDirectionalLightComponent*>(LightComponent);
		return Dir ? Dir->IsCSMEnabled() : false;
	}
	int32 GetCascadeCount() const 
	{
		auto* Dir = static_cast<const UDirectionalLightComponent*>(LightComponent);
		return Dir ? Dir->GetCascadeCount() : 2;
	}
	float GetMaxShadowDistance() const 
	{
		auto* Dir = static_cast<const UDirectionalLightComponent*>(LightComponent);
		return Dir ? Dir->GetMaxShadowDistance() : 150.0f;
	}
	float GetSplitLambda() const 
	{
		auto* Dir = static_cast<const UDirectionalLightComponent*>(LightComponent);
		return Dir ? Dir->GetSplitLambda() : 0.85f;
	}
private:
};
