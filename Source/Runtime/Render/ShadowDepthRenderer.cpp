#include "EnginePCH.h"

#include "ShadowDepthRenderer.h"
#include "RenderCommand.h"
#include "RenderResourceManager.h"
#include "Renderer.h"
#include "Material.h"
#include "Mesh.h"
#include "Engine/PrimitiveSceneProxy.h"

namespace
{
	// ShadowDepth.hlsl의 슬롯. UberLit과 같은 규칙(b0 = 패스, b2 = 오브젝트)이라 메인 패스가 다시 바인딩하면 덮인다.
	constexpr uint32 PassConstantSlot = 0;
	constexpr uint32 ObjectConstantSlot = 2;
}

bool FShadowDepthRenderer::Init()
{
	Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/ShadowDepth.hlsl");
	if (!Shader)
	{
		HTR_LOG(Error, "[ShadowDepth] shader not found");
		return false;
	}

	PassConstantBuffer = RenderCommand::CreateConstantBuffer(sizeof(FShadowDepthPassConstants));
	ObjectConstantBuffer = RenderCommand::CreateConstantBuffer(sizeof(FPerObjectConstants));

	PipelineState.Shader = Shader;
	PipelineState.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	PipelineState.RasterizerState = ERasterizerState::ShaderDepth;          // DepthBias로 shadow acne를 줄인다
	PipelineState.BlendState = EBlendState::Opaque;                          // RTV가 없어 의미는 없다
	PipelineState.DepthStencilState = EDepthStencilState::Default;           // LESS_EQUAL + 쓰기
	return true;
}

void FShadowDepthRenderer::OnRender(FTexture2D* ShadowMap, const FMatrix& LightViewProjection, const TArray<FPrimitiveSceneProxy*>& ShadowCasters)
{
	if (!ShadowMap) return;

	// RTV 없이 섀도우맵 DSV만 묶고 1.0으로 지운다. 뷰포트는 섀도우맵 해상도에 맞춘다.
	// 메인 패스가 이 섀도우맵을 SRV로 꽂게 되면, 여기서 DSV로 묶기 전에 그 슬롯을 UnbindShaderResource로 풀어야 한다.
	FRenderingInfo RenderingInfo;
	RenderingInfo.ViewportSetting.Width = ShadowMap->GetWidth();
	RenderingInfo.ViewportSetting.Height = ShadowMap->GetHeight();
	RenderingInfo.DepthStencil.Texture = ShadowMap;
	RenderingInfo.DepthStencil.LoadOp = ERenderTargetLoadOp::Clear;
	RenderingInfo.DepthStencil.ClearValue.DepthClearValue = 1.0f;
	RenderCommand::BeginRenderPass(RenderingInfo);

	RenderCommand::BindPipelineState(PipelineState);
	RenderCommand::GetContext()->PSSetShader(nullptr, nullptr, 0);   // 깊이만 쓰므로 PS를 돌리지 않는다

	const FShadowDepthPassConstants PassConstants{ LightViewProjection };
	RenderCommand::UpdateBufferData(PassConstantBuffer.get(), &PassConstants);
	RenderCommand::BindConstantBuffer(PassConstantSlot, PassConstantBuffer.get(), EShaderBindFlagBits::Vertex);
	RenderCommand::BindConstantBuffer(ObjectConstantSlot, ObjectConstantBuffer.get(), EShaderBindFlagBits::Vertex);

	for (const FPrimitiveSceneProxy* Caster : ShadowCasters)
		DrawCaster(Caster);

	// DSV를 풀어 메인 패스가 섀도우맵을 SRV로 읽을 수 있게 한다. 메인 패스는 자기 BeginRenderPass로 타깃·뷰포트를 다시 잡는다.
	RenderCommand::EndRenderPass(RenderingInfo);
}

void FShadowDepthRenderer::DrawCaster(const FPrimitiveSceneProxy* Caster)
{
	UStaticMesh* Mesh = Caster->GetMesh();
	const FMeshRenderState* RenderState = Caster->GetRenderState();
	if (!Mesh || !RenderState) return;

	// 그림자는 화면 크기와 무관하게 LOD 0으로 그린다. 카메라 LOD를 따르면 라이트마다 결과가 달라진다.
	constexpr uint32 LODIndex = 0;
	const FCachedMeshLOD& LOD = Caster->GetLOD(LODIndex);

	uint8* Dest = static_cast<uint8*>(RenderCommand::MapWriteDiscard(ObjectConstantBuffer.get()));
	if (!Dest) return;
	WritePerObjectConstants(Dest, Caster->GetLocalToWorld(), Caster->GetWorldToLocal());
	RenderCommand::Unmap(ObjectConstantBuffer.get());

	RenderCommand::BindMesh(Mesh, LODIndex);
	for (uint32 Index = LOD.FirstSection; Index < LOD.FirstSection + LOD.NumSections; ++Index)
	{
		const FCachedMeshSection& Section = Caster->GetSection(Index);

		// 지금은 불투명만 그린다. 반투명은 그림자를 만들지 않고, 마스크(알파 테스트)는 PS가 있는 별도 경로가 필요하다.
		if (Section.Material && Section.Material->BlendState != EBlendState::Opaque)
			continue;

		RenderCommand::DrawIndexed(Section.IndexCount, Section.StartIndex);
	}
}
