#pragma once

#include "Components/LightComponent.h"
#include "Render/RenderPacket.h"
#include "Render/ShadowAtlas.h"
#include "Render/ShadowUpdatePolicy.h"

#include "SceneManagement.h"


class FScene;
class FPrimitiveSceneProxy;

class FLightSceneProxy
{
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
	bool EvaluateShadowMap(FScene& Scene, const FShadowMapRequest& Request, const FShadowAtlas::FAllocation& Allocation, uint32 FaceIndex, const FShadowAtlas& Atlas, uint64 UpdateSerial);
	void ResetShadowMaps();
	void RemoveShadowCaster(const FPrimitiveSceneProxy* Caster, FShadowAtlas& Atlas);
	bool BuildShadowRenderQueue(FRenderQueue& OutQueue, uint32 FaceIndex) const;
	void CommitShadowCasters(uint32 FaceIndex);
	const FShadowMapState* GetShadowMap(uint32 FaceIndex = 0) const;
	void AddShadowCondition(TUniquePtr<IShadowInvalidationCondition> Condition);
	static bool IsShadowCaster(const FPrimitiveSceneProxy* Proxy);
	static void GatherShadowCasters(FScene& Scene, FRenderQueue& OutQueue, const FShadowView& View, int32 Resolution);


private:
	static void PrepareShadowMap(FScene& Scene, const FShadowMapRequest& Request, const FShadowAtlas::FAllocation& Allocation, uint32 FaceIndex, FShadowMapState& OutMap, uint64 UpdateSerial);
	static void GatherShadowCasterReferences(FScene& Scene, TArray<FShadowCasterReference>& OutCasters, const FShadowView& View, int32 Resolution, uint64 UpdateSerial);
	static void AddShadowCasterPackets(FRenderQueue& OutQueue, const FPrimitiveSceneProxy& Proxy, uint32 LODIndex, const FMatrix* Model = nullptr);

protected:
	const ULightComponent* LightComponent = nullptr;

private:
	friend class FScene;
	FScene* Scene = nullptr;
    int32 PackedIndex = INDEX_NONE;
	// 이 광원의 모든 그림자 맵에 적용할 재사용 조건.
	TUniquePtr<FShadowUpdatePolicy> mShadowUpdatePolicy{};
	// FaceIndex로 접근하며, Point Light는 여섯 면의 기준을 각각 유지한다.
	TArray<FShadowMapState> mShadowMaps{};
};






