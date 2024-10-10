#include "platform.h"
#include "vulkan/vulkan_core.h"
#include <vector>

#define MAX_FRAMES_IN_FLIGHT 2
uint32 CurrentFrame = 0;

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT MessageSeverity,
                                                    VkDebugUtilsMessageTypeFlagsEXT MessageType,
                                                    const VkDebugUtilsMessengerCallbackDataEXT* CallbackData,
                                                    void* UserData)
{
    printf("validation layer: \n");
    printf(CallbackData->pMessage);
    printf("\n\n");

    return VK_FALSE;
}

int main()
{
    Initialize();

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Vulkan Instance And Setup Validation Layer
    bool32 EnableValidationLayers = true;

    VkApplicationInfo AppInfo = {};
    AppInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    AppInfo.pApplicationName = "Hello Triangle";
    AppInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    AppInfo.pEngineName = "No Engine";
    AppInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    AppInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo InstanceCreateInfo = {};
    InstanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    InstanceCreateInfo.pApplicationInfo = &AppInfo;
    InstanceCreateInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

    std::vector<const char*> Extensions;
    Extensions.push_back("VK_KHR_surface");
    Extensions.push_back("VK_EXT_metal_surface");
    Extensions.push_back("VK_KHR_portability_enumeration");
    if(EnableValidationLayers)
    {
        Extensions.push_back("VK_EXT_debug_utils");
    }

    std::vector<const char*> ValidationLayers;
    ValidationLayers.push_back("VK_LAYER_KHRONOS_validation");

    std::vector<const char*> DeviceExtensions;
    DeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    InstanceCreateInfo.enabledExtensionCount = static_cast<uint32>(Extensions.size());
    InstanceCreateInfo.ppEnabledExtensionNames = Extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT DebugCreateInfo = {};
    if (EnableValidationLayers)
    {
        InstanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(ValidationLayers.size());
        InstanceCreateInfo.ppEnabledLayerNames = ValidationLayers.data();

        DebugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

        DebugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

        DebugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

        DebugCreateInfo.pfnUserCallback = DebugCallback;
        InstanceCreateInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &DebugCreateInfo;
    }
    else
    {
        InstanceCreateInfo.enabledLayerCount = 0;

        InstanceCreateInfo.pNext = nullptr;
    }

    VkInstance Instance;
    if (vkCreateInstance(&InstanceCreateInfo, nullptr, &Instance) != VK_SUCCESS)
    {
        printf("Failed to create instance!\n");
    }

    VkDebugUtilsMessengerEXT DebugMessenger;

    PFN_vkCreateDebugUtilsMessengerEXT CreateDebugUtilsMessengerEXT =
        (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(Instance, "vkCreateDebugUtilsMessengerEXT");
    if (CreateDebugUtilsMessengerEXT != nullptr)
    {
        VkResult Result = CreateDebugUtilsMessengerEXT(Instance, &DebugCreateInfo, nullptr, &DebugMessenger);
        if (Result != VK_SUCCESS)
        {
            printf("Failed to set up debug messenger!\n");
        }
    }
    else
    {
        printf("Failed to set up debug messenger!\n");
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Vulkan Surface
    CreateWindow(640,  480, "Vulkan From Scratch", 0);

    CAMetalLayer* MetalLayer = GetInternalStateMetalLayer();

    VkMetalSurfaceCreateInfoEXT SurfaceCreateInfo = {};
    SurfaceCreateInfo.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
    SurfaceCreateInfo.pNext = NULL;
    SurfaceCreateInfo.flags = 0;
    SurfaceCreateInfo.pLayer = MetalLayer;

    VkSurfaceKHR Surface;
    if (vkCreateMetalSurfaceEXT(Instance, &SurfaceCreateInfo, NULL, &Surface) != VK_SUCCESS)
    {
        printf("Failed to create surface!\n");
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Pick Physical Device
    uint32_t DeviceCount = 0;
    vkEnumeratePhysicalDevices(Instance, &DeviceCount, nullptr);
    if (DeviceCount == 0)
    {
        printf("Failed to find GPUs with Vulkan support! \n");
    }
    std::vector<VkPhysicalDevice> Devices(DeviceCount);
    vkEnumeratePhysicalDevices(Instance, &DeviceCount, Devices.data());

    VkPhysicalDevice PhysicalDevice = 0;

    //TODO(Lyubomir): Pick the best GPU, not the first available.
    if (Devices.size() > 0)
    {
        PhysicalDevice = Devices[0];
    }
    else
    {
        printf("Failed to find a suitable GPU!\n");
    }


    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Logical Device

    std::vector<VkDeviceQueueCreateInfo> QueueCreateInfos;

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo QueueCreateInfo = {};
    QueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    QueueCreateInfo.queueFamilyIndex = 0;
    QueueCreateInfo.queueCount = 1;
    QueueCreateInfo.pQueuePriorities = &queuePriority;

    QueueCreateInfos.push_back(QueueCreateInfo);

    VkPhysicalDeviceFeatures DeviceFeatures = {};

    VkDeviceCreateInfo DeviceCreateInfo = {};
    DeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    DeviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(QueueCreateInfos.size());
    DeviceCreateInfo.pQueueCreateInfos = QueueCreateInfos.data();

    DeviceCreateInfo.pEnabledFeatures = &DeviceFeatures;

    DeviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(DeviceExtensions.size());
    DeviceCreateInfo.ppEnabledExtensionNames = DeviceExtensions.data();

    if (EnableValidationLayers)
    {
        DeviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(ValidationLayers.size());
        DeviceCreateInfo.ppEnabledLayerNames = ValidationLayers.data();
    }
    else
    {
        DeviceCreateInfo.enabledLayerCount = 0;
    }

    VkDevice Device;
    if (vkCreateDevice(PhysicalDevice, &DeviceCreateInfo, nullptr, &Device) != VK_SUCCESS)
    {
        printf("Failed to create logical device!\n");
    }

    VkQueue GraphicsQueue;
    VkQueue PresentQueue;
    vkGetDeviceQueue(Device, 0, 0, &GraphicsQueue);
    vkGetDeviceQueue(Device, 0, 0, &PresentQueue);

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Swap Chain
    VkSurfaceCapabilitiesKHR Capabilities;
    std::vector<VkSurfaceFormatKHR> Formats;
    std::vector<VkPresentModeKHR> PresentModes;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PhysicalDevice, Surface, &Capabilities);

    uint32_t FormatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &FormatCount, nullptr);
    if (FormatCount != 0)
    {
        Formats.resize(FormatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &FormatCount, Formats.data());
    }
    uint32_t PresentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, Surface, &PresentModeCount, nullptr);
    if (PresentModeCount != 0)
    {
        PresentModes.resize(PresentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, Surface, &PresentModeCount, PresentModes.data());
    }

    VkSurfaceFormatKHR SurfaceFormat;
    for (int Index = 0; Index < Formats.size(); ++Index)
    {
        if (Formats[Index].format == VK_FORMAT_B8G8R8A8_SRGB &&
            Formats[Index].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            SurfaceFormat = Formats[Index];
            break;
        }
    }

    VkPresentModeKHR PresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (int Index = 0; Index < PresentModes.size(); ++Index)
    {
        if (PresentModes[Index] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            PresentMode = PresentModes[Index];
            break;
        }
    }

    //TODO(Lyubomir): Maybe resize the extent to match the framebuffer size?
    VkExtent2D Extent = Capabilities.currentExtent;

    uint32_t ImageCount = Capabilities.minImageCount + 1;
    if (Capabilities.maxImageCount > 0 && ImageCount > Capabilities.maxImageCount)
    {
        ImageCount = Capabilities.maxImageCount;
    }
    VkSwapchainCreateInfoKHR SpawChainCreateInfo = {};
    SpawChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    SpawChainCreateInfo.surface = Surface;
    SpawChainCreateInfo.minImageCount = ImageCount;
    SpawChainCreateInfo.imageFormat = SurfaceFormat.format;
    SpawChainCreateInfo.imageColorSpace = SurfaceFormat.colorSpace;
    SpawChainCreateInfo.imageExtent = Extent;
    SpawChainCreateInfo.imageArrayLayers = 1;
    SpawChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    //TODO(Lyubomir): What if the queueFamilyIndexCount is 2?
    SpawChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    SpawChainCreateInfo.preTransform = Capabilities.currentTransform;
    SpawChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    SpawChainCreateInfo.presentMode = PresentMode;
    SpawChainCreateInfo.clipped = VK_TRUE;
    SpawChainCreateInfo.oldSwapchain = 0;

    VkSwapchainKHR SwapChain;
    if (vkCreateSwapchainKHR(Device, &SpawChainCreateInfo, nullptr, &SwapChain) != VK_SUCCESS)
    {
        printf("Failed to create swap chain!\n");
    }

    std::vector<VkImage> SwapChainImages;
    VkFormat SwapChainImageFormat;
    VkExtent2D SwapChainExtent;

    vkGetSwapchainImagesKHR(Device, SwapChain, &ImageCount, nullptr);
    SwapChainImages.resize(ImageCount);
    vkGetSwapchainImagesKHR(Device, SwapChain, &ImageCount, SwapChainImages.data());
    SwapChainImageFormat = SurfaceFormat.format;
    SwapChainExtent = Extent;

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Image Views
    std::vector<VkImageView> SwapChainImageViews;
    SwapChainImageViews.resize(SwapChainImages.size());
    for (size_t i = 0; i < SwapChainImages.size(); i++)
    {
        VkImageViewCreateInfo ImageViewCreateInfo = {};
        ImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ImageViewCreateInfo.image = SwapChainImages[i];
        ImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ImageViewCreateInfo.format = SwapChainImageFormat;
        ImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        ImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        ImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        ImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        ImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        ImageViewCreateInfo.subresourceRange.levelCount = 1;
        ImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        ImageViewCreateInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(Device, &ImageViewCreateInfo, nullptr, &SwapChainImageViews[i]) != VK_SUCCESS)
        {
            printf("Failed to create image views!\n");
        }
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Render Pass
    VkAttachmentDescription ColorAttachment = {};
    ColorAttachment.format = SwapChainImageFormat;
    ColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    ColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    ColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    ColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    ColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ColorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference ColorAttachmentRef = {};
    ColorAttachmentRef.attachment = 0;
    ColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription SubPass = {};
    SubPass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    SubPass.colorAttachmentCount = 1;
    SubPass.pColorAttachments = &ColorAttachmentRef;

    VkSubpassDependency Dependency = {};
    Dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    Dependency.dstSubpass = 0;
    Dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    Dependency.srcAccessMask = 0;
    Dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    Dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo RenderPassInfo = {};
    RenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    RenderPassInfo.attachmentCount = 1;
    RenderPassInfo.pAttachments = &ColorAttachment;
    RenderPassInfo.subpassCount = 1;
    RenderPassInfo.pSubpasses = &SubPass;
    RenderPassInfo.dependencyCount = 1;
    RenderPassInfo.pDependencies = &Dependency;

    VkRenderPass RenderPass;
    if (vkCreateRenderPass(Device, &RenderPassInfo, nullptr, &RenderPass) != VK_SUCCESS)
    {
        printf("Failed to create render pass!\n");
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Graphics Pipeline

    //TODO(Lyubomir): Relative Paths!
    std::vector<char> VertexShaderCode = ReadFile("/Users/lyubomir.kostadinov/Projects/private/handmade_vulkan/shaders/vertex_default_shader.spv");
    std::vector<char> FragmentShaderCode = ReadFile("/Users/lyubomir.kostadinov/Projects/private/handmade_vulkan/shaders/fragment_default_shader.spv");

    VkShaderModuleCreateInfo VertexShaderCreateInfo = {};
    VertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    VertexShaderCreateInfo.codeSize = VertexShaderCode.size();
    VertexShaderCreateInfo.pCode = reinterpret_cast<const uint32_t*>(VertexShaderCode.data());

    VkShaderModule VertexShaderModule;
    if (vkCreateShaderModule(Device, &VertexShaderCreateInfo, nullptr, &VertexShaderModule) != VK_SUCCESS)
    {
        printf("Failed to create vertex shader module!\n");
    }

    VkShaderModuleCreateInfo FragmentShaderCreateInfo = {};
    FragmentShaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    FragmentShaderCreateInfo.codeSize = FragmentShaderCode.size();
    FragmentShaderCreateInfo.pCode = reinterpret_cast<const uint32_t*>(FragmentShaderCode.data());

    VkShaderModule FragmentShaderModule;
    if (vkCreateShaderModule(Device, &FragmentShaderCreateInfo, nullptr, &FragmentShaderModule) != VK_SUCCESS)
    {
        printf("Failed to create fragment shader module!\n");
    }

    VkPipelineShaderStageCreateInfo VertexShaderStageInfo = {};
    VertexShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    VertexShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    VertexShaderStageInfo.module = VertexShaderModule;
    VertexShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo FragmentShaderStageInfo = {};
    FragmentShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    FragmentShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    FragmentShaderStageInfo.module = FragmentShaderModule;
    FragmentShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo ShaderStages[] = {VertexShaderStageInfo, FragmentShaderStageInfo};

    VkPipelineVertexInputStateCreateInfo VertexInputInfo = {};
    VertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VertexInputInfo.vertexBindingDescriptionCount = 0;
    VertexInputInfo.vertexAttributeDescriptionCount = 0;

    VkPipelineInputAssemblyStateCreateInfo InputAssembly = {};
    InputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    InputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    InputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo ViewportState = {};
    ViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    ViewportState.viewportCount = 1;
    ViewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo Rasterizer = {};
    Rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    Rasterizer.depthClampEnable = VK_FALSE;
    Rasterizer.rasterizerDiscardEnable = VK_FALSE;
    Rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    Rasterizer.lineWidth = 1.0f;
    Rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    Rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    Rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo MultiSampling = {};
    MultiSampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    MultiSampling.sampleShadingEnable = VK_FALSE;
    MultiSampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState ColorBlendAttachment = {};
    ColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    ColorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo ColorBlending = {};
    ColorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    ColorBlending.logicOpEnable = VK_FALSE;
    ColorBlending.logicOp = VK_LOGIC_OP_COPY;
    ColorBlending.attachmentCount = 1;
    ColorBlending.pAttachments = &ColorBlendAttachment;
    ColorBlending.blendConstants[0] = 0.0f;
    ColorBlending.blendConstants[1] = 0.0f;
    ColorBlending.blendConstants[2] = 0.0f;
    ColorBlending.blendConstants[3] = 0.0f;

    std::vector<VkDynamicState> DynamicStates;
    DynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
    DynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);

    VkPipelineDynamicStateCreateInfo DynamicState = {};
    DynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    DynamicState.dynamicStateCount = static_cast<uint32_t>(DynamicStates.size());
    DynamicState.pDynamicStates = DynamicStates.data();

    VkPipelineLayoutCreateInfo PipelineLayoutInfo = {};
    PipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    PipelineLayoutInfo.setLayoutCount = 0;
    PipelineLayoutInfo.pushConstantRangeCount = 0;

    VkPipelineLayout PipelineLayout;
    if (vkCreatePipelineLayout(Device, &PipelineLayoutInfo, nullptr, &PipelineLayout) != VK_SUCCESS)
    {
        printf("Failed to create pipeline layout!\n");
    }

    VkGraphicsPipelineCreateInfo PipelineInfo = {};
    PipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    PipelineInfo.stageCount = 2;
    PipelineInfo.pStages = ShaderStages;
    PipelineInfo.pVertexInputState = &VertexInputInfo;
    PipelineInfo.pInputAssemblyState = &InputAssembly;
    PipelineInfo.pViewportState = &ViewportState;
    PipelineInfo.pRasterizationState = &Rasterizer;
    PipelineInfo.pMultisampleState = &MultiSampling;
    PipelineInfo.pDepthStencilState = nullptr; // Optional
    PipelineInfo.pColorBlendState = &ColorBlending;
    PipelineInfo.pDynamicState = &DynamicState;
    PipelineInfo.layout = PipelineLayout;
    PipelineInfo.renderPass = RenderPass;
    PipelineInfo.subpass = 0;
    PipelineInfo.basePipelineHandle = 0; // Optional
    PipelineInfo.basePipelineIndex = -1; // Optional

    VkPipeline GraphicsPipeline;
    if (vkCreateGraphicsPipelines(Device, 0, 1, &PipelineInfo, nullptr, &GraphicsPipeline) != VK_SUCCESS)
    {
        printf("Failed to create graphics pipeline!\n");
    }

    vkDestroyShaderModule(Device, FragmentShaderModule, nullptr);
    vkDestroyShaderModule(Device, VertexShaderModule, nullptr);

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Frame Buffers
    std::vector<VkFramebuffer> SwapChainFramebuffers;
    SwapChainFramebuffers.resize(SwapChainImageViews.size());

    for (int Index = 0; Index < SwapChainImageViews.size(); ++Index)
    {
        VkImageView Attachments[] =
        {
            SwapChainImageViews[Index]
        };

        VkFramebufferCreateInfo FramebufferInfo = {};
        FramebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        FramebufferInfo.renderPass = RenderPass;
        FramebufferInfo.attachmentCount = 1;
        FramebufferInfo.pAttachments = Attachments;
        FramebufferInfo.width = SwapChainExtent.width;
        FramebufferInfo.height = SwapChainExtent.height;
        FramebufferInfo.layers = 1;

        if (vkCreateFramebuffer(Device, &FramebufferInfo, nullptr,&SwapChainFramebuffers[Index]) != VK_SUCCESS)
        {
            printf("failed to create framebuffer!\n");
        }
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Command Pool
    VkCommandPoolCreateInfo PoolInfo = {};
    PoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    PoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    PoolInfo.queueFamilyIndex = 0;

    VkCommandPool CommandPool;
    if (vkCreateCommandPool(Device, &PoolInfo, nullptr, &CommandPool) != VK_SUCCESS)
    {
        printf("Failed to create command pool!\n");
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Command Buffers
    std::vector<VkCommandBuffer> CommandBuffers;
    CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo AllocInfo = {};
    AllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    AllocInfo.commandPool = CommandPool;
    AllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    AllocInfo.commandBufferCount = (uint32)CommandBuffers.size();

    if (vkAllocateCommandBuffers(Device, &AllocInfo, CommandBuffers.data()) != VK_SUCCESS)
    {
        printf("Failed to allocate command buffers!\n");
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Sync Objects
    std::vector<VkSemaphore> ImageAvailableSemaphores;
    std::vector<VkSemaphore> RenderFinishedSemaphores;
    std::vector<VkFence> InFlightFences;

    ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo SemaphoreInfo = {};
    SemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo FenceInfo = {};
    FenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    FenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32 Index = 0; Index < MAX_FRAMES_IN_FLIGHT; ++Index)
    {
        if (vkCreateSemaphore(Device, &SemaphoreInfo, nullptr, &ImageAvailableSemaphores[Index]) != VK_SUCCESS ||
            vkCreateSemaphore(Device, &SemaphoreInfo, nullptr, &RenderFinishedSemaphores[Index]) != VK_SUCCESS ||
            vkCreateFence(Device, &FenceInfo, nullptr, &InFlightFences[Index]) != VK_SUCCESS)
        {
            printf("Failed to create semaphores!\n");
        }
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Render Loop
    while (!ShouldClose())
    {
        PollEvents();

        vkWaitForFences(Device, 1, &InFlightFences[CurrentFrame], VK_TRUE, UINT64_MAX);
        vkResetFences(Device, 1, &InFlightFences[CurrentFrame]);

        //TODO(Lyubomir): Handle SwapChain recreation when the window size changes!!!

        uint32_t ImageIndex;
        vkAcquireNextImageKHR(Device, SwapChain, UINT64_MAX, ImageAvailableSemaphores[CurrentFrame], 0, &ImageIndex);

        vkResetCommandBuffer(CommandBuffers[CurrentFrame], 0);

        VkCommandBufferBeginInfo BeginInfo = {};
        BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        BeginInfo.flags = 0; // Optional
        BeginInfo.pInheritanceInfo = nullptr; // Optional

        if (vkBeginCommandBuffer(CommandBuffers[CurrentFrame], &BeginInfo) != VK_SUCCESS)
        {
            printf("Failed to begin recording command buffer!\n");
        }

        VkRenderPassBeginInfo RenderPassInfo = {};
        RenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        RenderPassInfo.renderPass = RenderPass;
        RenderPassInfo.framebuffer = SwapChainFramebuffers[ImageIndex];
        RenderPassInfo.renderArea.offset.x = 0;
        RenderPassInfo.renderArea.offset.x = 0;
        RenderPassInfo.renderArea.extent = SwapChainExtent;
        VkClearValue ClearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        RenderPassInfo.clearValueCount = 1;
        RenderPassInfo.pClearValues = &ClearColor;

        vkCmdBeginRenderPass(CommandBuffers[CurrentFrame], &RenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(CommandBuffers[CurrentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, GraphicsPipeline);

        VkViewport Viewport = {};
        Viewport.x = 0.0f;
        Viewport.y = 0.0f;
        Viewport.width = (float) SwapChainExtent.width;
        Viewport.height = (float) SwapChainExtent.height;
        Viewport.minDepth = 0.0f;
        Viewport.maxDepth = 1.0f;

        vkCmdSetViewport(CommandBuffers[CurrentFrame], 0, 1, &Viewport);

        VkRect2D Scissor = {};
        Scissor.offset.x = 0;
        Scissor.offset.y = 0;
        Scissor.extent = SwapChainExtent;

        vkCmdSetScissor(CommandBuffers[CurrentFrame], 0, 1, &Scissor);
        vkCmdDraw(CommandBuffers[CurrentFrame], 3, 1, 0, 0);
        vkCmdEndRenderPass(CommandBuffers[CurrentFrame]);

        if (vkEndCommandBuffer(CommandBuffers[CurrentFrame]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to record command buffer!");
        }

        VkSubmitInfo SubmitInfo = {};
        SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkSemaphore WaitSemaphores[] = { ImageAvailableSemaphores[CurrentFrame] };
        VkPipelineStageFlags WaitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        SubmitInfo.waitSemaphoreCount = 1;
        SubmitInfo.pWaitSemaphores = WaitSemaphores;
        SubmitInfo.pWaitDstStageMask = WaitStages;
        SubmitInfo.commandBufferCount = 1;
        SubmitInfo.pCommandBuffers = &CommandBuffers[CurrentFrame];

        VkSemaphore SignalSemaphores[] = { RenderFinishedSemaphores[CurrentFrame] };
        SubmitInfo.signalSemaphoreCount = 1;
        SubmitInfo.pSignalSemaphores = SignalSemaphores;

        if (vkQueueSubmit(GraphicsQueue, 1, &SubmitInfo, InFlightFences[CurrentFrame]) != VK_SUCCESS)
        {
            printf("Failed to submit draw command buffer!\n");
        }

        VkPresentInfoKHR PresentInfo = {};
        PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        PresentInfo.waitSemaphoreCount = 1;
        PresentInfo.pWaitSemaphores = SignalSemaphores;

        VkSwapchainKHR swapChains[] = {SwapChain};
        PresentInfo.swapchainCount = 1;
        PresentInfo.pSwapchains = swapChains;
        PresentInfo.pImageIndices = &ImageIndex;
        PresentInfo.pResults = nullptr; // Optional

        vkQueuePresentKHR(PresentQueue, &PresentInfo);

        CurrentFrame = (CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }
    vkDeviceWaitIdle(Device);

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Clean Up
    for (uint32 Index = 0; Index < MAX_FRAMES_IN_FLIGHT; ++Index)
    {
        vkDestroySemaphore(Device, ImageAvailableSemaphores[Index], nullptr);
        vkDestroySemaphore(Device, RenderFinishedSemaphores[Index], nullptr);
        vkDestroyFence(Device, InFlightFences[Index], nullptr);
    }
    vkDestroyCommandPool(Device, CommandPool, nullptr);

    for (int Index = 0;Index < SwapChainFramebuffers.size(); ++Index)
    {
        vkDestroyFramebuffer(Device, SwapChainFramebuffers[Index], nullptr);
    }

    vkDestroyPipeline(Device, GraphicsPipeline, nullptr);

    vkDestroyPipelineLayout(Device, PipelineLayout, nullptr);

    vkDestroyRenderPass(Device, RenderPass, nullptr);

    for (int Index; Index < SwapChainImageViews.size(); ++Index)
    {
        vkDestroyImageView(Device, SwapChainImageViews[Index], nullptr);
    }

    vkDestroySwapchainKHR(Device, SwapChain, nullptr);
    vkDestroyDevice(Device, nullptr);

    if (EnableValidationLayers)
    {
        PFN_vkDestroyDebugUtilsMessengerEXT DestroyDebugUtilsMessengerEXT =
            (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(Instance, "vkDestroyDebugUtilsMessengerEXT");
        if (DestroyDebugUtilsMessengerEXT != nullptr)
        {
            DestroyDebugUtilsMessengerEXT(Instance, DebugMessenger, nullptr);
        }
    }

    vkDestroySurfaceKHR(Instance, Surface, nullptr);
    vkDestroyInstance(Instance, nullptr);

    Shutdown();
}
