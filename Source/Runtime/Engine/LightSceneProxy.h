#pragma once

#include "Components/LightComponent.h"

#include "SceneManagement.h"


class FLightSceneProxy
{
public:
	FLightSceneProxy(const ULightComponent* InLightComponent)
		:LightComponent(InLightComponent) {};
	virtual ~FLightSceneProxy() = default;

	virtual void GetLightShaderParameters(FLightRenderParameters& OutParameters) const = 0;

	const ULightComponent* GetLightComponent() const { return LightComponent; }
	bool IsVisible() const { return LightComponent->IsVisible(); }
	FLightShadowSettings GetShadowSettings() const;

protected:
	const ULightComponent* LightComponent = nullptr;

private:
	friend class FScene;
	FScene* Scene = nullptr;
    int32 PackedIndex = INDEX_NONE;
};






