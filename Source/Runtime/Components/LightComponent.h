#pragma once

#include "LightComponentBase.h"
#include "Render/LineBatcher.h"
#include "Render/ShadowMap.h"

class FLightSceneProxy;

class ULightComponent : public ULightComponentBase {
	DECLARE_CLASS(ULightComponent, ULightComponentBase)
	REFLECT_START(ULightComponent)
		PROPERTY(mCastShadows)
		PROPERTY(mShadowMapResolution)
		PROPERTY(mShadowDistance)
		PROPERTY(mShadowDepthBias)
		PROPERTY(mShadowSlopeScaledDepthBias)
		PROPERTY(mShadowNormalBias)
	REFLECT_END()
public:
	ULightComponent();
	virtual ~ULightComponent() override;

public:
	virtual ELightType GetLightType() const = 0;
	virtual void DrawDebug(FLineBatcher* LineBatcher) const;
	virtual FLightSceneProxy* CreateSceneProxy() const;
	virtual void PostEditChangeProperty(const FProperty& Property) override;

	bool CastsShadows() const;
	FLightShadowSettings GetShadowSettings() const;
	void SetCastShadows(bool CastShadows);
	void SetShadowMapResolution(int32 Resolution);
	void SetShadowDistance(float Distance);
	void SetShadowDepthBias(float DepthBias);
	void SetShadowSlopeScaledDepthBias(float SlopeScaledDepthBias);
	void SetShadowNormalBias(float NormalBias);

	FLightSceneProxy* SceneProxy = nullptr;
protected:
	virtual bool ShouldCreateRenderState() const override;
	virtual void CreateRenderState() override;
	virtual void DestroyRenderState() override;
private:
	// 리플렉션에 등록되어 Details 편집과 장면 저장의 대상이 된다. GetShadowSettings로 렌더러용 설정을 수집한다.
	bool mCastShadows{ false }; // 조명의 그림자 생성 요청 여부. 실제 그림자 맵 할당 여부와는 별개이다.
	int32 mShadowMapResolution{ 2048 }; // 그림자 맵 한 변의 요청 해상도(텍셀).
	float mShadowDistance{ 1000.0f }; // 그림자 처리를 요청하는 최대 거리(월드 단위).
	float mShadowDepthBias{ 0.001f }; // 깊이 비교값에 적용할 고정 보정량.
	float mShadowSlopeScaledDepthBias{ 1.0f }; // 광원 시점의 깊이 기울기에 비례하는 보정의 배율.
	float mShadowNormalBias{ 0.01f }; // 표면 법선 방향으로 그림자 판정 위치를 옮기는 보정량.
};
