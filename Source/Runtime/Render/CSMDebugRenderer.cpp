#include "EnginePCH.h"

#include "CSMDebugRenderer.h"
#include "RenderCommand.h"
#include "RenderResourceManager.h"
#include "Render/Buffer.h"

bool FCSMDebugRenderer::Init()
{
	Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/CSMDebugShader.hlsl");
	if (!Shader)
	{
		HTR_LOG(Error, "[CSMDebugRenderer] shader not found");
		return false;
	}

	ConstantBuffer = RenderCommand::CreateConstantBuffer(sizeof(FCSMDebugShaderConstants));

	PipelineState.Shader = Shader;
	PipelineState.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	return true;
}

void FCSMDebugRenderer::OnRender(FTexture2D* DepthTexture, const FMatrix& ViewProjection, const FVector4& CascadeSplits, uint32 CascadeCount, float TransitionFraction)
{
	if (!DepthTexture || CascadeCount <= 1)
		return;

	FCSMDebugShaderConstants Constants{};
	Constants.InverseViewProjection = ViewProjection.Inverse();
	Constants.ViewProjection = ViewProjection;
	Constants.CascadeSplits = CascadeSplits;
	Constants.CascadeTransitionFraction = TransitionFraction;
	Constants.CascadeCount = CascadeCount;

	RenderCommand::BindPipelineState(PipelineState);
	RenderCommand::SetRasterizerState(ERasterizerState::SolidNone);
	RenderCommand::SetDepthStencilState(EDepthStencilState::Disabled);
	RenderCommand::SetBlendState(EBlendState::Multiply);

	RenderCommand::BindShaderResource(0, DepthTexture, EShaderBindFlagBits::Pixel);

	RenderCommand::UpdateBufferData(ConstantBuffer.get(), &Constants);
	RenderCommand::BindConstantBuffer(0, ConstantBuffer.get(), EShaderBindFlagBits::Pixel);
	RenderCommand::Draw(3);

	RenderCommand::UnbindShaderResource(0, EShaderBindFlagBits::Pixel);
}
