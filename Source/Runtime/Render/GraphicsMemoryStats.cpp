#include "EnginePCH.h"
#include "GraphicsMemoryStats.h"

#include "Texture.h"

#include <algorithm>
#include <bit>
#include <format>

namespace {
	constexpr GUID GraphicsMemoryTrackingId{ 0x7d7b6e95, 0xcab3, 0x4a61, { 0x8e, 0xa4, 0x8d, 0x69, 0xa2, 0x57, 0x84, 0xf3 } };

	uint32 GetCompressedBlockBytes(DXGI_FORMAT Format) {
		switch (Format) {
		case DXGI_FORMAT_BC1_TYPELESS:
		case DXGI_FORMAT_BC1_UNORM:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
		case DXGI_FORMAT_BC4_TYPELESS:
		case DXGI_FORMAT_BC4_UNORM:
		case DXGI_FORMAT_BC4_SNORM:
			return 8;
		case DXGI_FORMAT_BC2_TYPELESS:
		case DXGI_FORMAT_BC2_UNORM:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
		case DXGI_FORMAT_BC3_TYPELESS:
		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
		case DXGI_FORMAT_BC5_TYPELESS:
		case DXGI_FORMAT_BC5_UNORM:
		case DXGI_FORMAT_BC5_SNORM:
		case DXGI_FORMAT_BC6H_TYPELESS:
		case DXGI_FORMAT_BC6H_UF16:
		case DXGI_FORMAT_BC6H_SF16:
		case DXGI_FORMAT_BC7_TYPELESS:
		case DXGI_FORMAT_BC7_UNORM:
		case DXGI_FORMAT_BC7_UNORM_SRGB:
			return 16;
		default:
			return 0;
		}
	}
}

class FTrackedGraphicsResource final : public IUnknown {
public:
	FTrackedGraphicsResource(uint64 Bytes, bool Texture, const D3D11_TEXTURE2D_DESC& TextureDesc);
	~FTrackedGraphicsResource();

public:
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID InterfaceId, void** OutObject) override;
	ULONG STDMETHODCALLTYPE AddRef() override;
	ULONG STDMETHODCALLTYPE Release() override;

private:
	friend class FGraphicsMemoryStats;

	ULONG mReferenceCount{ 1 };
	uint64 mBytes{};
	bool mIsTexture{};
	D3D11_TEXTURE2D_DESC mTextureDesc{};
	FTrackedGraphicsResource* mPrevious{};
	FTrackedGraphicsResource* mNext{};
};

FTrackedGraphicsResource::FTrackedGraphicsResource(uint64 Bytes, bool Texture, const D3D11_TEXTURE2D_DESC& TextureDesc)
	: mBytes(Bytes),
	  mIsTexture(Texture),
	  mTextureDesc(TextureDesc) {
	mNext = FGraphicsMemoryStats::mFirstResource;
	if (mNext) {
		mNext->mPrevious = this;
	}
	FGraphicsMemoryStats::mFirstResource = this;
	if (mIsTexture) {
		FGraphicsMemoryStats::mTextureBytes += mBytes;
		++FGraphicsMemoryStats::mTextureCount;
	} else {
		FGraphicsMemoryStats::mBufferBytes += mBytes;
		++FGraphicsMemoryStats::mBufferCount;
	}
	if (mBytes == 0) {
		++FGraphicsMemoryStats::mUnmeasuredResourceCount;
	}
}

FTrackedGraphicsResource::~FTrackedGraphicsResource() {
	if (mPrevious) {
		mPrevious->mNext = mNext;
	} else {
		FGraphicsMemoryStats::mFirstResource = mNext;
	}
	if (mNext) {
		mNext->mPrevious = mPrevious;
	}
	if (mIsTexture) {
		FGraphicsMemoryStats::mTextureBytes -= mBytes;
		--FGraphicsMemoryStats::mTextureCount;
	} else {
		FGraphicsMemoryStats::mBufferBytes -= mBytes;
		--FGraphicsMemoryStats::mBufferCount;
	}
	if (mBytes == 0) {
		--FGraphicsMemoryStats::mUnmeasuredResourceCount;
	}
}

HRESULT STDMETHODCALLTYPE FTrackedGraphicsResource::QueryInterface(REFIID InterfaceId, void** OutObject) {
	if (!OutObject) {
		return E_POINTER;
	}
	*OutObject = nullptr;
	if (InterfaceId != __uuidof(IUnknown)) {
		return E_NOINTERFACE;
	}
	*OutObject = static_cast<IUnknown*>(this);
	AddRef();
	return S_OK;
}

ULONG STDMETHODCALLTYPE FTrackedGraphicsResource::AddRef() {
	return ++mReferenceCount;
}

ULONG STDMETHODCALLTYPE FTrackedGraphicsResource::Release() {
	const ULONG Remaining{ --mReferenceCount };
	if (Remaining == 0) {
		delete this;
	}
	return Remaining;
}

void FGraphicsMemoryStats::TrackResource(ID3D11Resource* Resource) {
	if (!Resource) {
		return;
	}
	IUnknown* Existing{};
	UINT DataSize{ sizeof(Existing) };
	if (SUCCEEDED(Resource->GetPrivateData(GraphicsMemoryTrackingId, &DataSize, &Existing)) && Existing) {
		Existing->Release();
		return;
	}

	D3D11_RESOURCE_DIMENSION Dimension{};
	Resource->GetType(&Dimension);
	uint64 Bytes{};
	D3D11_TEXTURE2D_DESC TextureDesc{};
	const bool IsTexture{ Dimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D };
	if (IsTexture) {
		Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture{};
		if (FAILED(Resource->QueryInterface(IID_PPV_ARGS(Texture.GetAddressOf())))) {
			return;
		}
		Texture->GetDesc(&TextureDesc);
		Bytes = CalculateTextureBytes(TextureDesc);
	} else if (Dimension == D3D11_RESOURCE_DIMENSION_BUFFER) {
		Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer{};
		if (FAILED(Resource->QueryInterface(IID_PPV_ARGS(Buffer.GetAddressOf())))) {
			return;
		}
		D3D11_BUFFER_DESC Desc{};
		Buffer->GetDesc(&Desc);
		Bytes = Desc.ByteWidth;
	} else {
		return;
	}

	FTrackedGraphicsResource* Tracking{ new FTrackedGraphicsResource{ Bytes, IsTexture, TextureDesc } };
	if (FAILED(Resource->SetPrivateDataInterface(GraphicsMemoryTrackingId, Tracking))) {
		HTR_LOG(Warning, "[GraphicsMemory] Failed to track resource ({} bytes)", Bytes);
	}
	Tracking->Release();
}

uint64 FGraphicsMemoryStats::CalculateTextureBytes(const D3D11_TEXTURE2D_DESC& Desc) {
	if (Desc.Width == 0 || Desc.Height == 0 || Desc.ArraySize == 0 || Desc.SampleDesc.Count == 0) {
		return 0;
	}
	const uint32 BlockBytes{ GetCompressedBlockBytes(Desc.Format) };
	const uint32 PixelBytes{ FormatToBytes(Desc.Format) };
	const bool Packed{ Desc.Format == DXGI_FORMAT_R8G8_B8G8_UNORM || Desc.Format == DXGI_FORMAT_G8R8_G8B8_UNORM || Desc.Format == DXGI_FORMAT_YUY2 || Desc.Format == DXGI_FORMAT_Y210 || Desc.Format == DXGI_FORMAT_Y216 };
	const uint32 PackedBytes{ Desc.Format == DXGI_FORMAT_Y210 || Desc.Format == DXGI_FORMAT_Y216 ? 8u : 4u };
	if (BlockBytes == 0 && PixelBytes == 0 && Desc.Format != DXGI_FORMAT_R1_UNORM && !Packed) {
		return 0;
	}
	const uint32 MipCount{ Desc.MipLevels != 0 ? Desc.MipLevels : static_cast<uint32>(std::bit_width(std::max(Desc.Width, Desc.Height))) };
	uint64 Width{ Desc.Width };
	uint64 Height{ Desc.Height };
	uint64 Bytes{};
	for (uint32 Mip{}; Mip < MipCount; ++Mip) {
		if (BlockBytes != 0) {
			Bytes += ((Width + 3) / 4) * ((Height + 3) / 4) * BlockBytes;
		} else if (Packed) {
			Bytes += ((Width + 1) / 2) * Height * PackedBytes;
		} else if (Desc.Format == DXGI_FORMAT_R1_UNORM) {
			Bytes += ((Width + 7) / 8) * Height;
		} else {
			Bytes += Width * Height * PixelBytes;
		}
		Width = std::max<uint64>(1, Width / 2);
		Height = std::max<uint64>(1, Height / 2);
	}
	return Bytes * Desc.ArraySize * Desc.SampleDesc.Count;
}

uint64 FGraphicsMemoryStats::GetTextureBytes() {
	return mTextureBytes;
}

uint64 FGraphicsMemoryStats::GetBufferBytes() {
	return mBufferBytes;
}

uint64 FGraphicsMemoryStats::GetTextureCount() {
	return mTextureCount;
}

uint64 FGraphicsMemoryStats::GetBufferCount() {
	return mBufferCount;
}

uint64 FGraphicsMemoryStats::GetUnmeasuredResourceCount() {
	return mUnmeasuredResourceCount;
}

FString FGraphicsMemoryStats::GetResourceReport() {
	TArray<const FTrackedGraphicsResource*> Resources{};
	for (const FTrackedGraphicsResource* Resource{ mFirstResource }; Resource; Resource = Resource->mNext) {
		Resources.Add(Resource);
	}
	std::sort(Resources.begin(), Resources.end(), [](const FTrackedGraphicsResource* Left, const FTrackedGraphicsResource* Right) {
		return Left->mBytes > Right->mBytes;
	});
	FString Report{ std::format("GPU resource payload: {} bytes ({:.2f} MiB)\nTextures: {} bytes ({} resources)\nBuffers: {} bytes ({} resources)\n", mTextureBytes + mBufferBytes, static_cast<double>(mTextureBytes + mBufferBytes) / (1024.0 * 1024.0), mTextureBytes, mTextureCount, mBufferBytes, mBufferCount) };
	for (const FTrackedGraphicsResource* Resource : Resources) {
		if (Resource->mIsTexture) {
			const D3D11_TEXTURE2D_DESC& Desc{ Resource->mTextureDesc };
			Report += std::format("Texture2D {}x{}, array {}, mips {}, samples {}, DXGI format {}: {} bytes ({:.2f} MiB)\n", Desc.Width, Desc.Height, Desc.ArraySize, Desc.MipLevels, Desc.SampleDesc.Count, static_cast<uint32>(Desc.Format), Resource->mBytes, static_cast<double>(Resource->mBytes) / (1024.0 * 1024.0));
		} else {
			Report += std::format("Buffer: {} bytes\n", Resource->mBytes);
		}
	}
	if (mUnmeasuredResourceCount != 0) {
		Report += std::format("Unmeasured resources: {}\n", mUnmeasuredResourceCount);
	}
	return Report;
}
