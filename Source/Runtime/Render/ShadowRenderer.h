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
		uint32 mPageIndex{};
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
	void SetShadowPcfEnabled(bool Enabled);
	bool IsShadowPcfEnabled() const;
	FTexture2D* GetShadowDepthTexture() const;
	uint32 GetShadowMapCount() const;
	const TArray<FShadowMapRenderParameters>& GetShadowMapRenderParameters() const;
	const FShadowMapRenderParameters* GetShadowMapRenderParameters(const ULightComponent* LightComponent, uint32 FaceIndex = 0) const;
	int32 GetShadowMapIndex(const ULightComponent* LightComponent) const;
	uint32 GetShadowMapViewCount(const ULightComponent* LightComponent) const;
	// 에디터 미리보기. 요청한 다음 그림자 렌더부터 해당 타일을 2D 텍스처로 그린다. nullptr로 요청하면 멈춘다.
	void RequestShadowPreview(const ULightComponent* LightComponent, uint32 FaceIndex);
	FTexture2D* GetShadowPreviewTexture() const;

private:
	static bool IsValidShadowView(const FShadowView& View);
	static bool IsValidShadowRequest(const FShadowMapRequest& Request);
	static bool IsShadowCaster(const FPrimitiveSceneProxy* Proxy);
	static int32 NormalizeAtlasResolution(int32 Resolution);
	static TArray<FShadowAtlasAllocation> BuildShadowAtlasAllocations(const TArray<FShadowMapRequest>& Requests, uint32 AtlasResolution, uint32& OutPageCount);
	static bool GatherShadowCasterBounds(const FScene& Scene, FBox& OutBounds);
	static void GatherShadowCasters(FScene& Scene, FRenderQueue& OutQueue, const FShadowView& View, int32 Resolution);
	bool InitializeShadowPass();
	bool InitializeAtlasResource(int32 Resolution, int32 PageCount);
	// 이번에 쓰는 페이지 수를 기록하고, 아틀라스에 남겨 둘 페이지 수를 돌려준다. 0이면 아틀라스를 해제해도 된다.
	uint32 UpdateAtlasPageUsage(uint32 UsedPageCount);
	bool InitializeComparisonSampler();
	bool SetShadowRasterizerState(EShadowDepthBiasPreset DepthBiasPreset, EShadowSlopeScaledDepthBiasPreset SlopeScaledDepthBiasPreset, bool CullBackFaces);
	void GatherShadowRequests(const FScene& Scene);
	void DrawShadowCasters(FScene& Scene, const FShadowView& View, const FShadowAtlasTile& Tile);
	FShadowMapRenderParameters BuildShadowMapRenderParameters(const FShadowView& View, const FLightShadowSettings& Settings, const FShadowAtlasTile& Tile) const;
	bool InitializePreviewPass();
	void RenderShadowPreview();

private:
	static constexpr int32 mDefaultAtlasResolution{ 4096 };
	static constexpr int32 mMaxAtlasResolution{ 8192 };
	static constexpr uint32 mTextureSlot{ 13 };
	static constexpr uint32 mBufferSlot{ 14 };
	static constexpr uint32 mSamplerSlot{ 1 };
	static constexpr int32 mMaxAtlasPageCount = 4;

	static constexpr uint32 mPreviewResolution{ 512 };
	// 쓰지 않는 아틀라스 페이지를 해제하기까지 기다리는 그림자 렌더 횟수. 뷰포트마다 한 번씩 세므로 뷰포트가 많으면 더 빨리 해제된다.
	static constexpr uint32 mAtlasPageIdleDelay{ 120 };

	TUniquePtr<FTexture2D> mDepthTexture{};

	//Preview : LightComponent ShadowMap Preview
	TUniquePtr<FTexture2D> mPreviewTexture{};
	TUniquePtr<FConstantBuffer> mPreviewCB{};
	FShaderProgram* mPreviewShader{};
	const ULightComponent* mPreviewLight{}; // 키로만 쓰고 역참조하지 않는다
	uint32 mPreviewFace{};
	bool mPreviewReady{};

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
	bool mShadowPcfEnabled{};
	uint32 mAtlasPageIdleCounts[mMaxAtlasPageCount]{}; // 페이지별로 연속해서 쓰이지 않은 그림자 렌더 횟수
};
