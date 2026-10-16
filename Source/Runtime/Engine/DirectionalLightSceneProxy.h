#pragma once

#include "LightSceneProxy.h"

struct FShadowView;
struct FBox;

class FDirectionalLightSceneProxy : public FLightSceneProxy
{
public:
	using FLightSceneProxy::FLightSceneProxy;
	void GetLightShaderParameters(FLightRenderParameters& Out) const override;
	bool BuildShadowView(const FBox& CasterBounds, FShadowView& OutView) const;
};
