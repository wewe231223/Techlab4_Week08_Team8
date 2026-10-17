#pragma once

#include "PointLightSceneProxy.h"

struct FShadowView;

class FSpotLightSceneProxy : public FPointLightSceneProxy {
public:
	using FPointLightSceneProxy::FPointLightSceneProxy;

public:
	void GetLightShaderParameters(FLightRenderParameters& Out) const override;
	bool BuildShadowView(FShadowView& OutView) const;
};
