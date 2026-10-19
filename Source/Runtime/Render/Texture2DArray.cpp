#include "EnginePCH.h"
#include "Texture2DArray.h"
#include "GraphicsMemoryStats.h"

FTexture2DArray::FTexture2DArray(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc, const D3D11_SUBRESOURCE_DATA* InitialData) {
	if (!Device || InDesc.ArraySize == 0 || (InDesc.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE) != 0) {
		return;
	}
	Microsoft::WRL::ComPtr<ID3D11Texture2D> Resource{};
	const HRESULT Result{ Device->CreateTexture2D(&InDesc, InitialData, Resource.GetAddressOf()) };
	if (FAILED(Result)) {
		HTR_LOG(Error, "[Texture2DArray] CreateTexture2D failed (hr=0x{:08X})", static_cast<uint32>(Result));
		return;
	}
	D3D11_TEXTURE2D_DESC Desc{};
	Resource->GetDesc(&Desc);
	Texture = Resource;
	Width = Desc.Width;
	Height = Desc.Height;
	Depth = 1;
	ArraySize = Desc.ArraySize;
	Format = Desc.Format;
	Dimension = ETextureDimension::Texture2DArray;
	if (!CreateViews(Device, Desc)) {
		mDepthStencilViews.Reset();
		mRenderTargetViews.Reset();
		SRV.Reset();
		Texture.Reset();
	}
	FGraphicsMemoryStats::TrackResource(Texture.Get());
}

FTexture2DArray::~FTexture2DArray() = default;

ID3D11RenderTargetView* FTexture2DArray::GetRTV(uint32 Slice) const {
	return Slice < static_cast<uint32>(mRenderTargetViews.Num()) ? mRenderTargetViews[Slice].Get() : nullptr;
}

ID3D11DepthStencilView* FTexture2DArray::GetDSV(uint32 Slice) const {
	return Slice < static_cast<uint32>(mDepthStencilViews.Num()) ? mDepthStencilViews[Slice].Get() : nullptr;
}

DXGI_FORMAT FTexture2DArray::GetShaderResourceFormat(DXGI_FORMAT InFormat) {
	switch (InFormat) {
	case DXGI_FORMAT_R16_TYPELESS:
		return DXGI_FORMAT_R16_UNORM;
	case DXGI_FORMAT_R24G8_TYPELESS:
		return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	case DXGI_FORMAT_R32_TYPELESS:
		return DXGI_FORMAT_R32_FLOAT;
	case DXGI_FORMAT_R32G8X24_TYPELESS:
		return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
	default:
		return InFormat;
	}
}

DXGI_FORMAT FTexture2DArray::GetDepthStencilFormat(DXGI_FORMAT InFormat) {
	switch (InFormat) {
	case DXGI_FORMAT_R16_TYPELESS:
		return DXGI_FORMAT_D16_UNORM;
	case DXGI_FORMAT_R24G8_TYPELESS:
		return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case DXGI_FORMAT_R32_TYPELESS:
		return DXGI_FORMAT_D32_FLOAT;
	case DXGI_FORMAT_R32G8X24_TYPELESS:
		return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	default:
		return InFormat;
	}
}

bool FTexture2DArray::CreateViews(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc) {
	const bool Multisampled{ InDesc.SampleDesc.Count > 1 };
	if ((InDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE) != 0) {
		D3D11_SHADER_RESOURCE_VIEW_DESC Desc{};
		Desc.Format = GetShaderResourceFormat(InDesc.Format);
		Desc.ViewDimension = Multisampled ? D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY : D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		if (Multisampled) {
			Desc.Texture2DMSArray.ArraySize = InDesc.ArraySize;
		} else {
			Desc.Texture2DArray.MipLevels = InDesc.MipLevels;
			Desc.Texture2DArray.ArraySize = InDesc.ArraySize;
		}
		const HRESULT Result{ Device->CreateShaderResourceView(Texture.Get(), &Desc, SRV.GetAddressOf()) };
		if (FAILED(Result)) {
			HTR_LOG(Error, "[Texture2DArray] CreateShaderResourceView failed (hr=0x{:08X})", static_cast<uint32>(Result));
			return false;
		}
	}

	if ((InDesc.BindFlags & D3D11_BIND_RENDER_TARGET) != 0) {
		for (uint32 Slice{}; Slice < InDesc.ArraySize; ++Slice) {
			D3D11_RENDER_TARGET_VIEW_DESC Desc{};
			Desc.Format = InDesc.Format;
			Desc.ViewDimension = Multisampled ? D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY : D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
			if (Multisampled) {
				Desc.Texture2DMSArray.FirstArraySlice = Slice;
				Desc.Texture2DMSArray.ArraySize = 1;
			} else {
				Desc.Texture2DArray.FirstArraySlice = Slice;
				Desc.Texture2DArray.ArraySize = 1;
			}
			Microsoft::WRL::ComPtr<ID3D11RenderTargetView> View{};
			const HRESULT Result{ Device->CreateRenderTargetView(Texture.Get(), &Desc, View.GetAddressOf()) };
			if (FAILED(Result)) {
				HTR_LOG(Error, "[Texture2DArray] CreateRenderTargetView failed (slice={}, hr=0x{:08X})", Slice, static_cast<uint32>(Result));
				return false;
			}
			mRenderTargetViews.Add(std::move(View));
		}
	}

	if ((InDesc.BindFlags & D3D11_BIND_DEPTH_STENCIL) != 0) {
		for (uint32 Slice{}; Slice < InDesc.ArraySize; ++Slice) {
			D3D11_DEPTH_STENCIL_VIEW_DESC Desc{};
			Desc.Format = GetDepthStencilFormat(InDesc.Format);
			Desc.ViewDimension = Multisampled ? D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY : D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
			if (Multisampled) {
				Desc.Texture2DMSArray.FirstArraySlice = Slice;
				Desc.Texture2DMSArray.ArraySize = 1;
			} else {
				Desc.Texture2DArray.FirstArraySlice = Slice;
				Desc.Texture2DArray.ArraySize = 1;
			}
			Microsoft::WRL::ComPtr<ID3D11DepthStencilView> View{};
			const HRESULT Result{ Device->CreateDepthStencilView(Texture.Get(), &Desc, View.GetAddressOf()) };
			if (FAILED(Result)) {
				HTR_LOG(Error, "[Texture2DArray] CreateDepthStencilView failed (slice={}, hr=0x{:08X})", Slice, static_cast<uint32>(Result));
				return false;
			}
			mDepthStencilViews.Add(std::move(View));
		}
	}
	return true;
}
