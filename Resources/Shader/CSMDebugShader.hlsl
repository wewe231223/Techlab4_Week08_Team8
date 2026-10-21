#pragma pack_matrix(row_major)

Texture2D<float> SceneDepth : register(t0);

cbuffer FCSMDebugConstants : register(b0)
{
    float4x4 InverseViewProjection;
    float4x4 ViewProjection;
    float4 CascadeSplits;
    float CascadeTransitionFraction;
    uint CascadeCount;
    float2 Padding;
};

struct PSInput
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
};

PSInput mainVS(uint VertexID : SV_VertexID)
{
    PSInput Output;
    float2 UV = float2((VertexID << 1) & 2, VertexID & 2);
    Output.Position = float4(UV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    Output.UV = UV;
    return Output;
}

float4 mainPS(PSInput Input) : SV_Target
{
    float Depth = SceneDepth.Load(int3(Input.Position.xy, 0));
    // 스카이박스나 원거리 배경(Depth >= 1.0f)은 디버그 색상을 입히지 않는다
    if (Depth >= 1.0f || CascadeCount <= 1)
    {
        discard;
    }

    float2 NDC = Input.UV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
    float4 WorldPos = mul(float4(NDC, Depth, 1.0f), InverseViewProjection);
    WorldPos /= WorldPos.w;

    // ViewProjection을 통해 원근 뷰 깊이(Clip.w) 계산
    const float ViewDepth = mul(float4(WorldPos.xyz, 1.0f), ViewProjection).w;

    static const float3 CascadeColors[4] =
    {
        float3(1.0f, 0.25f, 0.25f), // Cascade 0: Red
        float3(0.25f, 1.0f, 0.25f), // Cascade 1: Green
        float3(0.25f, 0.25f, 1.0f), // Cascade 2: Blue
        float3(1.0f, 1.0f, 0.25f)   // Cascade 3: Yellow
    };

    int CascadeIdx = -1;
    float NearSplit = 0.0f;
    float FarSplit = 0.0f;

    for (uint i = 0; i < CascadeCount; ++i)
    {
        if (ViewDepth < CascadeSplits[i])
        {
            CascadeIdx = (int) i;
            FarSplit = CascadeSplits[i];
            break;
        }
        NearSplit = CascadeSplits[i];
    }

    if (CascadeIdx < 0)
    {
        discard;
    }

    float3 Color = CascadeColors[min((uint) CascadeIdx, 3u)];
    if (CascadeIdx < (int) CascadeCount - 1 && CascadeTransitionFraction > 0.0f)
    {
        float CascadeRange = FarSplit - NearSplit;
        float DistToSplit = FarSplit - ViewDepth;
        float BlendRange = CascadeRange * CascadeTransitionFraction;

        if (DistToSplit < BlendRange && BlendRange > 0.0f)
        {
            float BlendRatio = 1.0f - (DistToSplit / BlendRange);
            float3 NextColor = CascadeColors[min((uint) (CascadeIdx + 1), 3u)];
            Color = lerp(Color, NextColor, BlendRatio);
        }
    }

    // 곱셈 블렌딩(Dest * Src)을 통해 기존 씬 색상/음영을 보존하며 캐스케이드 틴트 적용
    return float4(Color, 1.0f);
}
