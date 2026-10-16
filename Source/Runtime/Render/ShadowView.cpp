#include "EnginePCH.h"
#include "ShadowView.h"
#include "Texture2D.h"

#include <algorithm>
#include <cmath>

namespace {
	bool IsFiniteShadowVector(const FVector& Value) {
		return std::isfinite(Value.X) && std::isfinite(Value.Y) && std::isfinite(Value.Z);
	}
}

bool BuildDirectionalShadowView(const FVector& LightDirection, const FVector& Center, float Radius, FShadowView& OutView) {
	if (!IsFiniteShadowVector(LightDirection) || !IsFiniteShadowVector(Center) || !std::isfinite(Radius) || Radius < 0.01f) {
		return false;
	}

	const float DirectionLengthSquared{ LightDirection.SizeSquared() };
	if (!std::isfinite(DirectionLengthSquared) || DirectionLengthSquared < 1.0e-8f) {
		return false;
	}

	const FVector Forward{ LightDirection / std::sqrt(DirectionLengthSquared) };
	const FVector ReferenceUp{ std::abs(Forward.Z) < 0.99f ? FVector{ 0.0f, 0.0f, 1.0f } : FVector{ 0.0f, 1.0f, 0.0f } };
	const FVector Right{ FVector::Cross(ReferenceUp, Forward).GetSafeNormal() };
	const FVector Up{ FVector::Cross(Forward, Right) };
	constexpr float NearPlane{ 0.01f };
	const float FarPlane{ Radius * 2.0f + NearPlane };
	const FVector Position{ Center - Forward * (Radius + NearPlane) };
	const float ForwardOffset{ -FVector::Dot(Position, Forward) };
	const float RightOffset{ -FVector::Dot(Position, Right) };
	const float UpOffset{ -FVector::Dot(Position, Up) };
	if (!std::isfinite(FarPlane) || !IsFiniteShadowVector(Position) || !std::isfinite(ForwardOffset) || !std::isfinite(RightOffset) || !std::isfinite(UpOffset)) {
		return false;
	}

	const float Scale{ 1.0f / Radius };
	const float DepthScale{ 1.0f / (FarPlane - NearPlane) };
	const FMatrix View{
		Forward.X, Right.X, Up.X, 0.0f,
		Forward.Y, Right.Y, Up.Y, 0.0f,
		Forward.Z, Right.Z, Up.Z, 0.0f,
		ForwardOffset, RightOffset, UpOffset, 1.0f
	};
	const FMatrix Projection{
		0.0f, 0.0f, DepthScale, 0.0f,
		Scale, 0.0f, 0.0f, 0.0f,
		0.0f, Scale, 0.0f, 0.0f,
		0.0f, 0.0f, -NearPlane * DepthScale, 1.0f
	};

	FShadowView Result{};
	Result.mView = View;
	Result.mProjection = Projection;
	Result.mViewProjection = View * Projection;
	for (uint32 Row{}; Row < 4; ++Row) {
		for (uint32 Column{}; Column < 4; ++Column) {
			if (!std::isfinite(Result.mViewProjection.M[Row][Column])) {
				return false;
			}
		}
	}
	Result.mWorldPosition = Position;
	Result.mNearPlane = NearPlane;
	Result.mFarPlane = FarPlane;
	OutView = Result;
	return true;
}

FShadowMapRenderParameters BuildShadowMapRenderParameters(const FShadowView& View, const FLightShadowSettings& Settings, const FTexture2D* DepthTexture, const FShadowAtlasTile& Tile) {
	FShadowMapRenderParameters Parameters{};
	if (!DepthTexture || !DepthTexture->GetSRV() || !DepthTexture->GetDSV() || Tile.mResolution == 0 || Tile.mResolution > DepthTexture->GetWidth() || Tile.mResolution > DepthTexture->GetHeight() || Tile.mX > DepthTexture->GetWidth() - Tile.mResolution || Tile.mY > DepthTexture->GetHeight() - Tile.mResolution) {
		return Parameters;
	}

	Parameters.mLightViewProjection = View.mViewProjection;
	Parameters.mTexelSize = FVector2{ 1.0f / static_cast<float>(DepthTexture->GetWidth()), 1.0f / static_cast<float>(DepthTexture->GetHeight()) };
	Parameters.mUvScaleBias = FVector4{ static_cast<float>(Tile.mResolution) * Parameters.mTexelSize.X, static_cast<float>(Tile.mResolution) * Parameters.mTexelSize.Y, static_cast<float>(Tile.mX) * Parameters.mTexelSize.X, static_cast<float>(Tile.mY) * Parameters.mTexelSize.Y };
	Parameters.mDepthBias = std::isfinite(Settings.mDepthBias) ? std::max(Settings.mDepthBias, 0.0f) : FLightShadowSettings{}.mDepthBias;
	Parameters.mNormalBias = std::isfinite(Settings.mNormalBias) ? std::max(Settings.mNormalBias, 0.0f) : FLightShadowSettings{}.mNormalBias;
	return Parameters;
}
