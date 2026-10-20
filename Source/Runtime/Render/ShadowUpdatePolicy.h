#pragma once

#include "Containers/Array.h"
#include "ShadowInvalidationCondition.h"

// 캐시 내용이 있고 등록된 모든 조건이 통과하면 재사용을 허용한다.
class FShadowUpdatePolicy {
public:
	FShadowUpdatePolicy();
	~FShadowUpdatePolicy();
	FShadowUpdatePolicy(const FShadowUpdatePolicy&) = delete;
	FShadowUpdatePolicy& operator=(const FShadowUpdatePolicy&) = delete;

public:
	void AddCondition(TUniquePtr<IShadowInvalidationCondition> Condition);
	bool Evaluate(const FShadowUpdateContext& Context, const FShadowCacheEntry* Cached) const;

private:
	TArray<TUniquePtr<IShadowInvalidationCondition>> mConditions{};
};
