#ifndef SHADOW_COMMON_HLSLI
#define SHADOW_COMMON_HLSLI

// 그림자 데이터 또는 텍스처 리소스가 미할당된 상태. CPU의 InvalidShadowMapIndex와 같은 값이다.
static const int InvalidShadowMapIndex = { -1 };

// Render/ShadowMap.h의 FShadowMapRenderParameters와 동일한 112바이트 배치를 유지한다.
struct FShadowMapRenderParameters {
    row_major float4x4 mLightViewProjection; // 월드 위치를 광원 클립 공간으로 변환한다. mul(WorldPosition, Matrix) 순서로 사용한다.
    float4 mUvScaleBias; // 아틀라스 UV = 0~1 그림자 UV * XY + ZW. XY는 크기, ZW는 시작 위치이다.
    float2 mTexelSize; // 최종 샘플링 텍스처의 { 1 / Width, 1 / Height }. PCF에서 한 텍셀 단위로 이동할 때 사용한다.
    float mDepthPadding;
    float mNormalBias; // 그림자 판정 위치의 법선 방향 보정량.
    int mTextureIndex; // 그림자 텍스처 리소스의 인덱스. -1은 미할당이다.
    uint mArraySlice; // 선택한 텍스처 배열 내부의 층 번호. 단일 2D 텍스처에서는 0이다.
    uint2 mPadding; // CPU 구조체와 크기 및 배열 항목 간격을 일치시키는 패딩.
};

float2 GetShadowAtlasUv(float2 ShadowUv, FShadowMapRenderParameters Parameters) {
    float2 MinimumUv = { Parameters.mUvScaleBias.zw + Parameters.mTexelSize * 0.5f };
    float2 MaximumUv = { Parameters.mUvScaleBias.zw + Parameters.mUvScaleBias.xy - Parameters.mTexelSize * 0.5f };
    return clamp(ShadowUv * Parameters.mUvScaleBias.xy + Parameters.mUvScaleBias.zw, MinimumUv, MaximumUv);
}

#endif
