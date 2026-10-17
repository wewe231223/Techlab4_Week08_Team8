#pragma once

#include "Core/Types.h"
#include "Math/Matrix.h"
#include "Math/Vector2.h"
#include "Math/Vector4.h"

#include <cstddef>
#include <type_traits>

// 그림자 데이터나 텍스처가 아직 할당되지 않았음을 나타낸다. 생성 설정을 켜도 할당 전에는 이 값을 유지한다.
constexpr int32 InvalidShadowMapIndex{ -1 };
constexpr int32 MinShadowMapResolution{ 128 };
constexpr int32 MaxShadowMapResolution{ 8192 };
constexpr uint32 ShadowMapTextureSlot{ 13 };
constexpr uint32 ShadowMapBufferSlot{ 14 };
constexpr uint32 ShadowMapSamplerSlot{ 1 };
constexpr int32 DefaultShadowAtlasResolution{ 4096 };

struct FShadowAtlasTile {
	uint32 mX{};
	uint32 mY{};
	uint32 mResolution{};
};

// 조명 컴포넌트에서 수집하는 그림자 생성 요청 설정. 실제 맵 생성과 샘플링은 그림자 렌더 패스에서 처리한다.
struct FLightShadowSettings {
	bool mCastShadows{ false }; // 이 조명을 그림자 생성 대상으로 사용할지 여부. 기본값은 비활성이다.
	int32 mResolution{ 2048 }; // 정사각형 그림자 맵 한 변의 요청 해상도(텍셀). 컴포넌트에서 허용 범위로 제한한다.
	float mDepthBias{ 0.001f }; // 자기 표면을 그림자로 오판하는 현상을 줄이기 위해 깊이 비교값에 적용할 고정 보정량.
	float mSlopeScaledDepthBias{ 1.0f }; // 광원 시점의 깊이 기울기에 비례하는 추가 깊이 보정의 배율.
	float mNormalBias{ 0.01f }; // 그림자 판정 위치를 표면 법선 방향으로 옮기는 보정량. 깊이값을 직접 바꾸는 보정과 구분한다.
};

// 그림자 맵 생성 후 GPU에 전달할 데이터. ShadowCommon.hlsli의 구조체와 필드 순서 및 크기를 맞춘다.
struct alignas(16) FShadowMapRenderParameters {
	// 광원 클립 공간으로 변환한다.
	FMatrix mLightViewProjection{ FMatrix::Identity };
	// 0~1 그림자 UV를 아틀라스 영역으로 변환한다. XY는 크기, ZW는 오프셋이며 UV * XY + ZW로 적용한다. 기본값은 전체 영역이다.
	FVector4 mUvScaleBias{ 1.0f, 1.0f, 0.0f, 0.0f };
	// 최종 샘플링 텍스처의 { 1 / Width, 1 / Height }. PCF의 이웃 샘플 간격이며 아틀라스 사용 시 전체 아틀라스 해상도를 기준으로 한다.
	FVector2 mTexelSize{};

	float mDepthBias{}; // 이 그림자 맵의 깊이 비교에 적용할 고정 보정량.
	float mNormalBias{}; // 이 그림자 맵의 판정 위치에 적용할 법선 방향 보정량.
	int32 mTextureIndex{ InvalidShadowMapIndex }; // 렌더러가 관리하는 그림자 텍스처 리소스의 인덱스. 미할당은 -1이다.
	uint32 mArraySlice{}; // Texture2DArray 내부의 0부터 시작하는 층 번호. 단일 2D 텍스처에서는 사용하지 않고 0을 유지한다.
	uint32 mPadding[2]{}; // CPU/HLSL 공유 구조체를 16바이트 배수인 112바이트로 맞추는 패딩.
};

static_assert(sizeof(FShadowMapRenderParameters) == 112);
static_assert(alignof(FShadowMapRenderParameters) == 16);
static_assert(std::is_standard_layout_v<FShadowMapRenderParameters>);
static_assert(std::is_trivially_copy_constructible_v<FShadowMapRenderParameters>);
static_assert(offsetof(FShadowMapRenderParameters, mUvScaleBias) == 64);
static_assert(offsetof(FShadowMapRenderParameters, mTexelSize) == 80);
static_assert(offsetof(FShadowMapRenderParameters, mTextureIndex) == 96);
