#pragma once

#include "Core/Types.h"

class FPrimitiveSceneProxy;

// 맵의 Caster를 식별하고 수집 당시의 그림자 버전과 선택 LOD를 기록한다.
struct FShadowCasterReference {
	// Scene이 수명을 관리하는 비소유 포인터. 삭제 전 LightProxy 목록에서 제거한다.
	FPrimitiveSceneProxy* mProxy{};
	// 이 목록을 수집했을 때의 Primitive 그림자 상태 버전.
	uint64 mRevision{};
	uint32 mLODIndex{};
};
