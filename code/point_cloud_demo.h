#pragma once

#define VALIDATION 1

#include "framework_vulkan\framework_vulkan.h"

struct pc_point
{
    v3 Pos;
    u32 Color;
};

struct scene_globals
{
    m4 WVP;
    u32 RenderWidth;
    u32 RenderHeight;
    u32 NumPoints;
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

    // NOTE: Point Cloud
    vk_linear_arena RenderTargetArena;
    VkDescriptorSetLayout PointCloudDescLayout;
    VkDescriptorSet PointCloudDescriptor;
    VkBuffer SceneUniforms;
    u32 NumPoints;
    VkBuffer PointCloud;
    VkBuffer PCFrameBuffer;
    vk_image PCFloatFrameBuffer;
    vk_pipeline* PCNaivePipeline;
    vk_pipeline* PCDepthPipeline;
    vk_pipeline* PCOverlapFastPipeline;

    vk_pipeline* ConvertToFloatPipeline;
    
    ui_state UiState;
};

global demo_state* DemoState;
