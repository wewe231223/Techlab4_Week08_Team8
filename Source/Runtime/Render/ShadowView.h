#pragma once

#include "Math/Vector.h"
#include "Math/Matrix.h"
#include "ShadowMap.h"

class FTexture2D;

struct FShadowView {
	FMatrix mView{ FMatrix::Identity };
	FMatrix mProjection{ FMatrix::Identity };
	FMatrix mViewProjection{ FMatrix::Identity };
	FVector mWorldPosition{};
	float mNearPlane{};
	float mFarPlane{};
};

struct FShadowMapRequest {
	FShadowView mView{};
	FLightShadowSettings mSettings{};
};

bool BuildDirectionalShadowView(const FVector& LightDirection, const FVector& Center, float Radius, FShadowView& OutView);
FShadowMapRenderParameters BuildShadowMapRenderParameters(const FShadowView& View, const FLightShadowSettings& Settings, const FTexture2D* DepthTexture, const FShadowAtlasTile& Tile);
