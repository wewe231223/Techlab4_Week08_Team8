#pragma once
#include "Texture2D.h"

inline constexpr uint32 AtlasSize = 8192;
inline constexpr uint32 MaxDepth = 4;                                  // 8192 -> 4096 -> 2048 -> 1024 -> 512
inline constexpr uint32 NodeCount = (1 + 4 + 16 + 64 + 256);           // 341
inline constexpr uint32 MinShadowMapSize = 512;

inline bool IsValidTileSize(uint32 Size)
{
	return Size >= MinShadowMapSize && Size <= AtlasSize && std::has_single_bit(Size);
}

struct FShadowAtlasTile
{
	uint32 X = 0, Y = 0, Size = 0, Index = 0;
	bool IsValid() const { return IsValidTileSize(Size); }

	// 셰이더가 0~1 섀도우맵 UV를 아틀라스 UV로 옮길 때 쓴다: AtlasUV = UV * xy + zw
	FVector4 GetAtlasScaleOffset() const
	{
		const float Inv = 1.0f / static_cast<float>(AtlasSize);
		return FVector4(Size * Inv, Size * Inv, X * Inv, Y * Inv);
	}
};


class FShadowAtlas
{
public:
	void Init();
	bool Allocate(uint32 Size, FShadowAtlasTile& Out);
	bool DeAllocate(FShadowAtlasTile& Target);
	FTexture2D* GetTexture() const;

	// 아틀라스 전체를 깊이 1.0으로 지운다. 타일마다 지우면 앞서 그린 다른 라이트 타일까지 지워지므로 프레임에 한 번만 부른다.
	void ClearDepth();

	//현재 섀도우 맵의 Resolution을 가져온다.
	uint32 GetResolution(const FShadowAtlasTile& Target);

private:
	bool Allocate_Internal(uint32 index, uint32 Size, FShadowAtlasTile& Out);
	void Deallocate_Internal(uint32 index);
	TUniquePtr<FTexture2D> Atlas;

	enum class ENodeState : uint8 { Free, Split, Used };

	struct FNode
	{ 
		uint32 X, Y;
		uint32 Size = 0;
		ENodeState State = ENodeState::Free;
	};

	FNode Nodes[NodeCount];
};
