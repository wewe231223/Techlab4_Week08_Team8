#pragma once

#include "LightComponentBase.h"
#include "Render/LineBatcher.h"
#include "Render/Texture2D.h"

class ULightComponent : public ULightComponentBase
{
	DECLARE_CLASS(ULightComponent, ULightComponentBase)

	REFLECT_START(ULightComponent)
	PROPERTY(bCastShadows)
	PROPERTY(ShadowResolution)
	REFLECT_END()
public:
	ULightComponent();
	virtual ~ULightComponent() override;

	virtual ELightType GetLightType() const = 0;
	virtual void DrawDebug(FLineBatcher* LineBatcher) const;

	//TODO : LIghtProxy가 일단은 컴포넌트 포인터를 그대로 들고 있네? 나중엔 Proxy에 기록하도록 바꿔야 한다.
	bool IsCastShadows() const { return bCastShadows; }

	//index : PointLight 6면 ViewProj 가져오는 용도, 다른 라이트는 0을 사용
	virtual FMatrix GetShadowViewPorjection(FVector Center, float Radius, int index = 0) const = 0;

	// 섀도우맵 해상도는 2의 거듭제곱만 쓴다 (아틀라스 타일을 4등분으로 나눠 배치하기 위해).
	static constexpr uint32 MinShadowResolution = 512;
	static constexpr uint32 MaxShadowResolution = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;   // 16384, D3D11 텍스처 한 변 최대 크기
	// 저장 파일 등으로 범위를 벗어난 값이 들어와도 항상 유효한 해상도를 돌려준다.
	uint32 GetShadowResolution() const;
	void SetShadowResolution(uint32 InResolution);

	virtual void PostEditChangeProperty(const FProperty& Property) override;

	virtual class FLightSceneProxy* CreateSceneProxy() const
	{
		return NULL;
	}

	FLightSceneProxy* SceneProxy = nullptr;

protected:
	virtual bool ShouldCreateRenderState() const override
	{ return true; }

	virtual void CreateRenderState() override;
	virtual void DestroyRenderState() override;

	bool bCastShadows = true;
	uint32 ShadowResolution = 1024;
private:


};