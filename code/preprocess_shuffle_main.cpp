#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <algorithm>
#include <vector>
#include <windows.h>
#include "math\math.h"
#include "memory\memory.h"
#include "preprocess_shuffle_main.h"

b32 GroupSortComparison(sorted_group& A, sorted_group& B)
{
    b32 Result = A.RandValue < B.RandValue;
    return Result;
}

//
// NOTE: Morton Order Code
//

inline u64 SplitBy3(u32 Axis)
{
    u64 Result = Axis & 0x1fffff; // we only look Axist the first 21 bits
    Result = (Result | Result << 32) & 0x1f00000000ffff; // shift left 32 bits, OR with self, Axisnd 00011111000000000000000000000000000000001111111111111111
    Result = (Result | Result << 16) & 0x1f0000ff0000ff; // shift left 32 bits, OR with self, Axisnd 00011111000000000000000011111111000000000000000011111111
    Result = (Result | Result << 8) & 0x100f00f00f00f00f; // shift left 32 bits, OR with self, Axisnd 0001000000001111000000001111000000001111000000001111000000000000
    Result = (Result | Result << 4) & 0x10c30c30c30c30c3; // shift left 32 bits, OR with self, Axisnd 0001000011000011000011000011000011000011000011000011000100000000
    Result = (Result | Result << 2) & 0x1249249249249249;

    return Result;
}

inline u64 MortonEncode(u32 X, u32 Y, u32 Z)
{
    u64 Result = SplitBy3(X) | SplitBy3(Y) << 1 | SplitBy3(Z) << 2;
    return Result;
}

//
// NOTE: Shuffling Code
//

b32 MortonSortComparison(sorted_point& A, sorted_point& B)
{
    b32 Result = A.Key < B.Key;
    return Result;
}

inline pc_point* ShuffledMortonOrder(linear_arena* Arena, u32 TotalNumPoints, pc_point* SrcPoints)
{
    pc_point* Result = 0;
    
    v3 MinBounds = V3(F32_MAX);
    v3 MaxBounds = V3(F32_MIN);
    for (u32 PointId = 0; PointId < TotalNumPoints; ++PointId)
    {
        MinBounds = Min(MinBounds, SrcPoints[PointId].Pos);
        MaxBounds = Max(MaxBounds, SrcPoints[PointId].Pos);
    }

    f32 CubeSize = Max(Max(MaxBounds.x, MaxBounds.y), MaxBounds.z);

    // NOTE: Shuffle into morton order (https://github.com/m-schuetz/compute_rasterizer/blob/master/tools/sort_las/src/main.cpp)
    std::vector<sorted_point> SortedPoints = std::vector<sorted_point>();
    {
        SortedPoints.resize(TotalNumPoints);
        
        // NOTE: Generate morton keys
        f32 Factor = powf(2.0f, 21.0f);
        for (u32 PointId = 0; PointId < TotalNumPoints; ++PointId)
        {
            SortedPoints[PointId].Point = SrcPoints[PointId];

            v3 ReOrientedPos = (SrcPoints[PointId].Pos - MinBounds) / CubeSize;
            SortedPoints[PointId].Key = MortonEncode(u32(ReOrientedPos.x * Factor), u32(ReOrientedPos.y * Factor), u32(ReOrientedPos.z * Factor));
        }

        std::sort(SortedPoints.begin(), SortedPoints.end(), MortonSortComparison);
    }

#if 1
    // NOTE: Randomize groups of 128 points
    u32 NumSortedGroups = CeilU32(f32(TotalNumPoints) / 128.0f);
    std::vector<sorted_group> SortedGroups = std::vector<sorted_group>();
    {
        SortedGroups.resize(NumSortedGroups);

        for (u32 GroupId = 0; GroupId < NumSortedGroups; ++GroupId)
        {
            SortedGroups[GroupId].StartId = GroupId * 128;
            SortedGroups[GroupId].RandValue = rand();
        }

        std::sort(SortedGroups.begin(), SortedGroups.end(), GroupSortComparison);
    }

    // NOTE: Write out points into output format
    Result = PushArray(Arena, pc_point, TotalNumPoints);
    pc_point* CurrPoint = Result;
    for (u32 GroupId = 0; GroupId < NumSortedGroups; ++GroupId)
    {
        u32 Offset = SortedGroups[GroupId].StartId;
        for (u32 PointId = 0; PointId < 128 && (Offset + PointId) < TotalNumPoints; ++PointId, ++CurrPoint)
        {
            *CurrPoint = SortedPoints[Offset + PointId].Point;
        }
    }
#else
    // NOTE: Enable if you don't want random sort
    for (u32 PointId = 0; PointId < TotalNumPoints; ++PointId)
    {
        Result[PointId] = SortedPoints[PointId].Point;
    }
#endif

    return Result;
}    

int main(int argc, char** argv)
{
    FILE* InFile = fopen("wue_city.bin", "rb");
    FILE* OutFile = fopen("wue_city_sorted.bin", "wb");

    {
        u64 ArenaSize = GigaBytes(8);
        linear_arena Arena = LinearArenaCreate(malloc(ArenaSize), ArenaSize);

        u32 TotalNumPoints = 0;
        fread(&TotalNumPoints, sizeof(u32), 1, InFile);
        pc_point* SrcPoints = PushArray(&Arena, pc_point, TotalNumPoints);
        fread(SrcPoints, sizeof(pc_point), TotalNumPoints, InFile);

        fclose(InFile);

        pc_point* OutPoints = ShuffledMortonOrder(&Arena, TotalNumPoints, SrcPoints);
        
        fwrite(&TotalNumPoints, sizeof(u32), 1, OutFile);
        fwrite(OutPoints, sizeof(pc_point), TotalNumPoints, OutFile);
        //fwrite(SrcPoints, sizeof(pc_point), TotalNumPoints, OutFile);
    }
    
    fclose(InFile);
    fclose(OutFile);
    
    return 1;
}
