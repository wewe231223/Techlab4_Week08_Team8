#pragma once

#include "ShadowView.h"

// 그림자 맵 한 면의 입력값. Atlas에는 마지막 성공 렌더의 기준으로 저장한다.
struct FShadowCacheEntry {
	FShadowView mView{};
	EShadowDepthBiasPreset mDepthBiasPreset{ EShadowDepthBiasPreset::Medium };
	EShadowSlopeScaledDepthBiasPreset mSlopeScaledDepthBiasPreset{ EShadowSlopeScaledDepthBiasPreset::Medium };
	// 완성된 렌더 결과를 캐시로 보관 중인지. 현재 입력과의 일치 여부는 Evaluate에서 판단한다.
	bool mHasCachedContent{};
};
