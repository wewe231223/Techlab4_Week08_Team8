#pragma once

#include "ShadowCacheEntry.h"
#include "ShadowCasterReference.h"
#include "Containers/Array.h"

// 조건 객체가 비교할 현재 입력과 마지막 렌더 기준의 참조.
struct FShadowUpdateContext {
	const FShadowCacheEntry* mCurrentInput{};
	const TArray<FShadowCasterReference>* mCurrentCasters{};
	// 마지막 성공 렌더 당시의 목록. 빈 목록도 유효한 렌더 기준이 될 수 있다.
	const TArray<FShadowCasterReference>* mRenderedCasters{};
};

class IShadowInvalidationCondition {
public:
	IShadowInvalidationCondition();
	virtual ~IShadowInvalidationCondition();

public:
	// true이면 이 조건에서 기존 그림자 맵의 재사용을 허용한다.
	virtual bool Evaluate(const FShadowUpdateContext& Context, const FShadowCacheEntry* Cached) const = 0;
};

class FShadowViewCondition final : public IShadowInvalidationCondition {
public:
	bool Evaluate(const FShadowUpdateContext& Context, const FShadowCacheEntry* Cached) const override;
};

class FShadowCasterCondition final : public IShadowInvalidationCondition {
public:
	bool Evaluate(const FShadowUpdateContext& Context, const FShadowCacheEntry* Cached) const override;
};

class FShadowDepthStateCondition final : public IShadowInvalidationCondition {
public:
	bool Evaluate(const FShadowUpdateContext& Context, const FShadowCacheEntry* Cached) const override;
};
