#pragma once

#include "Buffer.h"
#include "PipelineState.h"
#include "Texture.h"
#include "Math/Matrix.h"
#include "Math/Vector4.h"

struct alignas(16) FCSMDebugShaderConstants {
	FMatrix mInverseViewProjection{ FMatrix::Identity };
	FMatrix mViewProjection{ FMatrix::Identity };
	FVector4 mCascadeSplits{};
	float mCascadeTransitionFraction{};
	uint32 mCascadeCount{};
	float mPadding[2]{};
};
static_assert(sizeof(FCSMDebugShaderConstants) == 160);

class FCSMDebugRenderer {
public:
	FCSMDebugRenderer() = default;
	~FCSMDebugRenderer() = default;
	FCSMDebugRenderer(const FCSMDebugRenderer&) = delete;
	FCSMDebugRenderer& operator=(const FCSMDebugRenderer&) = delete;

public:
	bool Init();
	void OnRender(FTexture* DepthTexture, const FMatrix& ViewProjection, const FVector4& CascadeSplits, uint32 CascadeCount, float TransitionFraction);

private:
	FPipelineState mPipelineState{};
	FShaderProgram* mShader{};
	TUniquePtr<FConstantBuffer> mConstantBuffer{};
};
