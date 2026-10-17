#pragma once

#include "Render/PipelineState.h"
#include "Render/Buffer.h"
#include "Texture2D.h"

class FRenderer;
class FRenderQueue;

struct FShadowConstants
{
	FMatrix ViewProjection;
};

class FShadowRenderer
{
public:
	FShadowRenderer() = default;
	~FShadowRenderer() = default;

	bool Init();
	void OnRender(FRenderer& Renderer, const FRenderQueue& Renderqueue, const FMatrix& ViewProjection); // Todo : 어떤 Atlas 위치의 Shadow맵을 사용할건지도 추가해야함

	FShadowRenderer(const FShadowRenderer&) = delete;
	FShadowRenderer& operator=(const FShadowRenderer&) = delete;

	FTexture2D* GetShadowBufferAtlas() { return ShadowBufferAtlas.get(); }

private:
	FShaderProgram* Shader = nullptr;
	FPipelineState PipelineState;
	TUniquePtr<FConstantBuffer> ConstantBuffer;
	TUniquePtr<FTexture2D> ShadowBufferAtlas;

	// Todo : Directional Light 이외의 Light 고려한 Atlas 크기 설계
	float AtlasHeight = 2048;
	float AtlasWidth = 2048;

};