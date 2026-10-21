#pragma once

#include "Containers/Array.h"
#include "Texture.h"

class FTexture2DArray : public FTexture {
public:
	FTexture2DArray(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc, const D3D11_SUBRESOURCE_DATA* InitialData = nullptr);
	~FTexture2DArray() override;
	FTexture2DArray(const FTexture2DArray&) = delete;
	FTexture2DArray& operator=(const FTexture2DArray&) = delete;

public:
	ID3D11RenderTargetView* GetRTV(uint32 Slice = 0) const override;
	ID3D11DepthStencilView* GetDSV(uint32 Slice = 0) const override;

private:
	static DXGI_FORMAT GetShaderResourceFormat(DXGI_FORMAT InFormat);
	static DXGI_FORMAT GetDepthStencilFormat(DXGI_FORMAT InFormat);
	bool CreateViews(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc);

private:
	TArray<Microsoft::WRL::ComPtr<ID3D11RenderTargetView>> mRenderTargetViews{};
	TArray<Microsoft::WRL::ComPtr<ID3D11DepthStencilView>> mDepthStencilViews{};
};
