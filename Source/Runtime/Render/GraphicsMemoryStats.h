#pragma once

#include "Core/Types.h"
#include "Core/EngineString.h"

#include <d3d11.h>

class FTrackedGraphicsResource;

class FGraphicsMemoryStats {
public:
	static void TrackResource(ID3D11Resource* Resource);
	static uint64 CalculateTextureBytes(const D3D11_TEXTURE2D_DESC& Desc);
	static uint64 GetTextureBytes();
	static uint64 GetBufferBytes();
	static uint64 GetTextureCount();
	static uint64 GetBufferCount();
	static uint64 GetUnmeasuredResourceCount();
	static FString GetResourceReport();

private:
	friend class FTrackedGraphicsResource;

	inline static uint64 mTextureBytes{};
	inline static uint64 mBufferBytes{};
	inline static uint64 mTextureCount{};
	inline static uint64 mBufferCount{};
	inline static uint64 mUnmeasuredResourceCount{};
	inline static FTrackedGraphicsResource* mFirstResource{};
};
