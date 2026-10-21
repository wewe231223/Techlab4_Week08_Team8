#include "EnginePCH.h"
#include "DirectionalLightSceneProxy.h"

#include "Components/DirectionalLightComponent.h"
#include "Math/Transform.h"
#include "Render/ShadowView.h"
#include "Core/StatOverlay.h"

void FDirectionalLightSceneProxy::GetLightShaderParameters(FLightRenderParameters& Out) const {
	const UDirectionalLightComponent* Dir{ static_cast<const UDirectionalLightComponent*>(LightComponent) };

	FTransform Transform{};
	Transform.Rotation = Dir->GetWorldRotation();
	const FLinearColor Color{ Dir->GetLightColor() };

	Out = FLightRenderParameters{};
	Out.Type = static_cast<uint32>(ELightType::Directional);
	Out.Direction = Transform.GetForward();
	Out.Color = FVector{ Color.R, Color.G, Color.B };
	Out.Intensity = Dir->GetIntensity();

}

bool FDirectionalLightSceneProxy::BuildShadowView(const FBox& CasterBounds, FShadowView& OutView) const {
	const FLightShadowSettings Settings{ GetShadowSettings() };
	if (!IsVisible() || !Settings.mCastShadows) {
		return false;
	}

	FCSMDebugSnapshot Snapshot{};
	Snapshot.bEnabled = false;
	Snapshot.CascadeCount = 0;
	FStatOverlay::UpdateCSMStats(Snapshot);

	const FTransform LightTransform{ LightComponent->GetWorldRotation(), LightComponent->GetWorldLocation() };
	return BuildDirectionalShadowView(LightTransform, CasterBounds, Settings.mResolution, OutView);
}

// TODO:: BuildShadowVIew와 합쳐서 CSM을 사용하지 않을 떄와 사용할 때 같은 함수에서 호출
bool FDirectionalLightSceneProxy::BuildShadowViews(const FBox& CasterBounds, FShadowView(&OutViews)[PointShadowFaceCount], const FShadowCameraContext& CamContext) const
{
	const FLightShadowSettings Settings{ GetShadowSettings() };
	if (!IsVisible() || !Settings.mCastShadows) {
		return false;
	}
	const FTransform LightTransform{ LightComponent->GetWorldRotation(), LightComponent->GetWorldLocation() };

	int32 CascadeCount = GetCascadeCount();
	float MaxShadowDistance = GetMaxShadowDistance();
	float SplitLambda = GetSplitLambda();

	// 월드 상에서 카메라 Frustum의 Near평면, Far 평면 
	TArray<FVector> CamFrustumNearPoints;
	TArray<FVector> CamFrustumFarPoints;
	FMatrix InvViewProj = CamContext.ViewProj.Inverse();
	FVector2 NDCPos[4] = { { -1.0f, -1.0f },{ 1.0f, -1.0f },{ 1.0f, 1.0f },{ -1.0f, 1.0f } };
	for (int i = 0; i < 4; i++)
	{

		FVector4 WorldFarPos4 = FVector4(NDCPos[i].X, NDCPos[i].Y, 1.0f, 1.0f) * InvViewProj;
		FVector4 WorldNearPos4 = FVector4(NDCPos[i].X, NDCPos[i].Y, 0.0f, 1.0f) * InvViewProj;
		
		FVector WorldFarPos = FVector{
			WorldFarPos4.X / WorldFarPos4.W,
			WorldFarPos4.Y / WorldFarPos4.W,
			WorldFarPos4.Z / WorldFarPos4.W
			};
		CamFrustumFarPoints.Add(WorldFarPos);

		FVector WorldNearPos = FVector{
			WorldNearPos4.X / WorldNearPos4.W,
			WorldNearPos4.Y / WorldNearPos4.W,
			WorldNearPos4.Z / WorldNearPos4.W
		};
		CamFrustumNearPoints.Add(WorldNearPos);
	}

	// CascadeCount에 따른 메인 카메라 분할 비율 구한다
	float ShadowFar = std::min(MaxShadowDistance, CamContext.FarZ);
	float ShadowNear = std::max(CamContext.NearZ, 0.01f);
	if (ShadowFar <= ShadowNear)
	{
		ShadowFar = ShadowNear + 1.0f;
	}

	TArray<float> SplitZRatio;
	for (int i = 0; i <= CascadeCount; i++)
	{
		float Fraction = static_cast<float>(i) / static_cast<float>(CascadeCount);
		float LinearSplit = ShadowNear + (ShadowFar - ShadowNear) * Fraction;
		float LogSplit = ShadowNear * pow((ShadowFar / ShadowNear), Fraction);
		float LerpedSplit = SplitLambda * LogSplit + (1.0f - SplitLambda) * LinearSplit;
		if (i > 0)
		{
			CascadeSplits[i-1] = LerpedSplit;
		}
		float Denom = CamContext.FarZ - CamContext.NearZ;
		float SplitRatio = (std::abs(Denom) > 0.001f) ? (LerpedSplit - CamContext.NearZ) / Denom : Fraction;
		SplitZRatio.Add(SplitRatio);
	}	

	FCSMDebugSnapshot Snapshot = FStatOverlay::GetCSMStats();
	Snapshot.bEnabled = IsCSMEnabled();
	Snapshot.CascadeCount = static_cast<uint32>(CascadeCount);

	// Frustum 분할 비율에 따라 Orthogornal 직육면체 Frustum 생성
	for (int i = 0; i < CascadeCount; i++)
	{
		float NearT = SplitZRatio[i];
		float FarT = SplitZRatio[i + 1];
		
		FCascadeSliceBounds SliceBounds;
		for (int j = 0; j < 4; j++)
		{
			FVector NearToFar = FVector(CamFrustumFarPoints[j] - CamFrustumNearPoints[j]);
			SliceBounds.CascadeSliceNear[j] = CamFrustumNearPoints[j] + NearToFar * NearT;
			SliceBounds.CascadeSliceFar[j] = CamFrustumNearPoints[j] + NearToFar * FarT;
		}

		FVector SphereCenter{};
		float SphereRadius = 0.0f;

		if (!BuildDirectionalCascadeShadowView(LightTransform, CasterBounds, SliceBounds, Settings.mResolution, OutViews[i], &SphereCenter, &SphereRadius))
		{
			FStatOverlay::UpdateCSMStats(Snapshot);
			return false;
		}

		if (i < 4)
		{
			Snapshot.Cascades[i].Near = (i == 0) ? CamContext.NearZ : CascadeSplits[i - 1];
			Snapshot.Cascades[i].Far = CascadeSplits[i];
			Snapshot.Cascades[i].SphereCenter = SphereCenter;
			Snapshot.Cascades[i].SphereRadius = SphereRadius;
		}
	}

	FStatOverlay::UpdateCSMStats(Snapshot);

	return true;
}