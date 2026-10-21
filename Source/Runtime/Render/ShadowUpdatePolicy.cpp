#include "EnginePCH.h"
#include "ShadowUpdatePolicy.h"

#include "Math/EpsilonCompare.h"

#include <algorithm>
#include <functional>

namespace {
	bool CompareCasterPointers(const FShadowCasterReference* Left, const FShadowCasterReference* Right) {
		return std::less<const FPrimitiveSceneProxy*>{}(Left->mProxy, Right->mProxy);
	}

	bool AreEqualShadowCasters(const FShadowCasterReference& Left, const FShadowCasterReference& Right) {
		return Left.mProxy == Right.mProxy && Left.mRevision == Right.mRevision && Left.mLODIndex == Right.mLODIndex;
	}

	bool AreEqualShadowViews(const FShadowView& Current, const FShadowView& Previous) {
		const bool SameMatrices{ EpsilonCompare<FMatrix>{ Current.mView } == EpsilonCompare<FMatrix>{ Previous.mView } && EpsilonCompare<FMatrix>{ Current.mProjection } == EpsilonCompare<FMatrix>{ Previous.mProjection } && EpsilonCompare<FMatrix>{ Current.mViewProjection } == EpsilonCompare<FMatrix>{ Previous.mViewProjection } };
		const bool SamePosition{ EpsilonCompare<FVector>{ Current.mWorldPosition } == EpsilonCompare<FVector>{ Previous.mWorldPosition } };
		const bool SameMetadata{ EpsilonCompare<float>{ Current.mNearPlane } == EpsilonCompare<float>{ Previous.mNearPlane } && Current.mIsOrthographic == Previous.mIsOrthographic && Current.mIsOmnidirectional == Previous.mIsOmnidirectional };
		return SameMatrices && SamePosition && SameMetadata;
	}

	bool AreEqualShadowDepthStates(const FShadowCacheEntry& Current, const FShadowCacheEntry& Previous) {
		return Current.mDepthBiasPreset == Previous.mDepthBiasPreset && Current.mSlopeScaledDepthBiasPreset == Previous.mSlopeScaledDepthBiasPreset && Current.mView.mCullBackFaces == Previous.mView.mCullBackFaces;
	}

	bool AreEqualShadowCasterLists(const TArray<FShadowCasterReference>& Current, const TArray<FShadowCasterReference>& Previous) {
		if (Current.Num() != Previous.Num()) {
			return false;
		}

		bool SameOrder{ true };
		for (uint32 Index{}; Index < static_cast<uint32>(Current.Num()); ++Index) {
			if (Current[Index].mProxy != Previous[Index].mProxy) {
				SameOrder = false;
				break;
			}
			if (!AreEqualShadowCasters(Current[Index], Previous[Index])) {
				return false;
			}
		}

		if (SameOrder) {
			return true;
		}

		TArray<const FShadowCasterReference*> SortedCurrent{};
		TArray<const FShadowCasterReference*> SortedPrevious{};
		SortedCurrent.Reserve(Current.Num());
		SortedPrevious.Reserve(Previous.Num());

		for (const FShadowCasterReference& Caster : Current) {
			SortedCurrent.Add(&Caster);
		}

		for (const FShadowCasterReference& Caster : Previous) {
			SortedPrevious.Add(&Caster);
		}

		std::sort(SortedCurrent.begin(), SortedCurrent.end(), CompareCasterPointers);
		std::sort(SortedPrevious.begin(), SortedPrevious.end(), CompareCasterPointers);

		for (uint32 Index{}; Index < static_cast<uint32>(Current.Num()); ++Index) {
			if (!AreEqualShadowCasters(*SortedCurrent[Index], *SortedPrevious[Index])) {
				return false;
			}
		}

		return true;
	}
}

bool CanReuseShadowMap(const FShadowCacheEntry& Current, const TArray<FShadowCasterReference>& CurrentCasters, const TArray<FShadowCasterReference>& RenderedCasters, const FShadowCacheEntry* Cached) {
	return Cached && Cached->mHasCachedContent && AreEqualShadowViews(Current.mView, Cached->mView) && AreEqualShadowDepthStates(Current, *Cached) && AreEqualShadowCasterLists(CurrentCasters, RenderedCasters);
}
