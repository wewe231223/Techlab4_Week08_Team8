#include "EnginePCH.h"
#include "Texture2D.h"

#include "RenderCommand.h"


FTexture2D::FTexture2D(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc, const FImageData& Image)
	: FTexture2D(Device, InDesc, Image.IsValid() ? Image.Pixels.GetData() : nullptr, Image.GetRowPitch())
{
}

FTexture2D::FTexture2D(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc, const void* InitialData, uint32 RowPitch, bool bArrayView)
{
	Width = InDesc.Width;
	Height = InDesc.Height;
	Depth = 1;
	Format = InDesc.Format;
	ArraySize = InDesc.ArraySize;
	Dimension = (bArrayView || InDesc.ArraySize > 1) ? ETextureDimension::Texture2DArray : ETextureDimension::Texture2D;

	if (InitialData && RowPitch == 0)
	{
		const uint32 BytesPerPixel = FormatToBytes(InDesc.Format);
		if (BytesPerPixel == 0) { /* 기존 에러 처리 그대로 */ return; }
		RowPitch = InDesc.Width * BytesPerPixel;
	}

	const bool bGenerateMips = (InDesc.MiscFlags & D3D11_RESOURCE_MISC_GENERATE_MIPS) != 0;

	D3D11_SUBRESOURCE_DATA SubData{ InitialData, RowPitch, 0 };
	const D3D11_SUBRESOURCE_DATA* InitPtr = (InitialData && !bGenerateMips) ? &SubData : nullptr;

	HRESULT hr = Device->CreateTexture2D(&InDesc, InitPtr, (ID3D11Texture2D**)Texture.GetAddressOf());
	if (FAILED(hr)) { /* 기존 로그 그대로 */ return; }

	CreateViews(Device, InDesc, bArrayView);

	if (bGenerateMips && InitialData && SRV)
	{
		ComPtr<ID3D11DeviceContext> Context;
		Device->GetImmediateContext(Context.GetAddressOf());
		Context->UpdateSubresource(Texture.Get(), 0, nullptr, InitialData, RowPitch, 0); // 0번 레벨
		Context->GenerateMips(SRV.Get());                                              // 나머지 레벨
	}
}
FTexture2D::FTexture2D(ID3D11Device* Device, ComPtr<ID3D11Resource> SwapchainTexture, const D3D11_TEXTURE2D_DESC& InDesc)
{
	Texture = std::move(SwapchainTexture);

	Width = InDesc.Width;
	Height = InDesc.Height;
	Depth = 1;
	Format = InDesc.Format;
	ArraySize = InDesc.ArraySize;
	Dimension = ETextureDimension::Texture2D;

	CreateViews(Device, InDesc);
}

void FTexture2D::CreateViews(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc, bool bArrayView)
{
	HRESULT hr;

	// TYPELESS 깊이 텍스처는 뷰마다 해석할 포맷을 지정해야 한다 (nullptr이면 생성 실패)
	const bool bTypelessDepth = (InDesc.Format == DXGI_FORMAT_R24G8_TYPELESS);
	// 층이 하나여도 셰이더가 Texture2DArray로 읽는다면 배열 뷰로 만들어야 차원이 맞는다.
	const bool bUseArrayView = bArrayView || InDesc.ArraySize > 1;

	if (InDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc{};
		SrvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;   // 깊이 24bit만 float로 읽는다
		if (bUseArrayView)
		{
			SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
			SrvDesc.Texture2DArray.MipLevels = 1;
			SrvDesc.Texture2DArray.ArraySize = InDesc.ArraySize;   // 전체 층을 하나의 SRV로 본다
		}
		else
		{
			SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			SrvDesc.Texture2D.MipLevels = 1;
		}

		hr = Device->CreateShaderResourceView(Texture.Get(), bTypelessDepth ? &SrvDesc : nullptr, SRV.GetAddressOf());
		if (FAILED(hr))
			HTR_LOG(Error, "[Texture2D] CreateShaderResourceView failed (hr=0x{:08X})", (uint32)hr);
	}

	if ((InDesc.BindFlags & D3D11_BIND_RENDER_TARGET) &&
		!(InDesc.MiscFlags & D3D11_RESOURCE_MISC_GENERATE_MIPS))
	{
		hr = Device->CreateRenderTargetView(Texture.Get(), nullptr, RTV.GetAddressOf());
		if (FAILED(hr))
			HTR_LOG(Error, "[Texture2D] CreateRenderTargetView failed (hr=0x{:08X})", (uint32)hr);
	}

	if (InDesc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
	{
		// DSV는 한 번에 한 층만 가리키므로 층마다 하나씩 만든다. 단일 텍스처는 1개다.
		const uint32 SliceCount = bUseArrayView ? InDesc.ArraySize : 1;
		for (uint32 Slice = 0; Slice < SliceCount; ++Slice)
		{
			D3D11_DEPTH_STENCIL_VIEW_DESC DsvDesc{};
			DsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			if (bUseArrayView)
			{
				DsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
				DsvDesc.Texture2DArray.FirstArraySlice = Slice;
				DsvDesc.Texture2DArray.ArraySize = 1;
			}
			else
			{
				DsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
			}

			ComPtr<ID3D11DepthStencilView> SliceDSV;
			hr = Device->CreateDepthStencilView(Texture.Get(), bTypelessDepth ? &DsvDesc : nullptr, SliceDSV.GetAddressOf());
			if (FAILED(hr))
			{
				HTR_LOG(Error, "[Texture2D] CreateDepthStencilView failed (slice={}, hr=0x{:08X})", Slice, (uint32)hr);
				DSVs.Empty();
				break;
			}
			DSVs.Add(SliceDSV);
		}
	}
}

UTexture2D::UTexture2D()
{

}
UTexture2D::~UTexture2D()
{
}

