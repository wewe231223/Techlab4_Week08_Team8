#pragma once

#include "Buffer.h"
#include "PipelineState.h"
#include "Texture2D.h"
#include "Math/Matrix.h"
#include "Math/Vector4.h"

struct alignas(16) FCSMDebugShaderConstants
{
	FMatrix InverseViewProjection;
	FMatrix ViewProjection;
	FVector4 CascadeSplits;
	float CascadeTransitionFraction;
	uint32 CascadeCount;
	float Padding[2];
};
static_assert(sizeof(FCSMDebugShaderConstants) == 160);

class FCSMDebugRenderer
{
public:
	FCSMDebugRenderer() = default;
	~FCSMDebugRenderer() = default;

	FCSMDebugRenderer(const FCSMDebugRenderer&) = delete;
	FCSMDebugRenderer& operator=(const FCSMDebugRenderer&) = delete;

	bool Init();
	void OnRender(FTexture2D* DepthTexture, const FMatrix& ViewProjection, const FVector4& CascadeSplits, uint32 CascadeCount, float TransitionFraction);

private:
	FPipelineState PipelineState;
	FShaderProgram* Shader = nullptr;
	TUniquePtr<FConstantBuffer> ConstantBuffer;
};
