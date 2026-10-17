#include "EnginePCH.h"

#include "RenderCommand.h"
#include "ShadowRenderer.h"
#include "RenderResourceManager.h"
#include "Renderer.h"

bool FShadowRenderer::Init()
{
	Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/ShadowShader.hlsl");
	if (!Shader)
	{
		HTR_LOG(Error, "[ExponentialHeightFog] shader not found");
		return false;
	}

	ConstantBuffer = RenderCommand::CreateConstantBuffer(sizeof(FShadowConstants));

	PipelineState.Shader = Shader;
	PipelineState.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	PipelineState.RasterizerState = ERasterizerState::SolidBack;
	PipelineState.BlendState = EBlendState::NoColorWrite;
	PipelineState.DepthStencilState = EDepthStencilState::Default;

	D3D11_TEXTURE2D_DESC Desc{};
	Desc.Height = AtlasHeight;
	Desc.Width = AtlasWidth;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	ShadowBufferAtlas = RenderCommand::CreateTexture2D(Desc);

	return true;
}

void FShadowRenderer::OnRender(FRenderer& Renderer, const FRenderQueue& Renderqueue, const FMatrix& ViewProjection)
{
	RenderCommand::BindPipelineState(PipelineState);

	Renderer.SetShadowBuffer(ShadowBufferAtlas.get(), ViewProjection);

	FShadowConstants Constants;
	Constants.ViewProjection = ViewProjection;

	RenderCommand::UpdateBufferData(ConstantBuffer.get(), &Constants);
	RenderCommand::BindConstantBuffer(0, ConstantBuffer.get(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
	
	Renderer.DrawShadowCasters(Renderqueue);
}