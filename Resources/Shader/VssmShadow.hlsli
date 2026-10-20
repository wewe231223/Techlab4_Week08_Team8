#ifndef VSSM_SHADOW_HLSLI
#define VSSM_SHADOW_HLSLI
#ifndef VSSM_COMMON_HLSLI
#include "VssmCommon.hlsli"
#endif

StructuredBuffer<uint4> VssmSat : register(t16);
StructuredBuffer<float2> VssmMinMax : register(t17);
// Float moment reconstruction can leave a few ulps of variance on a flat plane.
static const float VssmPlanarVariance = 2.0e-7f;

uint4 VssmPrefix(int2 P, FShadowMapRenderParameters S) {
    uint4 Value = uint4(0,0,0,0);
    if (all(P >= 0)) Value = VssmSat[S.mVssmSatOffset + (uint)P.y * S.mVssmResolution + (uint)P.x];
    return Value;
}

// Integer boundaries partition adjacent cells without gaps or overlaps. The
// rectangle is clipped to this view, never to a neighboring atlas allocation.
float3 VssmMoments(int2 Lo, int2 Hi, FShadowMapRenderParameters S) {
    Lo = clamp(Lo, 0, (int)S.mVssmResolution);
    Hi = clamp(Hi, 0, (int)S.mVssmResolution);
    float3 Value = float3(0,0,0);
    if (all(Hi > Lo)) {
        uint4 Sum = VssmSubtract(VssmPrefix(Hi - 1, S), VssmPrefix(int2(Lo.x - 1, Hi.y - 1), S));
        Sum = VssmAdd(VssmSubtract(Sum, VssmPrefix(int2(Hi.x - 1, Lo.y - 1), S)), VssmPrefix(Lo - 1, S));
        float Area = (float)(Hi.x - Lo.x) * (Hi.y - Lo.y);
        Value = float3(VssmDecode(Sum) / Area, Area);
    }
    return Value;
}

float2 VssmBounds(int2 Lo, int2 Hi, FShadowMapRenderParameters S) {
    Lo = clamp(Lo, 0, (int)S.mVssmResolution - 1) >> 4;
    Hi = clamp(Hi - 1, 0, (int)S.mVssmResolution - 1) >> 4;
    uint Size = S.mVssmResolution / 16;
    uint Offset = S.mVssmMinMaxOffset;
    [loop] while (any(Hi - Lo > 1)) {
        Offset += Size * Size;
        Size /= 2;
        Lo >>= 1;
        Hi >>= 1;
    }
    float2 Bounds = float2(1, 0);
    [unroll] for (int Y = 0; Y < 2; ++Y) {
        [unroll] for (int X = 0; X < 2; ++X) {
            int2 P = Lo + int2(X, Y);
            if (all(P <= Hi)) {
                float2 B = VssmMinMax[Offset + P.y * Size + P.x];
                Bounds = float2(min(Bounds.x, B.x), max(Bounds.y, B.y));
            }
        }
    }
    return Bounds;
}

float VssmVisibility(float2 Moments, float Receiver) {
    float Visibility = 1.0f;
    if (Receiver > Moments.x) {
        float Variance = max(Moments.y - Moments.x * Moments.x, 0.0f);
        float Delta = Receiver - Moments.x;
        Visibility = saturate(Variance / max(Variance + Delta * Delta, 1.0e-12f));
    }
    return Visibility;
}

float VssmReadDepth(int2 Texel, FShadowMapRenderParameters S) {
    int2 Origin = int2(round(S.mUvScaleBias.zw / S.mTexelSize));
    float Depth = ShadowAtlas.Load(int3(Origin + Texel, 0));
    return VssmLinearDepth(Depth, S.mNearFar, S.mIsOrthographic) / S.mNearFar.y;
}

float2 VssmReceiverGradient(float4 Clip, float3 Normal, FShadowMapRenderParameters S) {
    // Analytic plane derivatives also work inside a divergent light loop.
    float3 Axis = abs(Normal.z) < 0.9f ? float3(0,0,1) : float3(0,1,0);
    float3 Tangent = normalize(cross(Axis,Normal));
    float3 Bitangent = cross(Normal,Tangent);
    float4 A = mul(float4(Tangent,0),S.mLightViewProjection);
    float4 B = mul(float4(Bitangent,0),S.mLightViewProjection);
    float2 Dx = (A.xy*Clip.w-Clip.xy*A.w)/(Clip.w*Clip.w)*float2(0.5f,-0.5f);
    float2 Dy = (B.xy*Clip.w-Clip.xy*B.w)/(Clip.w*Clip.w)*float2(0.5f,-0.5f);
    // A plane's inverse linear depth is affine in perspective shadow UV.
    float Zx = S.mIsOrthographic != 0 ? A.z*(S.mNearFar.y-S.mNearFar.x)/S.mNearFar.y : -A.w*S.mNearFar.y/(Clip.w*Clip.w);
    float Zy = S.mIsOrthographic != 0 ? B.z*(S.mNearFar.y-S.mNearFar.x)/S.mNearFar.y : -B.w*S.mNearFar.y/(Clip.w*Clip.w);
    float Det = Dx.x * Dy.y - Dx.y * Dy.x;
    float2 Gradient = float2(0,0);
    if (abs(Det) >= 1.0e-12f) Gradient = float2(Zx * Dy.y - Zy * Dx.y, Zy * Dx.x - Zx * Dy.x) / Det;
    return Gradient;
}

float VssmReceiverAt(float2 DeltaUv, float Receiver, float2 Gradient, FShadowMapRenderParameters S) {
    float Depth = Receiver+dot(Gradient,DeltaUv);
    if (S.mIsOrthographic == 0) {
        float InverseDepth = rcp(max(Receiver,1.0e-8f))+dot(Gradient,DeltaUv);
        Depth = InverseDepth > 1.0e-8f ? rcp(InverseDepth) : 1.0e8f;
    }
    return Depth;
}

// Rebase each small cell to the receiver at the kernel center. Leaf comparisons
// additionally evaluate the receiver plane at the actual depth sample location.
float2 VssmAlignedMoments(float2 M, float2 CellUv, float2 Uv, float Receiver, float2 Gradient, FShadowMapRenderParameters S) {
    float Shift = VssmReceiverAt(CellUv-Uv,Receiver,Gradient,S)-Receiver;
    return float2(M.x - Shift, M.y - 2.0f * Shift * M.x + Shift * Shift);
}

float VssmBilinearPcf(float2 SampleUv, float2 Uv, float Receiver, float2 Gradient, FShadowMapRenderParameters S) {
    float2 Position = SampleUv * S.mVssmResolution - 0.5f;
    int2 Base = int2(floor(Position));
    float2 F = frac(Position);
    float Visibility = 0;
    [unroll] for (int Y = 0; Y < 2; ++Y) {
        [unroll] for (int X = 0; X < 2; ++X) {
            int2 P = Base + int2(X, Y);
            float Weight = (X == 0 ? 1-F.x : F.x) * (Y == 0 ? 1-F.y : F.y);
            float V = 1;
            if (all(P >= 0) && all(P < (int)S.mVssmResolution)) {
                float T = VssmReceiverAt((float2(P)+0.5f)/S.mVssmResolution-Uv,Receiver,Gradient,S);
                V = VssmReadDepth(P, S) + 1.0e-7f >= T ? 1.0f : 0.0f;
            }
            Visibility += V * Weight;
        }
    }
    return Visibility;
}

float VssmContactShadow(float2 Uv, float Receiver, float2 Gradient, FShadowMapRenderParameters S) {
    float Visibility = 0;
    [loop] for (int Y = -1; Y <= 1; ++Y) {
        [loop] for (int X = -1; X <= 1; ++X) {
            Visibility += VssmBilinearPcf(Uv + float2(X,Y) / S.mVssmResolution, Uv, Receiver, Gradient, S);
        }
    }
    return Visibility / 9.0f;
}

float2 VssmSampleBlockers(int2 Lo, int2 Hi, float2 Uv, float Receiver, float2 Gradient, FShadowMapRenderParameters S) {
    float2 SumCount = float2(0, 0);
    float Weight = (float)(Hi.x-Lo.x) * (Hi.y-Lo.y) * 0.25f;
    [unroll] for (uint Y = 0; Y < 2; ++Y) {
        [unroll] for (uint X = 0; X < 2; ++X) {
            int2 P = min(int2(lerp(float2(Lo), float2(Hi), (float2(X,Y)+0.5f)*0.5f)), Hi-1);
            if (all(P >= 0) && all(P < (int)S.mVssmResolution)) {
                float Z = VssmReadDepth(P, S);
                float Shift = VssmReceiverAt((float2(P)+0.5f)/S.mVssmResolution-Uv,Receiver,Gradient,S)-Receiver;
                if (Z + 1.0e-7f < Receiver + Shift) SumCount += float2(Z-Shift, 1) * Weight;
            }
        }
    }
    return SumCount;
}

// Paper section 5.2: accumulate moments of normal cells, and blocker-weighted
// depth/count from the 2x2 fallback cells. Do not average cell depths equally.
float VssmAverageBlocker(int2 Lo, int2 Hi, float2 Uv, float Receiver, float2 Gradient,
    float2 Bounds, FShadowMapRenderParameters S) {
    float3 NormalMoments = float3(0,0,0);
    float2 Blockers = float2(0,0);
    [loop] for (uint Y = 0; Y < 5; ++Y) {
        [loop] for (uint X = 0; X < 5; ++X) {
            int2 A = int2(round(lerp(float2(Lo), float2(Hi), float2(X,Y)/5.0f)));
            int2 B = int2(round(lerp(float2(Lo), float2(Hi), float2(X+1,Y+1)/5.0f)));
            float3 M = VssmMoments(A,B,S);
            if (M.z <= 0) continue;
            float2 C = (float2(max(A,0)) + float2(min(B,(int)S.mVssmResolution))) * 0.5f / S.mVssmResolution;
            M.xy = VssmAlignedMoments(M.xy,C,Uv,Receiver,Gradient,S);
            float Variance = max(M.y-M.x*M.x,0.0f);
            if (abs(M.x-Receiver) < 3.0e-5f) {
                Blockers += VssmSampleBlockers(A,B,Uv,Receiver,Gradient,S);
            } else if (M.x < Receiver) {
                NormalMoments += float3(M.xy*M.z,M.z);
            } else if (Variance > VssmPlanarVariance) {
                Blockers += VssmSampleBlockers(A,B,Uv,Receiver,Gradient,S);
            }
        }
    }
    if (NormalMoments.z > 0) {
        float2 M = NormalMoments.xy / NormalMoments.z;
        float Variance = max(M.y-M.x*M.x,0.0f);
        float P = VssmVisibility(M,Receiver);
        float Count = (1-P) * NormalMoments.z;
        // Algebraically equivalent to (mean - P*t)/(1-P), without cancellation.
        float Z = M.x - Variance / max(Receiver-M.x,1.0e-8f);
        Z = clamp(Z, Bounds.x, min(Receiver,Bounds.y));
        Blockers += float2(Z,1)*Count;
    }
    return Blockers.y > 1.0e-5f ? Blockers.x / Blockers.y : -1.0f;
}

uint2 VssmMortonPosition(uint Index) {
    return uint2((Index & 1) | ((Index >> 1) & 2) | ((Index >> 2) & 4),
                 ((Index >> 1) & 1) | ((Index >> 2) & 2) | ((Index >> 3) & 4));
}

// Stackless traversal of a depth-3 quad-tree, at most 64 leaves. Normal cell
// moments are pooled by area before the final VSM evaluation (paper Algorithm 2).
float VssmFilter(int2 Lo, int2 Hi, float2 Uv, float Receiver, float2 Gradient, FShadowMapRenderParameters S) {
    float TotalArea = max((float)(Hi.x-Lo.x)*(Hi.y-Lo.y),1.0f);
    float3 NormalMoments = float3(0,0,0);
    float LitArea = 0;
    uint Cursor = 0;
    [loop] while (Cursor < 64) {
        uint Depth = (Cursor & 63u) == 0 ? 0u : ((Cursor & 15u) == 0 ? 1u : ((Cursor & 3u) == 0 ? 2u : 3u));
        bool Descend = true;
        [loop] while (Descend) {
            uint Span = 64u >> (2*Depth);
            uint2 Cell = VssmMortonPosition(Cursor);
            uint Width = 8u >> Depth;
            int2 A = int2(round(lerp(float2(Lo),float2(Hi),float2(Cell)/8.0f)));
            int2 B = int2(round(lerp(float2(Lo),float2(Hi),float2(Cell+Width)/8.0f)));
            float Area = max((float)(B.x-A.x)*(B.y-A.y),0.0f);
            float3 M = VssmMoments(A,B,S);
            if (M.z == 0) {
                LitArea += Area;
                Cursor += Span;
                Descend = false;
                continue;
            }
            float2 C = (float2(max(A,0))+float2(min(B,(int)S.mVssmResolution))) * 0.5f / S.mVssmResolution;
            M.xy = VssmAlignedMoments(M.xy,C,Uv,Receiver,Gradient,S);
            float Variance = max(M.y-M.x*M.x,0.0f);
            if (M.x < Receiver-3.0e-5f) {
                NormalMoments += float3(M.xy*M.z,M.z);
                LitArea += Area-M.z;
            } else if (M.x >= Receiver-1.0e-7f && Variance < VssmPlanarVariance) {
                LitArea += Area;
            } else if (Depth < 3) {
                ++Depth;
                continue;
            } else {
                float Visibility = 0;
                [loop] for (uint Y = 0; Y < 2; ++Y) {
                    [loop] for (uint X = 0; X < 2; ++X) {
                        float2 SampleUv = lerp(float2(A),float2(B),(float2(X,Y)+0.5f)*0.5f) / S.mVssmResolution;
                        Visibility += VssmBilinearPcf(SampleUv,Uv,Receiver,Gradient,S);
                    }
                }
                LitArea += Visibility*0.25f*Area;
            }
            Cursor += Span;
            Descend = false;
        }
    }
    if (NormalMoments.z > 0) LitArea += VssmVisibility(NormalMoments.xy/NormalMoments.z,Receiver)*NormalMoments.z;
    return saturate(LitArea/TotalArea);
}

// Negative means statistics are unavailable or a cube seam requires PCSS.
float EvaluateVssmShadow(FShadowMapRenderParameters S, float2 Uv, float ReceiverWorld, float2 Gradient, bool CubeFace) {
    if (S.mVssmResolution == 0) return -1;
    bool Ortho = S.mIsOrthographic != 0;
    if ((Ortho && S.mSourceAngleTanHalf <= 0) || (!Ortho && all(S.mSourceRadius <= 0))) return -1;
    float Receiver = ReceiverWorld / S.mNearFar.y;
    float4x4 M = S.mLightViewProjection;
    float2 ProjectionScale = float2(length(float3(M[0][0],M[1][0],M[2][0])),length(float3(M[0][1],M[1][1],M[2][1])));
    float2 WorldToUv = ProjectionScale*0.5f / (Ortho ? 1.0f : ReceiverWorld);
    float Minimum = clamp(S.mMinCasterDepth,S.mNearFar.x,ReceiverWorld);
    float2 SearchRadius = Ortho ? WorldToUv*S.mSourceAngleTanHalf*max(ReceiverWorld-Minimum,0.0f) :
        WorldToUv*S.mSourceRadius*max(ReceiverWorld-Minimum,0.0f)/max(Minimum,1.0e-6f);
    bool SearchInside = all(Uv-SearchRadius >= 0) && all(Uv+SearchRadius <= 1);
    if (CubeFace && !SearchInside) return -1;
    int2 Lo = int2(round(saturate(Uv-SearchRadius)*S.mVssmResolution));
    int2 Hi = int2(round(saturate(Uv+SearchRadius)*S.mVssmResolution));
    if (any(Hi <= Lo)) return VssmContactShadow(Uv,Receiver,Gradient,S);
    float2 Bounds = VssmBounds(Lo,Hi,S);
    float2 A = float2(Lo)/S.mVssmResolution-Uv;
    float2 B = float2(Hi)/S.mVssmResolution-Uv;
    float4 PlaneDepths = float4(VssmReceiverAt(A,Receiver,Gradient,S),VssmReceiverAt(B,Receiver,Gradient,S),
        VssmReceiverAt(float2(A.x,B.y),Receiver,Gradient,S),VssmReceiverAt(float2(B.x,A.y),Receiver,Gradient,S));
    if (max(max(PlaneDepths.x,PlaneDepths.y),max(PlaneDepths.z,PlaneDepths.w)) <= Bounds.x+1.0e-7f) return 1;
    if (SearchInside && min(min(PlaneDepths.x,PlaneDepths.y),min(PlaneDepths.z,PlaneDepths.w)) > Bounds.y+1.0e-7f) return 0;
    float Blocker = VssmAverageBlocker(Lo,Hi,Uv,Receiver,Gradient,Bounds,S);
    if (Blocker < 0) return 1;
    float BlockerWorld = Blocker*S.mNearFar.y;
    float Separation = max(ReceiverWorld-BlockerWorld,0.0f);
    float2 Radius = Ortho ? WorldToUv*S.mSourceAngleTanHalf*Separation :
        WorldToUv*S.mSourceRadius*Separation/max(BlockerWorld,1.0e-6f);
    if (CubeFace && (any(Uv-Radius < 0) || any(Uv+Radius > 1))) return -1;
    if (max(Radius.x,Radius.y)*S.mVssmResolution < 1.5f || Separation < 3.0e-5f*S.mNearFar.y)
        return VssmContactShadow(Uv,Receiver,Gradient,S);
    // Integrate the in-tile part, and weight the out-of-frustum part as lit,
    // matching PCSS border behavior. Keep enormous source sizes out of int casts.
    Radius = max(Radius,0.5f/S.mVssmResolution);
    Lo = int2(round(saturate(Uv-Radius)*S.mVssmResolution));
    Hi = int2(round(saturate(Uv+Radius)*S.mVssmResolution));
    float2 Coverage = saturate((float2(Hi-Lo)/S.mVssmResolution)/max(2.0f*Radius,1.0e-8f));
    return lerp(1.0f,VssmFilter(Lo,Hi,Uv,Receiver,Gradient,S),Coverage.x*Coverage.y);
}

#endif
