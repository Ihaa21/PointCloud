
#include "point_cloud_demo.h"

inline void PointCloudReCreate(u32 Width, u32 Height)
{
    DemoState->WindowResized = true;
    VkArenaClear(&DemoState->RenderTargetArena);

    DemoState->PCFrameBuffer = VkBufferCreate(RenderState->Device, &RenderState->GpuArena,
                                              VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                              sizeof(u64)*Width*Height);
    DemoState->PCFloatFrameBuffer = VkImageCreate(RenderState->Device, &DemoState->RenderTargetArena, Width, Height,
                                                  VK_FORMAT_R8G8B8A8_UNORM,
                                                  VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                                  VK_IMAGE_ASPECT_COLOR_BIT);

#if FIXED_POINT
    VkDescriptorBufferWrite(&RenderState->DescriptorManager, DemoState->PointCloudData.Descriptor, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, DemoState->PCFrameBuffer);
    VkDescriptorImageWrite(&RenderState->DescriptorManager, DemoState->PointCloudData.Descriptor, 4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                           DemoState->PCFloatFrameBuffer.View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
#else
    VkDescriptorBufferWrite(&RenderState->DescriptorManager, DemoState->PointCloudData.Descriptor, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, DemoState->PCFrameBuffer);
    VkDescriptorImageWrite(&RenderState->DescriptorManager, DemoState->PointCloudData.Descriptor, 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                           DemoState->PCFloatFrameBuffer.View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
#endif

    VkDescriptorImageWrite(&RenderState->DescriptorManager, DemoState->CopyToSwapDesc, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           DemoState->PCFloatFrameBuffer.View, DemoState->PointSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorManagerFlush(RenderState->Device, &RenderState->DescriptorManager);
}

//
// NOTE: Demo Code
//

inline void DemoAllocGlobals(linear_arena* Arena)
{
    // IMPORTANT: These are always the top of the program memory
    DemoState = PushStruct(Arena, demo_state);
    RenderState = PushStruct(Arena, render_state);
}

DEMO_INIT(Init)
{
    // NOTE: Init Memory
    {
        linear_arena Arena = LinearArenaCreate(ProgramMemory, ProgramMemorySize);
        DemoAllocGlobals(&Arena);
        *DemoState = {};
        *RenderState = {};
        DemoState->Arena = Arena;
        DemoState->TempArena = LinearSubArena(&DemoState->Arena, MegaBytes(10));
    }

    // NOTE: Init Vulkan
    {
        {
            const char* DeviceExtensions[] =
            {
                "VK_EXT_shader_viewport_index_layer",
                "VK_KHR_shader_atomic_int64",
                "VK_EXT_shader_subgroup_ballot",
            };
            
            render_init_params InitParams = {};
            InitParams.ValidationEnabled = true;
            //InitParams.PresentMode = VK_PRESENT_MODE_FIFO_KHR;
            InitParams.WindowWidth = WindowWidth;
            InitParams.WindowHeight = WindowHeight;
            InitParams.GpuLocalSize = MegaBytes(1500);
            InitParams.DeviceExtensionCount = ArrayCount(DeviceExtensions);
            InitParams.DeviceExtensions = DeviceExtensions;
            VkInit(VulkanLib, hInstance, WindowHandle, &DemoState->Arena, &DemoState->TempArena, InitParams);
        }
    }
    
    // NOTE: Create samplers
    DemoState->PointSampler = VkSamplerCreate(RenderState->Device, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, 0.0f);
    DemoState->LinearSampler = VkSamplerCreate(RenderState->Device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, 0.0f);
    DemoState->AnisoSampler = VkSamplerMipMapCreate(RenderState->Device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 16.0f,
                                                    VK_SAMPLER_MIPMAP_MODE_LINEAR, 0, 0, 5);    
        
    // NOTE: Init render target entries
    DemoState->SwapChainEntry = RenderTargetSwapChainEntryCreate(RenderState->WindowWidth, RenderState->WindowHeight,
                                                                 RenderState->SwapChainFormat);

    // NOTE: Copy To Swap RT
    {
        render_target_builder Builder = RenderTargetBuilderBegin(&DemoState->Arena, &DemoState->TempArena, RenderState->WindowWidth,
                                                                 RenderState->WindowHeight);
        RenderTargetAddTarget(&Builder, &DemoState->SwapChainEntry, VkClearColorCreate(0, 0, 0, 1));
                            
        vk_render_pass_builder RpBuilder = VkRenderPassBuilderBegin(&DemoState->TempArena);

        u32 ColorId = VkRenderPassAttachmentAdd(&RpBuilder, RenderState->SwapChainFormat, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED,
                                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        VkRenderPassSubPassBegin(&RpBuilder, VK_PIPELINE_BIND_POINT_GRAPHICS);
        VkRenderPassColorRefAdd(&RpBuilder, ColorId, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderPassSubPassEnd(&RpBuilder);

        DemoState->CopyToSwapTarget = RenderTargetBuilderEnd(&Builder, VkRenderPassBuilderEnd(&RpBuilder, RenderState->Device));
        DemoState->CopyToSwapDesc = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, RenderState->CopyImageDescLayout);
        DemoState->CopyToSwapPipeline = FullScreenCopyImageCreate(DemoState->CopyToSwapTarget.RenderPass, 0);
    }

    // NOTE: Init camera
    {
        DemoState->Camera = CameraFpsCreate(V3(0, 0, -6), V3(0, 0, 1), true, 1.0f, 0.015f);
        CameraSetPersp(&DemoState->Camera, f32(RenderState->WindowWidth / RenderState->WindowHeight), 69.375f, 0.01f, 1000.0f);
    }

    // NOTE: Global Data
    {
        DemoState->RenderTargetArena = VkLinearArenaCreate(RenderState->Device, RenderState->LocalMemoryId, MegaBytes(32));
        DemoState->SceneUniforms = VkBufferCreate(RenderState->Device, &RenderState->GpuArena,
                                                  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                  sizeof(scene_globals));
    }

    // NOTE: Point Cloud Data
    {
        point_cloud_data* Data = &DemoState->PointCloudData;
        
#if FIXED_POINT
        
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&Data->DescLayout);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutEnd(RenderState->Device, &Builder);
        }

        Data->Descriptor = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Data->DescLayout);
        VkDescriptorBufferWrite(&RenderState->DescriptorManager, Data->Descriptor, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, DemoState->SceneUniforms);
        
        VkDescriptorSetLayout Layouts[] =
            {
                Data->DescLayout,
            };
        Data->NaivePipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                      "point_cloud_fp_naive.spv", "main", Layouts, ArrayCount(Layouts));
        Data->DepthPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                      "point_cloud_fp_depth.spv", "main", Layouts, ArrayCount(Layouts));
        Data->NaiveWaveOpsPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                             "point_cloud_fp_naive_wave_ops.spv", "main", Layouts, ArrayCount(Layouts));
        Data->DepthWaveOpsPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                             "point_cloud_fp_depth_wave_ops.spv", "main", Layouts, ArrayCount(Layouts));
        Data->DepthWaveOps2Pipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                              "point_cloud_fp_depth_wave_ops_2.spv", "main", Layouts, ArrayCount(Layouts));
        DemoState->ConvertToFloatPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager,
                                                                    &DemoState->TempArena, "convert_to_float_fp.spv",
                                                                    "main", Layouts, ArrayCount(Layouts));
#else
        
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&Data->DescLayout);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutEnd(RenderState->Device, &Builder);
        }

        Data->Descriptor = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Data->DescLayout);
        VkDescriptorBufferWrite(&RenderState->DescriptorManager, Data->Descriptor, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, DemoState->SceneUniforms);
        
        VkDescriptorSetLayout Layouts[] =
            {
                Data->DescLayout,
            };
        Data->NaivePipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                      "point_cloud_naive.spv", "main", Layouts, ArrayCount(Layouts));
        Data->DepthPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                      "point_cloud_depth.spv", "main", Layouts, ArrayCount(Layouts));
        Data->NaiveWaveOpsPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                             "point_cloud_naive_wave_ops.spv", "main", Layouts, ArrayCount(Layouts));
        Data->DepthWaveOpsPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                             "point_cloud_depth_wave_ops.spv", "main", Layouts, ArrayCount(Layouts));
        Data->DepthWaveOps2Pipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                              "point_cloud_depth_wave_ops_2.spv", "main", Layouts, ArrayCount(Layouts));
        DemoState->ConvertToFloatPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager,
                                                                    &DemoState->TempArena, "convert_to_float.spv",
                                                                    "main", Layouts, ArrayCount(Layouts));
#endif

        DemoState->RenderWidth = WindowWidth;
        DemoState->RenderHeight = WindowHeight;
        PointCloudReCreate(WindowWidth, WindowHeight);
    }
    
    // NOTE: Upload assets
    vk_commands* Commands = &RenderState->Commands;
    VkCommandsBegin(Commands, RenderState->Device);
    {
        // NOTE: Upload point cloud
        point_cloud_data* PointCloudData = &DemoState->PointCloudData;

#if 0
        {
            // NOTE: This is generating a point cloud sphere, for debugging
            u32 NumXSegments = 4000;
            u32 NumYSegments = 4000;

            DemoState->NumPoints = NumXSegments * NumYSegments;
            DemoState->PointCloud = VkBufferCreate(RenderState->Device, &RenderState->GpuArena,
                                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                   sizeof(pc_point) * DemoState->NumPoints);
            VkDescriptorBufferWrite(&RenderState->DescriptorManager, DemoState->PointCloudDescriptor, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, DemoState->PointCloud);

            pc_point* GpuPtr = VkTransferPushWriteArray(&RenderState->TransferManager, DemoState->PointCloud, pc_point, DemoState->NumPoints,
                                                        BarrierMask(VkAccessFlagBits(0), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT),
                                                        BarrierMask(VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT));

            pc_point* CurrPoint = GpuPtr;
            for (u32 Y = 0; Y <= NumYSegments; ++Y)
            {
                for (u32 X = 0; X <= NumXSegments; ++X)
                {
                    v2 Segment = V2(X, Y) / V2(NumXSegments, NumYSegments);
                    v3 Pos = V3(Cos(Segment.x * 2.0f * Pi32) * Sin(Segment.y * Pi32),
                                Cos(Segment.y * Pi32),
                                Sin(Segment.x * 2.0f * Pi32) * Sin(Segment.y * Pi32));

                    CurrPoint->Pos = Pos;
                    CurrPoint->Color = 0xFF00;
                    CurrPoint += 1;
                }
            }
        }
#else
        {
            temp_mem TempMem = BeginTempMem(&DemoState->Arena);

            // TODO: REMOVE
            {
                u64 TestInput = 2199024304128u;

                u32 PosX = u32(TestInput >> 0u) & 0x1FFFFF;
                u32 PosY = u32(TestInput >> 21u) & 0x1FFFFF;
                u32 PosZ = u32(TestInput >> 42u) & 0x0FFFFF;

                // NOTE: Move back to 32 bits
                PosX = PosX << 11u;
                PosY = PosY << 11u;
                PosZ = PosZ << 10u;

                i32 PosXI32 = *((i32*)&PosX);
                i32 PosYI32 = *((i32*)&PosY);
                i32 PosZI32 = *((i32*)&PosZ);

                v3 Result = V3(PosXI32, PosYI32, PosZI32) / (-I32_MIN);

                int i = 0;
            }
            
#if FIXED_POINT

            FILE* File = fopen("wue_city_sorted_fp.bin", "rb");
            fread(&DemoState->NumPoints, 1, sizeof(u32), File);
            fread(&DemoState->PointCloudRadius, 1, sizeof(v3), File);
            
            PointCloudData->Points = VkBufferCreate(RenderState->Device, &RenderState->GpuArena,
                                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                    (sizeof(u64) + sizeof(u32)) * DemoState->NumPoints);
            VkDescriptorBufferWrite(&RenderState->DescriptorManager, PointCloudData->Descriptor, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, PointCloudData->Points, 0);
            VkDescriptorBufferWrite(&RenderState->DescriptorManager, PointCloudData->Descriptor, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, PointCloudData->Points, sizeof(u64) * DemoState->NumPoints);

            // NOTE: We write the data in a SOA format
            u64* PosGpuPtr = VkCommandsPushWriteArray(&RenderState->Commands, PointCloudData->Points, u64, DemoState->NumPoints,
                                                      BarrierMask(VkAccessFlagBits(0), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT),
                                                      BarrierMask(VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT));
            
            u32* ColorGpuPtr = VkCommandsPushWriteArray(&RenderState->Commands, PointCloudData->Points,
                                                        sizeof(u64)*DemoState->NumPoints, u32, DemoState->NumPoints,
                                                        BarrierMask(VkAccessFlagBits(0), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT),
                                                        BarrierMask(VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT));

            pc_fixed_point* SrcPoints = PushArray(&DemoState->Arena, pc_fixed_point, DemoState->NumPoints);
            fread(SrcPoints, DemoState->NumPoints, sizeof(pc_fixed_point), File);

            for (u32 PointId = 0; PointId < DemoState->NumPoints; ++PointId)
            {
                PosGpuPtr[PointId] = SrcPoints[PointId].Pos;
                ColorGpuPtr[PointId] = SrcPoints[PointId].Color;
            }
            
            fclose(File);
            
#else
            
            FILE* File = fopen("wue_city_sorted.bin", "rb");
            fread(&DemoState->NumPoints, 1, sizeof(u32), File);

            PointCloudData->Points = VkBufferCreate(RenderState->Device, &RenderState->GpuArena,
                                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                    sizeof(pc_point) * DemoState->NumPoints);
            VkDescriptorBufferWrite(&RenderState->DescriptorManager, PointCloudData->Descriptor, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, PointCloudData->Points);

            pc_point* GpuPtr = VkCommandsPushWriteArray(&RenderState->Commands, PointCloudData->Points, pc_point, DemoState->NumPoints,
                                                        BarrierMask(VkAccessFlagBits(0), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT),
                                                        BarrierMask(VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT));

            fread(GpuPtr, DemoState->NumPoints, sizeof(pc_point), File);
            
            fclose(File);
            
#endif
            
            EndTempMem(TempMem);
        }
#endif
        
        // NOTE: Create UI
        UiStateCreate(RenderState->Device, &DemoState->Arena, &DemoState->TempArena, RenderState->LocalMemoryId,
                      &RenderState->DescriptorManager, &RenderState->PipelineManager, &RenderState->Commands,
                      RenderState->SwapChainFormat, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, &DemoState->UiState);
        
        VkDescriptorManagerFlush(RenderState->Device, &RenderState->DescriptorManager);
    }
    
    VkCommandsSubmit(Commands, RenderState->Device, RenderState->GraphicsQueue);
}

DEMO_DESTROY(Destroy)
{
}

DEMO_SWAPCHAIN_CHANGE(SwapChainChange)
{
    VkCheckResult(vkDeviceWaitIdle(RenderState->Device));
    VkSwapChainReCreate(&DemoState->TempArena, WindowWidth, WindowHeight, RenderState->PresentMode);

    DemoState->SwapChainEntry.Width = RenderState->WindowWidth;
    DemoState->SwapChainEntry.Height = RenderState->WindowHeight;

    DemoState->Camera.PerspAspectRatio = f32(RenderState->WindowWidth / RenderState->WindowHeight);

    DemoState->RenderWidth = RenderState->WindowWidth;
    DemoState->RenderHeight = RenderState->WindowHeight;
    PointCloudReCreate(WindowWidth, WindowHeight);
}

DEMO_CODE_RELOAD(CodeReload)
{
    linear_arena Arena = LinearArenaCreate(ProgramMemory, ProgramMemorySize);
    // IMPORTANT: We are relying on the memory being the same here since we have the same base ptr with the VirtualAlloc so we just need
    // to patch our global pointers here
    DemoAllocGlobals(&Arena);

    VkGetGlobalFunctionPointers(VulkanLib);
    VkGetInstanceFunctionPointers();
    VkGetDeviceFunctionPointers();
}

DEMO_MAIN_LOOP(MainLoop)
{
    u32 ImageIndex;
    VkCheckResult(vkAcquireNextImageKHR(RenderState->Device, RenderState->SwapChain, UINT64_MAX, RenderState->ImageAvailableSemaphore,
                                        VK_NULL_HANDLE, &ImageIndex));
    DemoState->SwapChainEntry.View = RenderState->SwapChainViews[ImageIndex];

    vk_commands* Commands = &RenderState->Commands;
    VkCommandsBegin(Commands, RenderState->Device);

    // NOTE: Update pipelines
    VkPipelineUpdateShaders(RenderState->Device, &RenderState->CpuArena, &RenderState->PipelineManager);

    RenderTargetUpdateEntries(&DemoState->TempArena, &DemoState->CopyToSwapTarget);
    
    // NOTE: Update Ui State
    {
        ui_state* UiState = &DemoState->UiState;
        
        ui_frame_input UiCurrInput = {};
        UiCurrInput.MouseDown = CurrInput->MouseDown;
        UiCurrInput.MousePixelPos = V2(CurrInput->MousePixelPos);
        UiCurrInput.MouseScroll = CurrInput->MouseScroll;
        Copy(CurrInput->KeysDown, UiCurrInput.KeysDown, sizeof(UiCurrInput.KeysDown));
        UiStateBegin(UiState, FrameTime, RenderState->WindowWidth, RenderState->WindowHeight, UiCurrInput);
        local_global v2 PanelPos = V2(100, 800);
        
        UiStateEnd(UiState, &RenderState->DescriptorManager);
    }

    // NOTE: Upload scene data
    {
        if (!(DemoState->UiState.MouseTouchingUi || DemoState->UiState.ProcessedInteraction))
        {
            CameraUpdate(&DemoState->Camera, CurrInput, PrevInput);
        }
        
        // NOTE: Push Scene Globals
        {
            scene_globals* GpuPtr = VkCommandsPushWriteStruct(&RenderState->Commands, DemoState->SceneUniforms, scene_globals,
                                                              BarrierMask(VkAccessFlagBits(0), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT),
                                                              BarrierMask(VK_ACCESS_UNIFORM_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT));
            
            *GpuPtr = {};
            GpuPtr->WVP = CameraGetVP(&DemoState->Camera) * M4Scale(V3(1, -1, 1)) * M4Scale(V3(0.0001f));
#if FIXED_POINT
            GpuPtr->WVP = GpuPtr->WVP * M4Scale(DemoState->PointCloudRadius);
#endif
            GpuPtr->RenderWidth = DemoState->RenderWidth;
            GpuPtr->RenderHeight = DemoState->RenderHeight;
            GpuPtr->NumPoints = DemoState->NumPoints;
        }
        
        VkCommandsTransferFlush(&RenderState->Commands, RenderState->Device);
    }

    // NOTE: Render Point Cloud
    {
        // NOTE: Transition to UAV
        {
            if (DemoState->WindowResized)
            {
                VkBarrierImageAdd(&RenderState->Commands,
                                  VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                  VK_IMAGE_ASPECT_COLOR_BIT, DemoState->PCFloatFrameBuffer.Image);
            }
            else
            {
                VkBarrierImageAdd(&RenderState->Commands,
                                  VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                  VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                  VK_IMAGE_ASPECT_COLOR_BIT, DemoState->PCFloatFrameBuffer.Image);
            }

            VkBarrierBufferAdd(&RenderState->Commands, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                               VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, DemoState->PCFrameBuffer);

            VkCommandsBarrierFlush(&RenderState->Commands);
        }
        
        // NOTE: Clear frame buffer
        {
            vkCmdFillBuffer(Commands->Buffer, DemoState->PCFrameBuffer, 0, VK_WHOLE_SIZE, 0);
        }

        // NOTE: Wait on all writes
        VkBarrierBufferAdd(&RenderState->Commands, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, DemoState->PCFrameBuffer);
        VkCommandsBarrierFlush(&RenderState->Commands);

        point_cloud_data* PointCloudData = &DemoState->PointCloudData;

        // NOTE: Splat points
        {            
            vk_pipeline* Pipeline = PointCloudData->NaivePipeline;
            //vk_pipeline* Pipeline = PointCloudData->DepthPipeline;
            //vk_pipeline* Pipeline = PointCloudData->NaiveWaveOpsPipeline;
            //vk_pipeline* Pipeline = PointCloudData->DepthWaveOpsPipeline;
            //vk_pipeline* Pipeline = PointCloudData->DepthWaveOps2Pipeline;
            vkCmdBindPipeline(Commands->Buffer, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline->Handle);
            VkDescriptorSet DescriptorSets[] =
                {
                    PointCloudData->Descriptor,
                };
            vkCmdBindDescriptorSets(Commands->Buffer, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline->Layout, 0,
                                    ArrayCount(DescriptorSets), DescriptorSets, 0, 0);

            u32 NumJobs = CeilU32(f32(DemoState->NumPoints) / 32.0f);
            u32 Stride = Max(1u, CeilU32(f32(NumJobs) / f32(0xFFFF)));
            u32 DispatchY = Stride;
            u32 DispatchX = CeilU32(f32(NumJobs) / f32(Stride));

            Assert((DispatchY * DispatchX) >= NumJobs);
            vkCmdDispatch(Commands->Buffer, DispatchX, DispatchY, 1);
        }

        // NOTE: Wait on all writes
        VkBarrierBufferAdd(&RenderState->Commands, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, DemoState->PCFrameBuffer);
        VkCommandsBarrierFlush(&RenderState->Commands);

        {
            vk_pipeline* Pipeline = DemoState->ConvertToFloatPipeline;
            vkCmdBindPipeline(Commands->Buffer, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline->Handle);
            VkDescriptorSet DescriptorSets[] =
                {
                    PointCloudData->Descriptor,
                };
            vkCmdBindDescriptorSets(Commands->Buffer, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline->Layout, 0,
                                    ArrayCount(DescriptorSets), DescriptorSets, 0, 0);
            u32 DispatchX = CeilU32(f32(RenderState->WindowWidth) / f32(8));
            u32 DispatchY = CeilU32(f32(RenderState->WindowHeight) / f32(8));
            vkCmdDispatch(Commands->Buffer, DispatchX, DispatchY, 1);
        }

        // NOTE: Transition to copy to swapchain
        VkBarrierImageAdd(&RenderState->Commands,
                          VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                          VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_IMAGE_ASPECT_COLOR_BIT, DemoState->PCFloatFrameBuffer.Image);
        VkCommandsBarrierFlush(&RenderState->Commands);
    }
    
    RenderTargetPassBegin(&DemoState->CopyToSwapTarget, Commands, RenderTargetRenderPass_SetViewPort | RenderTargetRenderPass_SetScissor);
    FullScreenPassRender(Commands, DemoState->CopyToSwapPipeline, 1, &DemoState->CopyToSwapDesc);
    RenderTargetPassEnd(Commands);
    UiStateRender(&DemoState->UiState, RenderState->Device, Commands, DemoState->SwapChainEntry.View);

    VkCommandsEnd(Commands, RenderState->Device);
                    
    // NOTE: Render to our window surface
    // NOTE: Tell queue where we render to surface to wait
    VkPipelineStageFlags WaitDstMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo SubmitInfo = {};
    SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfo.waitSemaphoreCount = 1;
    SubmitInfo.pWaitSemaphores = &RenderState->ImageAvailableSemaphore;
    SubmitInfo.pWaitDstStageMask = &WaitDstMask;
    SubmitInfo.commandBufferCount = 1;
    SubmitInfo.pCommandBuffers = &Commands->Buffer;
    SubmitInfo.signalSemaphoreCount = 1;
    SubmitInfo.pSignalSemaphores = &RenderState->FinishedRenderingSemaphore;
    VkCheckResult(vkQueueSubmit(RenderState->GraphicsQueue, 1, &SubmitInfo, Commands->Fence));
    
    VkPresentInfoKHR PresentInfo = {};
    PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    PresentInfo.waitSemaphoreCount = 1;
    PresentInfo.pWaitSemaphores = &RenderState->FinishedRenderingSemaphore;
    PresentInfo.swapchainCount = 1;
    PresentInfo.pSwapchains = &RenderState->SwapChain;
    PresentInfo.pImageIndices = &ImageIndex;
    VkResult Result = vkQueuePresentKHR(RenderState->PresentQueue, &PresentInfo);

    switch (Result)
    {
        case VK_SUCCESS:
        {
        } break;

        case VK_ERROR_OUT_OF_DATE_KHR:
        case VK_SUBOPTIMAL_KHR:
        {
            // NOTE: Window size changed
            InvalidCodePath;
        } break;

        default:
        {
            InvalidCodePath;
        } break;
    }

    DemoState->WindowResized = false;
}
