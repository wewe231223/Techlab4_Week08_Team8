#pragma once

#include "Math/Vector.h"
#include "Math/Matrix.h"
#include "ShadowMap.h"

class ULightComponent;
struct FBox;

struct FShadowView {
	FMatrix mView{ FMatrix::Identity };
	FMatrix mProjection{ FMatrix::Identity };
	FMatrix mViewProjection{ FMatrix::Identity };
	FVector mWorldPosition{};
	float mNearPlane{};
	float mFarPlane{};
	bool mIsOrthographic{ true };
	bool mIsOmnidirectional{ false };
};

struct FShadowMapRequest {
	FShadowView mViews[PointShadowFaceCount]{};
	uint32 mViewCount{ 1 };
	FLightShadowSettings mSettings{};
	const ULightComponent* mLightComponent{};
};

bool BuildDirectionalShadowView(const FVector& LightDirection, const FBox& CasterBounds, int32 Resolution, FShadowView& OutView);
bool BuildSpotShadowView(const FVector& LightPosition, const FVector& LightDirection, float OuterConeAngle, float AttenuationRadius, FShadowView& OutView);
bool BuildPointShadowViews(const FVector& LightPosition, float AttenuationRadius, FShadowView (&OutViews)[PointShadowFaceCount]);
