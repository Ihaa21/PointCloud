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

struct point
{
    vec3 Pos;
    uint Color;
};

layout(set = 0, binding = 1) buffer point_cloud
{
    point PointCloud[];
};

layout(set = 0, binding = 2) buffer frame_buffer
{
    uint64_t FrameBuffer[];
};

layout(set = 0, binding = 3, rgba8) uniform image2D FrameBufferFloat;

//=========================================================================================================================================
// NOTE: NAIVE
//=========================================================================================================================================

#if NAIVE

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;
void main()
{
    int PointId = int((gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x) * gl_LocalInvocationID.x);
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

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;
void main()
{
    int PointId = int((gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x) * gl_LocalInvocationID.x);
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
// NOTE: OVERLAPPING FAST PATH
//=========================================================================================================================================

#if OVERLAPPING_FAST_PATH

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;
void main()
{
    int PointId = int((gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x) * gl_LocalInvocationID.x);
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

        uint MinDepth = Depth;
        uint PixelIndex = PixelId.y * SceneGlobals.RenderWidth + PixelId.x;
        bool FastPath = subgroupAllEqual(PixelIndex);
        if (FastPath)
        {
            MinDepth = subgroupMin(Depth);
        }
        
        if (MinDepth == Depth)
        {
            uint64_t WritePixelValue = (uint64_t(MinDepth) << 32) | uint64_t(CurrPoint.Color);

            if (FastPath || subgroupClusteredXor(PixelIndex, 2) != 0 || subgroupClusteredMin(Depth, 2) == Depth)
            {
                //if (WritePixelValue > FrameBuffer[PixelIndex])
                {
                    atomicMax(FrameBuffer[PixelIndex], WritePixelValue);
                }
            }
        }
    }
}

#endif

//=========================================================================================================================================
// NOTE: CONVERT_TO_FLOAT
//=========================================================================================================================================

#if CONVERT_TO_FLOAT

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main()
{
    ivec2 PixelId = ivec2(gl_GlobalInvocationID.xy);
    if (PixelId.x < SceneGlobals.RenderWidth && PixelId.y < SceneGlobals.RenderHeight)
    {
        // NOTE: Reverse Y 
        uint64_t SrcColor = FrameBuffer[(SceneGlobals.RenderHeight - PixelId.y) * SceneGlobals.RenderWidth + PixelId.x] & 0xFFFFFFFF;
        vec4 FloatColor = vec4((SrcColor >> 0) & 0xFF, (SrcColor >> 8) & 0xFF, (SrcColor >> 16) & 0xFF, (SrcColor >> 24) & 0xFF);
        FloatColor /= 255.0f;
        imageStore(FrameBufferFloat, PixelId, FloatColor);
    }
}

#endif
