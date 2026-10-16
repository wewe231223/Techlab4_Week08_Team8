#pragma once

#include "LightSceneProxy.h"

class FDirectionalLightSceneProxy : public FLightSceneProxy
{
public:
	using FLightSceneProxy::FLightSceneProxy;
	void GetLightShaderParameters(FLightRenderParameters& Out) const override;
	int32 GetNumShadowMaps() const override { return 1; }
};