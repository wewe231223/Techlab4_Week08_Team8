#include "EnginePCH.h"
#include "DirectionalLightSceneProxy.h"

#include "Components/DirectionalLightComponent.h"
#include "Math/Transform.h"
#include "Render/ShadowView.h"

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
	float ShadowNear = CamContext.NearZ;

	TArray<float> SplitZRatio;
	for (int i = 0; i <= CascadeCount; i++)
	{
		float LinearSplit = CamContext.NearZ + (ShadowFar - CamContext.NearZ) * ((float)i / CascadeCount);
		float LogSplit = CamContext.NearZ * pow((ShadowFar / CamContext.NearZ), (float)i / CascadeCount);
		float LerpedSplit = SplitLambda * LogSplit + (1 - SplitLambda) * LinearSplit;
		float SplitRatio = (LerpedSplit - CamContext.NearZ) / (CamContext.FarZ - CamContext.NearZ);
		SplitZRatio.Add(SplitRatio);
	}

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
		if (!BuildDirectionalCascadeShadowView(LightTransform, CasterBounds, SliceBounds, Settings.mResolution, OutViews[i]))
		{
			return false;
		}
	}

	return true;
}