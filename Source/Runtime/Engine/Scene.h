#pragma once

#include "PrimitiveSceneProxy.h"
#include "LightSceneProxy.h"
#include "LocalLightSceneProxy.h"
#include "DirectionalLightSceneProxy.h"
#include "Components/BillboardComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Math/Frustum.h"
#include "Math/BVH.h"
#include "Render/FogInfo.h"
#include "Render/Renderer.h"
#include "Render/ShadowAtlas.h"
 
struct FExponentialHeightFogSceneInfo
{
	uint32 Id;
	FFogInfo FogInfo;
};

struct FLightSceneInfo
{
	uint32 Id;
	
};

class FScene
{
public:
	void Init();

	void AddPrimitive(UPrimitiveComponent* Component);
	void RemovePrimitive(UPrimitiveComponent* Component);
	// 모든 프록시를 한 번에 지운다. 액터를 통째로 지우기 전(ClearWorld)에 불러야 지워진 컴포넌트를 가리키는 프록시가 남지 않는다.
	void RemoveAllPrimitives();

	void AddLight(ULightComponent* Component);
	void RemoveLight(ULightComponent* Component);

	void RemoveAllLights();
	void UpdateLights(FRenderer* Renderer);
	// 그림자를 만드는 라이트마다 섀도우맵 면별로 caster를 골라 깊이를 기록한다. UpdateLights 뒤에 부른다.
	void RenderShadowDepth(FRenderer* Renderer);
	// Details 미리보기용. 라이트 타일은 FLightSceneProxy::GetTile로 찾는다.
	FTexture2D* GetShadowAtlasTexture() const { return ShadowAtlas.GetTexture(); }

	void AddFogInfo(uint32 Id, const FFogInfo& FogInfo);
	void RemoveFogInfo(uint32 Id);
	void RemoveAllFogInfo();
	void UpdateFogInfo(uint32 Id, const FFogInfo& FogInfo);
	void UpdateAllTransforms();

	void BuildBVH();

	void MarkDirty(FPrimitiveSceneProxy* Proxy);
	void MarkRenderStateDirty(FPrimitiveSceneProxy* Proxy);

	TArray<FPrimitiveSceneProxy*> PrimitiveSceneProxies;
	TArray<FPrimitiveSceneProxy*> DirtyProxies;
	TArray<FPrimitiveSceneProxy*> RenderStateDirtyProxies;
	TArray<FAABB> PrimitiveBounds;
	TArray<uint8> PrimitiveFlags;
	
	TArray<FExponentialHeightFogSceneInfo> FogInfos;

	// 컬링 결과로 프록시를 바로 내보내 컴포넌트를 역참조하지 않는다. 경계는 Build/Refit 때만 계산한다.
	TBVH<FPrimitiveSceneProxy*> BVH{
		[](const FPrimitiveSceneProxy* Proxy) -> FBox
		{
			const FAABB& B = Proxy->GetBounds();
			return FBox{ B.Center - B.Extent, B.Center + B.Extent };
		}
	};
	bool bElementListChanged = false;

	TArray<FLocalLightSceneProxy*>        LocalLightProxies;      // ↔ PackedIndex ↔
	TArray<FLightRenderParameters>        LocalLightParameters;   // GPU 업로드용, 같은 순서
	TArray<FDirectionalLightSceneProxy*>  DirectionalLightProxies;
	TArray<USkyLightComponent*>           SkyLights;               // 프록시 없음

private:
	// 이 라이트의 섀도우맵을 그릴지. UpdateLights(bHasShadow·ShadowIndex)와 RenderShadowDepth가 같이 써서
	// "플래그는 켜졌는데 섀도우맵은 안 그려진" 상태가 생기지 않게 한다.
	// 타일까지 할당받았을 때만 true.
	bool ShouldRenderShadow(const FLightSceneProxy* Proxy) const;
	// 타일과 무관하게 이 라이트가 그림자를 만들 조건인지 (보임, bCastShadows, 지원 라이트, 씬이 비지 않음).
	bool CanCastShadow(const FLightSceneProxy* Proxy) const;
	// 그림자 후보 라이트에 아틀라스 타일을 맞춰 준다. 조건이 깨졌거나 해상도가 바뀐 라이트만 다시 할당한다.
	void UpdateShadowTiles();
	void FreeShadowTiles(FLightSceneProxy* Proxy);
	void RenderLightShadowDepth(FRenderer* Renderer, FLightSceneProxy* Proxy, FTexture2D* Atlas, bool bDirectional);
	void GatherShadowCasters(const FMatrix& LightViewProjection);

	// 멤버로 두어 라이트·면마다 용량을 재사용한다.
	TArray<FPrimitiveSceneProxy*> ShadowCasters;
	FShadowAtlas ShadowAtlas;
};