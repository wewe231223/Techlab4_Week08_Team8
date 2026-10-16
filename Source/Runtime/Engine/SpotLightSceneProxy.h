#pragma once

#include "PointLightSceneProxy.h"

class FSpotLightSceneProxy : public FPointLightSceneProxy
{
public:
    using FPointLightSceneProxy::FPointLightSceneProxy;
    virtual void GetLightShaderParameters(FLightRenderParameters& Out) const override;
    int32 GetNumShadowMaps() const override { return 1; }   // Point를 상속하지만 원뿔 하나라 1장
};