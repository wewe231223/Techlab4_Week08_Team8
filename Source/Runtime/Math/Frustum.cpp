#include "EnginePCH.h"
#include "Frustum.h"

static FPlane NormalizePlane(const FPlane Plane)
{
    const FVector& N = Plane.Normal;
    const float Length = sqrtf(N.X * N.X + N.Y * N.Y + N.Z * N.Z);
    assert(Length > 1.0e-6f);

    const float InvLength = 1.0f / Length;
    return {
        { N.X * InvLength, N.Y * InvLength, N.Z * InvLength },
        Plane.Distance * InvLength
    };
}

FFrustumPlanes ExtractFrustumPlanes(const FMatrix& Matrix)
{
    auto MakePlane = [&Matrix](const int ColumnA, const float ScaleA, const int ColumnB, const float ScaleB)
        {
            return NormalizePlane({
                { ScaleA * Matrix.M[0][ColumnA] + ScaleB * Matrix.M[0][ColumnB], ScaleA * Matrix.M[1][ColumnA] + ScaleB * Matrix.M[1][ColumnB], ScaleA * Matrix.M[2][ColumnA] + ScaleB * Matrix.M[2][ColumnB] },
                ScaleA * Matrix.M[3][ColumnA] + ScaleB * Matrix.M[3][ColumnB] });
        };
    FFrustumPlanes Result{};
    Result.Planes[0] = MakePlane(3, 1.0f, 0, 1.0f);
    Result.Planes[1] = MakePlane(3, 1.0f, 0, -1.0f);
    Result.Planes[2] = MakePlane(3, 1.0f, 1, 1.0f);
    Result.Planes[3] = MakePlane(3, 1.0f, 1, -1.0f);
    Result.Planes[4] = NormalizePlane({ { Matrix.M[0][2], Matrix.M[1][2], Matrix.M[2][2] }, Matrix.M[3][2] });
    Result.Planes[5] = MakePlane(3, 1.0f, 2, -1.0f);
    return Result;
}

bool IsAABBInFrustum(const FAABB& Bounds, const FFrustumPlanes& Frustum)
{
    FVectorRegister Boundreg = VectorSIMD::LoadFloat3(&Bounds.Extent.X);
    FVectorRegister Centerreg = VectorSIMD::LoadFloat3(&Bounds.Center.X);
    for (const FPlane& Plane : Frustum.Planes)
    {
        FVectorRegister Planereg = VectorSIMD::LoadFloat3(&Plane.Normal.X);
        FVectorRegister absPlanereg = VectorSIMD::Abs(Planereg);

        const float Radius = VectorSIMD::Dot(absPlanereg, Boundreg);
        const float CenterDist = VectorSIMD::Dot(Planereg, Centerreg);

        if (CenterDist + Plane.Distance + Radius < 0.0f)
        {
            return false;
        }
    }
    return true;
}


FAABB MakeWorldBounds(const FBox& Value)
{
    const FVector Center = (Value.Min + Value.Max) * 0.5f;
    const FVector Extent = (Value.Max - Value.Min) * 0.5f;
    return { Center, Extent };
}

// 작은 행렬식을 특이행렬로 오판하지 않도록 double 부분 피벗 소거로 Grid 전용 역행렬을 구한다.
bool TryInvertGridViewProjection(const FMatrix& Matrix, FMatrix& Inverse)
{
    double Augmented[4][8]{};
    for (int Row = 0; Row < 4; ++Row)
    {
        for (int Column = 0; Column < 4; ++Column)
        {
            if (!std::isfinite(Matrix.M[Row][Column])) return false;
            Augmented[Row][Column] = Matrix.M[Row][Column];
        }
        Augmented[Row][Row + 4] = 1.0;
    }
    for (int Column = 0; Column < 4; ++Column)
    {
        int PivotRow = Column;
        for (int Row = Column + 1; Row < 4; ++Row)
            if (std::fabs(Augmented[Row][Column]) > std::fabs(Augmented[PivotRow][Column]))
                PivotRow = Row;
        const double Pivot = Augmented[PivotRow][Column];
        if (!std::isfinite(Pivot) || Pivot == 0.0) return false;
        for (int Entry = 0; Entry < 8; ++Entry)
            std::swap(Augmented[Column][Entry], Augmented[PivotRow][Entry]);
        for (int Entry = 0; Entry < 8; ++Entry) Augmented[Column][Entry] /= Pivot;
        for (int Row = 0; Row < 4; ++Row)
        {
            if (Row == Column) continue;
            const double Factor = Augmented[Row][Column];
            for (int Entry = 0; Entry < 8; ++Entry)
                Augmented[Row][Entry] -= Factor * Augmented[Column][Entry];
        }
    }
    for (int Row = 0; Row < 4; ++Row)
        for (int Column = 0; Column < 4; ++Column)
        {
            const double Value = Augmented[Row][Column + 4];
            if (!std::isfinite(Value) || std::fabs(Value) > std::numeric_limits<float>::max()) return false;
            Inverse.M[Row][Column] = static_cast<float>(Value);
        }
    return true;
}

bool BuildGridFrustum(const FMatrix& ViewProj, FGridFrustum& Result)
{
    FMatrix Inverse;
    if (!TryInvertGridViewProjection(ViewProj, Inverse)) return false;
    Result.AxisExtent = FVector(1, 1, 1);
    for (int Index = 0; Index < 8; ++Index)
    {
        const FVector4 H = FVector4(Index & 1 ? 1.0f : -1.0f,
            Index & 2 ? 1.0f : -1.0f, Index & 4 ? 1.0f : 0.0f, 1.0f) * Inverse;
        if (!std::isfinite(H.W) || std::fabs(H.W) < 1.0e-12f) return false;
        const FVector P(H.X / H.W, H.Y / H.W, H.Z / H.W);
        if (!std::isfinite(P.X) || !std::isfinite(P.Y) || !std::isfinite(P.Z)) return false;
        Result.Corners[Index] = P;
        Result.AxisExtent.X = std::max(Result.AxisExtent.X, std::fabs(P.X) + 1.0f);
        Result.AxisExtent.Y = std::max(Result.AxisExtent.Y, std::fabs(P.Y) + 1.0f);
        Result.AxisExtent.Z = std::max(Result.AxisExtent.Z, std::fabs(P.Z) + 1.0f);
    }
    return true;
}