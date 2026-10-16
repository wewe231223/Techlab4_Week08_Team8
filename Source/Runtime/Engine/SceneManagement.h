#pragma once

#include "Math/EngineMath.h"
#include "Math/Color.h"

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
	// -1이면 그림자 없음. 0 이상이면 섀도우 데이터(행렬·섀도우맵) 배열의 칸 번호. FScene::UpdateLights가 채운다.
	int32 ShadowIndex = -1;
};

static_assert(sizeof(FLightRenderParameters) == 96,"sizeof(UberLit / TileBasedLightCulling) == sizeof(FLightRenderParameters)");
