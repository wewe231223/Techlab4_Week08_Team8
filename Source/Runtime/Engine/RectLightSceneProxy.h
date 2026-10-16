#pragma once

#include "LocalLightSceneProxy.h"

class FRectLightSceneProxy : public FLocalLightSceneProxy
{
public:
    using FLocalLightSceneProxy::FLocalLightSceneProxy;
    void GetLightShaderParameters(FLightRenderParameters& Out) const override;
    int32 GetNumShadowMaps() const override { return 1; }
}; 