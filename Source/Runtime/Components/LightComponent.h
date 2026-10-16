#pragma once

#include "LightComponentBase.h"
#include "Render/LineBatcher.h"
#include "Render/Texture2D.h"

class ULightComponent : public ULightComponentBase
{
	DECLARE_CLASS(ULightComponent, ULightComponentBase)

	REFLECT_START(ULightComponent)
	PROPERTY(bCastShadows)
	REFLECT_END()
public:
	ULightComponent();
	virtual ~ULightComponent() override;

	virtual ELightType GetLightType() const = 0;
	virtual void DrawDebug(FLineBatcher* LineBatcher) const;

	//TODO : LIghtProxy가 일단은 컴포넌트 포인터를 그대로 들고 있네? 나중엔 Proxy에 기록하도록 바꿔야 한다.
	bool IsCastShadows() const { return bCastShadows; }

	//index : PointLight 6면 ViewProj 가져오는 용도, 다른 라이트는 0을 사용
	virtual FMatrix GetShadowViewPorjection(const uint32 ShadowMapResolution, FVector Center, float Radius, int index = 0) const = 0;

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
private:


};