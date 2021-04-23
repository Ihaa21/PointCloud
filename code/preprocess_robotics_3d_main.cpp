#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <windows.h>
#include "math\math.h"
#include "memory\memory.h"
#include "preprocess_robotics_3d_main.h"

int main(int argc, char** argv)
{    
    // NOTE: Give the file names here
    char* PcFileNames[] =
        {
            "wue_city\\scan000.3d",
            "wue_city\\scan001.3d",
            "wue_city\\scan002.3d",
            "wue_city\\scan003.3d",
            "wue_city\\scan004.3d",
            "wue_city\\scan005.3d",
        };

    char* PoseFileNames[] =
        {
            "wue_city\\scan000.pose",
            "wue_city\\scan001.pose",
            "wue_city\\scan002.pose",
            "wue_city\\scan003.pose",
            "wue_city\\scan004.pose",
            "wue_city\\scan005.pose",
        };

    FILE* OutFile = fopen("wue_city.bin", "wb");

    // NOTE: Generate point cloud for specified dataset
    {
        u64 ArenaSize = GigaBytes(8);
        linear_arena Arena = LinearArenaCreate(malloc(ArenaSize), ArenaSize);

        u32 TotalNumPoints = 0;
        u32 NumPoints[ArrayCount(PcFileNames)] = {};
        u64 FileSizes[ArrayCount(PcFileNames)] = {};
            
        // NOTE: Calculate total num points
        for (u32 FileId = 0; FileId < ArrayCount(PcFileNames); ++FileId)
        {
            temp_mem TempMem = BeginTempMem(&Arena);

            FILE* File = fopen(PcFileNames[FileId], "rb");

            // NOTE: Get file sizes
            fseek(File, 0L, SEEK_END);
            FileSizes[FileId] = ftell(File);
            fseek(File, 0L, SEEK_SET);

            char* FileData = (char*)PushSize(&Arena, FileSizes[FileId]);
            mm Test = fread(FileData, FileSizes[FileId], 1, File);
                
            // NOTE: Count # of new lines
            for (u64 CharId = 0; CharId < (FileSizes[FileId] / sizeof(char)); ++CharId)
            {
                NumPoints[FileId] += FileData[CharId] == '\n' ? 1 : 0;
            }
            TotalNumPoints += NumPoints[FileId];
                                
            fclose(File);
                
            EndTempMem(TempMem);
        }

        // NOTE: Populate our buffer with points
        pc_point* OutPointCloud = PushArray(&Arena, pc_point, TotalNumPoints);
        pc_point* CurrPoint = OutPointCloud;
        for (u32 FileId = 0; FileId < ArrayCount(PcFileNames); ++FileId)
        {
            temp_mem TempMem = BeginTempMem(&Arena);

            m4 PoseMat = {};
            {
                FILE* PoseFile = fopen(PoseFileNames[FileId], "rb");

                f32 PosX, PosY, PosZ, RotX, RotY, RotZ;
                fscanf(PoseFile, "%f %f %f\n%f %f %f", &PosX, &PosY, &PosZ, &RotX, &RotY, &RotZ);

                PoseMat = M4Pos(V3(PosX, PosY, PosZ)) * M4Rotation(V3(DegreeToRadians(RotX), DegreeToRadians(RotY), DegreeToRadians(RotZ))) * M4Scale(V3(1));
                    
                fclose(PoseFile);
            }
                
            FILE* PCFile = fopen(PcFileNames[FileId], "rb");

            for (u32 PointId = 0; PointId < NumPoints[FileId]; ++PointId, ++CurrPoint)
            {
                f32 Flt1, Flt2, Flt3, Flt4;

                fscanf(PCFile, "%f %f %f %f\n", &Flt1, &Flt2, &Flt3, &Flt4);
                u32 ColorChannel = u8(Abs(Flt4) * 5.0f);
                
                CurrPoint->Pos =  (PoseMat * V4(Flt1, Flt2, Flt3, 1.0f)).xyz;
                CurrPoint->Color = (ColorChannel << 0) | (ColorChannel << 8) | (ColorChannel << 16) | (0xFF << 24);
            }
                
            fclose(PCFile);
            EndTempMem(TempMem);
        }

        fwrite(&TotalNumPoints, 1, sizeof(u32), OutFile);
        fwrite(OutPointCloud, TotalNumPoints, sizeof(pc_point), OutFile);
    }

    fclose(OutFile);

    return 1;
}
