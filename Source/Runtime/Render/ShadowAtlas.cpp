#include "EnginePCH.h"
#include "ShadowAtlas.h"

#include "RenderCommand.h"
#include "Texture2DArray.h"

#include <algorithm>
#include <bit>
#include <functional>
#include <utility>

FShadowAtlas::FShadowAtlas() = default;

FShadowAtlas::~FShadowAtlas() = default;

void FShadowAtlas::Initialize(uint32 AtlasResolution, uint32 MaxPageCount) {
	if (mAtlasResolution == AtlasResolution && mMaxPageCount == MaxPageCount) {
		return;
	}
	Reset();
	if (AtlasResolution < static_cast<uint32>(MinShadowMapResolution) || AtlasResolution > mMaxAtlasResolution || !std::has_single_bit(AtlasResolution) || MaxPageCount == 0) {
		return;
	}
	mAtlasResolution = AtlasResolution;
	mMaxPageCount = MaxPageCount;
	mLevelCount = static_cast<int32>(std::bit_width(AtlasResolution / static_cast<uint32>(MinShadowMapResolution)));
}

void FShadowAtlas::Reset() {
	mDepthTexture.reset();
	mAllocations.Reset();
	mPages.Reset();
	mAtlasResolution = 0;
	mMaxPageCount = 0;
	mLevelCount = 0;
}

bool FShadowAtlas::UpdateTextureResource() {
	const uint32 PageCount{ GetPageCount() };
	if (PageCount == 0) {
		mDepthTexture.reset();
		return true;
	}
	const uint32 Size{ mAtlasResolution };
	if (mDepthTexture && mDepthTexture->GetRawPtr()
		&& mDepthTexture->GetSRV()
		&& mDepthTexture->GetDSV()
		&& mDepthTexture->GetWidth() == Size
		&& mDepthTexture->GetHeight() == Size
		&& mDepthTexture->GetArraySize() == PageCount) {
		return true;
	}

	D3D11_TEXTURE2D_DESC Desc{};
	Desc.Width = Size;
	Desc.Height = Size;
	Desc.MipLevels = 1;
	Desc.ArraySize = PageCount;
	Desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	// 페이지가 1장이어도 셰이더는 Texture2DArray로 읽으므로 배열 뷰로 만든다.
	TSharedPtr<FTexture2DArray> DepthTexture{ RenderCommand::CreateTexture2DArray(Desc) };
	if (!DepthTexture || !DepthTexture->GetRawPtr() || !DepthTexture->GetDSV() || !DepthTexture->GetSRV()) {
		return false;
	}

	if (mCacheEnabled) {
		// 새 텍스처로 복사한 앞쪽 페이지 수. 이 범위의 캐시 보관 상태만 유지한다.
		uint32 CopiedPageCount{};
		if (mDepthTexture && mDepthTexture->GetRawPtr() && mDepthTexture->GetWidth() == Size && mDepthTexture->GetHeight() == Size && mDepthTexture->GetFormat() == DepthTexture->GetFormat()) {
			CopiedPageCount = std::min(PageCount, mDepthTexture->GetArraySize());
			for (uint32 PageIndex{}; PageIndex < CopiedPageCount; ++PageIndex) {
				const uint32 Subresource{ D3D11CalcSubresource(0, PageIndex, 1) };
				RenderCommand::GetContext()->CopySubresourceRegion(DepthTexture->GetRawPtr(), Subresource, 0, 0, 0, mDepthTexture->GetRawPtr(), Subresource, nullptr);
			}
		}

		UpdateCacheEntries(CopiedPageCount);
	}

	mDepthTexture = std::move(DepthTexture);
	return true;
}

TWeakPtr<FTexture2DArray> FShadowAtlas::GetDepthTexture() const {
	return mDepthTexture;
}

TArray<FShadowAtlas::FAllocation> FShadowAtlas::UpdateAllocations(const TArray<FRequest>& Requests) {
	TArray<FAllocation> Allocations{};
	Allocations.SetNum(Requests.Num());
	TMap<const ULightComponent*, uint32> RequestIndices{};
	TArray<uint32> SortedIndices{};
	for (uint32 Index{}; Index < static_cast<uint32>(Requests.Num()); ++Index) {
		const FRequest& Request{ Requests[Index] };
		if (!IsValidRequest(Request) || RequestIndices.Contains(Request.mLightComponent)) {
			continue;
		}
		RequestIndices.Add(Request.mLightComponent, Index);
		SortedIndices.Add(Index);
	}

	TArray<const ULightComponent*> ReleasedLights{};
	for (const auto& AllocationEntry : mAllocations) {
		const ULightComponent* LightComponent{ AllocationEntry.first };
		const uint32* RequestIndex{ RequestIndices.Find(LightComponent) };
		if (!RequestIndex || AllocationEntry.second.mReallocationRequested || HasLightChanged(Requests[*RequestIndex])) {
			ReleasedLights.Add(LightComponent);
		}
	}
	for (const ULightComponent* LightComponent : ReleasedLights) {
		Free(LightComponent);
	}

	std::sort(SortedIndices.begin(), SortedIndices.end(), [&Requests](uint32 Left, uint32 Right) {
		return Requests[Left].mResolution != Requests[Right].mResolution ? Requests[Left].mResolution > Requests[Right].mResolution : Left < Right;
	});

	for (uint32 Index : SortedIndices) {
		const FRequest& Request{ Requests[Index] };
		if (const FCachedAllocation* CachedAllocation{ mAllocations.Find(Request.mLightComponent) }) {
			Allocations[Index] = CachedAllocation->mAllocation;
			continue;
		}
		FAllocation Allocation{};
		if (!Allocate(Request, Allocation)) {
			continue;
		}
		Allocations[Index] = Allocation;
		FCachedAllocation CachedAllocation{ Allocation, Request.mResolution };
		mAllocations.Add(Request.mLightComponent, std::move(CachedAllocation));
	}
	// 어떤 페이지도 쓰지 않는 상태가 이어지면 아틀라스를 통째로 해제한다.
	const uint32 KeepPageCount{ UpdateAtlasPageUsage() };
	if (KeepPageCount < GetPageCount()) {
		mPages.SetNum(static_cast<int32>(KeepPageCount));
	}
	if (KeepPageCount == 0) {
		mDepthTexture.reset();
	}
	return Allocations;
}

const FShadowAtlas::FAllocation* FShadowAtlas::GetAllocation(const ULightComponent* LightComponent) const {
	const FCachedAllocation* CachedAllocation{ mAllocations.Find(LightComponent) };
	return CachedAllocation ? &CachedAllocation->mAllocation : nullptr;
}

void FShadowAtlas::RequestReallocation(const ULightComponent* LightComponent) {
	if (FCachedAllocation* CachedAllocation{ mAllocations.Find(LightComponent) }) {
		CachedAllocation->mReallocationRequested = true;
	}
}

void FShadowAtlas::Free(const ULightComponent* LightComponent) {
	FCachedAllocation CachedAllocation{};
	if (mAllocations.RemoveAndCopyValue(LightComponent, CachedAllocation)) {
		FreeAllocation(CachedAllocation.mAllocation);
	}
}

uint32 FShadowAtlas::GetPageCount() const {
	// 지금 쓰는 페이지와, 쓰지 않게 된 지 오래되지 않은 페이지까지 남긴 층 수.
	return static_cast<uint32>(mPages.Num());
}

bool FShadowAtlas::HasLightChanged(const FRequest& Request) const {
	const FCachedAllocation* CachedAllocation{ mAllocations.Find(Request.mLightComponent) };
	return !CachedAllocation || CachedAllocation->mRequestedResolution != Request.mResolution || CachedAllocation->mAllocation.mViewCount != Request.mViewCount;
}

bool FShadowAtlas::IsValidRequest(const FRequest& Request) const {
	// 아틀라스보다 크면 들어갈 수 없으므로 건너뛴다.
	return Request.mLightComponent && mAtlasResolution != 0 && Request.mResolution >= static_cast<uint32>(MinShadowMapResolution) && Request.mResolution <= mAtlasResolution && std::has_single_bit(Request.mResolution) && Request.mViewCount > 0 && Request.mViewCount <= PointShadowFaceCount;
}

int32 FShadowAtlas::GetTargetLevel(const FRequest& Request) const {
	// 이 해상도의 타일이 놓일 레벨. 둘 다 2의 거듭제곱이므로 log2(mAtlasResolution / Request.mResolution)와 같다.
	int32 TargetLevel{ static_cast<int32>(std::countr_zero(mAtlasResolution / Request.mResolution)) };
	// 빈 페이지 한 장에 들어가는 타일 수는 4^TargetLevel이다. 이보다 면이 많으면 새 페이지를 열어도 들어가지 않으므로
	// 해상도를 반씩 낮춰 한 페이지에 모든 면이 들어가게 한다. (예: 4096 아틀라스의 2048 포인트 라이트 6면 -> 1024)
	// 여러 면을 쓰는 건 포인트 라이트뿐이고, 포인트 라이트 뷰는 해상도와 무관하므로 타일 크기만 줄이면 된다.
	while ((1u << (2 * TargetLevel)) < Request.mViewCount && TargetLevel + 1 < mLevelCount) {
		++TargetLevel;
	}
	return (1u << (2 * TargetLevel)) >= Request.mViewCount ? TargetLevel : -1;
}

uint32 FShadowAtlas::CountAvailableTiles(const FPage& Page, int32 TargetLevel) const {
	// 남은 빈 공간을 TargetLevel 크기 타일 개수로 환산한다.
	// 한 레벨 내려갈 때마다 4개로 쪼개지므로 레벨 L의 빈 타일 하나는 4^(TargetLevel - L)개에 해당한다.
	uint32 AvailableTileCount{};
	for (int32 Level{}; Level <= TargetLevel; ++Level) {
		AvailableTileCount += static_cast<uint32>(Page.mFreeTiles[Level].Num()) * (1u << (2 * (TargetLevel - Level)));
	}
	return AvailableTileCount;
}

bool FShadowAtlas::Allocate(const FRequest& Request, FAllocation& OutAllocation) {
	const int32 TargetLevel{ GetTargetLevel(Request) };
	if (TargetLevel < 0) {
		return false;
	}
	//첫 페이지부터 읽을 수 있는지 확인
	int32 PageIndex{ -1 };
	for (int32 Index{}; Index < mPages.Num(); ++Index) {
		if (CountAvailableTiles(mPages[Index], TargetLevel) >= Request.mViewCount) {
			PageIndex = Index;
			break;
		}
	}
	//어느 페이지에도 할당할 수 없다면 어떻게 처리해야 할까 -> mPages를 하나 늘려서 거기다 저장
	if (PageIndex < 0) {
		if (GetPageCount() >= mMaxPageCount) {
			return false;
		}
		//페이지 추가
		FPage& Page{ mPages.AddDefaulted_GetRef() };
		Page.mFreeTiles.SetNum(mLevelCount);
		// 페이지의 0레벨 초기화
		Page.mFreeTiles[0].Add(FTile{ 0, 0, mAtlasResolution });
		PageIndex = mPages.Num() - 1;
	}

	FAllocation Allocation{};
	Allocation.mPageIndex = static_cast<uint32>(PageIndex);
	Allocation.mViewCount = Request.mViewCount;
	FPage& Page{ mPages[PageIndex] };
	for (uint32 FaceIndex{}; FaceIndex < Request.mViewCount; ++FaceIndex) {
		// TargetLevel부터 위(더 큰 타일) 방향으로 올라가며 빈 타일이 있는 가장 작은 레벨을 찾는다.
		// 위의 개수 확인을 통과했으므로 레벨 0을 넘어가기 전에 반드시 찾는다.
		int32 AvailableLevel{ TargetLevel };
		while (AvailableLevel > 0 && Page.mFreeTiles[AvailableLevel].IsEmpty()) {
			--AvailableLevel;
		}
		// 가장 최근에 추가된 타일을 꺼낸다. 방금 쪼갠 타일의 형제부터 쓰게 되어 배치가 한쪽으로 모인다.
		FTile Tile{ Page.mFreeTiles[AvailableLevel].Last() };
		Page.mFreeTiles[AvailableLevel].RemoveLast();
		// 꺼낸 타일이 필요한 크기보다 크면 TargetLevel이 될 때까지 4등분한다.
		// 왼쪽 위 사분면은 계속 쪼갤 타일로 남기고, 나머지 세 사분면은 한 단계 아래 레벨의 빈 타일로 돌려준다.
		while (AvailableLevel < TargetLevel) {
			Tile.mResolution /= 2;
			++AvailableLevel;
			Page.mFreeTiles[AvailableLevel].Add(FTile{ Tile.mX + Tile.mResolution, Tile.mY + Tile.mResolution, Tile.mResolution }); // 오른쪽 아래
			Page.mFreeTiles[AvailableLevel].Add(FTile{ Tile.mX, Tile.mY + Tile.mResolution, Tile.mResolution }); // 왼쪽 아래
			Page.mFreeTiles[AvailableLevel].Add(FTile{ Tile.mX + Tile.mResolution, Tile.mY, Tile.mResolution }); // 오른쪽 위
		}
		// 남은 왼쪽 위 사분면이 정확히 요청 크기이므로 이 면에 할당한다.
		Allocation.mTiles[FaceIndex] = Tile;
	}
	Page.mAllocatedTileCount += Request.mViewCount;
	OutAllocation = Allocation;
	return true;
}

void FShadowAtlas::FreeAllocation(const FAllocation& Allocation) {
	mPages[Allocation.mPageIndex].mAllocatedTileCount -= Allocation.mViewCount;
	for (uint32 FaceIndex{}; FaceIndex < Allocation.mViewCount; ++FaceIndex) {
		FreeTile(Allocation.mPageIndex, Allocation.mTiles[FaceIndex]);
	}
}

void FShadowAtlas::FreeTile(uint32 PageIndex, const FTile& FreedTile) {
	FPage& Page{ mPages[PageIndex] };
	FTile Tile{ FreedTile };
	int32 Level{ static_cast<int32>(std::countr_zero(mAtlasResolution / Tile.mResolution)) };
	Page.mFreeTiles[Level].Add(Tile);
	while (Level > 0) {
		const uint32 ParentResolution{ Tile.mResolution * 2 };
		const FTile Parent{ Tile.mX / ParentResolution * ParentResolution, Tile.mY / ParentResolution * ParentResolution, ParentResolution };
		TArray<FTile>& FreeTiles{ Page.mFreeTiles[Level] };
		int32 SiblingIndices[4]{ -1, -1, -1, -1 };
		uint32 SiblingCount{};
		for (int32 Index{}; Index < FreeTiles.Num() && SiblingCount < 4; ++Index) {
			const FTile& Sibling{ FreeTiles[Index] };
			if (Sibling.mX / ParentResolution == Parent.mX / ParentResolution && Sibling.mY / ParentResolution == Parent.mY / ParentResolution) {
				SiblingIndices[SiblingCount++] = Index;
			}
		}
		if (SiblingCount != 4) {
			break;
		}
		std::sort(std::begin(SiblingIndices), std::end(SiblingIndices), std::greater<int32>{});
		for (int32 Index : SiblingIndices) {
			FreeTiles.RemoveAtSwap(static_cast<uint32>(Index));
		}
		Tile = Parent;
		--Level;
		Page.mFreeTiles[Level].Add(Tile);
	}
}

uint32 FShadowAtlas::UpdateAtlasPageUsage() {
	// 페이지(층)마다 연속으로 쓰이지 않은 렌더 횟수를 센다.
	// 기존 타일을 유지하므로 페이지별 실제 할당 여부로 이번에 쓰는 페이지를 판단한다.
	for (FPage& Page : mPages) {
		Page.mIdleCount = Page.mAllocatedTileCount != 0 ? 0 : std::min(Page.mIdleCount + 1, mAtlasPageIdleDelay);
	}

	// 지금 있는 층 중 쓰지 않게 된 지 mAtlasPageIdleDelay번이 안 된 페이지는 남겨 둔다.
	// Texture2DArray는 층 하나만 해제할 수 없으므로, 남길 페이지 중 가장 뒤 번호까지를 새 층 수로 쓴다.
	uint32 KeepPageCount{};
	for (uint32 PageIndex{}; PageIndex < GetPageCount(); ++PageIndex) {
		const FPage& Page{ mPages[PageIndex] };
		if (Page.mAllocatedTileCount != 0 || Page.mIdleCount < mAtlasPageIdleDelay) {
			KeepPageCount = PageIndex + 1;
		}
	}
	return KeepPageCount;
}

void FShadowAtlas::SetCacheEnabled(bool Enabled) {
	if (mCacheEnabled == Enabled) {
		return;
	}

	mCacheEnabled = Enabled;

	for (auto& AllocationEntry : mAllocations) {
		FCachedAllocation& Allocation{ AllocationEntry.second };
		for (FShadowCacheEntry& Entry : Allocation.mCacheEntries) {
			Entry = FShadowCacheEntry{};
		}
	}
}

bool FShadowAtlas::IsCacheEnabled() const {
	return mCacheEnabled;
}

const FShadowCacheEntry* FShadowAtlas::GetCacheEntry(const ULightComponent* LightComponent, uint32 FaceIndex) const {
	if (!mCacheEnabled) {
		return nullptr;
	}

	const FCachedAllocation* CachedAllocation{ mAllocations.Find(LightComponent) };
	return CachedAllocation && FaceIndex < CachedAllocation->mAllocation.mViewCount ? &CachedAllocation->mCacheEntries[FaceIndex] : nullptr;
}

void FShadowAtlas::StoreCacheEntry(const ULightComponent* LightComponent, uint32 FaceIndex, FShadowCacheEntry Entry) {
	const TSharedPtr<FTexture2DArray> DepthTexture{ GetDepthTexture().lock() };
	FCachedAllocation* CachedAllocation{ mAllocations.Find(LightComponent) };
	if (!mCacheEnabled || !CachedAllocation || FaceIndex >= CachedAllocation->mAllocation.mViewCount || !DepthTexture || !DepthTexture->GetRawPtr()) {
		return;
	}
	Entry.mHasCachedContent = true;
	CachedAllocation->mCacheEntries[FaceIndex] = std::move(Entry);
}

void FShadowAtlas::InvalidateCacheEntry(const ULightComponent* LightComponent, uint32 FaceIndex) {
	FCachedAllocation* CachedAllocation{ mAllocations.Find(LightComponent) };
	if (CachedAllocation && FaceIndex < CachedAllocation->mAllocation.mViewCount) {
		CachedAllocation->mCacheEntries[FaceIndex].mHasCachedContent = false;
	}
}

void FShadowAtlas::InvalidatePageCacheEntries(uint32 PageIndex) {
	for (auto& AllocationEntry : mAllocations) {
		FCachedAllocation& CachedAllocation{ AllocationEntry.second };
		if (CachedAllocation.mAllocation.mPageIndex != PageIndex) {
			continue;
		}
		for (uint32 FaceIndex{}; FaceIndex < CachedAllocation.mAllocation.mViewCount; ++FaceIndex) {
			CachedAllocation.mCacheEntries[FaceIndex].mHasCachedContent = false;
		}
	}
}

void FShadowAtlas::UpdateCacheEntries(uint32 CopiedPageCount) {
	for (auto& AllocationEntry : mAllocations) {
		FCachedAllocation& CachedAllocation{ AllocationEntry.second };
		for (uint32 FaceIndex{}; FaceIndex < CachedAllocation.mAllocation.mViewCount; ++FaceIndex) {
			FShadowCacheEntry& Entry{ CachedAllocation.mCacheEntries[FaceIndex] };
			Entry.mHasCachedContent = Entry.mHasCachedContent && CachedAllocation.mAllocation.mPageIndex < CopiedPageCount;
		}
	}
}
