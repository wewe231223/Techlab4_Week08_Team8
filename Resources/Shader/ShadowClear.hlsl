float4 mainVS(uint VertexId : SV_VertexID) : SV_POSITION {
    const float2 Position = float2((VertexId << 1) & 2, VertexId & 2);

    return float4(Position * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 1.0f, 1.0f);
}
