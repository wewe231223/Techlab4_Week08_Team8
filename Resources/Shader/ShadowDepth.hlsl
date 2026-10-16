cbuffer ShadowViewConstants : register(b0) {
    row_major float4x4 LightViewProjection;
};

cbuffer ShadowObjectConstants : register(b2) {
    row_major float4x4 World;
};

float4 mainVS(float3 Position : POSITION) : SV_POSITION {
    return mul(mul(float4(Position, 1.0f), World), LightViewProjection);
}
