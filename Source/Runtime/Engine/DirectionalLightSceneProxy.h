#pragma once

#include "LightSceneProxy.h"

struct FShadowView;

class FDirectionalLightSceneProxy : public FLightSceneProxy
{
public:
	using FLightSceneProxy::FLightSceneProxy;
	void GetLightShaderParameters(FLightRenderParameters& Out) const override;
	bool BuildShadowView(const FVector& Center, FShadowView& OutView) const;
};