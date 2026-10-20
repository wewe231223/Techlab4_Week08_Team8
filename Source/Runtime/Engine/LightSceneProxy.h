#pragma once

#include "Components/LightComponent.h"
#include "Render/RenderPacket.h"
#include "Render/ShadowAtlas.h"
#include "Render/ShadowCasterReference.h"

#include "SceneManagement.h"


class FScene;
class FPrimitiveSceneProxy;
struct FLODViewContext;

class FLightSceneProxy {
public:
	// Face 하나의 현재 입력과 마지막 성공 렌더 기준.
	struct FShadowMapState {
		// 이번 프러스텀 조회에서 수집한 Caster와 Revision, LOD.
		TArray<FShadowCasterReference> mCasters{};
		// 재사용 시 유지하고, 렌더에 성공했을 때만 현재 목록으로 교체한다.
		TArray<FShadowCasterReference> mRenderedCasters{};
		// 이번 평가에 사용할 View와 DepthState.
		FShadowCacheEntry mInput{};
	};

public:
	FLightSceneProxy(const ULightComponent* InLightComponent);
	virtual ~FLightSceneProxy() = default;

public:
	virtual void GetLightShaderParameters(FLightRenderParameters& OutParameters) const = 0;

	const ULightComponent* GetLightComponent() const;
	bool IsVisible() const;
	FLightShadowSettings GetShadowSettings() const;
	bool EvaluateShadowMap(FScene& Scene, const FShadowMapRequest& Request, const FShadowAtlas::FAllocation& Allocation, uint32 FaceIndex, const FShadowAtlas& Atlas, uint64 UpdateSerial, float MinCasterTexelRadius);
	void ResetShadowMaps();
	void RemoveShadowCaster(const FPrimitiveSceneProxy* Caster, FShadowAtlas& Atlas);
	bool BuildShadowRenderQueue(FRenderQueue& OutQueue, uint32 FaceIndex) const;
	void CommitShadowCasters(uint32 FaceIndex);
	const FShadowMapState* GetShadowMap(uint32 FaceIndex = 0) const;
	static bool IsShadowCaster(const FPrimitiveSceneProxy* Proxy);
	// MinCasterTexelRadius 이하로 그려질 Caster는 제외한다. 0 이하이면 크기 컬링을 끈다.
	static void GatherShadowCasters(FScene& Scene, FRenderQueue& OutQueue, const FShadowView& View, int32 Resolution, float MinCasterTexelRadius);


private:
	// 그림자 맵에서 Caster 경계 구의 반지름이 MinCasterTexelRadius 텍셀보다 작은지.
	static bool IsShadowCasterTooSmall(const FPrimitiveSceneProxy& Proxy, const FLODViewContext& View, float MinCasterTexelRadius);
	static void PrepareShadowMap(FScene& Scene, const FShadowMapRequest& Request, const FShadowAtlas::FAllocation& Allocation, uint32 FaceIndex, FShadowMapState& OutMap, uint64 UpdateSerial, float MinCasterTexelRadius);
	static void CollectShadowCasters(FScene& Scene, const FShadowView& View, int32 Resolution, FRenderQueue* OutQueue, TArray<FShadowCasterReference>* OutCasters, uint64 UpdateSerial, float MinCasterTexelRadius);
	static void AddShadowCasterPackets(FRenderQueue& OutQueue, const FPrimitiveSceneProxy& Proxy, uint32 LODIndex, const FMatrix* Model = nullptr);

protected:
	const ULightComponent* LightComponent = nullptr;

private:
	friend class FScene;
	FScene* Scene = nullptr;
    int32 PackedIndex = INDEX_NONE;
	// FaceIndex로 접근하며, Point Light는 여섯 면의 기준을 각각 유지한다.
	TArray<FShadowMapState> mShadowMaps{};
};






