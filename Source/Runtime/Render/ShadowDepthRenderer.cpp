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

void FShadowDepthRenderer::OnRender(FTexture2D* Atlas, const FShadowAtlasTile& Tile, const FMatrix& LightViewProjection, const TArray<FPrimitiveSceneProxy*>& ShadowCasters)
{
	if (!Atlas || !Tile.IsValid()) return;

	// 지난 프레임 메인 패스가 아틀라스를 SRV로 꽂아 두었으므로, DSV로 묶기 전에 풀어야 한다(동시에 묶으면 런타임이 무시한다).
	RenderCommand::UnbindShaderResource(ShadowAtlasSRVSlot, EShaderBindFlagBits::Pixel);

	// RTV 없이 아틀라스 DSV만 묶는다. 뷰포트를 타일로 잡으면 라이트 행렬이 만든 NDC -1~1이 타일 영역에만 그려져
	// 옆 타일을 침범하지 않는다. 지우면 다른 라이트 타일까지 지워지므로 Load로 둔다.
	FRenderingInfo RenderingInfo;
	RenderingInfo.ViewportSetting.StartX = static_cast<int32>(Tile.X);
	RenderingInfo.ViewportSetting.StartY = static_cast<int32>(Tile.Y);
	RenderingInfo.ViewportSetting.Width = Tile.Size;
	RenderingInfo.ViewportSetting.Height = Tile.Size;
	RenderingInfo.DepthStencil.Texture = Atlas;
	RenderingInfo.DepthStencil.LoadOp = ERenderTargetLoadOp::Load;
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

	//TODO : 큰 슬롯 버퍼에 한 번에 올리고 BindConstanBufferRanger로 칸만 바꿔가며 바인딩하는 방식으로 바꾸기
	uint8* Dest = static_cast<uint8*>(RenderCommand::MapWriteDiscard(ObjectConstantBuffer.get()));
	if (!Dest) return;
	WritePerObjectConstants(Dest, Caster->GetLocalToWorld(), Caster->GetWorldToLocal(), Caster->IsReciveShadow());
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
