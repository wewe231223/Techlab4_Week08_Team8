#include "EnginePCH.h"
#include "LightSceneProxy.h"

#include "Asset/LOD/StaticMeshLODSelector.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Scene.h"
#include "Render/ShadowUpdatePolicy.h"

#include <algorithm>
#include <cmath>

FLightSceneProxy::FLightSceneProxy(const ULightComponent* InLightComponent)
	: LightComponent{ InLightComponent } {
}

const ULightComponent* FLightSceneProxy::GetLightComponent() const {
	return LightComponent;
}

bool FLightSceneProxy::IsVisible() const {
	return LightComponent && LightComponent->IsVisible();
}

FLightShadowSettings FLightSceneProxy::GetShadowSettings() const {
	return LightComponent ? LightComponent->GetShadowSettings() : FLightShadowSettings{};
}

bool FLightSceneProxy::EvaluateShadowMap(FScene& Scene, const FShadowMapRequest& Request, const FShadowAtlas::FAllocation& Allocation, uint32 FaceIndex, const FShadowAtlas& Atlas, uint64 UpdateSerial, float MinCasterTexelRadius) {
	if (!Atlas.IsCacheEnabled() || FaceIndex >= Allocation.mViewCount) {
		return false;
	}

	mShadowMaps.SetNum(Allocation.mViewCount);
	FShadowMapState& Map{ mShadowMaps[FaceIndex] };
	PrepareShadowMap(Scene, Request, Allocation, FaceIndex, Map, UpdateSerial, MinCasterTexelRadius);

	return CanReuseShadowMap(Map.mInput, Map.mCasters, Map.mRenderedCasters, Atlas.GetCacheEntry(Request.mLightComponent, FaceIndex));
}

void FLightSceneProxy::ResetShadowMaps() {
	mShadowMaps.Empty();
}

const FLightSceneProxy::FShadowMapState* FLightSceneProxy::GetShadowMap(uint32 FaceIndex) const {
	return FaceIndex < static_cast<uint32>(mShadowMaps.Num()) ? &mShadowMaps[FaceIndex] : nullptr;
}

bool FLightSceneProxy::IsShadowCaster(const FPrimitiveSceneProxy* Proxy) {
	if (!Proxy || !Proxy->IsVisible() || !Proxy->CastsShadows() || !Proxy->GetMesh() || !Proxy->GetRenderState()) {
		return false;
	}
	const UStaticMeshComponent* Component{ Cast<UStaticMeshComponent>(Proxy->GetComponent()) };
	if (Component && Component->IsEditorIcon()) {
		return false;
	}
	for (const FCachedMeshSection& Section : Proxy->GetRenderState()->Sections) {
		if (Section.Material && Section.Material->BlendState == EBlendState::Opaque && Section.IndexCount != 0) {
			return true;
		}
	}
	return false;
}

// 섀도우 맵에서의 반지름(텍셀) = r * ProjectionScale / Depth * (Resolution / 2)
// 원근: Depth = 라이트 forward 방향 거리,  직교: Depth = 1
bool FLightSceneProxy::IsShadowCasterTooSmall(const FPrimitiveSceneProxy& Proxy, const FLODViewContext& View, float MinCasterTexelRadius) {
	if (!(MinCasterTexelRadius > 0.0f)) {
		return false;
	}

	const FLODSphere& Sphere{ Proxy.GetLODSphere() };
	const float Depth{ View.bOrthographic ? 1.0f : FVector::Dot(Sphere.Center, View.CameraForward) - View.CameraDepth };
	// 라이트에 너무 가까우면 투영 크기를 신뢰할 수 없으므로 유지한다.
	if (!View.bOrthographic && Depth - View.NearZ <= 0.0f) {
		return false;
	}

	const float HalfResolution{ 0.5f * static_cast<float>(View.Width) };
	// 양변을 제곱해 sqrt 없이 비교한다.
	return Sphere.RadiusSquared * View.ProjectionScaleSquared * HalfResolution * HalfResolution < MinCasterTexelRadius * MinCasterTexelRadius * Depth * Depth;
}

void FLightSceneProxy::GatherShadowCasters(FScene& Scene, FRenderQueue& OutQueue, const FShadowView& View, int32 Resolution, float MinCasterTexelRadius) {
	CollectShadowCasters(Scene, View, Resolution, &OutQueue, nullptr, 0, MinCasterTexelRadius);
}

void FLightSceneProxy::PrepareShadowMap(FScene& Scene, const FShadowMapRequest& Request, const FShadowAtlas::FAllocation& Allocation, uint32 FaceIndex, FShadowMapState& OutMap, uint64 UpdateSerial, float MinCasterTexelRadius) {
	const FShadowAtlas::FTile& Tile{ Allocation.mTiles[FaceIndex] };
	FShadowCacheEntry& Input{ OutMap.mInput };
	Input.mView = Request.mViews[FaceIndex];
	Input.mView.mCullBackFaces = Request.mViews[0].mCullBackFaces;
	Input.mDepthBiasPreset = NormalizeShadowDepthBiasPreset(static_cast<int32>(Request.mSettings.mDepthBiasPreset));
	Input.mSlopeScaledDepthBiasPreset = NormalizeShadowSlopeScaledDepthBiasPreset(static_cast<int32>(Request.mSettings.mSlopeScaledDepthBiasPreset));
	Input.mHasCachedContent = false;

	CollectShadowCasters(Scene, Input.mView, static_cast<int32>(Tile.mResolution), nullptr, &OutMap.mCasters, UpdateSerial, MinCasterTexelRadius);
}

void FLightSceneProxy::CollectShadowCasters(FScene& Scene, const FShadowView& View, int32 Resolution, FRenderQueue* OutQueue, TArray<FShadowCasterReference>* OutCasters, uint64 UpdateSerial, float MinCasterTexelRadius) {
	if (OutQueue) {
		OutQueue->Reset();
	}
	if (OutCasters) {
		OutCasters->Reset();
	}

	const FFrustumPlanes Frustum{ ExtractFrustumPlanes(View.mViewProjection) };
	const float ProjectionScale{ std::max(std::abs(View.mProjection.M[1][0]), std::abs(View.mProjection.M[2][1])) };
	const uint32 Size{ static_cast<uint32>(std::clamp(Resolution, MinShadowMapResolution, MaxShadowMapResolution)) };
	FLODViewContext LODView{};
	LODView.Width = Size;
	LODView.Height = Size;
	LODView.CameraPosition = View.mWorldPosition;
	LODView.CameraForward = FVector{ View.mView.M[0][0], View.mView.M[1][0], View.mView.M[2][0] };
	LODView.ProjectionScaleSquared = ProjectionScale * ProjectionScale;
	LODView.NearZ = View.mNearPlane;
	LODView.bOrthographic = View.mIsOrthographic;
	LODView.ViewProjection = View.mViewProjection;
	LODView.Prepare();

	Scene.BVH.QueryCull(FrustumAllPlanesMask, [&Frustum](const FBox& Bounds, uint32& Mask) {
		return static_cast<EBVHCullResult>(ClassifyBoxInFrustum(Bounds, Frustum, Mask));
	}, [OutQueue, OutCasters, &LODView, &View, UpdateSerial, MinCasterTexelRadius](FPrimitiveSceneProxy* Proxy) {
		if (!Proxy) {
			return;
		}
		if (OutCasters) {
			Proxy->UpdateShadowRevision(UpdateSerial);
		}
		if (!IsShadowCaster(Proxy)) {
			return;
		}

		FLODViewContext CasterLODView{ LODView };
		if (View.mIsOmnidirectional) {
			CasterLODView.CameraForward = (Proxy->GetLODSphere().Center - View.mWorldPosition).GetSafeNormal();
			CasterLODView.Prepare();
		}
		// 그림자 맵에 그려질 텍셀이 너무 작으면 목록에서 제외해 캐시 비교 대상에서도 뺀다.
		if (IsShadowCasterTooSmall(*Proxy, CasterLODView, MinCasterTexelRadius)) {
			return;
		}
		const uint32 LODIndex{ SelectLOD(*Proxy, CasterLODView) };
		if (OutCasters) {
			OutCasters->Add(FShadowCasterReference{ Proxy, Proxy->GetShadowRevision(), LODIndex });
		} else if (OutQueue) {
			AddShadowCasterPackets(*OutQueue, *Proxy, LODIndex);
		}
	});
}

void FLightSceneProxy::AddShadowCasterPackets(FRenderQueue& OutQueue, const FPrimitiveSceneProxy& Proxy, uint32 LODIndex, const FMatrix* Model) {
	const FCachedMeshLOD& LOD{ Proxy.GetLOD(LODIndex) };
	for (uint32 SectionIndex{}; SectionIndex < LOD.NumSections; ++SectionIndex) {
		const FCachedMeshSection& Section{ Proxy.GetSection(LOD.FirstSection + SectionIndex) };
		if (!Section.Material || Section.Material->BlendState != EBlendState::Opaque || Section.IndexCount == 0) {
			continue;
		}

		FRenderPacket& Packet{ OutQueue.AddDefaulted_GetRef() };
		Packet.Proxy = &Proxy;
		Packet.Model = Model;
		Packet.Mesh = Proxy.GetMesh();
		Packet.Material = Section.Material;
		Packet.StartIndex = Section.StartIndex;
		Packet.IndexCount = Section.IndexCount;
		Packet.LODIndex = static_cast<uint8>(LODIndex);
	}
}

bool FLightSceneProxy::BuildShadowRenderQueue(FRenderQueue& OutQueue, uint32 FaceIndex) const {
	OutQueue.Reset();
	const FShadowMapState* Map{ GetShadowMap(FaceIndex) };
	if (!Map) {
		return false;
	}

	for (const FShadowCasterReference& Caster : Map->mCasters) {
		if (!IsShadowCaster(Caster.mProxy) || Caster.mProxy->GetShadowRevision() != Caster.mRevision || Caster.mLODIndex >= Caster.mProxy->GetLODCount()) {
			return false;
		}
		AddShadowCasterPackets(OutQueue, *Caster.mProxy, Caster.mLODIndex, &Caster.mProxy->GetShadowLocalToWorld());
	}
	return true;
}

void FLightSceneProxy::CommitShadowCasters(uint32 FaceIndex) {
	if (FaceIndex < static_cast<uint32>(mShadowMaps.Num())) {
		FShadowMapState& Map{ mShadowMaps[FaceIndex] };
		Map.mRenderedCasters = Map.mCasters;
	}
}

void FLightSceneProxy::RemoveShadowCaster(const FPrimitiveSceneProxy* Caster, FShadowAtlas& Atlas) {
	for (uint32 FaceIndex{}; FaceIndex < static_cast<uint32>(mShadowMaps.Num()); ++FaceIndex) {
		FShadowMapState& Map{ mShadowMaps[FaceIndex] };
		bool Removed{};
		for (TArray<FShadowCasterReference>* Casters : { &Map.mCasters, &Map.mRenderedCasters }) {
			for (int32 Index{ Casters->Num() - 1 }; Index >= 0; --Index) {
				if ((*Casters)[Index].mProxy == Caster) {
					Casters->RemoveAtSwap(static_cast<uint32>(Index));
					Removed = true;
				}
			}
		}

		if (Removed) {
			Atlas.InvalidateCacheEntry(LightComponent, FaceIndex);
		}
	}
}
