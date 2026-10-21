#include "EnginePCH.h"
#include "ShadowView.h"
#include "Math/Box.h"
#include "Math/Transform.h"

#include <algorithm>
#include <cmath>

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

	bool BuildShadowBasis(const FTransform& LightTransform, FVector& OutForward, FVector& OutRight, FVector& OutUp) {
		const FQuat Orientation{ LightTransform.GetOrientation() };
		OutForward = Orientation.RotateVector(FVector{ 1.0f, 0.0f, 0.0f });
		OutRight = Orientation.RotateVector(FVector{ 0.0f, 1.0f, 0.0f });
		OutUp = Orientation.RotateVector(FVector{ 0.0f, 0.0f, 1.0f });
		return IsFiniteShadowVector(OutForward) && IsFiniteShadowVector(OutRight) && IsFiniteShadowVector(OutUp);
	}

	FMatrix BuildShadowViewMatrix(const FVector& Position, const FVector& Forward, const FVector& Right, const FVector& Up) {
		return FMatrix{
			Forward.X, Right.X, Up.X, 0.0f,
			Forward.Y, Right.Y, Up.Y, 0.0f,
			Forward.Z, Right.Z, Up.Z, 0.0f,
			-FVector::Dot(Position, Forward), -FVector::Dot(Position, Right), -FVector::Dot(Position, Up), 1.0f
		};
	}

	FMatrix BuildPerspectiveShadowProjection(float WidthScale, float HeightScale, float NearPlane, float FarPlane) {
		const float DepthScale{ FarPlane / (FarPlane - NearPlane) };
		return FMatrix{
			0.0f, 0.0f, DepthScale, 1.0f,
			WidthScale, 0.0f, 0.0f, 0.0f,
			0.0f, HeightScale, 0.0f, 0.0f,
			0.0f, 0.0f, -NearPlane * DepthScale, 0.0f
		};
	}

	bool FinalizeShadowView(FShadowView& Result, const FVector& Position, float NearPlane, float FarPlane, FShadowView& OutView) {
		if (!std::isfinite(NearPlane) || !std::isfinite(FarPlane) || NearPlane < 0.0f || FarPlane <= NearPlane) {
			return false;
		}
		Result.mViewProjection = Result.mView * Result.mProjection;
		if (!IsFiniteShadowMatrix(Result.mViewProjection)) {
			return false;
		}
		Result.mWorldPosition = Position;
		Result.mNearPlane = NearPlane;
		Result.mFarPlane = FarPlane;
		OutView = Result;
		return true;
	}
}

bool BuildDirectionalShadowView(const FTransform& LightTransform, const FBox& CasterBounds, int32 Resolution, FShadowView& OutView) {
	if (!IsFiniteShadowVector(CasterBounds.Min) || !IsFiniteShadowVector(CasterBounds.Max) || CasterBounds.Min.X > CasterBounds.Max.X || CasterBounds.Min.Y > CasterBounds.Max.Y || CasterBounds.Min.Z > CasterBounds.Max.Z) {
		return false;
	}

	FVector Forward{};
	FVector Right{};
	FVector Up{};
	if (!BuildShadowBasis(LightTransform, Forward, Right, Up)) {
		return false;
	}

	const FVector Center{ CasterBounds.Min * 0.5f + CasterBounds.Max * 0.5f };
	const FVector Extent{ CasterBounds.Max * 0.5f - CasterBounds.Min * 0.5f };
	const FVector Maximum{ FVector::Dot(Extent, Forward.GetAbs()), FVector::Dot(Extent, Right.GetAbs()), FVector::Dot(Extent, Up.GetAbs()) };
	const FVector Minimum{ -Maximum };

	const float BorderScale{ 1.0f + 2.0f / static_cast<float>(NormalizeShadowMapResolution(Resolution)) };
	const float Width{ std::max(Maximum.Y - Minimum.Y, 0.01f) * BorderScale };
	const float Height{ std::max(Maximum.Z - Minimum.Z, 0.01f) * BorderScale };
	constexpr float NearPlane{ 0.01f };
	constexpr float DepthPadding{ 0.01f };
	const float FarPlane{ NearPlane + std::max(Maximum.X - Minimum.X, 0.01f) + DepthPadding * 2.0f };
	const FVector Position{ Center + Forward * (Minimum.X - NearPlane - DepthPadding) + Right * ((Minimum.Y + Maximum.Y) * 0.5f) + Up * ((Minimum.Z + Maximum.Z) * 0.5f) };
	const FMatrix View{ BuildShadowViewMatrix(Position, Forward, Right, Up) };
	if (!std::isfinite(FarPlane) || !IsFiniteShadowVector(Position) || !IsFiniteShadowMatrix(View)) {
		return false;
	}

	const float WidthScale{ 2.0f / Width };
	const float HeightScale{ 2.0f / Height };
	const float DepthScale{ 1.0f / (FarPlane - NearPlane) };
	if (!std::isfinite(WidthScale) || !std::isfinite(HeightScale) || std::min({ WidthScale, HeightScale, DepthScale }) <= 1.0e-6f) {
		return false;
	}
	const FMatrix Projection{
		0.0f, 0.0f, DepthScale, 0.0f,
		WidthScale, 0.0f, 0.0f, 0.0f,
		0.0f, HeightScale, 0.0f, 0.0f,
		0.0f, 0.0f, -NearPlane * DepthScale, 1.0f
	};

	FShadowView Result{};
	Result.mView = View;
	Result.mProjection = Projection;
	return FinalizeShadowView(Result, Position, NearPlane, FarPlane, OutView);
}

bool BuildDirectionalCascadeShadowView(const FTransform& LightTransform, const FBox& CasterBounds, const FCascadeSliceBounds& SliceBounds, int32 Resolution, FShadowView& OutView, FVector* OutSphereCenter, float* OutSphereRadius) {
	FVector Forward{};
	FVector Right{};
	FVector Up{};
	if (!BuildShadowBasis(LightTransform, Forward, Right, Up)) {
		return false;
	}

	const FVector CenterNear{ (SliceBounds.mCascadeSliceNear[0] + SliceBounds.mCascadeSliceNear[1] + SliceBounds.mCascadeSliceNear[2] + SliceBounds.mCascadeSliceNear[3]) * 0.25f };
	const FVector CenterFar{ (SliceBounds.mCascadeSliceFar[0] + SliceBounds.mCascadeSliceFar[1] + SliceBounds.mCascadeSliceFar[2] + SliceBounds.mCascadeSliceFar[3]) * 0.25f };
	const float NearFarDistanceSquared{ (CenterFar - CenterNear).SizeSquared() };
	const float RadiusFarSquared{ ((SliceBounds.mCascadeSliceFar[0] - SliceBounds.mCascadeSliceFar[2]) * 0.5f).SizeSquared() };
	const float RadiusNearSquared{ ((SliceBounds.mCascadeSliceNear[0] - SliceBounds.mCascadeSliceNear[2]) * 0.5f).SizeSquared() };
	const float CenterOffsetRatio{ NearFarDistanceSquared > 1.0e-8f ? std::clamp(0.5f + (RadiusFarSquared - RadiusNearSquared) / (2.0f * NearFarDistanceSquared), 0.0f, 1.0f) : 0.5f };
	FVector BoundingSphereCenter{ CenterNear + (CenterFar - CenterNear) * CenterOffsetRatio };
	float BoundingSphereRadiusSquared{};
	for (uint32 Index{}; Index < 4; ++Index) {
		BoundingSphereRadiusSquared = std::max(BoundingSphereRadiusSquared, (SliceBounds.mCascadeSliceFar[Index] - BoundingSphereCenter).SizeSquared());
		BoundingSphereRadiusSquared = std::max(BoundingSphereRadiusSquared, (SliceBounds.mCascadeSliceNear[Index] - BoundingSphereCenter).SizeSquared());
	}
	const float BoundingSphereRadius = std::ceil(std::sqrt(BoundingSphereRadiusSquared));

	//const float BoundingSphereRadius{ std::sqrt(BoundingSphereRadiusSquared) };
	if (!std::isfinite(BoundingSphereRadius) || BoundingSphereRadius <= 1.0e-6f) {
		return false;
	}

	const float TileResolution{ static_cast<float>(NormalizeShadowMapResolution(Resolution)) };
	const float BorderScale{ 1.0f + 2.0f / TileResolution };
	const float Width{ 2.0f * BoundingSphereRadius * BorderScale };
	const float Height{ Width };
	constexpr float DepthPadding{ 0.01f };
	const FVector CasterBoxCenter{ CasterBounds.Min * 0.5f + CasterBounds.Max * 0.5f };
	const FVector CasterBoxExtent{ CasterBounds.Max * 0.5f - CasterBounds.Min * 0.5f };
	const float CasterHalfDepth{ FVector::Dot(CasterBoxExtent, Forward.GetAbs()) };
	const float CasterCenterDepth{ FVector::Dot(CasterBoxCenter, Forward) };
	const float CasterMinZ{ CasterCenterDepth - CasterHalfDepth };
	const float SphereCenterZ{ FVector::Dot(BoundingSphereCenter, Forward) };
	const float BackOffDistance{ std::max(BoundingSphereRadius, SphereCenterZ - CasterMinZ) + DepthPadding };
	const float WorldSizePerTexel{ Width / TileResolution };
	const float CenterX{ FVector::Dot(BoundingSphereCenter, Right) };
	const float CenterY{ FVector::Dot(BoundingSphereCenter, Up) };
	const float SnappedX{ std::round(CenterX / WorldSizePerTexel) * WorldSizePerTexel };
	const float SnappedY{ std::round(CenterY / WorldSizePerTexel) * WorldSizePerTexel };
	BoundingSphereCenter += Right * (SnappedX - CenterX) + Up * (SnappedY - CenterY);
	const FVector Position{ BoundingSphereCenter - Forward * BackOffDistance };
	constexpr float NearPlane{ 0.01f };
	const float FarPlane{ NearPlane + BackOffDistance + BoundingSphereRadius + DepthPadding };
	const FMatrix View{ BuildShadowViewMatrix(Position, Forward, Right, Up) };
	if (!std::isfinite(FarPlane) || !IsFiniteShadowVector(Position) || !IsFiniteShadowMatrix(View)) {
		return false;
	}

	const float WidthScale{ 2.0f / Width };
	const float HeightScale{ 2.0f / Height };
	const float DepthScale{ 1.0f / (FarPlane - NearPlane) };
	if (!std::isfinite(WidthScale) || !std::isfinite(HeightScale) || std::min({ WidthScale, HeightScale, DepthScale }) <= 1.0e-6f) {
		return false;
	}

	const FMatrix Projection{
		0.0f, 0.0f, DepthScale, 0.0f,
		WidthScale, 0.0f, 0.0f, 0.0f,
		0.0f, HeightScale, 0.0f, 0.0f,
		0.0f, 0.0f, -NearPlane * DepthScale, 1.0f
	};
	FShadowView Result{};
	Result.mProjection = Projection;
	Result.mView = View;
	if (!FinalizeShadowView(Result, Position, NearPlane, FarPlane, OutView)) {
		return false;
	}

	if (OutSphereCenter) {
		*OutSphereCenter = BoundingSphereCenter;
	}
	if (OutSphereRadius) {
		*OutSphereRadius = BoundingSphereRadius;
	}
	return true;
}

bool BuildSpotShadowView(const FTransform& LightTransform, float OuterConeAngle, float AttenuationRadius, FShadowView& OutView) {
	const FVector& LightPosition{ LightTransform.Location };
	if (!IsFiniteShadowVector(LightPosition) || !std::isfinite(OuterConeAngle) || OuterConeAngle <= 0.0f || !std::isfinite(AttenuationRadius) || AttenuationRadius <= 0.0f) {
		return false;
	}

	FVector Forward{};
	FVector Right{};
	FVector Up{};
	if (!BuildShadowBasis(LightTransform, Forward, Right, Up)) {
		return false;
	}

	const float SafeOuterAngle{ std::clamp(OuterConeAngle, 0.1f, 89.0f) };
	const float NearPlane{ std::min(0.01f, AttenuationRadius * 0.01f) };
	const float ProjectionScale{ 1.0f / std::tan(FMath::DegreesToRadians(SafeOuterAngle)) };
	if (NearPlane <= 0.0f) {
		return false;
	}

	FShadowView Result{};
	Result.mView = BuildShadowViewMatrix(LightPosition, Forward, Right, Up);
	Result.mProjection = BuildPerspectiveShadowProjection(ProjectionScale, ProjectionScale, NearPlane, AttenuationRadius);
	Result.mIsOrthographic = false;
	return FinalizeShadowView(Result, LightPosition, NearPlane, AttenuationRadius, OutView);
}

bool BuildRectShadowView(const FTransform& LightTransform, float HalfWidth, float HalfHeight, float AttenuationRadius, FShadowView& OutView) {
	const FVector& LightPosition{ LightTransform.Location };
	if (!IsFiniteShadowVector(LightPosition) || !std::isfinite(HalfWidth) || HalfWidth <= 0.0f || !std::isfinite(HalfHeight) || HalfHeight <= 0.0f || !std::isfinite(AttenuationRadius) || AttenuationRadius <= 0.0f) {
		return false;
	}

	FVector Forward{};
	FVector Right{};
	FVector Up{};
	if (!BuildShadowBasis(LightTransform, Forward, Right, Up)) {
		return false;
	}

	const float NearPlane{ std::max({ HalfWidth, HalfHeight, 0.01f }) };
	const float FarPlane{ NearPlane + AttenuationRadius };
	const FVector Position{ LightPosition - Forward * NearPlane };
	if (!std::isfinite(FarPlane) || FarPlane <= NearPlane || !IsFiniteShadowVector(Position)) {
		return false;
	}

	const float WidthScale{ NearPlane / HalfWidth };
	const float HeightScale{ NearPlane / HalfHeight };
	FShadowView Result{};
	Result.mView = BuildShadowViewMatrix(Position, Forward, Right, Up);
	Result.mProjection = BuildPerspectiveShadowProjection(WidthScale, HeightScale, NearPlane, FarPlane);
	if (WidthScale <= 0.0f || HeightScale <= 0.0f || !IsFiniteShadowMatrix(Result.mView) || !IsFiniteShadowMatrix(Result.mProjection)) {
		return false;
	}
	Result.mIsOrthographic = false;
	Result.mCullBackFaces = false;
	return FinalizeShadowView(Result, Position, NearPlane, FarPlane, OutView);
}

bool BuildPointShadowViews(const FTransform& LightTransform, float AttenuationRadius, FShadowView (&OutViews)[PointShadowFaceCount]) {
	const FVector& LightPosition{ LightTransform.Location };
	if (!IsFiniteShadowVector(LightPosition) || !std::isfinite(AttenuationRadius) || AttenuationRadius <= 0.0f) {
		return false;
	}

	const float NearPlane{ std::min(0.01f, AttenuationRadius * 0.01f) };
	if (NearPlane <= 0.0f) {
		return false;
	}
	const float ProjectionScale{ 1.0f / std::tan(FMath::DegreesToRadians(45.0f)) };
	const FMatrix Projection{ BuildPerspectiveShadowProjection(ProjectionScale, ProjectionScale, NearPlane, AttenuationRadius) };
	const FVector Directions[PointShadowFaceCount]{ { 1.0f, 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } };
	FShadowView Views[PointShadowFaceCount]{};
	for (uint32 FaceIndex{}; FaceIndex < PointShadowFaceCount; ++FaceIndex) {
		const FVector& Forward{ Directions[FaceIndex] };
		const FVector ReferenceUp{ std::abs(Forward.Z) < 0.99f ? FVector{ 0.0f, 0.0f, 1.0f } : FVector{ 0.0f, 1.0f, 0.0f } };
		const FVector Right{ FVector::Cross(ReferenceUp, Forward).GetSafeNormal() };
		const FVector Up{ FVector::Cross(Forward, Right) };
		FShadowView Result{};
		Result.mView = BuildShadowViewMatrix(LightPosition, Forward, Right, Up);
		Result.mProjection = Projection;
		Result.mIsOrthographic = false;
		Result.mIsOmnidirectional = true;
		if (!FinalizeShadowView(Result, LightPosition, NearPlane, AttenuationRadius, Views[FaceIndex])) {
			return false;
		}
	}
	std::copy(std::begin(Views), std::end(Views), std::begin(OutViews));
	return true;
}
