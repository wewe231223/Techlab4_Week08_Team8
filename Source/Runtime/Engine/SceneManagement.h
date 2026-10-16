#pragma once

#include "Math/EngineMath.h"
#include "Math/Color.h"
#include "Render/ShadowMap.h"

//static_assert(sizeof(FLightRenderParameters) == 96, "UberLit / 컬링 CS의 구조체와 같아야 한다");

struct FLightRenderParameters
{
	FVector Position;
	float Radius;
	FVector Color;
	float Intensity;
	FVector Direction;
	float InnerConeCos;
	FVector Right;
	float FalloffExponent;
	float OuterConeCos;
	float HalfWidth;
	float HalfHeight;
	uint32 Type;
	// Rect 전용: 발광면 가장자리의 문(Barn Door)
	float BarnDoorCos;
	float BarnDoorSin;
	float BarnDoorLength;
	int32 mShadowIndex{ InvalidShadowMapIndex }; // FShadowMapRenderParameters 배열의 항목 인덱스. 텍스처 번호와는 별개이며 미할당은 -1이다.
};

static_assert(sizeof(FLightRenderParameters) == 96,"sizeof(UberLit / TileBasedLightCulling) == sizeof(FLightRenderParameters)");
static_assert(offsetof(FLightRenderParameters, mShadowIndex) == 92);
