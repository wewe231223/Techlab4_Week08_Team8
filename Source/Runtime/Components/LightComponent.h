#pragma once

#include "LightComponentBase.h"
#include "Render/LineBatcher.h"
#include "Render/ShadowMap.h"

class ULightComponent : public ULightComponentBase {
	DECLARE_CLASS(ULightComponent, ULightComponentBase)
	REFLECT_START(ULightComponent)
		PROPERTY(mCastShadows)
		PROPERTY(mShadowMapResolution)
		PROPERTY(mShadowDepthBiasPreset)
		PROPERTY(mShadowSlopeScaledDepthBiasPreset)
		PROPERTY(mShadowNormalBias)
		InClass->AddProperty<decltype(ThisClass::mMinCasterDepth)>("MinCasterDepth", offsetof(ThisClass, mMinCasterDepth));
	REFLECT_END()
public:
	ULightComponent();
	virtual ~ULightComponent() override;

public:
	virtual ELightType GetLightType() const = 0;
	virtual void DrawDebug(FLineBatcher* LineBatcher) const;
	virtual void PostEditChangeProperty(const FProperty& Property) override;

	bool CastsShadows() const;
	virtual FLightShadowSettings GetShadowSettings() const;
	float GetMinCasterDepth() const;
	EShadowDepthBiasPreset GetShadowDepthBiasPreset() const;
	EShadowSlopeScaledDepthBiasPreset GetShadowSlopeScaledDepthBiasPreset() const;
	void SetCastShadows(bool CastShadows);
	void SetShadowMapResolution(int32 Resolution);
	void SetShadowDepthBiasPreset(EShadowDepthBiasPreset Preset);
	void SetShadowSlopeScaledDepthBiasPreset(EShadowSlopeScaledDepthBiasPreset Preset);
	void SetShadowNormalBias(float NormalBias);
	void SetMinCasterDepth(float Depth);
	virtual class FLightSceneProxy* CreateSceneProxy() const;
	FLightSceneProxy* SceneProxy = nullptr;
protected:
	virtual bool ShouldCreateRenderState() const override;

	virtual void CreateRenderState() override;
	virtual void DestroyRenderState() override;
private:
	// 리플렉션에 등록되어 Details 편집과 장면 저장의 대상이 된다. GetShadowSettings로 렌더러용 설정을 수집한다.
	bool mCastShadows{ true }; // 조명의 그림자 생성 요청 여부. 실제 그림자 맵 할당 여부와는 별개이다.
	int32 mShadowMapResolution{ DefaultShadowMapResolution }; // 그림자 맵 한 변의 요청 해상도(텍셀).
	int32 mShadowDepthBiasPreset{ static_cast<int32>(EShadowDepthBiasPreset::Medium) };
	int32 mShadowSlopeScaledDepthBiasPreset{ static_cast<int32>(EShadowSlopeScaledDepthBiasPreset::Medium) };
	float mShadowNormalBias{ 1.0f }; // 표면 법선 방향으로 그림자 판정 위치를 옮기는 보정량(그림자 맵 텍셀 수).
	float mMinCasterDepth{ DefaultShadowMinCasterDepth }; // 광원 뷰 전방 깊이 단위의 PCSS 탐색 하한. 클리핑에는 사용하지 않는다.
};
