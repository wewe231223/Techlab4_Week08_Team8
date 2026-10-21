#pragma once

#include "Core/Types.h"
#include "Math/Vector4.h"
#include "Texture.h"
#include "Containers/Array.h"

enum class ERenderTargetLoadOp
{
	Load,
	Clear,
	DontCare,
};

enum class ERenderTargetStoreOp
{
	Store,
	DontCare,
};

struct FViewportSettings
{
	int32 StartX = 0;
	int32 StartY = 0;
	uint32 Width = 0;
	uint32 Height = 0;
	float MinDepth = 0.0f;
	float MaxDepth = 1.0f;
};

struct FClearValue
{
	FVector4 ColorClearValue = { 0.3f, 0.3f, 0.3f, 1.0f };
	float DepthClearValue = 1.0f;
	uint8 StencilClearValue = 0;
};

struct FRenderingDesc
{
	FTexture* Texture = nullptr;
	ERenderTargetLoadOp  LoadOp = ERenderTargetLoadOp::Clear;
	ERenderTargetStoreOp StoreOp = ERenderTargetStoreOp::Store;
	FClearValue ClearValue = FClearValue();
	uint32 ArraySlice = 0;
};

struct FRenderingInfo
{
	FViewportSettings ViewportSetting;
	TArray<FRenderingDesc> ColorRenderTargets;
	FRenderingDesc DepthStencil;
};