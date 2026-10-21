#pragma pack_matrix(row_major)

#include "ColorSpace.hlsli"
#include "ShadowCommon.hlsli"

#ifndef SHADOW_PCF
#define SHADOW_PCF 0
#endif

#ifndef SHADOW_PCSS
#define SHADOW_PCSS 0
#endif

#define LIGHT_TYPE_POINT 1   // C++ ELightType과 같아야 함
#define LIGHT_TYPE_SPOT  2
#define LIGHT_TYPE_RECT  3

// LightCullingCommon.hlsli의 MAX_LIGHTS_PER_CELL / CELL_STRIDE와 같아야 한다.
#define MAX_LIGHTS_PER_CELL 32
#define CELL_STRIDE (MAX_LIGHTS_PER_CELL + 1)

// LightGrid.LightDebugMode 값. 에디터 SettingsPanel의 "Light Debug" 콤보 순서와 같아야 한다.
#define LIGHT_DEBUG_MODE_NONE         0
#define LIGHT_DEBUG_MODE_CELL_HEATMAP 1
#define LIGHT_DEBUG_MODE_BRUTE_FORCE  2   // 컬링 결과를 무시하고 모든 로컬 라이트를 순회
#define LIGHT_DEBUG_MODE_SLICES       3

// 조명 모델은 C++(FRenderResourceManager::LoadUberLit)이 매크로로 하나만 넘긴다.
// 아무 매크로 없이 컴파일될 때(기존 머티리얼이 쓰는 기본 키)만 Lambert를 켠다.
#if !defined(LIGHTING_MODEL_UNLIT) && !defined(LIGHTING_MODEL_GOURAUD) && \
    !defined(LIGHTING_MODEL_LAMBERT) && !defined(LIGHTING_MODEL_BLINN_PHONG) && \
    !defined(WORLD_NORMAL)
#define LIGHTING_MODEL_LAMBERT 1
#endif

// PCSS시 샘플링 횟수
#define PCSS_SAMPLES 32

static const float2 StaticDisk[PCSS_SAMPLES] = {
    // Blocker Search: 16 samples
    float2(-0.636791, -0.743962),
    float2(-0.719773, 0.656960),
    float2(0.750434, -0.614145),
    float2(0.607429, 0.745835),

    float2(0.021358, 0.442181),
    float2(-0.012337, -0.962720),
    float2(0.293914, -0.591202),
    float2(-0.013464, 0.963713),

    float2(0.665566, -0.175815),
    float2(0.519167, 0.299741),
    float2(-0.135956, -0.444189),
    float2(-0.411811, 0.346109),

    float2(-0.542867, -0.090981),
    float2(-0.959427, 0.193738),
    float2(0.961053, 0.189313),
    float2(0.226131, -0.039612),

    // Additional PCF samples: 16 samples
    float2(0.822834, 0.495325),
    float2(-0.403873, 0.885576),
    float2(0.308058, -0.928791),
    float2(-0.910616, -0.352620),

    float2(0.256258, 0.727389),
    float2(-0.290916, -0.768350),
    float2(-0.715251, 0.360189),
    float2(0.437650, -0.343009),

    float2(-0.162802, 0.249552),
    float2(0.140733, -0.330226),
    float2(0.242522, 0.267609),
    float2(-0.162354, -0.022769),

    float2(-0.481790, -0.431918),
    float2(-0.213640, 0.643764),
    float2(0.704105, 0.093870),
    float2(0.968942, -0.141221)
};

// 아래 구조체와 cbuffer Lighting은 Renderer.h의 FLightingConstants와 바이트 단위로 같아야 한다.
struct FAmbientLightInfo
{
    float3 Color;
    float Intensity;
};

struct FDirectionalLightInfo
{
    float3 Direction; // 빛이 향하는 방향
    float Intensity;
    float3 Color;
    int mShadowIndex; // 방향광의 그림자 데이터 항목 인덱스. -1은 그림자 맵 미할당이다.
    uint mCascadeCount;
    float4 mCascadeSplits;
    float mCascadeTransitionFraction;
    float2 mPadding;
};

// SceneManagement.h의 FLightRenderParameters와 바이트 단위로 같아야 한다.
struct FLightRenderParameters
{
    float3 Position;
    float Radius;
    float3 Color;
    float Intensity;
    float3 Direction;
    float InnerConeCos;
    float3 Right;
    float FalloffExponent;
    float OuterConeCos;
    float HalfWidth;
    float HalfHeight;
    uint Type;
    float BarnDoorCos;     // Rect 전용
    float BarnDoorSin;
    float BarnDoorLength;
    int mShadowIndex; // 로컬 조명의 그림자 데이터 항목 인덱스. 텍스처/배열 층 번호와 구분하며 미할당은 -1이다.
};



// Renderer.h의 FViewConstants와 바이트 단위로 같아야 한다. VS와 PS 둘 다 b0에 바인딩된다.
cbuffer ViewConstants : register(b0)
{
    matrix ViewProjection;
    float3 CameraWorldPos; // Specular의 V = normalize(CameraWorldPos - WorldPos)
    float ViewPadding;
};

cbuffer MaterialParams : register(b1)
{
    float4 BaseColor;
    float2 UVOffset;
    float bOpaque;
    float Shininess; // Blinn-Phong 광택 지수 (Material.h FStaticMeshMaterialParams::Shininess)
};

// Renderer.h의 FPerObjectConstants와 바이트 단위로 같아야 한다.
cbuffer PerObjectConstants : register(b2)
{
    matrix World;
    matrix WorldInverseTranspose; // (World^-1)^T
    uint mReceiveShadows;
    uint3 ObjectPadding;
};

// b3는 FireBall이 쓰는 동안 비워 둔다.

cbuffer Lighting : register(b4)
{
    FAmbientLightInfo Ambient;
    uint NumLocalLights;
    uint NumShadowMaps;
    uint NumDirectionalLights;
    uint mDebugCSM;
};


cbuffer LightGrid : register(b5)
{
    uint NumTilesX;
    uint NumTilesY;
    uint NumSlices;
    uint LightDebugMode; // LIGHT_DEBUG_MODE_*
    float NearZ;
    float FarZ;
    float SliceScale;
    float SliceBias;
    uint TileSize;
    uint bOrthographic;
    uint2 LightGridPadding;
};

StructuredBuffer<FLightRenderParameters> LocalLights : register(t10);
// 셀마다 [개수][라이트 번호 x MAX_LIGHTS_PER_CELL] 형태로 CELL_STRIDE칸씩 이어져 있다.
StructuredBuffer<uint> CellLightIndices : register(t12);
Texture2DArray<float> ShadowAtlas : register(t13);
StructuredBuffer<FShadowMapRenderParameters> ShadowMaps : register(t14);
StructuredBuffer<FDirectionalLightInfo> DirectionalLights : register(t15);
SamplerComparisonState ShadowSampler : register(s1);

Texture2D DiffuseTexture : register(t0);
Texture2D NormalTexture : register(t1);
SamplerState MaterialSampler : register(s0);

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float4 Tangent : TANGENT;   // w = 바이탄젠트 부호
    float2 UV : TEXCOORD;
    float4 Color : COLOR;
};

// VS가 출력하고 PS가 입력으로 받는다.
struct VS_OUTPUT
{
    float4 SvPosition : SV_POSITION; // VS: 클립 공간 / PS: 픽셀 좌표(xy), 깊이(z), View 깊이(w)
    float3 WorldPos : TEXCOORD1;
    float3 Normal : NORMAL;
    float4 Tangent : TANGENT;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR;
#if LIGHTING_MODEL_GOURAUD
    float3 VertexDiffuse : COLOR1;   // 정점에서 미리 계산한 Diffuse 조명
#endif
};

// Diffuse와 Specular를 담는 구조체
struct FLightContribution
{
    float3 Diffuse;
    float3 Specular;
};

#if defined(LIGHTING_MODEL_LAMBERT) || defined(LIGHTING_MODEL_BLINN_PHONG)
// WorldPos에서 그림자 맵 텍셀 하나가 덮는 월드 길이. 원근은 깊이 W에 비례하고 직교는 W = 1로 일정하다.
float GetShadowTexelWorldSize(FShadowMapRenderParameters Parameters, float3 WorldPos) {
    const float TileResolution = { Parameters.mUvScaleBias.x / Parameters.mTexelSize.x };
    // 행 벡터 곱이므로 0, 1열이 월드에서 클립 X, Y로 가는 축이며 길이가 투영 배율이다.
    const float3 AxisX = { float3(Parameters.mLightViewProjection[0][0], Parameters.mLightViewProjection[1][0], Parameters.mLightViewProjection[2][0]) };
    const float3 AxisY = { float3(Parameters.mLightViewProjection[0][1], Parameters.mLightViewProjection[1][1], Parameters.mLightViewProjection[2][1]) };
    const float ProjectionScale = { min(length(AxisX), length(AxisY)) };
    const float W = { mul(float4(WorldPos, 1.0f), Parameters.mLightViewProjection).w };
    return 2.0f * max(W, 0.0f) / max(TileResolution * ProjectionScale, 1.0e-6f);
}

#if SHADOW_PCSS
float2 MakeDiskRotation(float2 PixelPosition) {
    // 화면 픽셀 좌표로 고정된 임의 각도를 만든다. 시간값은 사용하지 않는다.
    const float2 P = { floor(PixelPosition) };
    const float Random = { frac(52.9829189f * frac(dot(P, float2(0.06711056f, 0.00583715f)))) };
    float S = { 0.0f };
    float C = { 0.0f };
    sincos(Random * 6.28318530718f, S, C);
    return float2(C, S);
}

float2 RotateDisk(float2 Offset, float2 Rotation) {
    return float2(Offset.x * Rotation.x - Offset.y * Rotation.y, Offset.x * Rotation.y + Offset.y * Rotation.x);
}
#endif

float CastShadow(int ShadowIndex, float3 WorldPos, float3 Normal, float2 DiskRotation) {
    if (mReceiveShadows == 0 || ShadowIndex < 0 || (uint) ShadowIndex >= NumShadowMaps) {
        return 1.0f;
    }

    const FShadowMapRenderParameters Parameters = { ShadowMaps[ShadowIndex] };
    if (Parameters.mTextureIndex != 0 || any(Parameters.mUvScaleBias.xy <= 0.0f)) {
        return 1.0f;
    }

    // mNormalBias는 텍셀 수 단위라 거리와 해상도가 달라도 같은 값이 통한다.
    const float NormalOffset = { Parameters.mNormalBias * GetShadowTexelWorldSize(Parameters, WorldPos) };
    const float4 LightClip = { mul(float4(WorldPos + Normal * NormalOffset, 1.0f), Parameters.mLightViewProjection) };
    if (LightClip.w <= 0.0f) {
        return 1.0f;
    }
    const float3 ShadowNdc = { LightClip.xyz / LightClip.w };
    if (any(abs(ShadowNdc.xy) > 1.0f) || ShadowNdc.z < 0.0f || ShadowNdc.z > 1.0f) {
        return 1.0f;
    }
    const float2 ShadowUv = { ShadowNdc.xy * float2(0.5f, -0.5f) + 0.5f };
    const float ComparisonDepth = { ShadowNdc.z };
#if SHADOW_PCF
    const float2 ShadowTexelSize = { Parameters.mTexelSize / Parameters.mUvScaleBias.xy };
    float Visibility = { 0.0f };
    [unroll]
    for (int Y = { -1 }; Y <= 1; ++Y) {
        [unroll]
        for (int X = { -1 }; X <= 1; ++X) {
            const float2 AtlasUv = { GetShadowAtlasUv(ShadowUv + float2(X, Y) * ShadowTexelSize, Parameters) };
            Visibility += ShadowAtlas.SampleCmpLevelZero(ShadowSampler, float3(AtlasUv, Parameters.mArraySlice), ComparisonDepth);
        }
    }
    return Visibility / 9.0f;
#elif SHADOW_PCSS
    const float NearPlane = { Parameters.mNearFar.x };
    const float FarPlane = { Parameters.mNearFar.y };
    const bool IsOrthographic = { Parameters.mIsOrthographic != 0 };
    if ((IsOrthographic && Parameters.mSourceAngleTanHalf <= 0.0f) || (!IsOrthographic && all(Parameters.mSourceRadius <= 0.0f))) {
        return ShadowAtlas.SampleCmpLevelZero(ShadowSampler, float3(GetShadowAtlasUv(ShadowUv, Parameters), Parameters.mArraySlice), ComparisonDepth);
    }

    const float Receiver = { IsOrthographic ? lerp(NearPlane, FarPlane, ComparisonDepth) : LightClip.w };
    const float SearchMinDepth = { clamp(Parameters.mMinCasterDepth, NearPlane, Receiver) };
    const float4x4 M = { Parameters.mLightViewProjection };
    const float2 ProjectionScale = { float2(length(float3(M[0][0], M[1][0], M[2][0])), length(float3(M[0][1], M[1][1], M[2][1]))) };
    const float2 WorldToShadowUv = { ProjectionScale * 0.5f / (IsOrthographic ? 1.0f : Receiver) };
    const float SearchDistance = { max(Receiver - SearchMinDepth, 0.0f) };
    const float2 SearchRadiusUv = { IsOrthographic ? WorldToShadowUv * Parameters.mSourceAngleTanHalf * SearchDistance : WorldToShadowUv * Parameters.mSourceRadius * SearchDistance / max(SearchMinDepth, 1.0e-6f) };

    float BlockerAvgDepth = { 0.0f };
    int BlockerCnt = { 0 };

    [unroll]
    for (int Index = { 0 }; Index < PCSS_SAMPLES / 2; ++Index) {
        // 중앙을 포함해 작은 차폐물을 놓치지 않는다. 타일 밖 깊이를 경계에 반복 누적하지 않는다.
        const float2 Offset = { Index == 0 ? float2(0.0f, 0.0f) : RotateDisk(StaticDisk[Index], DiskRotation) };
        const float2 SampleUv = { ShadowUv + Offset * SearchRadiusUv };
        if (any(SampleUv < 0.0f) || any(SampleUv > 1.0f)) {
            continue;
        }
        const float2 AtlasUv = { GetShadowAtlasUv(SampleUv, Parameters) };
        const int2 AtlasTexel = { int2(AtlasUv / Parameters.mTexelSize) };
        const float StoredDepth = { ShadowAtlas.Load(int4(AtlasTexel, Parameters.mArraySlice, 0)) };

        if (StoredDepth < ComparisonDepth) {
            const float BlockerLinearDepth = { IsOrthographic ? lerp(NearPlane, FarPlane, StoredDepth) : (NearPlane * FarPlane) / (FarPlane - StoredDepth * (FarPlane - NearPlane)) };
            BlockerAvgDepth += BlockerLinearDepth;
            BlockerCnt++;
        }
    }

    if (BlockerCnt < 1) {
        return 1.0f;
    }

    BlockerAvgDepth /= BlockerCnt;
    const float BlockerSeparation = { max(Receiver - BlockerAvgDepth, 0.0f) };
    const float2 FilterRadiusUv = { IsOrthographic ? WorldToShadowUv * Parameters.mSourceAngleTanHalf * BlockerSeparation : WorldToShadowUv * Parameters.mSourceRadius * BlockerSeparation / max(BlockerAvgDepth, 1.0e-6f) };

    const float2 ShadowTexelSize = { Parameters.mTexelSize / Parameters.mUvScaleBias.xy };
    const float2 MinRadiusUV = { ShadowTexelSize * 1.0f };
    const float2 FinalRadiusUV = { max(MinRadiusUV, FilterRadiusUv) };

    float Visibility = { 0.0f };
    [unroll]
    for (int Index = { 0 }; Index < PCSS_SAMPLES; ++Index) {
        const float2 Offset = { RotateDisk(StaticDisk[Index], DiskRotation) };
        const float2 SampleUv = { ShadowUv + Offset * FinalRadiusUV };
        if (any(SampleUv < 0.0f) || any(SampleUv > 1.0f)) {
            Visibility += 1.0f;
        }
        else {
            Visibility += ShadowAtlas.SampleCmpLevelZero(ShadowSampler, float3(GetShadowAtlasUv(SampleUv, Parameters), Parameters.mArraySlice), ComparisonDepth);
        }
    }
    return Visibility / PCSS_SAMPLES;
#else
    return ShadowAtlas.SampleCmpLevelZero(ShadowSampler, float3(GetShadowAtlasUv(ShadowUv, Parameters), Parameters.mArraySlice), ComparisonDepth);
#endif
}

float SamplePointShadow(FShadowMapRenderParameters Parameters, float4 LightClip) {
    if (Parameters.mTextureIndex != 0 || any(Parameters.mUvScaleBias.xy <= 0.0f) || LightClip.w <= 0.0f) {
        return 1.0f;
    }
    const float3 ShadowNdc = { LightClip.xyz / LightClip.w };
    if (any(abs(ShadowNdc.xy) > 1.000001f) || ShadowNdc.z < 0.0f || ShadowNdc.z > 1.0f) {
        return 1.0f;
    }
    const float2 ShadowUv = { ShadowNdc.xy * float2(0.5f, -0.5f) + 0.5f };
    const float2 AtlasUv = { GetShadowAtlasUv(ShadowUv, Parameters) };
    return ShadowAtlas.SampleCmpLevelZero(ShadowSampler, float3(AtlasUv, Parameters.mArraySlice), ShadowNdc.z);
}

float CastPointShadow(FLightRenderParameters Light, float3 WorldPos, float3 Normal, float2 DiskRotation) {
    if (mReceiveShadows == 0 || Light.mShadowIndex < 0 || NumShadowMaps < PointShadowFaceCount || (uint) Light.mShadowIndex > NumShadowMaps - PointShadowFaceCount) {
        return 1.0f;
    }
    const uint FirstIndex = { (uint) Light.mShadowIndex };
    // 보정 전 위치가 속한 면의 투영으로 텍셀 크기를 구해 텍셀 수 단위의 보정량을 월드 길이로 바꾼다.
    const FShadowMapRenderParameters ReceiverParameters = { ShadowMaps[FirstIndex + GetPointShadowFaceIndex(WorldPos - Light.Position)] };
    const float NormalOffset = { ReceiverParameters.mNormalBias * GetShadowTexelWorldSize(ReceiverParameters, WorldPos) };
    const float3 BiasedPosition = { WorldPos + Normal * NormalOffset };
    const float3 ReceiverDirection = { BiasedPosition - Light.Position };
    const float DistanceSquared = { dot(ReceiverDirection, ReceiverDirection) };
    if (DistanceSquared <= 1.0e-12f) {
        return 1.0f;
    }
    const uint FaceIndex = { GetPointShadowFaceIndex(ReceiverDirection) };
    const FShadowMapRenderParameters Parameters = { ShadowMaps[FirstIndex + FaceIndex] };
    const float4 LightClip = { mul(float4(BiasedPosition, 1.0f), Parameters.mLightViewProjection) };
#if SHADOW_PCF
    const float3 Right = { float3(Parameters.mLightViewProjection[0][0], Parameters.mLightViewProjection[1][0], Parameters.mLightViewProjection[2][0]) };
    const float3 Up = { float3(Parameters.mLightViewProjection[0][1], Parameters.mLightViewProjection[1][1], Parameters.mLightViewProjection[2][1]) };
    const float2 WorldTexelSize = { 2.0f * LightClip.w * Parameters.mTexelSize / Parameters.mUvScaleBias.xy };
    const float ReceiverDistance = { sqrt(DistanceSquared) };
    float Visibility = { 0.0f };
    [unroll]
    for (int Y = { -1 }; Y <= 1; ++Y) {
        [unroll]
        for (int X = { -1 }; X <= 1; ++X) {
            const float3 SampleDirection = { ReceiverDirection + Right * (X * WorldTexelSize.x) - Up * (Y * WorldTexelSize.y) };
            const uint SampleFaceIndex = { GetPointShadowFaceIndex(SampleDirection) };
            const FShadowMapRenderParameters SampleParameters = { ShadowMaps[FirstIndex + SampleFaceIndex] };
            const float3 SamplePosition = { Light.Position + SampleDirection * (ReceiverDistance * rsqrt(dot(SampleDirection, SampleDirection))) };
            const float4 SampleClip = { mul(float4(SamplePosition, 1.0f), SampleParameters.mLightViewProjection) };
            Visibility += SamplePointShadow(SampleParameters, SampleClip);
        }
    }
    return Visibility / 9.0f;
#elif SHADOW_PCSS
    const float SourceRadius = { Parameters.mSourceRadius.x };
    if (SourceRadius <= 0.0f) {
        return SamplePointShadow(Parameters, LightClip);
    }

    const float ReceiverDistance = { sqrt(DistanceSquared) };
    const float3 Right = { float3(Parameters.mLightViewProjection[0][0], Parameters.mLightViewProjection[1][0], Parameters.mLightViewProjection[2][0]) };
    const float3 Up = { float3(Parameters.mLightViewProjection[0][1], Parameters.mLightViewProjection[1][1], Parameters.mLightViewProjection[2][1]) };
    // 단위 큐브 투영에서 선택한 면의 전방 깊이 하한을 수신점 구면의 샘플 반경으로 변환한다.
    const float SearchMinDepth = { clamp(Parameters.mMinCasterDepth, Parameters.mNearFar.x, LightClip.w) };
    const float SearchRadius = { SourceRadius * max(LightClip.w - SearchMinDepth, 0.0f) / max(SearchMinDepth, 1.0e-6f) };
    float BlockerDistanceSum = { 0.0f };
    int BlockerCnt = { 0 };

    [unroll]
    for (int Index = { 0 }; Index < PCSS_SAMPLES / 2; ++Index) {
        const float2 Offset = { Index == 0 ? float2(0.0f, 0.0f) : RotateDisk(StaticDisk[Index], DiskRotation) };
        const float3 SampleDirection = { ReceiverDirection + (Right * Offset.x - Up * Offset.y) * SearchRadius };
        const uint SampleFaceIndex = { GetPointShadowFaceIndex(SampleDirection) };
        const FShadowMapRenderParameters SampleParameters = { ShadowMaps[FirstIndex + SampleFaceIndex] };
        const float3 SamplePosition = { Light.Position + normalize(SampleDirection) * ReceiverDistance };
        const float4 SampleClip = { mul(float4(SamplePosition, 1.0f), SampleParameters.mLightViewProjection) };
        const float3 SampleNdc = { SampleClip.xyz / SampleClip.w };
        if (SampleNdc.z < 0.0f || SampleNdc.z > 1.0f) {
            continue;
        }
        const float2 SampleUv = { SampleNdc.xy * float2(0.5f, -0.5f) + 0.5f };
        const int2 AtlasTexel = { int2(GetShadowAtlasUv(SampleUv, SampleParameters) / SampleParameters.mTexelSize) };
        const float StoredDepth = { ShadowAtlas.Load(int4(AtlasTexel, SampleParameters.mArraySlice, 0)) };
        if (StoredDepth < SampleNdc.z) {
            const float NearPlane = { SampleParameters.mNearFar.x };
            const float FarPlane = { SampleParameters.mNearFar.y };
            const float BlockerViewDepth = { (NearPlane * FarPlane) / (FarPlane - StoredDepth * (FarPlane - NearPlane)) };
            // 큐브 면마다 다른 전방 깊이를 광원까지의 거리로 통일해 평균한다.
            BlockerDistanceSum += BlockerViewDepth * ReceiverDistance / SampleClip.w;
            BlockerCnt++;
        }
    }

    if (BlockerCnt == 0) {
        return 1.0f;
    }

    const float AverageBlockerDistance = { BlockerDistanceSum / BlockerCnt };
    const float PenumbraRadiusWorld = { SourceRadius * max(ReceiverDistance - AverageBlockerDistance, 0.0f) / max(AverageBlockerDistance, 1.0e-6f) };

    float Visibility = { 0.0f };
    [unroll]
    for (int Index = { 0 }; Index < PCSS_SAMPLES; ++Index) {
        const float2 Offset = { RotateDisk(StaticDisk[Index], DiskRotation) };
        const float3 SampleDirection = { ReceiverDirection + (Right * Offset.x - Up * Offset.y) * PenumbraRadiusWorld };
        const uint SampleFaceIndex = { GetPointShadowFaceIndex(SampleDirection) };
        const FShadowMapRenderParameters SampleParameters = { ShadowMaps[FirstIndex + SampleFaceIndex] };
        const float3 SamplePosition = { Light.Position + normalize(SampleDirection) * ReceiverDistance };
        const float4 SampleClip = { mul(float4(SamplePosition, 1.0f), SampleParameters.mLightViewProjection) };
        Visibility += SamplePointShadow(SampleParameters, SampleClip);
    }
    return Visibility / PCSS_SAMPLES;

#else
    return SamplePointShadow(Parameters, LightClip);
#endif
}
#endif

// 원뿔 감쇠량. 0(Outer 밖) ~ 1(Inner 안)
float GetSpotConeAttenuation(FLightRenderParameters Light, float3 L)
{
    return smoothstep(Light.OuterConeCos, Light.InnerConeCos, dot(-L, Light.Direction));
}

// 거리 감쇠량을 돌려주고, 표면에서 빛을 향하는 단위 벡터 L을 OutL로 내보낸다.
float GetDistanceAttenuation(float3 LightPos, float AttenuationRadius, float FalloffExponent, float3 WorldPos, out float3 OutL)
{
    const float3 ToLight = LightPos - WorldPos; // 픽셀의 월드 좌표에서 빛을 향하는 벡터
    const float Distance = length(ToLight);
    OutL = ToLight / max(Distance, 0.0001f);
    // 거리가 Radius에 가까울수록 0으로
    return pow(saturate(1.0f - Distance / max(AttenuationRadius, 0.001f)), max(FalloffExponent, 0.001f));
}



// 라이트 하나의 Diffuse와 Specular를 구한다. Radiance = Color * Intensity * Attenuation (* ConeAttenuation)
FLightContribution SurfaceShading(float3 N, float3 L, float3 V, float3 Radiance, float InShininess)
{
    const float NdotL = saturate(dot(normalize(N), normalize(L)));
    FLightContribution Contribution = (FLightContribution) 0;
    Contribution.Diffuse = NdotL;

#if LIGHTING_MODEL_BLINN_PHONG
    // Blinn Specular
    const float3 H = normalize(L + V);
    if (dot(N, L) > 0)
    {
        Contribution.Specular = pow(saturate(dot(normalize(N), H)), max(InShininess, 1.f));
    }
#endif
    Contribution.Diffuse *= Radiance;
    Contribution.Specular *= Radiance;
    return Contribution;
}

// 표면에 들어오는 빛의 양만 돌려준다. 표면색(albedo)은 호출한 쪽에서 곱한다.
float3 GetAmbientLighting()
{
    return Ambient.Color * Ambient.Intensity;
}

// Directional Light의 영향을 구한다
FLightContribution CalcDirectionalLight(FDirectionalLightInfo Light, float3 N, float3 V, float3 WorldPos, float InShininess, float2 DiskRotation) {
    const float3 Radiance = { Light.Color * Light.Intensity };
    FLightContribution Result = { SurfaceShading(N, -Light.Direction, V, Radiance, InShininess) };
#if defined(LIGHTING_MODEL_LAMBERT) || defined(LIGHTING_MODEL_BLINN_PHONG)
    int ShadowIndex = { Light.mShadowIndex };
    const float TransitionFraction = { Light.mCascadeTransitionFraction };
    float NearSplit = { 0.0f };
    float FarSplit = { 0.0f };
    int CascadeIndex = { -1 };
    const float3 ViewForward = { normalize(float3(ViewProjection[0][2], ViewProjection[1][2], ViewProjection[2][2])) };
    const float ViewDepth = { dot(WorldPos - CameraWorldPos, ViewForward) };
    if (Light.mCascadeCount > 1 && Light.mShadowIndex >= 0) {
        ShadowIndex = -1;
        for (uint Index = { 0 }; Index < min(Light.mCascadeCount, 4u); ++Index) {
            if (ViewDepth < Light.mCascadeSplits[Index]) {
                CascadeIndex = (int) Index;
                ShadowIndex = Light.mShadowIndex + (int) Index;
                FarSplit = Light.mCascadeSplits[Index];
                break;
            }
            NearSplit = Light.mCascadeSplits[Index];
        }
    }
    float Visibility = { CastShadow(ShadowIndex, WorldPos, N, DiskRotation) };

    if (CascadeIndex >= 0 && CascadeIndex < (int) min(Light.mCascadeCount, 4u) - 1 && TransitionFraction > 0.0f) {
        const float CascadeRange = { FarSplit - NearSplit };
        const float DistToSplit = { FarSplit - ViewDepth };
        const float BlendRange = { CascadeRange * TransitionFraction };

        if (DistToSplit < BlendRange && BlendRange > 0.0f) {
            const float BlendRatio = { saturate(1.0f - DistToSplit / BlendRange) };
            const float NextVisibility = { CastShadow(ShadowIndex + 1, WorldPos, N, DiskRotation) };
            Visibility = lerp(Visibility, NextVisibility, BlendRatio);
        }
    }


    Result.Diffuse *= Visibility;
    Result.Specular *= Visibility;
#endif
    return Result;
}

FLightContribution AccumulateDirectionalLighting(float3 N, float3 V, float3 WorldPos, float InShininess, float2 DiskRotation) {
    FLightContribution Total = { (FLightContribution) 0 };
    for (uint Index = { 0 }; Index < NumDirectionalLights; ++Index) {
        const FLightContribution Contribution = { CalcDirectionalLight(DirectionalLights[Index], N, V, WorldPos, InShininess, DiskRotation) };
        Total.Diffuse += Contribution.Diffuse;
        Total.Specular += Contribution.Specular;
    }
    return Total;
}

// 해당 WorldPos에서 PointLight 하나의 영향(밝기)을 구한다
FLightContribution CalcPointLight(FLightRenderParameters Light, float3 N, float3 V, float3 WorldPos, float InShininess)
{
    float3 L;
    const float Attenuation = GetDistanceAttenuation(Light.Position, Light.Radius, Light.FalloffExponent, WorldPos, L);
    return SurfaceShading(N, L, V, Light.Color * Light.Intensity * Attenuation, InShininess);
}

// Spot은 Point에서 원뿔 감쇠만 곱하면 된다
FLightContribution CalcSpotLight(FLightRenderParameters Light, float3 N, float3 V, float3 WorldPos, float InShininess)
{
    float3 L;
    const float Attenuation = GetDistanceAttenuation(Light.Position, Light.Radius, Light.FalloffExponent, WorldPos, L);
    return SurfaceShading(N, L, V, Light.Color * Light.Intensity * Attenuation * GetSpotConeAttenuation(Light, L), InShininess);
}

FLightContribution CalcRectLight(FLightRenderParameters Light, float3 N, float3 V, float3 WorldPos, float InShininess) {
    const FLightContribution Unlit = { (FLightContribution) 0 };
    if (any(!isfinite(float3(Light.HalfWidth, Light.HalfHeight, Light.Radius))) || Light.HalfWidth <= 0.0f || Light.HalfHeight <= 0.0f || Light.Radius <= 0.0f) {
        return Unlit;
    }

    const float DirectionLengthSquared = { dot(Light.Direction, Light.Direction) };
    if (!isfinite(DirectionLengthSquared) || DirectionLengthSquared < 1.0e-8f) {
        return Unlit;
    }
    const float3 Forward = { Light.Direction * rsqrt(DirectionLengthSquared) };
    const float3 RightAxis = { Light.Right - Forward * dot(Light.Right, Forward) };
    const float RightLengthSquared = { dot(RightAxis, RightAxis) };
    if (!isfinite(RightLengthSquared) || RightLengthSquared < 1.0e-8f) {
        return Unlit;
    }
    const float3 Right = { RightAxis * rsqrt(RightLengthSquared) };
    const float3 Up = { cross(Forward, Right) };
    const float3 CenterToSurface = { WorldPos - Light.Position };
    const float ForwardDistance = { dot(CenterToSurface, Forward) };
    if (!isfinite(ForwardDistance) || ForwardDistance <= 0.0f) {
        return Unlit;
    }

    const float NearPlane = { max(max(Light.HalfWidth, Light.HalfHeight), 0.01f) };
    const float IntersectionScale = { NearPlane / (NearPlane + ForwardDistance) };
    const float2 EmitterCoordinates = { float2(dot(CenterToSurface, Right), dot(CenterToSurface, Up)) * IntersectionScale };
    if (any(!isfinite(EmitterCoordinates)) || any(abs(EmitterCoordinates) > float2(Light.HalfWidth, Light.HalfHeight))) {
        return Unlit;
    }

    const float3 EmitterPoint = { Light.Position + Right * EmitterCoordinates.x + Up * EmitterCoordinates.y };
    float3 L = { 0.0f, 0.0f, 0.0f };
    const float Attenuation = { GetDistanceAttenuation(EmitterPoint, Light.Radius, Light.FalloffExponent, WorldPos, L) };
    if (Attenuation <= 0.0f) {
        return Unlit;
    }
    const float FrontFace = { saturate(dot(-L, Forward)) };
    float BarnDoorMask = { 1.0f };
    if (Light.BarnDoorLength > 0.0f) {
        const float3 EmitterToSurface = { WorldPos - EmitterPoint };
        const float Distance = { max(length(EmitterToSurface), 0.0001f) };
        const float LateralX = { abs(dot(EmitterToSurface, Right)) };
        const float LateralY = { abs(dot(EmitterToSurface, Up)) };
        const float SlackX = { (ForwardDistance * Light.BarnDoorSin - LateralX * Light.BarnDoorCos) / Distance };
        const float SlackY = { (ForwardDistance * Light.BarnDoorSin - LateralY * Light.BarnDoorCos) / Distance };
        BarnDoorMask = smoothstep(-0.05f, 0.0f, SlackX) * smoothstep(-0.05f, 0.0f, SlackY);
    }

    const float3 Radiance = { Light.Color * Light.Intensity * Attenuation * FrontFace * BarnDoorMask };
    return SurfaceShading(N, L, V, Radiance, InShininess);
}

// Type에 따라 Point / Spot / Rect로 나눠 계산한다
FLightContribution CalcLocalLight(FLightRenderParameters Light, float3 N, float3 V, float3 WorldPos, float InShininess, float2 DiskRotation)
{
    FLightContribution Result;
    if (Light.Type == LIGHT_TYPE_SPOT)
        Result = CalcSpotLight(Light, N, V, WorldPos, InShininess);
    else if (Light.Type == LIGHT_TYPE_RECT)
        Result = CalcRectLight(Light, N, V, WorldPos, InShininess);
    else
        Result = CalcPointLight(Light, N, V, WorldPos, InShininess);
#if defined(LIGHTING_MODEL_LAMBERT) || defined(LIGHTING_MODEL_BLINN_PHONG)
    const float Visibility = { Light.Type == LIGHT_TYPE_POINT ? CastPointShadow(Light, WorldPos, N, DiskRotation) : (Light.Type == LIGHT_TYPE_SPOT || Light.Type == LIGHT_TYPE_RECT) ? CastShadow(Light.mShadowIndex, WorldPos, N, DiskRotation) : 1.0f };
    Result.Diffuse *= Visibility;
    Result.Specular *= Visibility;
#endif
    return Result;
}

// 픽셀이 속한 깊이 슬라이스 번호
uint GetSliceIndex(float4 SvPosition)
{
    float Slice;
    if (bOrthographic)
    {
        // 직교: SV_Position.z(0~1)가 Near~Far에 선형으로 대응
        Slice = SvPosition.z * NumSlices;
    }
    else
    {
        // 원근: SV_Position.w = 클립 공간 w = View 깊이(X)
        Slice = log(SvPosition.w) * SliceScale + SliceBias;
    }
    return (uint) clamp(Slice, 0.0f, NumSlices - 1.0f);
}

// 픽셀이 속한 셀(타일 x 슬라이스)의 목록이 CellLightIndices에서 시작하는 위치
uint GetCellListOffset(float4 SvPosition)
{
    const uint2 Tile = (uint2) SvPosition.xy / TileSize;
    const uint Slice = GetSliceIndex(SvPosition);
    return ((Slice * NumTilesY + Tile.y) * NumTilesX + Tile.x) * CELL_STRIDE;
}

// 라이트 개수를 검정 → 파랑 → 초록 → 빨강 → 흰색 히트맵으로
float3 GetHeatmapColor(uint LightCount)
{
    if (LightCount == 0)
        return float3(0, 0, 0);
    if (LightCount >= MAX_LIGHTS_PER_CELL)
        return float3(1, 1, 1);

    const float t = LightCount / (float) MAX_LIGHTS_PER_CELL;
    return saturate(float3(t * 2 - 0.5, 1 - abs(t * 2 - 1), 1.5 - t * 2));
}

// Directional + 로컬 라이트의 Diffuse/Specular를 모두 더한다. Ambient는 포함하지 않는다.
// bUseLightGrid = true면 SvPosition이 속한 셀의 라이트만, false면 전체 로컬 라이트를 순회한다.
// VS(Gouraud)는 픽셀 위치가 없어 셀을 쓸 수 없으므로 false로 호출한다.
FLightContribution AccumulateDirectLighting(float3 N, float3 V, float3 WorldPos, float InShininess, float4 SvPosition, bool bUseLightGrid)
{
    float2 DiskRotation = { float2(1.0f, 0.0f) };
#if SHADOW_PCSS && (defined(LIGHTING_MODEL_LAMBERT) || defined(LIGHTING_MODEL_BLINN_PHONG))
    // 픽셀당 한 번 계산해 차폐물 탐색과 최종 필터에서 같은 회전값을 사용한다.
    DiskRotation = MakeDiskRotation(SvPosition.xy);
#endif
    FLightContribution Total = { AccumulateDirectionalLighting(N, V, WorldPos, InShininess, DiskRotation) };

    uint LightCount = NumLocalLights;
    uint CellOffset = 0;
    bool bUseCellList = false;
    if (bUseLightGrid && NumTilesX > 0 && LightDebugMode != LIGHT_DEBUG_MODE_BRUTE_FORCE)
    {
        CellOffset = GetCellListOffset(SvPosition);
        const uint CellLightCount = CellLightIndices[CellOffset];
        // 셀이 넘쳤으면(상한 이상) 목록이 잘렸으므로 전체 순회로 돌아간다
        if (CellLightCount < MAX_LIGHTS_PER_CELL)
        {
            LightCount = CellLightCount;
            bUseCellList = true;
        }
    }

    for (uint Index = 0; Index < LightCount; ++Index)
    {
        const uint LightIndex = bUseCellList ? CellLightIndices[CellOffset + 1 + Index] : Index;
        const FLightContribution LightContribution = CalcLocalLight(LocalLights[LightIndex], N, V, WorldPos, InShininess, DiskRotation);
        Total.Diffuse += LightContribution.Diffuse;
        Total.Specular += LightContribution.Specular;
    }

    return Total;
}

VS_OUTPUT mainVS(VS_INPUT Input)
{
    VS_OUTPUT Output;

    const float4 WorldPos = mul(float4(Input.Position, 1.0f), World);
    Output.SvPosition = mul(WorldPos, ViewProjection);
    Output.WorldPos = WorldPos.xyz;

    // 노멀은 역전치로, 탄젠트는 표면을 따라 놓인 방향이라 World로 변환한다.
    // World가 뒤집혀 있으면(행렬식 < 0) 바이탄젠트 부호도 뒤집는다.
    Output.Normal = mul(float4(Input.Normal, 0.0f), WorldInverseTranspose).xyz;
    Output.Tangent = float4(mul(float4(Input.Tangent.xyz, 0.0f), World).xyz, Input.Tangent.w * (determinant((float3x3) World) < 0.0f ? -1.0f : 1.0f));

    Output.UV = Input.UV;
    Output.Color = Input.Color;
#if LIGHTING_MODEL_GOURAUD
    const float3 V = normalize(CameraWorldPos - Output.WorldPos);
    const FLightContribution Contribution = AccumulateDirectLighting(normalize(Output.Normal), V, Output.WorldPos, 1, Output.SvPosition, false);
    Output.VertexDiffuse = Contribution.Diffuse;
#endif
    return Output;
}

float4 mainPS(VS_OUTPUT Input) : SV_TARGET
{
    const float2 UV = Input.UV + UVOffset;
    const float4 TexColor = DiffuseTexture.Sample(MaterialSampler, UV);
    float4 Albedo = TexColor * BaseColor;
#if !defined(EDITOR_ARROW_SHADER)
    // 씬은 HDR 타깃에 선형으로 쌓이므로 sRGB로 인코딩된 디퓨즈 텍스처와 BaseColor를 각각 선형으로 바꾼다.
    // 에디터 화살표는 톤 매핑 이후 LDR 타깃에 그려지므로 sRGB 그대로 둔다.
    Albedo.rgb = SRGBToLinear(TexColor.rgb) * SRGBToLinear(BaseColor.rgb);

    if (LightDebugMode == LIGHT_DEBUG_MODE_CELL_HEATMAP && NumTilesX > 0)
    {
        const uint CellLightCount = CellLightIndices[GetCellListOffset(Input.SvPosition)];
        return float4(lerp(Albedo.rgb * 0.2f, GetHeatmapColor(CellLightCount), 0.7f), 1.0f);
    }
    // 슬라이스 시각화: 이웃 슬라이스끼리 색이 크게 달라지도록 황금비로 색상환을 돈다.
    if (LightDebugMode == LIGHT_DEBUG_MODE_SLICES && NumTilesX > 0)
    {
        const float Hue = frac(GetSliceIndex(Input.SvPosition) * 0.618034f);
        const float3 SliceColor = saturate(abs(frac(Hue + float3(0.0f, 2.0f, 1.0f) / 3.0f) * 6.0f - 3.0f) - 1.0f);
        return float4(lerp(Albedo.rgb * 0.2f, SliceColor, 0.7f), 1.0f);
    }
#endif

    const float3 VertexNormal = { normalize(Input.Normal) };
#if defined(LIGHTING_MODEL_LAMBERT) || defined(LIGHTING_MODEL_BLINN_PHONG) || defined(WORLD_NORMAL)
    // TBN: 노멀맵의 탄젠트 공간 노멀을 월드 공간으로 옮긴다.
    const float3 T = { normalize(Input.Tangent.xyz - VertexNormal * dot(VertexNormal, Input.Tangent.xyz)) };
    const float3 B = { cross(VertexNormal, T) * (Input.Tangent.w < 0.0f ? -1.0f : 1.0f) };
    const float3 TangentNormal = { NormalTexture.Sample(MaterialSampler, UV).xyz * 2.0f - 1.0f };
    const float3 N = { normalize(T * TangentNormal.x + B * -TangentNormal.y + VertexNormal * TangentNormal.z) };
#endif

    // Opaque는 알파를 1로 고정한다. 뷰포트 RT를 ImGui가 알파 블렌딩으로 그리므로 알파가 남으면 비쳐 보인다
    const float Alpha = { bOpaque > 0.5f ? 1.0f : Albedo.a };

#if LIGHTING_MODEL_UNLIT
    return float4(Albedo.rgb, Alpha);
#elif WORLD_NORMAL
    return float4(N * 0.5f + 0.5f, 1.0f);
#else
    float3 DiffuseLighting = { GetAmbientLighting() };
    float3 SpecularLighting = { 0.0f, 0.0f, 0.0f };

#if defined(LIGHTING_MODEL_LAMBERT) || defined(LIGHTING_MODEL_BLINN_PHONG)
    const float3 V = { normalize(CameraWorldPos - Input.WorldPos) };
    const FLightContribution Contribution = { AccumulateDirectLighting(N, V, Input.WorldPos, Shininess, Input.SvPosition, true) };
    DiffuseLighting += Contribution.Diffuse;
    SpecularLighting = Contribution.Specular;
#elif LIGHTING_MODEL_GOURAUD
    DiffuseLighting += Input.VertexDiffuse;
#endif

    float3 FinalColor = Albedo.rgb * DiffuseLighting + SpecularLighting;
    return float4(FinalColor, Alpha);
#endif
}
