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
constexpr int32 MaxShadowMapResolution{ 2048 };
constexpr int32 DefaultShadowMapResolution{ 512 };
constexpr int32 DefaultDirectionalShadowMapResolution{ 2048 };
constexpr uint32 PointShadowFaceCount{ 6 };
constexpr float DefaultShadowMinCasterDepth{ 1.0f };
constexpr float DefaultShadowSourceRadius{ 0.05f };
constexpr float MaxShadowSourceRadius{ 0.5f }; // 그림자용 반경 상한. Rect의 HalfWidth/HalfHeight에는 적용하지 않는다.
constexpr float DefaultShadowSourceAngle{ 0.5357f }; // 방향광의 발광원 전체 각지름(도).
constexpr float MaxShadowSourceAngle{ 2.0f };

int32 NormalizeShadowMapResolution(int32 Resolution);
float NormalizeShadowMinCasterDepth(float Depth);
float NormalizeShadowSourceRadius(float Radius);
float NormalizeShadowSourceAngle(float Angle);

enum class EShadowFilterMode : int32 {
	None,
	PCF,
	PCSS,
	Count
};

EShadowFilterMode NormalizeShadowFilterMode(int32 Mode);

enum class EShadowDepthBiasPreset : int32 {
	None,
	Low,
	Medium,
	High,
	VeryHigh,
	LowSixteenth,
	LowEighth,
	LowQuarter,
	LowHalf,
	LowMedium,
	BelowMedium,
	MediumHigh,
	HighVeryHigh,
	Count
};

enum class EShadowSlopeScaledDepthBiasPreset : int32 {
	None,
	Low,
	Medium,
	High,
	VeryHigh,
	LowSixteenth,
	LowEighth,
	LowQuarter,
	LowHalf,
	LowMedium,
	BelowMedium,
	MediumHigh,
	HighVeryHigh,
	Count
};

constexpr int32 ShadowDepthBiasPresetCount{ static_cast<int32>(EShadowDepthBiasPreset::Count) };
constexpr int32 ShadowSlopeScaledDepthBiasPresetCount{ static_cast<int32>(EShadowSlopeScaledDepthBiasPreset::Count) };

struct FShadowDepthBiasPreset {
	const char* mName{};
	int32 mDepthBias{};
};

struct FShadowSlopeScaledDepthBiasPreset {
	const char* mName{};
	float mSlopeScaledDepthBias{};
};

EShadowDepthBiasPreset NormalizeShadowDepthBiasPreset(int32 Preset);
EShadowSlopeScaledDepthBiasPreset NormalizeShadowSlopeScaledDepthBiasPreset(int32 Preset);
const FShadowDepthBiasPreset& GetShadowDepthBiasPreset(EShadowDepthBiasPreset Preset);
const FShadowSlopeScaledDepthBiasPreset& GetShadowSlopeScaledDepthBiasPreset(EShadowSlopeScaledDepthBiasPreset Preset);

// 조명 컴포넌트에서 수집하는 그림자 생성 요청 설정. 실제 맵 생성과 샘플링은 그림자 렌더 패스에서 처리한다.
struct FLightShadowSettings {
	bool mCastShadows{ false }; // 이 조명을 그림자 생성 대상으로 사용할지 여부. 기본값은 비활성이다.
	int32 mResolution{ DefaultShadowMapResolution }; // 정사각형 그림자 맵 한 변의 요청 해상도(텍셀). 컴포넌트에서 허용 범위로 제한한다.
	EShadowDepthBiasPreset mDepthBiasPreset{ EShadowDepthBiasPreset::Medium };
	EShadowSlopeScaledDepthBiasPreset mSlopeScaledDepthBiasPreset{ EShadowSlopeScaledDepthBiasPreset::Medium };
	float mNormalBias{ 1.0f }; // 그림자 판정 위치를 표면 법선 방향으로 옮기는 보정량(그림자 맵 텍셀 수). 깊이값을 직접 바꾸는 보정과 구분한다.
	float mMinCasterDepth{ DefaultShadowMinCasterDepth }; // PCSS 탐색용 최소 깊이의 사용자 근사값. 투영 Near를 변경하지 않는다.
	FVector2 mSourceRadius{}; // 원근 광원의 월드 단위 그림자 반경. Rect는 발광면 종횡비를 유지한 별도 반경을 사용한다.
	float mSourceAngle{}; // 방향광의 발광원 전체 각지름(도).
};

// 그림자 맵 생성 후 GPU에 전달할 데이터. ShadowCommon.hlsli의 구조체와 필드 순서 및 크기를 맞춘다.
struct alignas(16) FShadowMapRenderParameters {
	// 광원 클립 공간으로 변환한다.
	FMatrix mLightViewProjection{ FMatrix::Identity };
	// 0~1 그림자 UV를 아틀라스 영역으로 변환한다. XY는 크기, ZW는 오프셋이며 UV * XY + ZW로 적용한다. 기본값은 전체 영역이다.
	FVector4 mUvScaleBias{ 1.0f, 1.0f, 0.0f, 0.0f };
	// 최종 샘플링 텍스처의 { 1 / Width, 1 / Height }. PCF의 이웃 샘플 간격이며 아틀라스 사용 시 전체 아틀라스 해상도를 기준으로 한다.
	FVector2 mTexelSize{};

	FVector2 mNearFar{}; // PCSS 에서 view공간 깊이를 복원하기 위해 필요한 값

	float mMinCasterDepth{ DefaultShadowMinCasterDepth };
	float mNormalBias{}; // 이 그림자 맵의 판정 위치에 적용할 법선 방향 보정량(텍셀 수). 셰이더가 받는 점의 텍셀 월드 크기를 곱한다.
	int32 mTextureIndex{ InvalidShadowMapIndex }; // 렌더러가 관리하는 그림자 텍스처 리소스의 인덱스. 미할당은 -1이다.
	uint32 mArraySlice{}; // Texture2DArray 내부의 0부터 시작하는 층 번호. 단일 2D 텍스처에서는 사용하지 않고 0을 유지한다.
	FVector2 mSourceRadius{};
	float mSourceAngleTanHalf{}; // tan(SourceAngle / 2). 직교 PCSS의 거리 차를 반그림자 반경으로 변환한다.
	uint32 mIsOrthographic{};
};

static_assert(sizeof(FShadowMapRenderParameters) == 128);
static_assert(alignof(FShadowMapRenderParameters) == 16);
static_assert(std::is_standard_layout_v<FShadowMapRenderParameters>);
static_assert(std::is_trivially_copy_constructible_v<FShadowMapRenderParameters>);
static_assert(offsetof(FShadowMapRenderParameters, mUvScaleBias) == 64);
static_assert(offsetof(FShadowMapRenderParameters, mTexelSize) == 80);
static_assert(offsetof(FShadowMapRenderParameters, mNearFar) == 88);
static_assert(offsetof(FShadowMapRenderParameters, mMinCasterDepth) == 96);
static_assert(offsetof(FShadowMapRenderParameters, mNormalBias) == 100);
static_assert(offsetof(FShadowMapRenderParameters, mTextureIndex) == 104);
static_assert(offsetof(FShadowMapRenderParameters, mArraySlice) == 108);
static_assert(offsetof(FShadowMapRenderParameters, mSourceRadius) == 112);
static_assert(offsetof(FShadowMapRenderParameters, mSourceAngleTanHalf) == 120);
static_assert(offsetof(FShadowMapRenderParameters, mIsOrthographic) == 124);
