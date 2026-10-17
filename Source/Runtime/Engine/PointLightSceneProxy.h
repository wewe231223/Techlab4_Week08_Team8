#pragma once

#include "LocalLightSceneProxy.h"

struct FShadowView;

class FPointLightSceneProxy : public FLocalLightSceneProxy {
public:
	using FLocalLightSceneProxy::FLocalLightSceneProxy;

public:
	void GetLightShaderParameters(FLightRenderParameters& Out) const override;
	bool BuildShadowViews(FShadowView (&OutViews)[PointShadowFaceCount]) const;
};
