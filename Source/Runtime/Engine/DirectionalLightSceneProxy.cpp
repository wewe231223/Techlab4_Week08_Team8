#include "EnginePCH.h"
#include "DirectionalLightSceneProxy.h"

#include "Components/DirectionalLightComponent.h"
#include "Core/StatOverlay.h"
#include "Math/Transform.h"

#include <algorithm>
#include <cmath>

void FDirectionalLightSceneProxy::GetLightShaderParameters(FLightRenderParameters& Out) const {
	const UDirectionalLightComponent* Light{ static_cast<const UDirectionalLightComponent*>(LightComponent) };
	FTransform Transform{};
	Transform.Rotation = Light->GetWorldRotation();
	const FLinearColor Color{ Light->GetLightColor() };
	Out = FLightRenderParameters{};
	Out.Type = static_cast<uint32>(ELightType::Directional);
	Out.Direction = Transform.GetForward();
	Out.Color = FVector{ Color.R, Color.G, Color.B };
	Out.Intensity = Light->GetIntensity();
}

bool FDirectionalLightSceneProxy::BuildShadowView(const FBox& CasterBounds, FShadowView& OutView) const {
	const FLightShadowSettings Settings{ GetShadowSettings() };
	if (!IsVisible() || !Settings.mCastShadows) {
		return false;
	}

	const FTransform LightTransform{ LightComponent->GetWorldRotation(), LightComponent->GetWorldLocation() };
	return BuildDirectionalShadowView(LightTransform, CasterBounds, Settings.mResolution, OutView);
}

bool FDirectionalLightSceneProxy::BuildShadowViews(const FBox& CasterBounds, FShadowView (&OutViews)[PointShadowFaceCount], const FShadowCameraContext& CameraContext) const {
	const FLightShadowSettings Settings{ GetShadowSettings() };
	if (!IsVisible() || !Settings.mCastShadows || !std::isfinite(CameraContext.mNearZ) || !std::isfinite(CameraContext.mFarZ) || CameraContext.mFarZ <= CameraContext.mNearZ) {
		return false;
	}

	const FTransform LightTransform{ LightComponent->GetWorldRotation(), LightComponent->GetWorldLocation() };
	const uint32 CascadeCount{ GetCascadeCount() };
	const FMatrix InverseViewProjection{ CameraContext.mViewProjection.Inverse() };
	const FVector2 NdcPositions[4]{ { -1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f } };
	FVector FrustumNearPoints[4]{};
	FVector FrustumFarPoints[4]{};
	for (uint32 Index{}; Index < 4; ++Index) {
		const FVector4 FarPosition{ FVector4{ NdcPositions[Index].X, NdcPositions[Index].Y, 1.0f, 1.0f } * InverseViewProjection };
		const FVector4 NearPosition{ FVector4{ NdcPositions[Index].X, NdcPositions[Index].Y, 0.0f, 1.0f } * InverseViewProjection };
		if (!std::isfinite(FarPosition.W) || !std::isfinite(NearPosition.W) || std::abs(FarPosition.W) <= 1.0e-8f || std::abs(NearPosition.W) <= 1.0e-8f) {
			return false;
		}
		FrustumFarPoints[Index] = FVector{ FarPosition.X / FarPosition.W, FarPosition.Y / FarPosition.W, FarPosition.Z / FarPosition.W };
		FrustumNearPoints[Index] = FVector{ NearPosition.X / NearPosition.W, NearPosition.Y / NearPosition.W, NearPosition.Z / NearPosition.W };
	}

	const float ShadowNear{ std::max(CameraContext.mNearZ, 0.01f) };
	const float ShadowFar{ std::min(GetMaxShadowDistance(), CameraContext.mFarZ) };
	if (ShadowFar <= ShadowNear) {
		return false;
	}

	mCascadeSplits = FVector4{};
	float SplitRatios[5]{};
	for (uint32 Index{}; Index <= CascadeCount; ++Index) {
		const float Fraction{ static_cast<float>(Index) / static_cast<float>(CascadeCount) };
		const float LinearSplit{ ShadowNear + (ShadowFar - ShadowNear) * Fraction };
		const float LogSplit{ ShadowNear * std::pow(ShadowFar / ShadowNear, Fraction) };
		const float SplitDepth{ GetSplitLambda() * LogSplit + (1.0f - GetSplitLambda()) * LinearSplit };
		SplitRatios[Index] = (SplitDepth - CameraContext.mNearZ) / (CameraContext.mFarZ - CameraContext.mNearZ);
		if (Index > 0) {
			mCascadeSplits[Index - 1] = SplitDepth;
		}
	}

	FCSMDebugSnapshot Snapshot{};
	Snapshot.mEnabled = true;
	Snapshot.mCascadeCount = CascadeCount;
	for (uint32 Index{}; Index < CascadeCount; ++Index) {
		FCascadeSliceBounds SliceBounds{};
		for (uint32 Corner{}; Corner < 4; ++Corner) {
			const FVector NearToFar{ FrustumFarPoints[Corner] - FrustumNearPoints[Corner] };
			SliceBounds.mCascadeSliceNear[Corner] = FrustumNearPoints[Corner] + NearToFar * SplitRatios[Index];
			SliceBounds.mCascadeSliceFar[Corner] = FrustumNearPoints[Corner] + NearToFar * SplitRatios[Index + 1];
		}

		FVector SphereCenter{};
		float SphereRadius{};
		if (!BuildDirectionalCascadeShadowView(LightTransform, CasterBounds, SliceBounds, Settings.mResolution, OutViews[Index], &SphereCenter, &SphereRadius)) {
			FStatOverlay::UpdateCSMStats(FCSMDebugSnapshot{});
			return false;
		}
		Snapshot.mCascades[Index] = FCascadeDebugInfo{ Index == 0 ? ShadowNear : mCascadeSplits[Index - 1], mCascadeSplits[Index], SphereCenter, SphereRadius };
	}

	FStatOverlay::UpdateCSMStats(Snapshot);
	return true;
}

bool FDirectionalLightSceneProxy::IsCSMEnabled() const {
	const UDirectionalLightComponent* Light{ static_cast<const UDirectionalLightComponent*>(LightComponent) };
	return Light && Light->IsCSMEnabled();
}

uint32 FDirectionalLightSceneProxy::GetCascadeCount() const {
	const UDirectionalLightComponent* Light{ static_cast<const UDirectionalLightComponent*>(LightComponent) };
	return Light ? Light->GetCascadeCount() : 2;
}

float FDirectionalLightSceneProxy::GetMaxShadowDistance() const {
	const UDirectionalLightComponent* Light{ static_cast<const UDirectionalLightComponent*>(LightComponent) };
	return Light ? Light->GetMaxShadowDistance() : 50.0f;
}

float FDirectionalLightSceneProxy::GetSplitLambda() const {
	const UDirectionalLightComponent* Light{ static_cast<const UDirectionalLightComponent*>(LightComponent) };
	return Light ? Light->GetSplitLambda() : 0.85f;
}

FVector4 FDirectionalLightSceneProxy::GetCascadeSplits() const {
	return mCascadeSplits;
}

float FDirectionalLightSceneProxy::GetCascadeTransitionFraction() const {
	const UDirectionalLightComponent* Light{ static_cast<const UDirectionalLightComponent*>(LightComponent) };
	return Light ? Light->GetCascadeTransitionFraction() : 0.0f;
}
