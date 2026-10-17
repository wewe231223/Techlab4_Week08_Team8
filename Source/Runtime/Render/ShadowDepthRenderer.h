#pragma once

#include "Buffer.h"
#include "PipelineState.h"
#include "Texture2D.h"
#include "ShadowAtlas.h"
#include "Containers/Array.h"

class FPrimitiveSceneProxy;

// 메인 패스(UberLit)가 섀도우 아틀라스를 읽는 슬롯. 섀도우 패스가 DSV로 묶기 전에 이 슬롯을 비워야 한다.
inline constexpr uint32 ShadowAtlasSRVSlot = 20;

// ShadowDepth.hlsl cbuffer ShadowDepthConstants(b0)와 바이트 단위로 같아야 한다.
struct FShadowDepthPassConstants
{
	FMatrix LightViewProjection;
};

// 라이트 시점에서 불투명 caster의 깊이만 섀도우맵에 기록한다 (언리얼의 Shadow Depth Pass).
// 셰이더가 FRenderResourceManager::Init 이후에야 생기므로 Init은 그 뒤에 불러야 한다.
class FShadowDepthRenderer
{
public:
	FShadowDepthRenderer() = default;
	~FShadowDepthRenderer() = default;

	FShadowDepthRenderer(const FShadowDepthRenderer&) = delete;
	FShadowDepthRenderer& operator=(const FShadowDepthRenderer&) = delete;

	bool Init();
	// 아틀라스의 Tile 영역에만 caster 깊이를 그린다. 지우지 않으므로 아틀라스는 프레임 시작에 한 번 지워 둬야 한다.
	void OnRender(FTexture2D* Atlas, const FShadowAtlasTile& Tile, const FMatrix& LightViewProjection, const TArray<FPrimitiveSceneProxy*>& ShadowCasters);

private:
	void DrawCaster(const FPrimitiveSceneProxy* Caster);

	FPipelineState PipelineState;
	FShaderProgram* Shader = nullptr;
	TUniquePtr<FConstantBuffer> PassConstantBuffer;
	TUniquePtr<FConstantBuffer> ObjectConstantBuffer;   // 메인 패스의 오브젝트 슬롯 버퍼와 섞이지 않게 따로 둔다
};
