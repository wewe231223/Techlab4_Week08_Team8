#include "EnginePCH.h"
#include "ShadowInvalidationCondition.h"

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
}

IShadowInvalidationCondition::IShadowInvalidationCondition() = default;

IShadowInvalidationCondition::~IShadowInvalidationCondition() = default;

bool FShadowViewCondition::Evaluate(const FShadowUpdateContext& Context, const FShadowCacheEntry* Cached) const {
	if (!Context.mCurrentInput || !Cached) {
		return false;
	}

	const FShadowView& Current{ Context.mCurrentInput->mView };
	const FShadowView& Previous{ Cached->mView };

	const EpsilonCompare<FMatrix> CurrentView{ Current.mView };
	const EpsilonCompare<FMatrix> PreviousView{ Previous.mView };

	const EpsilonCompare<FMatrix> CurrentProjection{ Current.mProjection };
	const EpsilonCompare<FMatrix> PreviousProjection{ Previous.mProjection };

	const EpsilonCompare<FMatrix> CurrentViewProjection{ Current.mViewProjection };
	const EpsilonCompare<FMatrix> PreviousViewProjection{ Previous.mViewProjection };

	const EpsilonCompare<FVector> CurrentPosition{ Current.mWorldPosition };
	const EpsilonCompare<FVector> PreviousPosition{ Previous.mWorldPosition };

	const EpsilonCompare<float> CurrentNearPlane{ Current.mNearPlane };
	const EpsilonCompare<float> PreviousNearPlane{ Previous.mNearPlane };

	const bool SameMatrices{ CurrentView == PreviousView && CurrentProjection == PreviousProjection && CurrentViewProjection == PreviousViewProjection };
	const bool SamePosition{ CurrentPosition == PreviousPosition };
	const bool SameMetadata{ CurrentNearPlane == PreviousNearPlane && Current.mIsOrthographic == Previous.mIsOrthographic && Current.mIsOmnidirectional == Previous.mIsOmnidirectional };

	return SameMatrices && SamePosition && SameMetadata;
}

bool FShadowCasterCondition::Evaluate(const FShadowUpdateContext& Context, const FShadowCacheEntry* Cached) const {
	if (!Context.mCurrentInput || !Cached || !Context.mCurrentCasters || !Context.mRenderedCasters) {
		return false;
	}

	const TArray<FShadowCasterReference>& Current{ *Context.mCurrentCasters };
	const TArray<FShadowCasterReference>& Previous{ *Context.mRenderedCasters };

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

bool FShadowDepthStateCondition::Evaluate(const FShadowUpdateContext& Context, const FShadowCacheEntry* Cached) const {
	if (!Context.mCurrentInput || !Cached) {
		return false;
	}

	const FShadowCacheEntry& Current{ *Context.mCurrentInput };
	const bool Changed{ Current.mDepthBiasPreset != Cached->mDepthBiasPreset || Current.mSlopeScaledDepthBiasPreset != Cached->mSlopeScaledDepthBiasPreset || Current.mView.mCullBackFaces != Cached->mView.mCullBackFaces };

	return !Changed;
}
