#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_EXT_shader_atomic_int64 : enable
#extension GL_KHR_shader_subgroup_vote : enable
#extension GL_KHR_shader_subgroup_arithmetic : enable
#extension GL_KHR_shader_subgroup_clustered : enable

layout(set = 0, binding = 0) uniform scene_globals
{
    mat4 WVP;
    uint RenderWidth;
    uint RenderHeight;
    uint NumPoints;
} SceneGlobals;

layout(set = 0, binding = 1) buffer point_cloud
{
    point PointCloud[];
};

layout(set = 0, binding = 2) buffer frame_buffer
{
    uint64_t FrameBuffer[];
};

layout(set = 0, binding = 3, rgba8) uniform image2D FrameBufferFloat;

#define WAVE_SIZE 32

//=========================================================================================================================================
// NOTE: NAIVE
//=========================================================================================================================================

#if NAIVE

layout(local_size_x = WAVE_SIZE, local_size_y = 1, local_size_z = 1) in;
void main()
{
    int WorkGroupId = int(gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x);
    int PointId = WorkGroupId * WAVE_SIZE + int(gl_LocalInvocationID.x);
    if (PointId < SceneGlobals.NumPoints)
    {
        point CurrPoint = PointCloud[PointId];

        // NOTE: Project Pos
        uint Depth;
        ivec2 PixelId;
        {
            vec4 ProjectedPos = (SceneGlobals.WVP * vec4(CurrPoint.Pos, 1));
            ProjectedPos.xyz /= ProjectedPos.w;

            // NOTE: Frustum Cull
            if (ProjectedPos.x <= -1.0f || ProjectedPos.x >= 1.0f ||
                ProjectedPos.y <= -1.0f || ProjectedPos.y >= 1.0f ||
                ProjectedPos.w <= 0.0f)
            {
                return;
            }
            
            ProjectedPos.xy = 0.5f * ProjectedPos.xy + vec2(0.5f);
            ProjectedPos.xy *= vec2(SceneGlobals.RenderWidth, SceneGlobals.RenderHeight);
            
            Depth = floatBitsToInt(ProjectedPos.z);
            PixelId = ivec2(ProjectedPos.xy);
        }

        uint64_t WritePixelValue = (uint64_t(Depth) << 32) | uint64_t(CurrPoint.Color);
        uint PixelIndex = PixelId.y * SceneGlobals.RenderWidth + PixelId.x;
        // IMPORTANT: We are rendering with reversed Z
        atomicMax(FrameBuffer[PixelIndex], WritePixelValue);
    }
}

#endif

//=========================================================================================================================================
// NOTE: DEPTH TEST
//=========================================================================================================================================

#if DEPTH_TEST

layout(local_size_x = WAVE_SIZE, local_size_y = 1, local_size_z = 1) in;
void main()
{
    int WorkGroupId = int(gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x);
    int PointId = WorkGroupId * WAVE_SIZE + int(gl_LocalInvocationID.x);
    if (PointId < SceneGlobals.NumPoints)
    {
        point CurrPoint = PointCloud[PointId];

        // NOTE: Project Pos
        uint Depth;
        ivec2 PixelId;
        {
            vec4 ProjectedPos = (SceneGlobals.WVP * vec4(CurrPoint.Pos, 1));
            ProjectedPos.xyz /= ProjectedPos.w;

            // NOTE: Frustum Cull
            if (ProjectedPos.x <= -1.0f || ProjectedPos.x >= 1.0f ||
                ProjectedPos.y <= -1.0f || ProjectedPos.y >= 1.0f ||
                ProjectedPos.w <= 0.0f)
            {
                return;
            }
            
            ProjectedPos.xy = 0.5f * ProjectedPos.xy + vec2(0.5f);
            ProjectedPos.xy *= vec2(SceneGlobals.RenderWidth, SceneGlobals.RenderHeight);
            
            Depth = floatBitsToInt(ProjectedPos.z);
            PixelId = ivec2(ProjectedPos.xy);
        }

        uint64_t WritePixelValue = (uint64_t(Depth) << 32) | uint64_t(CurrPoint.Color);
        uint PixelIndex = PixelId.y * SceneGlobals.RenderWidth + PixelId.x;

        // IMPORTANT: We are rendering with reversed Z
        if (WritePixelValue > FrameBuffer[PixelIndex])
        {
            atomicMax(FrameBuffer[PixelIndex], WritePixelValue);
        }
    }
}

#endif

//=========================================================================================================================================
// NOTE: NAIVE_WAVE_OPS
//=========================================================================================================================================

#if NAIVE_WAVE_OPS

layout(local_size_x = WAVE_SIZE, local_size_y = 1, local_size_z = 1) in;
void main()
{
    int WorkGroupId = int(gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x);
    int PointId = WorkGroupId * WAVE_SIZE + int(gl_LocalInvocationID.x);
    if (PointId < SceneGlobals.NumPoints)
    {
        point CurrPoint = PointCloud[PointId];

        // NOTE: Project Pos
        uint Depth;
        ivec2 PixelId;
        {
            vec4 ProjectedPos = (SceneGlobals.WVP * vec4(CurrPoint.Pos, 1));
            ProjectedPos.xyz /= ProjectedPos.w;

            // NOTE: Frustum Cull
            if (ProjectedPos.x <= -1.0f || ProjectedPos.x >= 1.0f ||
                ProjectedPos.y <= -1.0f || ProjectedPos.y >= 1.0f ||
                ProjectedPos.w <= 0.0f)
            {
                return;
            }
            
            ProjectedPos.xy = 0.5f * ProjectedPos.xy + vec2(0.5f);
            ProjectedPos.xy *= vec2(SceneGlobals.RenderWidth, SceneGlobals.RenderHeight);
            
            Depth = floatBitsToInt(ProjectedPos.z);
            PixelId = ivec2(ProjectedPos.xy);
        }

        uint PixelIndex = PixelId.y * SceneGlobals.RenderWidth + PixelId.x;
        
        if (subgroupAllEqual(PixelIndex))
        {
            // NOTE: Scalarized version
            uint MinDepth = subgroupMin(Depth);
            if (MinDepth == Depth)
            {
                if (subgroupElect())
                {
                    uint64_t WritePixelValue = (uint64_t(MinDepth) << 32) | uint64_t(CurrPoint.Color);
                    atomicMax(FrameBuffer[PixelIndex], WritePixelValue);
                }
            }
        }
        else
        {
            // IMPORTANT: We are rendering with reversed Z
            uint64_t WritePixelValue = (uint64_t(Depth) << 32) | uint64_t(CurrPoint.Color);
            atomicMax(FrameBuffer[PixelIndex], WritePixelValue);
        }
    }
}

#endif

//=========================================================================================================================================
// NOTE: DEPTH_WAVE_OPS
//=========================================================================================================================================

#if DEPTH_WAVE_OPS

layout(local_size_x = WAVE_SIZE, local_size_y = 1, local_size_z = 1) in;
void main()
{
    int WorkGroupId = int(gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x);
    int PointId = WorkGroupId * WAVE_SIZE + int(gl_LocalInvocationID.x);
    if (PointId < SceneGlobals.NumPoints)
    {
        point CurrPoint = PointCloud[PointId];

        // NOTE: Project Pos
        uint Depth;
        ivec2 PixelId;
        {
            vec4 ProjectedPos = (SceneGlobals.WVP * vec4(CurrPoint.Pos, 1));
            ProjectedPos.xyz /= ProjectedPos.w;

            // NOTE: Frustum Cull
            if (ProjectedPos.x <= -1.0f || ProjectedPos.x >= 1.0f ||
                ProjectedPos.y <= -1.0f || ProjectedPos.y >= 1.0f ||
                ProjectedPos.w <= 0.0f)
            {
                return;
            }
            
            ProjectedPos.xy = 0.5f * ProjectedPos.xy + vec2(0.5f);
            ProjectedPos.xy *= vec2(SceneGlobals.RenderWidth, SceneGlobals.RenderHeight);
            
            Depth = floatBitsToInt(ProjectedPos.z);
            PixelId = ivec2(ProjectedPos.xy);
        }

        uint PixelIndex = PixelId.y * SceneGlobals.RenderWidth + PixelId.x;
        
        if (subgroupAllEqual(PixelIndex))
        {
            // NOTE: Scalarized version
            uint MinDepth = subgroupMin(Depth);
            if (MinDepth == Depth)
            {
                if (subgroupElect())
                {
                    uint64_t WritePixelValue = (uint64_t(MinDepth) << 32) | uint64_t(CurrPoint.Color);
                    if (WritePixelValue > FrameBuffer[PixelIndex])
                    {
                        atomicMax(FrameBuffer[PixelIndex], WritePixelValue);
                    }
                }
            }
        }
        else
        {
            // IMPORTANT: We are rendering with reversed Z
            uint64_t WritePixelValue = (uint64_t(Depth) << 32) | uint64_t(CurrPoint.Color);
            if (WritePixelValue > FrameBuffer[PixelIndex])
            {
                atomicMax(FrameBuffer[PixelIndex], WritePixelValue);
            }
        }
    }
}

#endif


//=========================================================================================================================================
// NOTE: DEPTH_WAVE_OPS_2
//=========================================================================================================================================

#if DEPTH_WAVE_OPS_2

layout(local_size_x = WAVE_SIZE, local_size_y = 1, local_size_z = 1) in;
void main()
{
    int WorkGroupId = int(gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x);
    int PointId = WorkGroupId * WAVE_SIZE + int(gl_LocalInvocationID.x);
    if (PointId < SceneGlobals.NumPoints)
    {
        point CurrPoint = PointCloud[PointId];

        // NOTE: Project Pos
        uint Depth;
        ivec2 PixelId;
        {
            vec4 ProjectedPos = (SceneGlobals.WVP * vec4(CurrPoint.Pos, 1));
            ProjectedPos.xyz /= ProjectedPos.w;

            // NOTE: Frustum Cull
            if (ProjectedPos.x <= -1.0f || ProjectedPos.x >= 1.0f ||
                ProjectedPos.y <= -1.0f || ProjectedPos.y >= 1.0f ||
                ProjectedPos.w <= 0.0f)
            {
                return;
            }
            
            ProjectedPos.xy = 0.5f * ProjectedPos.xy + vec2(0.5f);
            ProjectedPos.xy *= vec2(SceneGlobals.RenderWidth, SceneGlobals.RenderHeight);
            
            Depth = floatBitsToInt(ProjectedPos.z);
            PixelId = ivec2(ProjectedPos.xy);
        }

        uint PixelIndex = PixelId.y * SceneGlobals.RenderWidth + PixelId.x;

        uint MinDepth = Depth;
        bool FastPath = subgroupAllEqual(PixelIndex);
        if (FastPath) // fast path for min
        {       
            MinDepth = subgroupMin(MinDepth);
        }

        // no fast path or thread was depth comp winner
        if (MinDepth == Depth)
        {
            uint64_t WritePixelValue = (uint64_t(Depth) << 32) | uint64_t(CurrPoint.Color);

            // last chance for reduction
            if (FastPath
                || subgroupClusteredXor(PixelIndex, 2) != 0 
                || subgroupClusteredMin(Depth, 2) == Depth)
            {
                if (WritePixelValue > FrameBuffer[PixelIndex])
                {
                    atomicMax(FrameBuffer[PixelIndex], WritePixelValue);
                }
            }
        }
        
#if 0
        if (subgroupAllEqual(PixelIndex))
        {
            // NOTE: Scalarized version (all threads are inside the same pixel
            uint MinDepth = subgroupMin(Depth);
            if (MinDepth == Depth)
            {
                if (subgroupElect())
                {
                    uint64_t WritePixelValue = (uint64_t(MinDepth) << 32) | uint64_t(CurrPoint.Color);
                    if (WritePixelValue > FrameBuffer[PixelIndex])
                    {
                        atomicMax(FrameBuffer[PixelIndex], WritePixelValue);
                    }
                }
            }
        }
        else
        {
            // IMPORTANT: We are rendering with reversed Z
            uint idXOr = subgroupClusteredXor(PixelIndex, 2);
            uint MinDepth = subgroupClusteredMin(Depth, 2);
            
            if (idXOr != 0 || MinDepth == Depth)
            {
                uint64_t WritePixelValue = (uint64_t(MinDepth) << 32) | uint64_t(CurrPoint.Color);
                if (WritePixelValue > FrameBuffer[PixelIndex])
                {
                    atomicMax(FrameBuffer[PixelIndex], WritePixelValue);
                }
            }
        }
#endif
    }
}

#endif
