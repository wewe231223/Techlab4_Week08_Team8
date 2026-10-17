#pragma pack_matrix(row_major)

cbuffer ShadowConstants : register(b0)
{
    matrix shadowviewproj;
};

cbuffer Worldconstants : register(b2)
{
    matrix World;
};

struct VS_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    
    output.position = mul(mul(float4(input.position, 1.0f), World), shadowviewproj);
    output.normal = input.normal;
    
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    return float4(0.0f, 0.0f, 0.0f, 1.0f);
}