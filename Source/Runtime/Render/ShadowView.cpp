#include "EnginePCH.h"
#include "ShadowView.h"
#include "Texture2D.h"
#include "Math/Box.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
	bool IsFiniteShadowVector(const FVector& Value) {
		return std::isfinite(Value.X) && std::isfinite(Value.Y) && std::isfinite(Value.Z);
	}

	bool IsFiniteShadowMatrix(const FMatrix& Matrix) {
		for (uint32 Row{}; Row < 4; ++Row) {
			for (uint32 Column{}; Column < 4; ++Column) {
				if (!std::isfinite(Matrix.M[Row][Column])) {
					return false;
				}
			}
		}
		return true;
	}
}

bool BuildDirectionalShadowView(const FVector& LightDirection, const FBox& CasterBounds, int32 Resolution, FShadowView& OutView) {
	if (!IsFiniteShadowVector(LightDirection) || !IsFiniteShadowVector(CasterBounds.Min) || !IsFiniteShadowVector(CasterBounds.Max) || CasterBounds.Min.X > CasterBounds.Max.X || CasterBounds.Min.Y > CasterBounds.Max.Y || CasterBounds.Min.Z > CasterBounds.Max.Z) {
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
	const FVector Center{ CasterBounds.Min * 0.5f + CasterBounds.Max * 0.5f };
	FVector Minimum{ std::numeric_limits<float>::max() };
	FVector Maximum{ std::numeric_limits<float>::lowest() };
	for (uint32 Index{}; Index < 8; ++Index) {
		const FVector Corner{ Index & 1 ? CasterBounds.Max.X : CasterBounds.Min.X, Index & 2 ? CasterBounds.Max.Y : CasterBounds.Min.Y, Index & 4 ? CasterBounds.Max.Z : CasterBounds.Min.Z };
		const FVector Offset{ Corner - Center };
		const FVector LightPosition{ FVector::Dot(Offset, Forward), FVector::Dot(Offset, Right), FVector::Dot(Offset, Up) };
		for (uint32 Axis{}; Axis < 3; ++Axis) {
			Minimum[Axis] = std::min(Minimum[Axis], LightPosition[Axis]);
			Maximum[Axis] = std::max(Maximum[Axis], LightPosition[Axis]);
		}
	}

	const float BorderScale{ 1.0f + 2.0f / static_cast<float>(std::clamp(Resolution, MinShadowMapResolution, MaxShadowMapResolution)) };
	const float Width{ std::max(Maximum.Y - Minimum.Y, 0.01f) * BorderScale };
	const float Height{ std::max(Maximum.Z - Minimum.Z, 0.01f) * BorderScale };
	constexpr float NearPlane{ 0.01f };
	constexpr float DepthPadding{ 0.01f };
	const float FarPlane{ NearPlane + std::max(Maximum.X - Minimum.X, 0.01f) + DepthPadding * 2.0f };
	const FVector Position{ Center + Forward * (Minimum.X - NearPlane - DepthPadding) + Right * ((Minimum.Y + Maximum.Y) * 0.5f) + Up * ((Minimum.Z + Maximum.Z) * 0.5f) };
	const float ForwardOffset{ -FVector::Dot(Position, Forward) };
	const float RightOffset{ -FVector::Dot(Position, Right) };
	const float UpOffset{ -FVector::Dot(Position, Up) };
	if (!std::isfinite(FarPlane) || !IsFiniteShadowVector(Position) || !std::isfinite(ForwardOffset) || !std::isfinite(RightOffset) || !std::isfinite(UpOffset)) {
		return false;
	}

	const float WidthScale{ 2.0f / Width };
	const float HeightScale{ 2.0f / Height };
	const float DepthScale{ 1.0f / (FarPlane - NearPlane) };
	if (!std::isfinite(WidthScale) || !std::isfinite(HeightScale) || std::min({ WidthScale, HeightScale, DepthScale }) <= 1.0e-6f) {
		return false;
	}
	const FMatrix View{
		Forward.X, Right.X, Up.X, 0.0f,
		Forward.Y, Right.Y, Up.Y, 0.0f,
		Forward.Z, Right.Z, Up.Z, 0.0f,
		ForwardOffset, RightOffset, UpOffset, 1.0f
	};
	const FMatrix Projection{
		0.0f, 0.0f, DepthScale, 0.0f,
		WidthScale, 0.0f, 0.0f, 0.0f,
		0.0f, HeightScale, 0.0f, 0.0f,
		0.0f, 0.0f, -NearPlane * DepthScale, 1.0f
	};

	FShadowView Result{};
	Result.mView = View;
	Result.mProjection = Projection;
	Result.mViewProjection = View * Projection;
	if (!IsFiniteShadowMatrix(Result.mViewProjection)) {
		return false;
	}
	Result.mWorldPosition = Position;
	Result.mNearPlane = NearPlane;
	Result.mFarPlane = FarPlane;
	OutView = Result;
	return true;
}

bool BuildSpotShadowView(const FVector& LightPosition, const FVector& LightDirection, float OuterConeAngle, float AttenuationRadius, FShadowView& OutView) {
	if (!IsFiniteShadowVector(LightPosition) || !IsFiniteShadowVector(LightDirection) || !std::isfinite(OuterConeAngle) || OuterConeAngle <= 0.0f || !std::isfinite(AttenuationRadius) || AttenuationRadius <= 0.0f) {
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
	const float SafeOuterAngle{ std::clamp(OuterConeAngle, 0.1f, 89.0f) };
	const float NearPlane{ std::min(0.01f, AttenuationRadius * 0.01f) };
	const float ProjectionScale{ 1.0f / std::tan(FMath::DegreesToRadians(SafeOuterAngle)) };
	const float DepthScale{ AttenuationRadius / (AttenuationRadius - NearPlane) };
	const FMatrix View{
		Forward.X, Right.X, Up.X, 0.0f,
		Forward.Y, Right.Y, Up.Y, 0.0f,
		Forward.Z, Right.Z, Up.Z, 0.0f,
		-FVector::Dot(LightPosition, Forward), -FVector::Dot(LightPosition, Right), -FVector::Dot(LightPosition, Up), 1.0f
	};
	const FMatrix Projection{
		0.0f, 0.0f, DepthScale, 1.0f,
		ProjectionScale, 0.0f, 0.0f, 0.0f,
		0.0f, ProjectionScale, 0.0f, 0.0f,
		0.0f, 0.0f, -NearPlane * DepthScale, 0.0f
	};

	FShadowView Result{};
	Result.mView = View;
	Result.mProjection = Projection;
	Result.mViewProjection = View * Projection;
	if (NearPlane <= 0.0f || !IsFiniteShadowMatrix(Result.mViewProjection)) {
		return false;
	}
	Result.mWorldPosition = LightPosition;
	Result.mNearPlane = NearPlane;
	Result.mFarPlane = AttenuationRadius;
	Result.mIsOrthographic = false;
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
