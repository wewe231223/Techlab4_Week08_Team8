#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "RenderPacket.h"
#include "ShadowAtlas.h"
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
class FTexture2DArray;
class FPrimitiveSceneProxy;
class ULightComponent;
struct FBox;
struct FRenderingInfo;

class FShadowRenderer {
private:
	using FShadowAtlasTile = FShadowAtlas::FTile;
	using FShadowAtlasAllocation = FShadowAtlas::FAllocation;
	using FShadowRasterizerState = Microsoft::WRL::ComPtr<ID3D11RasterizerState>;
	using FShadowRasterizerStateKey = std::tuple<EShadowDepthBiasPreset, EShadowSlopeScaledDepthBiasPreset, bool>;
	using FShadowRasterizerStateCache = std::map<FShadowRasterizerStateKey, FShadowRasterizerState>;

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
	bool Init();
	bool Render(FScene& Scene, const FShadowCameraContext& CameraContext, int32 AtlasResolution = mDefaultAtlasResolution);
	bool RenderShadowAtlas(FScene& Scene, const TArray<FShadowMapRequest>& Requests, int32 AtlasResolution = mDefaultAtlasResolution);
	void Reset();
	void BindShadowMaps();
	void SetShadowFilterMode(EShadowFilterMode Mode);
	EShadowFilterMode GetShadowFilterMode() const;
	void SetShadowCacheEnabled(bool Enabled);
	bool IsShadowCacheEnabled() const;
	// 그림자 맵에서 이 텍셀 반지름보다 작게 그려질 Caster를 제외한다. 0이면 끈다.
	void SetMinShadowCasterTexelRadius(float TexelRadius);
	float GetMinShadowCasterTexelRadius() const;
	FTexture2DArray* GetShadowDepthTexture() const;
	uint32 GetShadowMapCount() const;
	const TArray<FShadowMapRenderParameters>& GetShadowMapRenderParameters() const;
	const FShadowMapRenderParameters* GetShadowMapRenderParameters(const ULightComponent* LightComponent, uint32 FaceIndex = 0) const;
	int32 GetShadowMapIndex(const ULightComponent* LightComponent) const;
	uint32 GetShadowMapViewCount(const ULightComponent* LightComponent) const;
	// 에디터 미리보기. 요청한 다음 그림자 렌더부터 해당 타일을 2D 텍스처로 그린다. nullptr로 요청하면 멈춘다.
	void RequestShadowPreview(const ULightComponent* LightComponent, uint32 FaceIndex, bool LinearDepth = false);
	FTexture2D* GetShadowPreviewTexture() const;
	void RequestShadowAtlasPreview(int32 PageIndex, bool LinearDepth = false);
	FTexture2D* GetShadowAtlasPreviewTexture(uint32 PageIndex) const;

private:
	static bool IsValidShadowView(const FShadowView& View);
	static bool IsValidShadowRequest(const FShadowMapRequest& Request);
	static int32 NormalizeAtlasResolution(int32 Resolution);
	static TArray<FShadowAtlasAllocation> BuildShadowAtlasAllocations(FShadowAtlas& Atlas, const TArray<FShadowMapRequest>& Requests, uint32 AtlasResolution);
	static bool GatherShadowCasterBounds(const FScene& Scene, FBox& OutBounds);
	bool InitializeShadowPass();
	bool SetShadowRasterizerState(EShadowDepthBiasPreset DepthBiasPreset, EShadowSlopeScaledDepthBiasPreset SlopeScaledDepthBiasPreset, bool CullBackFaces);
	void GatherShadowRequests(const FScene& Scene);
	void ConfigureShadowCache(FScene& Scene, const TArray<FShadowMapRequest>& Requests);
	void BeginShadowPass(const FRenderingInfo& PassInfo);
	void RenderLightShadowMaps(FScene& Scene, const FShadowMapRequest& Request, const FShadowAtlasAllocation& Allocation);
	void UpdateLightShadowMaps(FScene& Scene, const FShadowMapRequest& Request, const FShadowAtlasAllocation& Allocation, FShadowAtlas& Atlas, const FRenderingInfo& PassInfo, bool& PassStarted, uint64 UpdateSerial);
	const FShadowCacheEntry* UpdateShadowFace(FScene& Scene, const FShadowMapRequest& Request, const FShadowAtlasAllocation& Allocation, uint32 FaceIndex, FShadowAtlas& Atlas, const FRenderingInfo& PassInfo, bool& PassStarted, uint64 UpdateSerial);
	void AddShadowMapParameters(const FShadowMapRequest& Request, const FShadowAtlasAllocation& Allocation, const FShadowView* Views);
	bool DrawShadowCasters(const FRenderQueue& Queue, const FShadowView& View, const FShadowAtlasTile& Tile);
	bool InitializeShadowClear();
	void ClearShadowTile(const FShadowAtlasTile& Tile);
	FShadowMapRenderParameters BuildShadowMapRenderParameters(const FShadowView& View, const FLightShadowSettings& Settings, const FShadowAtlasTile& Tile) const;
	bool InitializePreviewPass(TUniquePtr<FTexture2D>& Texture, uint32 Resolution);
	bool RenderPreviewTexture(FTexture2D& Texture, const FVector2& UvMin, const FVector2& UvMax, uint32 PageIndex, bool LinearDepth, const FShadowMapRenderParameters* Parameters = nullptr);
	void RenderShadowPreview();
	void RenderShadowAtlasPreview();

private:
	static constexpr int32 mDefaultAtlasResolution{ 4096 };
	static constexpr int32 mMaxAtlasResolution{ 8192 };
	static constexpr uint32 mTextureSlot{ 13 };
	static constexpr uint32 mBufferSlot{ 14 };
	static constexpr uint32 mSamplerSlot{ 1 };
	static constexpr int32 mMaxAtlasPageCount{ 4 };

	static constexpr uint32 mPreviewResolution{ 512 };
	static constexpr uint32 mAtlasPreviewResolution{ 1024 };

	// Scene의 Atlas가 소유하는 깊이 텍스처의 비소유 참조.
	TWeakPtr<FTexture2DArray> mDepthTexture{};

	//Preview : LightComponent ShadowMap Preview
	TUniquePtr<FTexture2D> mPreviewTexture{};
	TUniquePtr<FConstantBuffer> mPreviewCB{};
	FShaderProgram* mPreviewShader{};
	const ULightComponent* mPreviewLight{}; // 키로만 쓰고 역참조하지 않는다
	uint32 mPreviewFace{};
	bool mPreviewLinearDepth{};
	bool mPreviewReady{};
	TUniquePtr<FTexture2D> mAtlasPreviewTexture{};
	int32 mAtlasPreviewPage{ -1 };
	uint32 mRenderedAtlasPreviewPage{};
	bool mAtlasPreviewLinearDepth{};
	bool mAtlasPreviewReady{};

	TUniquePtr<FStructuredBuffer> mShadowMapBuffer{};
	TUniquePtr<FConstantBuffer> mShadowViewCB{};
	TUniquePtr<FConstantBuffer> mShadowObjectCB{};
	Microsoft::WRL::ComPtr<ID3D11SamplerState> mComparisonSampler{};
	FShadowRasterizerStateCache mShadowRasterizerStates{};
	FShaderProgram* mShadowShader{};
	// 다른 타일을 보존하면서 갱신할 타일만 초기화하는 전용 셰이더와 상태.
	FShaderProgram* mShadowClearShader{};
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> mShadowClearDepthState{};
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> mShadowClearRasterizerState{};
	TArray<FShadowMapRequest> mShadowRequests{};
	// 현재 그림자 맵을 그릴 드로우 패킷.
	FRenderQueue mShadowRenderQueue{};
	TArray<FShadowMapRenderParameters> mShadowMapRenderParameters{};
	TMap<const ULightComponent*, FShadowMapAllocation> mShadowMapAllocations{};
	bool mShadowMapReady{};
	EShadowFilterMode mShadowFilterMode{ EShadowFilterMode::None };
	// OFF에서는 Caster 변경 추적과 재사용 평가를 건너뛴다.
	bool mShadowCacheEnabled{ true };
	// 값이 바뀌면 수집되는 Caster 목록이 달라져 캐시 조건이 다시 그리게 한다.
	float mMinShadowCasterTexelRadius{ 1.0f };
	FShadowCameraContext mCurrentCameraContext{};
};
