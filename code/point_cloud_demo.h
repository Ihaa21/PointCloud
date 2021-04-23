#pragma once

#define VALIDATION 1

#include "framework_vulkan\framework_vulkan.h"

struct pc_point
{
    v3 Pos;
    u32 Color;
};

struct pc_fixed_point
{
    u64 Pos;
    u32 Color;
};

struct scene_globals
{
    m4 WVP;
    u32 RenderWidth;
    u32 RenderHeight;
    u32 NumPoints;
};

#define FIXED_POINT 1
struct point_cloud_data
{
    VkDescriptorSetLayout DescLayout;
    VkDescriptorSet Descriptor;
    VkBuffer Points;
    vk_pipeline* NaivePipeline;
    vk_pipeline* DepthPipeline;
    vk_pipeline* NaiveWaveOpsPipeline;
    vk_pipeline* DepthWaveOpsPipeline;
    vk_pipeline* DepthWaveOps2Pipeline;
};

struct demo_state
{
    linear_arena Arena;
    linear_arena TempArena;

    // NOTE: Samplers
    VkSampler PointSampler;
    VkSampler LinearSampler;
    VkSampler AnisoSampler;

    // NOTE: Swap Chain Targets
    b32 WindowResized;
    u32 RenderWidth;
    u32 RenderHeight;
    VkFormat RenderFormat;
    render_target_entry SwapChainEntry;
    render_target CopyToSwapTarget;
    VkDescriptorSet CopyToSwapDesc;
    vk_pipeline* CopyToSwapPipeline;
    
    camera Camera;

    vk_linear_arena RenderTargetArena;
    VkBuffer SceneUniforms;
    u32 NumPoints;
    VkBuffer PCFrameBuffer;
    vk_image PCFloatFrameBuffer;

    v3 PointCloudRadius;
    point_cloud_data PointCloudData;
    
    vk_pipeline* ConvertToFloatPipeline;
    
    ui_state UiState;
};

global demo_state* DemoState;
