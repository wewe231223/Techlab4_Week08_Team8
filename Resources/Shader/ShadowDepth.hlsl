#pragma pack_matrix(row_major)

// 슬롯은 UberLit과 같은 규칙을 따른다 (b0 = 패스/뷰, b2 = 오브젝트).
// ShadowDepthRenderer.h의 FShadowDepthPassConstants와 바이트 단위로 같아야 한다.
cbuffer ShadowDepthConstants : register(b0)
{
    matrix ViewProj;
};

// Renderer.h의 FPerObjectConstants와 바이트 단위로 같아야 한다.
cbuffer PerObjectConstants : register(b2)
{
    matrix World;
    matrix WorldInverseTranspose; // (World^-1)^T
};

Texture2D BaseColorTexture : register(t0);
SamplerState LinearSampler : register(s0);

// InputLayout을 이 시그니처에서 리플렉션으로 만들고 오프셋은 앞에서부터 쌓는다(APPEND_ALIGNED).
// 메시 정점 버퍼(Vertex.h FVertex)와 순서·크기가 같아야 UV가 Tangent 자리를 읽지 않는다.
struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float4 Tangent : TANGENT;
    float2 UV : TEXCOORD;
    float4 Color : COLOR;
};

struct VS_OUTPUT
{
    float4 SvPosition : SV_POSITION;
    float2 UV : TEXCOORD0;
};

VS_OUTPUT mainVS(VS_INPUT Input)
{
    VS_OUTPUT Output;
    Output.SvPosition = mul(mul(float4(Input.Position, 1.f), World), ViewProj);
    Output.UV = Input.UV;
    return Output;
}

// 셰이더 로더(FRenderResourceManager)가 VS·PS 둘 다 있어야 프로그램을 만들어서 빈 PS를 둔다.
// 불투명은 FShadowDepthRenderer가 PS를 null로 바인딩하므로 실제로 돌지 않는다.
void mainPS(VS_OUTPUT Input)
{
}

//TODO : 투명 객체를 위한 그림자 생성 시, PS가 포함된 별도의 셰이더 프로그램을 만들어야 한다.
//void mainPS(VS_OUTPUT Input)
//{
//    float Alpha = BaseColorTexture.Sample(LinearSampler, Input.UV);
//    clip(Alpha - AlphaCutoff);
//}