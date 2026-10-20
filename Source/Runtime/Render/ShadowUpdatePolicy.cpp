#include "EnginePCH.h"
#include "ShadowUpdatePolicy.h"

#include <utility>

FShadowUpdatePolicy::FShadowUpdatePolicy() {
	AddCondition(MakeUnique<FShadowViewCondition>());
	AddCondition(MakeUnique<FShadowDepthStateCondition>());
	AddCondition(MakeUnique<FShadowCasterCondition>());
}

FShadowUpdatePolicy::~FShadowUpdatePolicy() = default;

void FShadowUpdatePolicy::AddCondition(TUniquePtr<IShadowInvalidationCondition> Condition) {
	if (Condition) {
		mConditions.Add(std::move(Condition));
	}
}

bool FShadowUpdatePolicy::Evaluate(const FShadowUpdateContext& Context, const FShadowCacheEntry* Cached) const {
	if (!Cached || !Cached->mHasCachedContent || !Context.mCurrentInput) {
		return false;
	}

	for (const TUniquePtr<IShadowInvalidationCondition>& Condition : mConditions) {
		if (!Condition->Evaluate(Context, Cached)) {
			return false;
		}
	}

	return true;
}
