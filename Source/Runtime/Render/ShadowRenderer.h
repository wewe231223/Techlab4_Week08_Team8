#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "RenderPacket.h"
#include "ShadowView.h"

#include <d3d11.h>
#include <map>
#include <tuple>
#include <wrl/client.h>

class FConstantBuffer;
class FScene;
class FShaderProgram;
class FStructuredBuffer;
class FTexture2D;
class FPrimitiveSceneProxy;
class ULightComponent;
class FVarianceShadowMap;
struct FBox;

class FShadowRenderer {
private:
	using FShadowRasterizerState = Microsoft::WRL::ComPtr<ID3D11RasterizerState>;
	using FShadowRasterizerStateKey = std::tuple<EShadowDepthBiasPreset, EShadowSlopeScaledDepthBiasPreset, bool>;
	using FShadowRasterizerStateCache = std::map<FShadowRasterizerStateKey, FShadowRasterizerState>;

	struct FShadowAtlasTile {
		uint32 mX{};
		uint32 mY{};
		uint32 mResolution{};
	};

	struct FShadowAtlasAllocation {
		FShadowAtlasTile mTiles[PointShadowFaceCount]{};
		uint32 mViewCount{};
	};

	struct FShadowMapAllocation {
		int32 mFirstIndex{ InvalidShadowMapIndex };
		uint32 mViewCount{};
	};

public:
	FShadowRenderer();
	~FShadowRenderer();
	FShadowRenderer(const FShadowRenderer&) = delete;
	FShadowRenderer& operator=(const FShadowRenderer&) = delete;

public:
	bool Render(FScene& Scene, int32 AtlasResolution = mDefaultAtlasResolution);
	bool RenderShadowAtlas(FScene& Scene, const TArray<FShadowMapRequest>& Requests, int32 AtlasResolution = mDefaultAtlasResolution);
	void Reset();
	void BindShadowMaps();
	void SetShadowFilterMode(EShadowFilterMode Mode);
	EShadowFilterMode GetShadowFilterMode() const;
	FTexture2D* GetShadowDepthTexture() const;
	uint32 GetShadowMapCount() const;
	const TArray<FShadowMapRenderParameters>& GetShadowMapRenderParameters() const;
	const FShadowMapRenderParameters* GetShadowMapRenderParameters(const ULightComponent* LightComponent, uint32 FaceIndex = 0) const;
	int32 GetShadowMapIndex(const ULightComponent* LightComponent) const;
	uint32 GetShadowMapViewCount(const ULightComponent* LightComponent) const;

private:
	static bool IsValidShadowView(const FShadowView& View);
	static bool IsValidShadowRequest(const FShadowMapRequest& Request);
	static bool IsShadowCaster(const FPrimitiveSceneProxy* Proxy);
	static int32 NormalizeAtlasResolution(int32 Resolution);
	static TArray<FShadowAtlasAllocation> BuildShadowAtlasAllocations(const TArray<FShadowMapRequest>& Requests, uint32 AtlasResolution);
	static bool GatherShadowCasterBounds(const FScene& Scene, FBox& OutBounds);
	static void GatherShadowCasters(FScene& Scene, FRenderQueue& OutQueue, const FShadowView& View, int32 Resolution);
	bool InitializeShadowPass();
	bool InitializeAtlasResource(int32 Resolution);
	bool InitializeComparisonSampler();
	bool SetShadowRasterizerState(EShadowDepthBiasPreset DepthBiasPreset, EShadowSlopeScaledDepthBiasPreset SlopeScaledDepthBiasPreset, bool CullBackFaces);
	void GatherShadowRequests(const FScene& Scene);
	void DrawShadowCasters(FScene& Scene, const FShadowView& View, const FShadowAtlasTile& Tile);
	FShadowMapRenderParameters BuildShadowMapRenderParameters(const FShadowView& View, const FLightShadowSettings& Settings, const FShadowAtlasTile& Tile) const;

private:
	static constexpr int32 mDefaultAtlasResolution{ 8192 };
	static constexpr int32 mMaxAtlasResolution{ 8192 };
	static constexpr uint32 mTextureSlot{ 13 };
	static constexpr uint32 mBufferSlot{ 14 };
	static constexpr uint32 mSamplerSlot{ 1 };
	TUniquePtr<FTexture2D> mDepthTexture{};
	TUniquePtr<FVarianceShadowMap> mVarianceShadowMap{};
	TUniquePtr<FStructuredBuffer> mShadowMapBuffer{};
	TUniquePtr<FConstantBuffer> mShadowViewCB{};
	TUniquePtr<FConstantBuffer> mShadowObjectCB{};
	Microsoft::WRL::ComPtr<ID3D11SamplerState> mComparisonSampler{};
	FShadowRasterizerStateCache mShadowRasterizerStates{};
	FShaderProgram* mShadowShader{};
	TArray<FShadowMapRequest> mShadowRequests{};
	FRenderQueue mShadowRenderQueue{};
	TArray<FShadowMapRenderParameters> mShadowMapRenderParameters{};
	TMap<const ULightComponent*, FShadowMapAllocation> mShadowMapAllocations{};
	bool mShadowMapReady{};
	EShadowFilterMode mShadowFilterMode{ EShadowFilterMode::None };
};
