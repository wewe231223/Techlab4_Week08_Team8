#include "EnginePCH.h"
#include "ShadowMapResource.h"
#include "ShadowMap.h"
#include "RenderCommand.h"
#include "Texture2D.h"

#include <algorithm>

FShadowMapResource::FShadowMapResource() {
}

FShadowMapResource::~FShadowMapResource() {
}

FShadowMapResource::FShadowMapResource(FShadowMapResource&&) noexcept = default;

FShadowMapResource& FShadowMapResource::operator=(FShadowMapResource&&) noexcept = default;

bool FShadowMapResource::Initialize(int32 Resolution) {
	const uint32 Size{ static_cast<uint32>(std::clamp(Resolution, MinShadowMapResolution, MaxShadowMapResolution)) };
	if (IsValid() && mDepthTexture->GetWidth() == Size && mDepthTexture->GetHeight() == Size) {
		return true;
	}

	D3D11_TEXTURE2D_DESC Desc{};
	Desc.Width = Size;
	Desc.Height = Size;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	TUniquePtr<FTexture2D> DepthTexture{ RenderCommand::CreateTexture2D(Desc) };
	if (!DepthTexture || !DepthTexture->GetRawPtr() || !DepthTexture->GetDSV() || !DepthTexture->GetSRV()) {
		return false;
	}

	mDepthTexture = std::move(DepthTexture);
	return true;
}

void FShadowMapResource::Reset() {
	mDepthTexture.reset();
}

bool FShadowMapResource::IsValid() const {
	return mDepthTexture && mDepthTexture->GetRawPtr() && mDepthTexture->GetSRV() && mDepthTexture->GetDSV();
}

FTexture2D* FShadowMapResource::GetDepthTexture() const {
	return mDepthTexture.get();
}