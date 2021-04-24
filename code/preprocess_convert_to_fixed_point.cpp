#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <algorithm>
#include <vector>
#include <windows.h>
#include "math\math.h"
#include "memory\memory.h"
#include "preprocess_convert_to_fixed_point.h"

int main(int argc, char** argv)
{
    FILE* InFile = fopen("wue_city_sorted.bin", "rb");
    FILE* OutFile = fopen("wue_city_sorted_fp.bin", "wb");

    {
        u64 ArenaSize = GigaBytes(8);
        linear_arena Arena = LinearArenaCreate(malloc(ArenaSize), ArenaSize);

        u32 TotalNumPoints = 0;
        fread(&TotalNumPoints, sizeof(u32), 1, InFile);
        pc_point* SrcPoints = PushArray(&Arena, pc_point, TotalNumPoints);
        fread(SrcPoints, sizeof(pc_point), TotalNumPoints, InFile);

        fclose(InFile);

        v3 MinBounds = V3(F32_MAX);
        v3 MaxBounds = V3(F32_MIN);
        for (u32 PointId = 0; PointId < TotalNumPoints; ++PointId)
        {
            MinBounds = Min(MinBounds, SrcPoints[PointId].Pos);
            MaxBounds = Max(MaxBounds, SrcPoints[PointId].Pos);
        }

        v3 CubeRadius = (MaxBounds - MinBounds) / 2.0f;
        v3 CubeShift = MaxBounds - CubeRadius;

        // NOTE: Convert to fixed point format
        pc_fixed_point* OutPoints = PushArray(&Arena, pc_fixed_point, TotalNumPoints);
        for (u32 PointId = 0; PointId < TotalNumPoints; ++PointId)
        {
            OutPoints[PointId].Color = SrcPoints[PointId].Color;

            // NOTE: Encode each point in fixed point 24bits per dimension
            v3 PosFlt = SrcPoints[PointId].Pos;
            v3 PosFltNormalized = (PosFlt - CubeShift) / CubeRadius;

            // NOTE: Convert to 32bit fixed point
            PosFltNormalized *= -I32_MIN;
            i32 PosX = i32(PosFltNormalized.x + 0.5f);
            i32 PosY = i32(PosFltNormalized.y + 0.5f);
            i32 PosZ = i32(PosFltNormalized.z + 0.5f);

            // NOTE: Only keep top 21 bits
            i64 PosX64 = PosX;
            i64 PosY64 = PosY;
            i64 PosZ64 = PosZ;
            PosX64 = (PosX64 >> 11u) & 0x1FFFFF;
            PosY64 = (PosY64 >> 11u) & 0x1FFFFF;
            PosZ64 = (PosZ64 >> 12u) & 0x0FFFFF;

            u64 PosXU64 = *((u64*)&PosX64);
            u64 PosYU64 = *((u64*)&PosY64);
            u64 PosZU64 = *((u64*)&PosZ64);
            
            // NOTE: Pack as a U64
            u64 PackedPos = (PosXU64 << 0u) | (PosYU64 << 21u) | (PosZU64 << 42u);

            OutPoints[PointId].Pos = PackedPos;
        }
        
        fwrite(&TotalNumPoints, sizeof(u32), 1, OutFile);
        fwrite(&CubeRadius, sizeof(v3), 1, OutFile);
        fwrite(OutPoints, sizeof(pc_fixed_point), TotalNumPoints, OutFile);
    }

    fclose(OutFile);

    return 1;
}
