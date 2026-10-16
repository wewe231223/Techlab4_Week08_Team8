#pragma once

#include "Core/Types.h"

class FTexture2D;

class FShadowMapResource {
public:
	FShadowMapResource();
	~FShadowMapResource();
	FShadowMapResource(const FShadowMapResource&) = delete;
	FShadowMapResource& operator=(const FShadowMapResource&) = delete;
	FShadowMapResource(FShadowMapResource&&) noexcept;
	FShadowMapResource& operator=(FShadowMapResource&&) noexcept;

public:
	bool Initialize(int32 Resolution);
	void Reset();
	bool IsValid() const;
	FTexture2D* GetDepthTexture() const;

private:
	TUniquePtr<FTexture2D> mDepthTexture{};
};