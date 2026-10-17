#pragma once

#include "LocalLightSceneProxy.h"

struct FShadowView;

class FRectLightSceneProxy : public FLocalLightSceneProxy {
public:
	using FLocalLightSceneProxy::FLocalLightSceneProxy;

public:
	void GetLightShaderParameters(FLightRenderParameters& Out) const override;
	bool BuildShadowView(FShadowView& OutView) const;
};
