#ifndef VSSM_COMMON_HLSLI
#define VSSM_COMMON_HLSLI

// Each moment has a 64-bit unsigned accumulator (low, high), emulated on SM5.
// Subtract before converting to float: small rectangles must not lose precision
// by subtracting two large floating-point prefix sums.
static const float VssmMomentScale = 1073741824.0f; // 2^30, depth normalized by Far.

uint2 VssmAdd64(uint2 A, uint2 B) {
    uint Low = A.x + B.x;
    return uint2(Low, A.y + B.y + (Low < A.x ? 1u : 0u));
}

uint2 VssmSub64(uint2 A, uint2 B) {
    return uint2(A.x - B.x, A.y - B.y - (A.x < B.x ? 1u : 0u));
}

uint4 VssmAdd(uint4 A, uint4 B) {
    return uint4(VssmAdd64(A.xy, B.xy), VssmAdd64(A.zw, B.zw));
}

uint4 VssmSubtract(uint4 A, uint4 B) {
    return uint4(VssmSub64(A.xy, B.xy), VssmSub64(A.zw, B.zw));
}

float2 VssmDecode(uint4 Value) {
    return (float2(Value.x, Value.z) + float2(Value.y, Value.w) * 4294967296.0f) / VssmMomentScale;
}

float VssmLinearDepth(float Depth, float2 NearFar, uint Orthographic) {
    return Orthographic != 0 ? lerp(NearFar.x, NearFar.y, Depth) :
        NearFar.x * NearFar.y / max(NearFar.y - Depth * (NearFar.y - NearFar.x), 1.0e-8f);
}

#endif
