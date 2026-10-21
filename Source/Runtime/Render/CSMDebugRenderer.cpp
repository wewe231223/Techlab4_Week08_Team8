#include "EnginePCH.h"
#include "CSMDebugRenderer.h"

#include "RenderCommand.h"
#include "RenderResourceManager.h"

#include <algorithm>

bool FCSMDebugRenderer::Init() {
	mShader = FRenderResourceManager::GetShaderProgram("Resources/Shader/CSMDebugShader.hlsl");
	if (!mShader || !mShader->VertexShader || !mShader->VertexShader->IsValid() || !mShader->PixelShader || !mShader->PixelShader->IsValid()) {
		return false;
	}

	mConstantBuffer = RenderCommand::CreateConstantBuffer(sizeof(FCSMDebugShaderConstants));
	mPipelineState.Shader = mShader;
	mPipelineState.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	return mConstantBuffer && mConstantBuffer->GetBuffer();
}

void FCSMDebugRenderer::OnRender(FTexture* DepthTexture, const FMatrix& ViewProjection, const FVector4& CascadeSplits, uint32 CascadeCount, float TransitionFraction) {
	if (!DepthTexture || !mConstantBuffer || !mConstantBuffer->GetBuffer() || CascadeCount <= 1) {
		return;
	}

	FCSMDebugShaderConstants Constants{};
	Constants.mInverseViewProjection = ViewProjection.Inverse();
	Constants.mViewProjection = ViewProjection;
	Constants.mCascadeSplits = CascadeSplits;
	Constants.mCascadeTransitionFraction = TransitionFraction;
	Constants.mCascadeCount = std::min(CascadeCount, 4u);

	RenderCommand::BindPipelineState(mPipelineState);
	RenderCommand::SetRasterizerState(ERasterizerState::SolidNone);
	RenderCommand::SetDepthStencilState(EDepthStencilState::Disabled);
	RenderCommand::SetBlendState(EBlendState::Multiply);
	RenderCommand::BindShaderResource(0, DepthTexture, EShaderBindFlagBits::Pixel);
	RenderCommand::UpdateBufferData(mConstantBuffer.get(), &Constants);
	RenderCommand::BindConstantBuffer(0, mConstantBuffer.get(), EShaderBindFlagBits::Pixel);
	RenderCommand::Draw(3);
	RenderCommand::UnbindShaderResource(0, EShaderBindFlagBits::Pixel);
}
