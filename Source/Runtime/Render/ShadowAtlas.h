#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "ShadowMap.h"
#include "ShadowCacheEntry.h"

class FTexture2DArray;
class ULightComponent;

class FShadowAtlas {
public:
	// 그림자 맵 한 면이 차지하는 아틀라스 영역. 좌표와 해상도는 텍셀 단위다.
	struct FTile {
		uint32 mX{};
		uint32 mY{};
		uint32 mResolution{};
	};

	// 광원 하나의 페이지 번호와 면별 실제 타일 배치.
	struct FAllocation {
		FTile mTiles[PointShadowFaceCount]{};
		uint32 mViewCount{};
		uint32 mPageIndex{};
	};

	struct FRequest {
		const ULightComponent* mLightComponent{};
		uint32 mResolution{};
		uint32 mViewCount{};
	};

private:
	//FPage = 아틀라스 페이지
	struct FPage {
		TArray<TArray<FTile>> mFreeTiles{};
		uint32 mAllocatedTileCount{};
		uint32 mIdleCount{}; // 페이지별로 연속해서 쓰이지 않은 그림자 렌더 횟수
	};

	// 광원별 실제 배치와 각 Face의 마지막 렌더 입력을 함께 보관한다.
	struct FCachedAllocation {
		FAllocation mAllocation{};
		// 할당된 타일 해상도와 달라질 수 있는 원래 요청 해상도.
		uint32 mRequestedResolution{};
		bool mReallocationRequested{};
		FShadowCacheEntry mCacheEntries[PointShadowFaceCount]{};
	};

public:
	FShadowAtlas();
	~FShadowAtlas();
	FShadowAtlas(const FShadowAtlas&) = delete;
	FShadowAtlas& operator=(const FShadowAtlas&) = delete;

public:
	void Initialize(uint32 AtlasResolution, uint32 MaxPageCount);
	void Reset();
	bool UpdateTextureResource();
	TWeakPtr<FTexture2DArray> GetDepthTexture() const;
	TArray<FAllocation> UpdateAllocations(const TArray<FRequest>& Requests);
	const FAllocation* GetAllocation(const ULightComponent* LightComponent) const;
	void RequestReallocation(const ULightComponent* LightComponent);
	void Free(const ULightComponent* LightComponent);
	uint32 GetPageCount() const;
	bool HasLightChanged(const FRequest& Request) const;
	void SetCacheEnabled(bool Enabled);
	bool IsCacheEnabled() const;
	const FShadowCacheEntry* GetCacheEntry(const ULightComponent* LightComponent, uint32 FaceIndex = 0) const;
	void StoreCacheEntry(const ULightComponent* LightComponent, uint32 FaceIndex, FShadowCacheEntry Entry);
	void InvalidateCacheEntry(const ULightComponent* LightComponent, uint32 FaceIndex);
	void InvalidatePageCacheEntries(uint32 PageIndex);

private:
	void UpdateCacheEntries(uint32 CopiedPageCount);
	bool IsValidRequest(const FRequest& Request) const;
	int32 GetTargetLevel(const FRequest& Request) const;
	uint32 CountAvailableTiles(const FPage& Page, int32 TargetLevel) const;
	bool Allocate(const FRequest& Request, FAllocation& OutAllocation);
	void FreeAllocation(const FAllocation& Allocation);
	void FreeTile(uint32 PageIndex, const FTile& FreedTile);
	// 페이지별 사용 여부를 기록하고, 아틀라스에 남겨 둘 페이지 수를 돌려준다. 0이면 아틀라스를 해제해도 된다.
	uint32 UpdateAtlasPageUsage();

private:
	static constexpr uint32 mMaxAtlasResolution{ 8192 };
	// 쓰지 않는 아틀라스 페이지를 해제하기까지 기다리는 그림자 렌더 횟수. 뷰포트마다 한 번씩 세므로 뷰포트가 많으면 더 빨리 해제된다.
	static constexpr uint32 mAtlasPageIdleDelay{ 120 };

	uint32 mAtlasResolution{};
	uint32 mMaxPageCount{};
	int32 mLevelCount{};
	TArray<FPage> mPages{};
	TMap<const ULightComponent*, FCachedAllocation> mAllocations{};
	// 이전 깊이 결과를 페이지별로 보존하는 텍스처 배열.
	TSharedPtr<FTexture2DArray> mDepthTexture{};
	bool mCacheEnabled{ true };
};
