#include "EnginePCH.h"
#include "Scene.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Render/RenderCommand.h"

void FScene::Init()
{
	ShadowAtlas.Init();
}

void FScene::AddPrimitive(UPrimitiveComponent* Component)
{
	if (!Component || Component->SceneProxy) return;

	FPrimitiveSceneProxy* Proxy = new FPrimitiveSceneProxy(Component);
	Proxy->Scene = this;
	Proxy->PackedIndex = PrimitiveSceneProxies.Num();
	Component->SceneProxy = Proxy;
	MarkDirty(Proxy);
	MarkRenderStateDirty(Proxy);

	PrimitiveSceneProxies.Add(Proxy);
	PrimitiveBounds.Add(FAABB{});
	PrimitiveFlags.Add(0);

	bElementListChanged = true;
}

void FScene::RemovePrimitive(UPrimitiveComponent* Component)
{
	if (!Component || !Component->SceneProxy) return;

	FPrimitiveSceneProxy* Proxy = Component->SceneProxy;
	if (!Proxy) return;

	const uint32 Index = static_cast<uint32>(Proxy->PackedIndex);

	PrimitiveSceneProxies.RemoveAtSwap(Index);
	PrimitiveBounds.RemoveAtSwap(Index);
	PrimitiveFlags.RemoveAtSwap(Index);

	if (Index < static_cast<uint32>(PrimitiveSceneProxies.Num()))
		PrimitiveSceneProxies[Index]->PackedIndex = static_cast<int32>(Index);

	if (Proxy->bQueuedForUpdate)
	{
		const int32 Found = DirtyProxies.Find(Proxy);   // 프로젝트 TArray의 Find 이름에 맞게
		if (Found != INDEX_NONE) DirtyProxies.RemoveAtSwap(Found);
	}

	if (Proxy->bRenderStateQueued)
	{
		const int32 Found = RenderStateDirtyProxies.Find(Proxy);
		if (Found != INDEX_NONE) RenderStateDirtyProxies.RemoveAtSwap(Found);
	}

	Component->SceneProxy = nullptr;
	delete Proxy;

	bElementListChanged = true;
}

void FScene::RemoveAllPrimitives()
{
	// 하나씩 RemovePrimitive하면 대기열 검색(Find)이 물체 수만큼 반복되므로 통째로 비운다.
	for (FPrimitiveSceneProxy* Proxy : PrimitiveSceneProxies)
	{
		if (UPrimitiveComponent* Component = Proxy->GetComponent())
			Component->SceneProxy = nullptr;
		delete Proxy;
	}
	PrimitiveSceneProxies.Reset();
	PrimitiveBounds.Reset();
	PrimitiveFlags.Reset();
	DirtyProxies.Reset();
	RenderStateDirtyProxies.Reset();
	BVH.Clear();
	bElementListChanged = true;
}

void FScene::AddLight(ULightComponent* Component)
{
	if (!Component || Component->SceneProxy) return;

	FLightSceneProxy* Proxy = Component->CreateSceneProxy();
	if (!Proxy)
	{
		if (USkyLightComponent* Sky = Cast<USkyLightComponent>(Component))
			if (SkyLights.Find(Sky) == INDEX_NONE) SkyLights.Add(Sky);
		return;
	}

	Proxy->Scene = this;
	Component->SceneProxy = Proxy;

	if (Component->GetLightType() == ELightType::Directional)
	{
		Proxy->PackedIndex = DirectionalLightProxies.Num();
		DirectionalLightProxies.Add(static_cast<FDirectionalLightSceneProxy*>(Proxy));
	}
	else
	{
		Proxy->PackedIndex = LocalLightProxies.Num();
		LocalLightProxies.Add(static_cast<FLocalLightSceneProxy*>(Proxy));
		LocalLightParameters.Add(FLightRenderParameters{});
	}
}

void FScene::RemoveLight(ULightComponent* Component)
{
	if (!Component) return;

	FLightSceneProxy* Proxy = Component->SceneProxy;
	if (!Proxy)
	{
		if (USkyLightComponent* Sky = Cast<USkyLightComponent>(Component))
		{
			const int32 Found = SkyLights.Find(Sky);
			if (Found != INDEX_NONE) SkyLights.RemoveAtSwap(Found);
		}
		return;
	}

	FreeShadowTiles(Proxy);   // 돌려주지 않으면 라이트를 지울 때마다 아틀라스 공간이 샌다

	const int32 Index = Proxy->PackedIndex;
	if (Component->GetLightType() == ELightType::Directional)
	{
		DirectionalLightProxies.RemoveAtSwap(Index);
		if (Index < DirectionalLightProxies.Num())
			DirectionalLightProxies[Index]->PackedIndex = Index;
	}
	else
	{
		LocalLightProxies.RemoveAtSwap(Index);
		LocalLightParameters.RemoveAtSwap(Index);      // 두 배열을 같이 움직여야 순서가 맞음
		if (Index < LocalLightProxies.Num())
			LocalLightProxies[Index]->PackedIndex = Index;
	}

	Component->SceneProxy = nullptr;
	delete Proxy;
}

void FScene::RemoveAllLights()
{
	for (FLightSceneProxy* Proxy : DirectionalLightProxies) { FreeShadowTiles(Proxy); delete Proxy; }
	for (FLightSceneProxy* Proxy : LocalLightProxies)       { FreeShadowTiles(Proxy); delete Proxy; }
	DirectionalLightProxies.Reset();
	LocalLightProxies.Reset();
	LocalLightParameters.Reset();
	SkyLights.Reset();
}

// 등록된 라이트를 셰이더용 상수로 모아 Renderer에 올린다. 컴포넌트 값을 매 프레임 직접 읽으므로 별도 갱신 경로가 필요 없다.
void FScene::UpdateLights(FRenderer* Renderer)
{
	if (!Renderer) return;

	// 아래 bHasShadow·ShadowIndex와 RenderShadowDepth가 같은 할당 결과를 보도록 먼저 타일을 맞춘다.
	UpdateShadowTiles();

	FLightingConstants Constants{};
	Constants.Ambient.Color = FVector(1, 1, 1);
	Constants.Ambient.Intensity = 0.0f;

	for (USkyLightComponent* Sky : SkyLights)            // 기존 Sky 코드 그대로
	{
		if (!Sky->IsVisible()) continue;
		const FLinearColor Color(Sky->GetLightColor());
		Constants.Ambient.Color = FVector(Color.R, Color.G, Color.B);
		Constants.Ambient.Intensity = Sky->GetIntensity();
		break;
	}

	for (FDirectionalLightSceneProxy* Proxy : DirectionalLightProxies)
	{
		if (!Proxy->IsVisible()) continue;
		FLightRenderParameters P;
		Proxy->GetLightShaderParameters(P);
		Constants.Directional.Direction = P.Direction;
		Constants.Directional.Color = P.Color;
		Constants.Directional.Intensity = P.Intensity;
		Constants.Directional.bHasShadow = ShouldRenderShadow(Proxy) ? 1u : 0u;
		break;                                            // 셰이더는 하나만 받음
	}

	// 그림자를 그리는 Local 라이트에만 앞에서부터 칸 번호를 매긴다. RenderShadowDepth가 같은 순서로 돈다.
	int32 NextShadowIndex = 0;
	for (FLocalLightSceneProxy* Proxy : LocalLightProxies)
	{
		FLightRenderParameters& P = LocalLightParameters[Proxy->PackedIndex];
		Proxy->GetLightShaderParameters(P);
		if (!Proxy->IsVisible())
			P.Radius = 0.0f;                              // 칸은 유지하고 영향만 없앤다
		P.ShadowIndex = ShouldRenderShadow(Proxy) ? NextShadowIndex++ : -1;
	}

	Renderer->SetLighting(Constants, LocalLightParameters);
}

void FScene::RenderShadowDepth(FRenderer* Renderer)
{
	if (!Renderer) return;

	FTexture2D* Atlas = ShadowAtlas.GetTexture();
	if (!Atlas) return;
	Renderer->SetShadowAtlas(Atlas);

	// 타일마다 지우면 앞서 그린 다른 라이트 타일까지 지워지므로, 그릴 타일이 있을 때 프레임에 한 번만 전체를 지운다.
	// 지난 프레임 메인 패스가 SRV로 꽂아 둔 슬롯은 DSV로 쓰기 전에 푼다.
	bool bAtlasCleared = false;
	auto RenderLight = [&](FLightSceneProxy* Proxy, bool bDirectional)
	{
		if (!ShouldRenderShadow(Proxy)) return;
		if (!bAtlasCleared)
		{
			RenderCommand::UnbindShaderResource(ShadowAtlasSRVSlot, EShaderBindFlagBits::Pixel);
			ShadowAtlas.ClearDepth();
			bAtlasCleared = true;
		}
		RenderLightShadowDepth(Renderer, Proxy, Atlas, bDirectional);
	};

	// 셰이더가 Directional을 하나만 받으므로 UpdateLights와 같은 규칙으로 첫 번째 보이는 것만 그린다.
	for (FDirectionalLightSceneProxy* Proxy : DirectionalLightProxies)
	{
		if (!Proxy->IsVisible()) continue;
		RenderLight(Proxy, true);
		break;
	}

	for (FLocalLightSceneProxy* Proxy : LocalLightProxies)
		RenderLight(Proxy, false);
}

bool FScene::CanCastShadow(const FLightSceneProxy* Proxy) const
{
	if (!Proxy->IsVisible()) return false;
	if (!Proxy->GetLightComponent()->IsCastShadows()) return false;
	if (Proxy->GetNumShadowMaps() <= 0) return false;   // 그림자를 지원하지 않는 라이트

	// 씬에 물체가 없으면 caster도 receiver도 없다. 바운딩 구 반지름이 0이면 투영 행렬도 만들 수 없다.
	if (BVH.IsEmpty()) return false;
	const FBox& Bounds = BVH.GetRootBounds();
	return (Bounds.Max - Bounds.Min).Length() > KINDA_SMALL_NUMBER;
}

bool FScene::ShouldRenderShadow(const FLightSceneProxy* Proxy) const
{
	// 아틀라스가 꽉 차 타일을 다 못 받은 라이트는 그림자를 끈다 (빈 타일을 샘플링하지 않게).
	return CanCastShadow(Proxy) && Proxy->HasAllShadowTiles();
}

void FScene::FreeShadowTiles(FLightSceneProxy* Proxy)
{
	for (FShadowAtlasTile& Tile : Proxy->ShadowTiles)
		ShadowAtlas.DeAllocate(Tile);
	Proxy->ShadowTiles.Reset();
}

void FScene::UpdateShadowTiles()
{
	// 할당은 유지하고(나중에 캐싱할 수 있도록), 그림자를 그리지 않게 됐거나 해상도가 바뀐 라이트만 타일을 돌려받아 다시 준다.
	TArray<FLightSceneProxy*> Pending;
	auto Visit = [&](FLightSceneProxy* Proxy, bool bCandidate)
	{
		// 아틀라스보다 큰 해상도는 아틀라스 크기로 줄인다.
		const uint32 Resolution = std::min(Proxy->GetLightComponent()->GetShadowResolution(), AtlasSize);
		if (bCandidate && Proxy->HasAllShadowTiles() && Proxy->ShadowTiles[0].Size == Resolution)
			return;   // 그대로 쓴다

		FreeShadowTiles(Proxy);
		if (bCandidate)
			Pending.Add(Proxy);
	};

	// Directional은 셰이더가 하나만 받으므로 첫 번째 보이는 것만 후보다. 나머지는 타일을 돌려준다.
	bool bFoundDirectional = false;
	for (FDirectionalLightSceneProxy* Proxy : DirectionalLightProxies)
	{
		const bool bFirstVisible = !bFoundDirectional && Proxy->IsVisible();
		bFoundDirectional |= bFirstVisible;
		Visit(Proxy, bFirstVisible && CanCastShadow(Proxy));
	}
	for (FLocalLightSceneProxy* Proxy : LocalLightProxies)
		Visit(Proxy, CanCastShadow(Proxy));

	// 큰 해상도부터 넣어야 쿼드트리에 빈틈이 덜 생긴다. 모든 면을 못 받으면 받은 것도 돌려준다(부분 그림자 없음).
	std::sort(Pending.begin(), Pending.end(), [](const FLightSceneProxy* A, const FLightSceneProxy* B)
		{ return A->GetLightComponent()->GetShadowResolution() > B->GetLightComponent()->GetShadowResolution(); });

	for (FLightSceneProxy* Proxy : Pending)
	{
		const uint32 Resolution = std::min(Proxy->GetLightComponent()->GetShadowResolution(), AtlasSize);
		for (int32 Index = 0; Index < Proxy->GetNumShadowMaps(); ++Index)
		{
			FShadowAtlasTile Tile;
			if (!ShadowAtlas.Allocate(Resolution, Tile))
			{
				FreeShadowTiles(Proxy);
				break;
			}
			Proxy->ShadowTiles.Add(Tile);
		}
	}
}

void FScene::RenderLightShadowDepth(FRenderer* Renderer, FLightSceneProxy* Proxy, FTexture2D* Atlas, bool bDirectional)
{
	// 건너뛸지·타일 할당은 이미 끝났다(ShouldRenderShadow, UpdateShadowTiles). 여기서는 그리기만 한다.
	const ULightComponent* LightComponent = Proxy->GetLightComponent();

	const FBox& Bounds = BVH.GetRootBounds();
	const FVector Center = (Bounds.Min + Bounds.Max) * 0.5f;
	const float   Radius = (Bounds.Max - Bounds.Min).Length() * 0.5f;

	// Point는 면마다 절두체가 달라 caster도 면마다 다시 고른다.
	for (int32 Index = 0; Index < Proxy->GetNumShadowTiles(); ++Index)
	{
		const FMatrix LightViewProjection = LightComponent->GetShadowViewPorjection(Center, Radius, Index);
		GatherShadowCasters(LightViewProjection);

		const FShadowAtlasTile& Tile = Proxy->GetTile(Index);

		// 셰이더는 아직 Directional 그림자만 읽는다. Local 라이트 값으로 덮어쓰지 않게 Directional일 때만 올린다.
		if (bDirectional)
			Renderer->TEMP_SetDirectionalShadow(LightViewProjection, Tile.GetAtlasScaleOffset());

		// caster가 없어도 그린다 (타일은 프레임 시작에 1.0으로 지워져 있다).
		Renderer->RenderShadowDepth(Atlas, Tile, LightViewProjection, ShadowCasters);
	}
}

void FScene::GatherShadowCasters(const FMatrix& LightViewProjection)
{
	ShadowCasters.Reset();

	// 직교(Directional)든 원근(Spot·Point 면)이든 ViewProjection에서 뽑은 평면 6개로 같은 검사를 한다.
	const FFrustumPlanes Frustum = ExtractFrustumPlanes(LightViewProjection);
	BVH.QueryCull(
		FrustumAllPlanesMask,
		[&Frustum](const FBox& Bounds, uint32& Mask)
		{
			return static_cast<EBVHCullResult>(ClassifyBoxInFrustum(Bounds, Frustum, Mask));
		},
		[this](FPrimitiveSceneProxy* Proxy)
		{
			if (!Proxy->IsVisible() || !Proxy->GetMesh()) return;
			const UPrimitiveComponent* Component = Proxy->GetComponent();
			if (!Component || !Component->IsCastShadow()) return;
			ShadowCasters.Add(Proxy);
		});
}

void FScene::AddFogInfo(uint32 Id, const FFogInfo& FogInfo)
{
	FExponentialHeightFogSceneInfo sceneFogInfo;
	sceneFogInfo.Id = Id;
	sceneFogInfo.FogInfo = FogInfo;


	FogInfos.Add(sceneFogInfo);
}

void FScene::RemoveFogInfo(uint32 Id)
{
	// 렌더러는 FogInfos[0]만 쓰므로, 순서를 유지하려고 RemoveAtSwap 대신 RemoveAt을 쓴다.
	for (int32 i = 0; i < FogInfos.Num(); ++i)
	{
		if (FogInfos[i].Id == Id)
		{
			FogInfos.RemoveAt(i);
			return;
		}
	}
}

void FScene::RemoveAllFogInfo()
{
	FogInfos.Reset();

}

void FScene::UpdateFogInfo(uint32 Id, const FFogInfo& FogInfo)
{
	for (FExponentialHeightFogSceneInfo& SceneFogInfo : FogInfos)
	{
		if (SceneFogInfo.Id == Id)
		{
			SceneFogInfo.FogInfo = FogInfo;
			return;
		}


	}
}

//void FScene::RemoveFogInfo(uint32 Id)
//{
//	for (int32 i = 0; i < FogInfos.Num(); ++i)
//	{
//		if (FogInfos[i].Id == Id)
//		{
//			FogInfos.RemoveAtSwap(i);
//			return;
//		}
//	}
//}


void FScene::UpdateAllTransforms()
{
	for (FPrimitiveSceneProxy* Proxy : RenderStateDirtyProxies)
	{
		Proxy->UpdateRenderState();
		Proxy->bRenderStateQueued = false;
	}
	RenderStateDirtyProxies.Reset();

	for (FPrimitiveSceneProxy* Proxy : DirtyProxies)
	{
		Proxy->UpdateTransform();
		PrimitiveBounds[Proxy->PackedIndex] = Proxy->GetBounds();
		PrimitiveFlags[Proxy->PackedIndex] = Proxy->GetComponent()->IsVisible() ? 1 : 0;
		Proxy->bQueuedForUpdate = false;
	}

	//const int32 Count = Proxies.Num();
	//for (int32 i = 0; i < Count; ++i)
	//{
	//	FPrimitiveSceneProxy* Proxy = Proxies[i];
	//	Proxy->UpdateTransform();

	//	const FAABB& Bounds = Proxy->GetBounds();
	//	if (PrimitiveBounds[i].Center != Bounds.Center || PrimitiveBounds[i].Extent != Bounds.Extent)
	//	{
	//		bBoundsChanged = true;
	//	}

	//	PrimitiveBounds[i] = Proxy->GetBounds();
	//	PrimitiveFlags[i] = Proxy->GetComponent()->IsVisible() ? 1 : 0;
	//}

	const bool bAnyMoved = DirtyProxies.Num() > 0;
	DirtyProxies.Reset();


	if (bElementListChanged)
	{
		BuildBVH();
		bElementListChanged = false;
	}
	else if (bAnyMoved)
	{
		BVH.Refit();
	}
}

void FScene::BuildBVH()
{
	BVH.Clear();
	// BillboardComponents.Reset();

	TArray<FPrimitiveSceneProxy*> Elements;
	Elements.Reserve(PrimitiveSceneProxies.Num());
	for (FPrimitiveSceneProxy* Proxy : PrimitiveSceneProxies)
	{
		if (Proxy && Proxy->GetComponent())
			Elements.Add(Proxy);
	}

	BVH.Build(std::span(Elements.GetData(), Elements.Num()));
}

void FScene::MarkDirty(FPrimitiveSceneProxy* Proxy)
{
	if (Proxy->bQueuedForUpdate) return;            // 중복 추가 방지
	Proxy->bQueuedForUpdate = true;
	DirtyProxies.Add(Proxy);
}

void FScene::MarkRenderStateDirty(FPrimitiveSceneProxy* Proxy)
{
	if (!Proxy || Proxy->bRenderStateQueued) return;
	Proxy->bRenderStateQueued = true;
	RenderStateDirtyProxies.Add(Proxy);
}
