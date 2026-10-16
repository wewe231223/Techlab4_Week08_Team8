#pragma once

#include "LocalLightSceneProxy.h"

class FPointLightSceneProxy : public FLocalLightSceneProxy
{
public:
    using FLocalLightSceneProxy::FLocalLightSceneProxy;
    void GetLightShaderParameters(FLightRenderParameters& Out) const override;
    int32 GetNumShadowMaps() const override { return 6; }   // +X, -X, +Y, -Y, +Z, -Z
};