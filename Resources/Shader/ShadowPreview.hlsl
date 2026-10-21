// 에디터 Details 패널의 그림자 맵 미리보기.
// ImGui는 Texture2D만 읽을 수 있으므로, 그림자 아틀라스(Texture2DArray)의 한 층에서 타일 영역만 잘라 2D 렌더 타깃에 그린다.
// 정점 버퍼 없이 화면을 덮는 삼각형 하나를 SV_VertexID로 만든다.

Texture2DArray<float> ShadowAtlas : register(t0);

cbuffer FShadowPreviewConstants : register(b0) {
    float2 UvMin; // 아틀라스 UV 기준 타일 시작점
    float2 UvMax; // 아틀라스 UV 기준 타일 끝점
    float2 NearFar;
    uint Slice; // 타일이 있는 아틀라스 층 번호
    uint IsOrthographic;
    uint LinearDepth;
    uint3 Padding;
};

struct PSInput {
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
};

PSInput mainVS(uint VertexID : SV_VertexID) {
    PSInput Output = { float4(0.0f, 0.0f, 0.0f, 0.0f), float2(0.0f, 0.0f) };
    float2 UV = { float2((VertexID << 1) & 2, VertexID & 2) }; // (0,0) (2,0) (0,2)
    Output.Position = float4(UV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    Output.UV = UV;
    return Output;
}

float4 mainPS(PSInput Input) : SV_Target {
    uint Width = { 0 };
    uint Height = { 0 };
    uint Elements = { 0 };
    ShadowAtlas.GetDimensions(Width, Height, Elements);

    // 미리보기 0~1 UV를 타일 영역으로 옮긴 뒤 텍셀 단위로 읽는다. 샘플러가 필요 없다.
    const float2 AtlasUv = { lerp(UvMin, UvMax, saturate(Input.UV)) };
    const int2 AtlasPixel = { min(int2(AtlasUv * float2(Width, Height)), int2(Width, Height) - 1) };
    const float Depth = { ShadowAtlas.Load(int4(AtlasPixel, Slice, 0)) };
    if (LinearDepth != 0) {
        const float NearPlane = { NearFar.x };
        const float FarPlane = { NearFar.y };
        const float ViewDepth = { IsOrthographic != 0 ? lerp(NearPlane, FarPlane, Depth) : NearPlane * FarPlane / max(FarPlane - Depth * (FarPlane - NearPlane), 1.0e-8f) };
        const float Shade = { saturate((ViewDepth - NearPlane) / max(FarPlane - NearPlane, 1.0e-8f)) };
        return float4(Shade.xxx, 1.0f);
    }
    return float4(Depth, 0.0f, 0.0f, 1.0f);
}
