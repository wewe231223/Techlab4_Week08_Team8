#pragma once

#include "Containers/Array.h"
#include "ShadowView.h"

struct FShadowCasterState {
	uint32 mComponentId{};
	uint32 mMeshId{};
	FMatrix mLocalToWorld{ FMatrix::Identity };
	uint32 mLODIndex{};
	uint32 mStartIndex{};
	uint32 mIndexCount{};
};

struct FShadowCacheEntry {
	FShadowView mView{};
	EShadowDepthBiasPreset mDepthBiasPreset{ EShadowDepthBiasPreset::Medium };
	EShadowSlopeScaledDepthBiasPreset mSlopeScaledDepthBiasPreset{ EShadowSlopeScaledDepthBiasPreset::Medium };
	TArray<FShadowCasterState> mCasters{};
	uint64 mResourceGeneration{};
	uint64 mAllocationGeneration{};
	uint32 mPageIndex{};
	uint32 mTileX{};
	uint32 mTileY{};
	uint32 mResolution{};
	bool mIsValid{};
};
