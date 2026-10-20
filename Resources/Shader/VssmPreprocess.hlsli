#include "VssmCommon.hlsli"

cbuffer VssmBuildConstants : register(b0) {
    uint2 TileOrigin;
    uint Resolution;
    uint SatOffset;
    uint MinMaxOffset;
    uint SourceOffset;
    uint SourceResolution;
    uint DestinationOffset;
    float2 NearFar;
    uint Orthographic;
    uint BuildPadding;
};

Texture2D<float> SourceDepth : register(t0);
StructuredBuffer<uint4> RowPrefixes : register(t1);
RWStructuredBuffer<uint4> PrefixOutput : register(u0);
RWStructuredBuffer<float2> MinMaxOutput : register(u1);
groupshared uint4 ScanValues[2048];

// A group scans one complete row/column. Every shadow tile has its own prefix
// origin and packed buffer range; neither adjacent lights nor cube faces mix.
void ScanLine(uint Lane) {
    for (uint Step = 1; Step < Resolution; Step <<= 1) {
        uint4 Values[8];
        [unroll] for (uint Chunk = 0; Chunk < 8; ++Chunk) {
            uint Index = Lane + Chunk * 256;
            Values[Chunk] = Index < Resolution && Index >= Step ? ScanValues[Index - Step] : uint4(0, 0, 0, 0);
        }
        GroupMemoryBarrierWithGroupSync();
        [unroll] for (uint StoreChunk = 0; StoreChunk < 8; ++StoreChunk) {
            uint Index = Lane + StoreChunk * 256;
            if (Index < Resolution) ScanValues[Index] = VssmAdd(ScanValues[Index], Values[StoreChunk]);
        }
        GroupMemoryBarrierWithGroupSync();
    }
}

[numthreads(256, 1, 1)]
void ScanRows(uint3 Group : SV_GroupID, uint Lane : SV_GroupIndex) {
    [unroll] for (uint OutputChunk = 0; OutputChunk < 8; ++OutputChunk) {
        uint X = Lane + OutputChunk * 256;
        if (X < Resolution) {
            float Depth = SourceDepth.Load(int3(TileOrigin + uint2(X, Group.x), 0));
            float Z = saturate(VssmLinearDepth(Depth, NearFar, Orthographic) / NearFar.y);
            ScanValues[X] = uint4((uint)round(Z * VssmMomentScale), 0,
                (uint)round(Z * Z * VssmMomentScale), 0);
        }
    }
    GroupMemoryBarrierWithGroupSync();
    ScanLine(Lane);
    [unroll] for (uint Chunk = 0; Chunk < 8; ++Chunk) {
        uint X = Lane + Chunk * 256;
        // Transpose intermediate data so the column pass reads contiguous rows.
        if (X < Resolution) PrefixOutput[SatOffset + X * Resolution + Group.x] = ScanValues[X];
    }
}

[numthreads(256, 1, 1)]
void ScanColumns(uint3 Group : SV_GroupID, uint Lane : SV_GroupIndex) {
    [unroll] for (uint OutputChunk = 0; OutputChunk < 8; ++OutputChunk) {
        uint Y = Lane + OutputChunk * 256;
        if (Y < Resolution) ScanValues[Y] = RowPrefixes[SatOffset + Group.x * Resolution + Y];
    }
    GroupMemoryBarrierWithGroupSync();
    ScanLine(Lane);
    [unroll] for (uint Chunk = 0; Chunk < 8; ++Chunk) {
        uint Y = Lane + Chunk * 256;
        if (Y < Resolution) PrefixOutput[SatOffset + Y * Resolution + Group.x] = ScanValues[Y];
    }
}

// The base hierarchy summarizes 16x16 texels. Bounds are conservative even for
// queries smaller than a base cell. This keeps the auxiliary hierarchy small.
[numthreads(8, 8, 1)]
void BuildMinMax(uint3 Id : SV_DispatchThreadID) {
    uint Size = Resolution / 16;
    if (any(Id.xy >= Size)) return;
    float2 Bounds = float2(1.0f, 0.0f);
    [loop] for (uint Y = 0; Y < 16; ++Y) {
        [loop] for (uint X = 0; X < 16; ++X) {
            float Depth = SourceDepth.Load(int3(TileOrigin + Id.xy * 16 + uint2(X, Y), 0));
            float Z = saturate(VssmLinearDepth(Depth, NearFar, Orthographic) / NearFar.y);
            Bounds = float2(min(Bounds.x, Z), max(Bounds.y, Z));
        }
    }
    MinMaxOutput[MinMaxOffset + Id.y * Size + Id.x] = Bounds;
}

[numthreads(8, 8, 1)]
void ReduceMinMax(uint3 Id : SV_DispatchThreadID) {
    uint Size = SourceResolution / 2;
    if (any(Id.xy >= Size)) return;
    uint Index = MinMaxOffset + SourceOffset + Id.y * 2 * SourceResolution + Id.x * 2;
    float2 A = MinMaxOutput[Index];
    float2 B = MinMaxOutput[Index + 1];
    float2 C = MinMaxOutput[Index + SourceResolution];
    float2 D = MinMaxOutput[Index + SourceResolution + 1];
    MinMaxOutput[MinMaxOffset + DestinationOffset + Id.y * Size + Id.x] =
        float2(min(min(A.x, B.x), min(C.x, D.x)), max(max(A.y, B.y), max(C.y, D.y)));
}
