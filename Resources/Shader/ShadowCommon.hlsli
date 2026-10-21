#ifndef SHADOW_COMMON_HLSLI
#define SHADOW_COMMON_HLSLI

// 그림자 데이터 또는 텍스처 리소스가 미할당된 상태. CPU의 InvalidShadowMapIndex와 같은 값이다.
static const int InvalidShadowMapIndex = { -1 };
static const uint PointShadowFaceCount = { 6 };

// Render/ShadowMap.h의 FShadowMapRenderParameters와 동일한 128바이트 배치를 유지한다.
struct FShadowMapRenderParameters {
    row_major float4x4 mLightViewProjection; // 월드 위치를 광원 클립 공간으로 변환한다. mul(WorldPosition, Matrix) 순서로 사용한다.
    float4 mUvScaleBias; // 아틀라스 UV = 0~1 그림자 UV * XY + ZW. XY는 크기, ZW는 시작 위치이다.
    float2 mTexelSize; // 최종 샘플링 텍스처의 { 1 / Width, 1 / Height }. PCF에서 한 텍셀 단위로 이동할 때 사용한다.
    float2 mNearFar; // PCSS 선형 깊이 복원용 빛 카메라 기준 Near, Far
    float mMinCasterDepth; // PCSS 탐색 하한. 깊이 복원에는 mNearFar를 사용한다.
    float mNormalBias; // 그림자 판정 위치의 법선 방향 보정량(텍셀 수). 텍셀 월드 크기를 곱해 사용한다.
    int mTextureIndex; // 그림자 텍스처 리소스의 인덱스. -1은 미할당이다.
    uint mArraySlice; // 선택한 텍스처 배열 내부의 층 번호. 단일 2D 텍스처에서는 0이다.
    float2 mSourceRadius; // 원근 광원의 월드 단위 발광면 반경.
    float mSourceAngleTanHalf; // 방향광의 tan(각지름 / 2).
    uint mIsOrthographic;
};

uint GetPointShadowFaceIndex(float3 Direction) {
    const float3 AbsoluteDirection = { abs(Direction) };
    if (AbsoluteDirection.x >= AbsoluteDirection.y && AbsoluteDirection.x >= AbsoluteDirection.z) {
        return Direction.x >= 0.0f ? 0u : 1u;
    }
    if (AbsoluteDirection.y >= AbsoluteDirection.z) {
        return Direction.y >= 0.0f ? 2u : 3u;
    }
    return Direction.z >= 0.0f ? 4u : 5u;
}

float2 GetShadowAtlasUv(float2 ShadowUv, FShadowMapRenderParameters Parameters) {
    float2 MinimumUv = { Parameters.mUvScaleBias.zw + Parameters.mTexelSize * 0.5f };
    float2 MaximumUv = { Parameters.mUvScaleBias.zw + Parameters.mUvScaleBias.xy - Parameters.mTexelSize * 0.5f };
    return clamp(ShadowUv * Parameters.mUvScaleBias.xy + Parameters.mUvScaleBias.zw, MinimumUv, MaximumUv);
}

#endif
