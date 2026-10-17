#include "EnginePCH.h"
#include "ShadowAtlas.h"
#include "Render/RenderCommand.h"

void FShadowAtlas::Init()
{
	D3D11_TEXTURE2D_DESC Desc{};
	Desc.Width = AtlasSize;
	Desc.Height = AtlasSize;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	
	Atlas = RenderCommand::CreateTexture2D(Desc);

	Nodes[0].Size = AtlasSize;
	Nodes[0].X = 0;
	Nodes[0].Y = 0;
	Nodes[0].State = ENodeState::Free;

	for (int p = 0; p < NodeCount - 256; ++p)
	{
		const int ChildSize = Nodes[p].Size / 2;
		for (int i = 0; i <= 3; ++i)
		{
			int c = 4 * p + i + 1;
			Nodes[c].Size = ChildSize;
			Nodes[c].X = Nodes[p].X + ChildSize * (i & 1);  // i = 0(0), i = 1(1), i = 2(0), i = 3(1)
			Nodes[c].Y = Nodes[p].Y + ChildSize * (i >> 1);  // i = 0(0), i = 1(0), i = 2(1), i = 3(1)
		}
	}
}

bool FShadowAtlas::Allocate(uint32 Size, FShadowAtlasTile &Out)
{
	Out = {};
	if (!IsValidTileSize(Size)) return false;
	
	return Allocate_Internal(0, Size, Out);
}

bool FShadowAtlas::Allocate_Internal(uint32 index, uint32 Size, FShadowAtlasTile& Out)
{
	FNode& Node = Nodes[index];
	if (Node.State == ENodeState::Used || Size > Node.Size)
		return false;

	if (Size == Node.Size)
	{
		// 안쪽 일부가 쓰이는 노드(Split)는 통째로 줄 수 없다
		if (Node.State != ENodeState::Free)
			return false;

		Node.State = ENodeState::Used;
		Out = { Node.X, Node.Y, Size, index };
		return true;
	}

	// Size < Node.Size: 자식에서 찾는다. 성공한 경우에만 Split으로 표시해서 실패하면 원래 상태가 그대로 남는다
	for (uint32 Child = index * 4 + 1; Child <= index * 4 + 4; ++Child)
	{
		if (Allocate_Internal(Child, Size, Out))
		{
			Node.State = ENodeState::Split;
			return true;
		}
	}
	return false;
}


bool FShadowAtlas::DeAllocate(FShadowAtlasTile& Target)
{
	if (!Target.IsValid())
		return false;

	const uint32 Index = Target.Index;
	FNode& Node = Nodes[Index];
	if (Node.State != ENodeState::Used || Node.X != Target.X || Node.Y != Target.Y || Node.Size != Target.Size)
		return false;   // 이미 해제됐거나 다른 타일

	Node.State = ENodeState::Free;
	Target = {};
	if (Index == 0)
	{
		return true;
	}


	uint32 Parent = (Index - 1) / 4;
	Deallocate_Internal(Parent);


	return true;
}

void FShadowAtlas::Deallocate_Internal(uint32 index)
{
	uint32 FreeCount = 0;
	for (uint32 Child = index * 4 + 1; Child <= index * 4 + 4; ++Child)
	{
		if (Nodes[Child].State == ENodeState::Free)
		{
			++FreeCount;
		}
	}

	if (FreeCount == 4)
	{
		Nodes[index].State = ENodeState::Free;
		if (index == 0)
			return;

		uint32 Parent = (index - 1) / 4;
		Deallocate_Internal(Parent);
	}
}


void FShadowAtlas::ClearDepth()
{
	RenderCommand::ClearDepthStencil(Atlas.get(), 1.0f);
}

FTexture2D* FShadowAtlas::GetTexture() const
{
	return Atlas.get();
}

uint32 FShadowAtlas::GetResolution(const FShadowAtlasTile& Target)
{
	return Nodes[Target.Index].Size;
}
