#pragma once

#include "LightSceneProxy.h"
#include "Render/ShadowView.h"

struct FBox;

class FDirectionalLightSceneProxy : public FLightSceneProxy {
public:
	using FLightSceneProxy::FLightSceneProxy;

public:
	void GetLightShaderParameters(FLightRenderParameters& Out) const override;
	bool BuildShadowView(const FBox& CasterBounds, FShadowView& OutView) const;
	bool BuildShadowViews(const FBox& CasterBounds, FShadowView (&OutViews)[PointShadowFaceCount], const FShadowCameraContext& CameraContext) const;
	bool IsCSMEnabled() const;
	uint32 GetCascadeCount() const;
	float GetMaxShadowDistance() const;
	float GetSplitLambda() const;
	FVector4 GetCascadeSplits() const;
	float GetCascadeTransitionFraction() const;

private:
	mutable FVector4 mCascadeSplits{};
};
