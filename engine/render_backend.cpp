#include "render_backend.h"
#include "renderer.cpp"

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT MessageSeverity,
                                                    VkDebugUtilsMessageTypeFlagsEXT MessageType,
                                                    const VkDebugUtilsMessengerCallbackDataEXT* CallbackData,
                                                    void* UserData)
{
    printf("Validation layer: \n");
    printf(CallbackData->pMessage);
    printf("\n\n");

    return VK_FALSE;
}

uint32 FindMemoryType(uint32 TypeFilter, VkMemoryPropertyFlags Properties)
{
        VkPhysicalDeviceMemoryProperties MemoryProperties;
        vkGetPhysicalDeviceMemoryProperties(RenderBackend.PhysicalDevice, &MemoryProperties);

        for (uint32_t Index = 0; Index < MemoryProperties.memoryTypeCount; ++Index)
        {
            if ((TypeFilter & (1 << Index)) && (MemoryProperties.memoryTypes[Index].propertyFlags & Properties) == Properties)
            {
                return Index;
            }
        }

        printf("Failed to find suitable memory type!\n");
        return MemoryProperties.memoryTypeCount + 1;
}

void CreateBuffer(VkDeviceSize Size, VkBufferUsageFlags Usage, VkMemoryPropertyFlags MemoryProperties, VkBuffer& Buffer, VkDeviceMemory& BufferMemory)
{
    VkBufferCreateInfo BufferInfo = {};
    BufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    BufferInfo.size = Size;
    BufferInfo.usage = Usage;
    BufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(RenderBackend.Device, &BufferInfo, nullptr, &Buffer) != VK_SUCCESS)
    {
        printf("Failed to create buffer!\n");
    }

    VkMemoryRequirements MemoryRequirements;
    vkGetBufferMemoryRequirements(RenderBackend.Device, Buffer, &MemoryRequirements);

    VkMemoryAllocateInfo AllocateInfo = {};
    AllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    AllocateInfo.allocationSize = MemoryRequirements.size;
    AllocateInfo.memoryTypeIndex = FindMemoryType(MemoryRequirements.memoryTypeBits, MemoryProperties);

    //NOTE(Lyubomir): The maximum number of simultaneous memory allocations is limited by the maxMemoryAllocationCount
    // physical device limit, which may be as low as 4096 even on high end hardware like an NVIDIA GTX 1080.
    // The right way to allocate memory for a large number of objects at the same time is to create a custom allocator
    // that splits up a single allocation among many different objects by using the offset parameters.
    if (vkAllocateMemory(RenderBackend.Device, &AllocateInfo, nullptr, &BufferMemory) != VK_SUCCESS)
    {
        printf("Failed to allocate buffer memory!\n");
    }

    vkBindBufferMemory(RenderBackend.Device, Buffer, BufferMemory, 0);
}

void CopyBuffer(VkBuffer SourceBuffer, VkBuffer DestinationBuffer, VkDeviceSize Size)
{
    VkCommandBufferAllocateInfo AllocateInfo = {};
    AllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    AllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    AllocateInfo.commandPool = RenderBackend.CommandPool;
    AllocateInfo.commandBufferCount = 1;

    VkCommandBuffer CommandBuffer;
    vkAllocateCommandBuffers(RenderBackend.Device, &AllocateInfo, &CommandBuffer);

    VkCommandBufferBeginInfo BeginInfo = {};
    BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    BeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(CommandBuffer, &BeginInfo);

    VkBufferCopy CopyRegion = {};
    CopyRegion.srcOffset = 0; // Optional
    CopyRegion.dstOffset = 0; // Optional
    CopyRegion.size = Size;
    vkCmdCopyBuffer(CommandBuffer, SourceBuffer, DestinationBuffer, 1, &CopyRegion);

    vkEndCommandBuffer(CommandBuffer);

    VkSubmitInfo SubmitInfo = {};
    SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfo.commandBufferCount = 1;
    SubmitInfo.pCommandBuffers = &CommandBuffer;

    vkQueueSubmit(RenderBackend.GraphicsQueue, 1, &SubmitInfo, 0);
    vkQueueWaitIdle(RenderBackend.GraphicsQueue);

    vkFreeCommandBuffers(RenderBackend.Device, RenderBackend.CommandPool, 1, &CommandBuffer);
}

void CreateFrameUniformBuffers(render_backend* RenderBackend,
                               std::vector<VkBuffer>* UniformBuffers,
                               std::vector<VkDeviceMemory>* UniformBuffersMemory,
                               std::vector<void*>* UniformBuffersMapped)
{
    VkDeviceSize BufferSize = sizeof(uniform_buffer);

    UniformBuffers->resize(MAX_FRAMES_IN_FLIGHT);
    UniformBuffersMemory->resize(MAX_FRAMES_IN_FLIGHT);
    UniformBuffersMapped->resize(MAX_FRAMES_IN_FLIGHT);

    for (uint32 Index = 0; Index < MAX_FRAMES_IN_FLIGHT; ++Index)
    {
        CreateBuffer(BufferSize,
                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     (*UniformBuffers)[Index], (*UniformBuffersMemory)[Index]);

        vkMapMemory(RenderBackend->Device, (*UniformBuffersMemory)[Index], 0, BufferSize, 0, &(*UniformBuffersMapped)[Index]);
    }
}

void CreateDescriptorSets(render_backend* RenderBackend,
                          std::vector<VkDescriptorSetLayout>* DescriptorSetLayouts,
                          std::vector<VkDescriptorSet>* DescriptorSets,
                          VkDescriptorPool* DescriptorPool,
                          std::vector<VkBuffer>* UniformBuffers)
{
    VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo = {};
    DescriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    DescriptorSetAllocateInfo.descriptorPool = *DescriptorPool;
    DescriptorSetAllocateInfo.descriptorSetCount = static_cast<uint32>(MAX_FRAMES_IN_FLIGHT);
    DescriptorSetAllocateInfo.pSetLayouts = DescriptorSetLayouts->data();

    DescriptorSets->resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(RenderBackend->Device, &DescriptorSetAllocateInfo, DescriptorSets->data()) != VK_SUCCESS)
    {
        printf("Failed to allocate descriptor sets!\n");
    }

    for (uint32 Index = 0; Index < MAX_FRAMES_IN_FLIGHT; ++Index)
    {
        VkDescriptorBufferInfo DescriptorBufferInfo = {};
        DescriptorBufferInfo.buffer = (*UniformBuffers)[Index];
        DescriptorBufferInfo.offset = 0;
        DescriptorBufferInfo.range = sizeof(uniform_buffer);

        VkWriteDescriptorSet DescriptorWrite = {};
        DescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        DescriptorWrite.dstSet = (*DescriptorSets)[Index];
        DescriptorWrite.dstBinding = 0;
        DescriptorWrite.dstArrayElement = 0;
        DescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        DescriptorWrite.descriptorCount = 1;
        DescriptorWrite.pBufferInfo = &DescriptorBufferInfo;
        DescriptorWrite.pImageInfo = nullptr; // Optional
        DescriptorWrite.pTexelBufferView = nullptr; // Optional

        vkUpdateDescriptorSets(RenderBackend->Device, 1, &DescriptorWrite, 0, nullptr);
    }
}

void InitializeRenderBackend(game_memory* GameMemory)
{
    InitializeArena(&RenderBackend.GraphicsArena, GameMemory->PermanentStorageSize - sizeof(game_memory),
                    (uint8 *)GameMemory->PermanentStorage + sizeof(game_memory));

    vertex Vertices[NumVertices] = {};
    Vertices[0].VertexPosition   =    glm::vec3(-0.5f, -0.5f, -0.5f);
    Vertices[0].VertexColor      =    glm::vec3(1.0, 0.0, 0.0);
    Vertices[1].VertexPosition   =    glm::vec3(-0.5f,  0.5f, -0.5f);
    Vertices[1].VertexColor      =    glm::vec3(1.0, 0.0, 0.0);
    Vertices[2].VertexPosition   =    glm::vec3(0.5f, 0.5f, -0.5f);
    Vertices[2].VertexColor      =    glm::vec3(1.0, 0.0, 0.0);
    Vertices[3].VertexPosition   =    glm::vec3(0.5f, -0.5f, -0.5f);
    Vertices[3].VertexColor      =    glm::vec3(1.0, 0.0, 0.0);
    Vertices[4].VertexPosition   =    glm::vec3(-0.5f, -0.5f,  0.5f);
    Vertices[4].VertexColor      =    glm::vec3(1.0, 0.0, 0.0);
    Vertices[5].VertexPosition   =    glm::vec3(-0.5f, 0.5f, 0.5f);
    Vertices[5].VertexColor      =    glm::vec3(1.0, 0.0, 0.0);
    Vertices[6].VertexPosition   =    glm::vec3(0.5f, 0.5f, 0.5f);
    Vertices[6].VertexColor      =    glm::vec3(1.0, 0.0, 0.0);
    Vertices[7].VertexPosition   =    glm::vec3(0.5f, -0.5f, 0.5f);
    Vertices[7].VertexColor      =    glm::vec3(1.0, 0.0, 0.0);

    uint32 Indices[NumIndices] =
    {
        0, 1, 2, 0, 2, 3,
        4, 6, 5, 4, 7, 6,
        4, 5, 1, 4, 1, 0,
        3, 2, 6, 3, 6, 7,
        1, 5, 6, 1, 6, 2,
        4, 0, 3, 4, 3, 7
    };

    VkVertexInputBindingDescription BindingDescription = {};
    BindingDescription.binding = 0;
    BindingDescription.stride = sizeof(vertex);
    BindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription AttributeDescriptions[2] = {};
    AttributeDescriptions[0].binding = 0;
    AttributeDescriptions[0].location = 0;
    AttributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    AttributeDescriptions[0].offset = offsetof(vertex, VertexPosition);

    AttributeDescriptions[1].binding = 0;
    AttributeDescriptions[1].location = 1;
    AttributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    AttributeDescriptions[1].offset = offsetof(vertex, VertexColor);

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Vulkan Instance And Setup Validation Layer
    RenderBackend.EnableValidationLayers = true;

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
    if(RenderBackend.EnableValidationLayers)
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
    if (RenderBackend.EnableValidationLayers)
    {
        InstanceCreateInfo.enabledLayerCount = static_cast<uint32>(ValidationLayers.size());
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

    if (vkCreateInstance(&InstanceCreateInfo, nullptr, &RenderBackend.Instance) != VK_SUCCESS)
    {
        printf("Failed to create instance!\n");
    }

    PFN_vkCreateDebugUtilsMessengerEXT CreateDebugUtilsMessengerEXT =
        (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(RenderBackend.Instance, "vkCreateDebugUtilsMessengerEXT");
    if (CreateDebugUtilsMessengerEXT != nullptr)
    {
        VkResult Result = CreateDebugUtilsMessengerEXT(RenderBackend.Instance, &DebugCreateInfo, nullptr, &RenderBackend.DebugMessenger);
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

    if (vkCreateMetalSurfaceEXT(RenderBackend.Instance, &SurfaceCreateInfo, NULL, &RenderBackend.Surface) != VK_SUCCESS)
    {
        printf("Failed to create surface!\n");
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Pick Physical Device
    uint32_t DeviceCount = 0;
    vkEnumeratePhysicalDevices(RenderBackend.Instance, &DeviceCount, nullptr);
    if (DeviceCount == 0)
    {
        printf("Failed to find GPUs with Vulkan support! \n");
    }
    std::vector<VkPhysicalDevice> Devices(DeviceCount);
    vkEnumeratePhysicalDevices(RenderBackend.Instance, &DeviceCount, Devices.data());

    //TODO(Lyubomir): Pick the best GPU, not the first available.
    if (Devices.size() > 0)
    {
        RenderBackend.PhysicalDevice = Devices[0];
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

    DeviceCreateInfo.queueCreateInfoCount = static_cast<uint32>(QueueCreateInfos.size());
    DeviceCreateInfo.pQueueCreateInfos = QueueCreateInfos.data();

    DeviceCreateInfo.pEnabledFeatures = &DeviceFeatures;

    DeviceCreateInfo.enabledExtensionCount = static_cast<uint32>(DeviceExtensions.size());
    DeviceCreateInfo.ppEnabledExtensionNames = DeviceExtensions.data();

    if (RenderBackend.EnableValidationLayers)
    {
        DeviceCreateInfo.enabledLayerCount = static_cast<uint32>(ValidationLayers.size());
        DeviceCreateInfo.ppEnabledLayerNames = ValidationLayers.data();
    }
    else
    {
        DeviceCreateInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(RenderBackend.PhysicalDevice, &DeviceCreateInfo, nullptr, &RenderBackend.Device) != VK_SUCCESS)
    {
        printf("Failed to create logical device!\n");
    }

    vkGetDeviceQueue(RenderBackend.Device, 0, 0, &RenderBackend.GraphicsQueue);
    vkGetDeviceQueue(RenderBackend.Device, 0, 0, &RenderBackend.PresentQueue);

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Swap Chain
    VkSurfaceCapabilitiesKHR Capabilities;
    std::vector<VkSurfaceFormatKHR> Formats;
    std::vector<VkPresentModeKHR> PresentModes;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(RenderBackend.PhysicalDevice, RenderBackend.Surface, &Capabilities);

    uint32 FormatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(RenderBackend.PhysicalDevice, RenderBackend.Surface, &FormatCount, nullptr);
    if (FormatCount != 0)
    {
        Formats.resize(FormatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(RenderBackend.PhysicalDevice, RenderBackend.Surface, &FormatCount, Formats.data());
    }
    uint32 PresentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(RenderBackend.PhysicalDevice, RenderBackend.Surface, &PresentModeCount, nullptr);
    if (PresentModeCount != 0)
    {
        PresentModes.resize(PresentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(RenderBackend.PhysicalDevice, RenderBackend.Surface, &PresentModeCount, PresentModes.data());
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

    uint32 ImageCount = Capabilities.minImageCount + 1;
    if (Capabilities.maxImageCount > 0 && ImageCount > Capabilities.maxImageCount)
    {
        ImageCount = Capabilities.maxImageCount;
    }
    VkSwapchainCreateInfoKHR SpawChainCreateInfo = {};
    SpawChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    SpawChainCreateInfo.surface = RenderBackend.Surface;
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

    if (vkCreateSwapchainKHR(RenderBackend.Device, &SpawChainCreateInfo, nullptr, &RenderBackend.SwapChain) != VK_SUCCESS)
    {
        printf("Failed to create swap chain!\n");
    }

    vkGetSwapchainImagesKHR(RenderBackend.Device, RenderBackend.SwapChain, &ImageCount, nullptr);
    RenderBackend.SwapChainImages.resize(ImageCount);
    vkGetSwapchainImagesKHR(RenderBackend.Device, RenderBackend.SwapChain, &ImageCount, RenderBackend.SwapChainImages.data());
    RenderBackend.SwapChainImageFormat = SurfaceFormat.format;
    RenderBackend.SwapChainExtent = Extent;

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Image Views
    RenderBackend.SwapChainImageViews.resize(RenderBackend.SwapChainImages.size());
    for (uint32 Index = 0; Index < RenderBackend.SwapChainImages.size(); Index++)
    {
        VkImageViewCreateInfo ImageViewCreateInfo = {};
        ImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ImageViewCreateInfo.image = RenderBackend.SwapChainImages[Index];
        ImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ImageViewCreateInfo.format = RenderBackend.SwapChainImageFormat;
        ImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        ImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        ImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        ImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        ImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        ImageViewCreateInfo.subresourceRange.levelCount = 1;
        ImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        ImageViewCreateInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(RenderBackend.Device, &ImageViewCreateInfo, nullptr, &RenderBackend.SwapChainImageViews[Index]) != VK_SUCCESS)
        {
            printf("Failed to create image views!\n");
        }
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Render Pass
    VkAttachmentDescription ColorAttachment = {};
    ColorAttachment.format = RenderBackend.SwapChainImageFormat;
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

    if (vkCreateRenderPass(RenderBackend.Device, &RenderPassInfo, nullptr, &RenderBackend.RenderPass) != VK_SUCCESS)
    {
        printf("Failed to create render pass!\n");
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Descriptor Set Layout For Uniform Buffer
    VkDescriptorSetLayoutBinding UniformBufferLayoutBinding = {};
    UniformBufferLayoutBinding.binding = 0;
    UniformBufferLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    UniformBufferLayoutBinding.descriptorCount = 1;
    UniformBufferLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo UniformDescriptorLayoutInfo = {};
    UniformDescriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    UniformDescriptorLayoutInfo.bindingCount = 1;
    UniformDescriptorLayoutInfo.pBindings = &UniformBufferLayoutBinding;

    if (vkCreateDescriptorSetLayout(RenderBackend.Device, &UniformDescriptorLayoutInfo, nullptr, &RenderBackend.DescriptorSetLayout) != VK_SUCCESS)
    {
        printf("Failed to create descriptor set layout!\n");
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Graphics Pipeline

    std::vector<char> VertexShaderCode = ReadFile("../shaders/vertex_default_shader.spv");
    std::vector<char> FragmentShaderCode = ReadFile("../shaders/fragment_default_shader.spv");

    VkShaderModuleCreateInfo VertexShaderCreateInfo = {};
    VertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    VertexShaderCreateInfo.codeSize = VertexShaderCode.size();
    VertexShaderCreateInfo.pCode = reinterpret_cast<const uint32*>(VertexShaderCode.data());

    VkShaderModule VertexShaderModule;
    if (vkCreateShaderModule(RenderBackend.Device, &VertexShaderCreateInfo, nullptr, &VertexShaderModule) != VK_SUCCESS)
    {
        printf("Failed to create vertex shader module!\n");
    }

    VkShaderModuleCreateInfo FragmentShaderCreateInfo = {};
    FragmentShaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    FragmentShaderCreateInfo.codeSize = FragmentShaderCode.size();
    FragmentShaderCreateInfo.pCode = reinterpret_cast<const uint32*>(FragmentShaderCode.data());

    VkShaderModule FragmentShaderModule;
    if (vkCreateShaderModule(RenderBackend.Device, &FragmentShaderCreateInfo, nullptr, &FragmentShaderModule) != VK_SUCCESS)
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
    VertexInputInfo.vertexBindingDescriptionCount = 1;
    VertexInputInfo.vertexAttributeDescriptionCount = 2;
    VertexInputInfo.pVertexBindingDescriptions = &BindingDescription;
    VertexInputInfo.pVertexAttributeDescriptions = AttributeDescriptions;

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
    Rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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
    DynamicState.dynamicStateCount = static_cast<uint32>(DynamicStates.size());
    DynamicState.pDynamicStates = DynamicStates.data();

    VkPipelineLayoutCreateInfo PipelineLayoutInfo = {};
    PipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    PipelineLayoutInfo.setLayoutCount = 1;
    PipelineLayoutInfo.pSetLayouts = &RenderBackend.DescriptorSetLayout;
    PipelineLayoutInfo.pushConstantRangeCount = 0;

    if (vkCreatePipelineLayout(RenderBackend.Device, &PipelineLayoutInfo, nullptr, &RenderBackend.PipelineLayout) != VK_SUCCESS)
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
    PipelineInfo.layout = RenderBackend.PipelineLayout;
    PipelineInfo.renderPass = RenderBackend.RenderPass;
    PipelineInfo.subpass = 0;
    PipelineInfo.basePipelineHandle = 0; // Optional
    PipelineInfo.basePipelineIndex = -1; // Optional

    if (vkCreateGraphicsPipelines(RenderBackend.Device, 0, 1, &PipelineInfo, nullptr, &RenderBackend.GraphicsPipeline) != VK_SUCCESS)
    {
        printf("Failed to create graphics pipeline!\n");
    }

    vkDestroyShaderModule(RenderBackend.Device, FragmentShaderModule, nullptr);
    vkDestroyShaderModule(RenderBackend.Device, VertexShaderModule, nullptr);

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Frame Buffers
    RenderBackend.SwapChainFramebuffers.resize(RenderBackend.SwapChainImageViews.size());

    for (int Index = 0; Index < RenderBackend.SwapChainImageViews.size(); ++Index)
    {
        VkImageView Attachments[] =
        {
            RenderBackend.SwapChainImageViews[Index]
        };

        VkFramebufferCreateInfo FramebufferInfo = {};
        FramebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        FramebufferInfo.renderPass = RenderBackend.RenderPass;
        FramebufferInfo.attachmentCount = 1;
        FramebufferInfo.pAttachments = Attachments;
        FramebufferInfo.width = RenderBackend.SwapChainExtent.width;
        FramebufferInfo.height = RenderBackend.SwapChainExtent.height;
        FramebufferInfo.layers = 1;

        if (vkCreateFramebuffer(RenderBackend.Device, &FramebufferInfo, nullptr, &RenderBackend.SwapChainFramebuffers[Index]) != VK_SUCCESS)
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

    if (vkCreateCommandPool(RenderBackend.Device, &PoolInfo, nullptr, &RenderBackend.CommandPool) != VK_SUCCESS)
    {
        printf("Failed to create command pool!\n");
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Vertex Buffer
    VkDeviceSize VertexBufferSize = sizeof(Vertices[0]) * NumVertices;

    VkBuffer StagingVertexBuffer;
    VkDeviceMemory StagingVertexBufferMemory;

    CreateBuffer(VertexBufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 StagingVertexBuffer, StagingVertexBufferMemory);

    void* VertexBufferData;
    vkMapMemory(RenderBackend.Device, StagingVertexBufferMemory, 0, VertexBufferSize, 0, &VertexBufferData);
    memcpy(VertexBufferData, Vertices, (uint64)VertexBufferSize);
    vkUnmapMemory(RenderBackend.Device, StagingVertexBufferMemory);

    CreateBuffer(VertexBufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 RenderBackend.VertexBuffer, RenderBackend.VertexBufferMemory);

    CopyBuffer(StagingVertexBuffer, RenderBackend.VertexBuffer, VertexBufferSize);

    vkDestroyBuffer(RenderBackend.Device, StagingVertexBuffer, nullptr);
    vkFreeMemory(RenderBackend.Device, StagingVertexBufferMemory, nullptr);

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Index Buffer
    VkDeviceSize IndexBufferSize = sizeof(Indices[0]) * NumIndices;

    VkBuffer StagingIndexBuffer;
    VkDeviceMemory StagingIndexBufferMemory;

    CreateBuffer(IndexBufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 StagingIndexBuffer, StagingIndexBufferMemory);

    void* IndexBufferData;
    vkMapMemory(RenderBackend.Device, StagingIndexBufferMemory, 0, IndexBufferSize, 0, &IndexBufferData);
    memcpy(IndexBufferData, Indices, (uint64)IndexBufferSize);
    vkUnmapMemory(RenderBackend.Device, StagingIndexBufferMemory);

    CreateBuffer(IndexBufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 RenderBackend.IndexBuffer, RenderBackend.IndexBufferMemory);

    CopyBuffer(StagingIndexBuffer, RenderBackend.IndexBuffer, IndexBufferSize);

    vkDestroyBuffer(RenderBackend.Device, StagingIndexBuffer, nullptr);
    vkFreeMemory(RenderBackend.Device, StagingIndexBufferMemory, nullptr);

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Uniform Buffers
    RenderBackend.CubeModel = CreateModel(&RenderBackend.GraphicsArena, CUBE, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 5.0f, 1.0f), glm::vec3(1.3f, 1.3f, 1.3f));

    CreateFrameUniformBuffers(&RenderBackend, &RenderBackend.UniformBuffers, &RenderBackend.UniformBuffersMemory, &RenderBackend.UniformBuffersMapped);

    CreateFrameUniformBuffers(&RenderBackend, &RenderBackend.UniformBuffers2, &RenderBackend.UniformBuffersMemory2, &RenderBackend.UniformBuffersMapped2);

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Descriptor Pool
    VkDescriptorPoolSize DescriptorPoolSize = {};
    DescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    DescriptorPoolSize.descriptorCount = static_cast<uint32>(MAX_FRAMES_IN_FLIGHT * 2);

    VkDescriptorPoolCreateInfo DescriptorPoolInfo = {};
    DescriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    DescriptorPoolInfo.poolSizeCount = 1;
    DescriptorPoolInfo.pPoolSizes = &DescriptorPoolSize;
    DescriptorPoolInfo.maxSets = static_cast<uint32>(MAX_FRAMES_IN_FLIGHT * 2);

    if (vkCreateDescriptorPool(RenderBackend.Device, &DescriptorPoolInfo, nullptr, &RenderBackend.DescriptorPool) != VK_SUCCESS)
    {
        printf("Failed to create descriptor pool!\n");
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Descriptor Sets
    RenderBackend.DescriptorSetLayouts.resize(MAX_FRAMES_IN_FLIGHT);
    RenderBackend.DescriptorSetLayouts2.resize(MAX_FRAMES_IN_FLIGHT);
    for(int32 Index = 0; Index < MAX_FRAMES_IN_FLIGHT; ++Index)
    {
        RenderBackend.DescriptorSetLayouts[Index] = RenderBackend.DescriptorSetLayout;
        RenderBackend.DescriptorSetLayouts2[Index] = RenderBackend.DescriptorSetLayout;
    }

    //model* CubeModel = RenderBackend.CubeModel;
    CreateDescriptorSets(&RenderBackend, &RenderBackend.DescriptorSetLayouts, &RenderBackend.DescriptorSets, &RenderBackend.DescriptorPool, &RenderBackend.UniformBuffers);

    CreateDescriptorSets(&RenderBackend, &RenderBackend.DescriptorSetLayouts2, &RenderBackend.DescriptorSets2, &RenderBackend.DescriptorPool, &RenderBackend.UniformBuffers2);

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Command Buffers
    RenderBackend.CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo AllocInfo = {};
    AllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    AllocInfo.commandPool = RenderBackend.CommandPool;
    AllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    AllocInfo.commandBufferCount = (uint32)RenderBackend.CommandBuffers.size();

    if (vkAllocateCommandBuffers(RenderBackend.Device, &AllocInfo, RenderBackend.CommandBuffers.data()) != VK_SUCCESS)
    {
        printf("Failed to allocate command buffers!\n");
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Sync Objects
    RenderBackend.ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    RenderBackend.RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    RenderBackend.InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo SemaphoreInfo = {};
    SemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo FenceInfo = {};
    FenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    FenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32 Index = 0; Index < MAX_FRAMES_IN_FLIGHT; ++Index)
    {
        if (vkCreateSemaphore(RenderBackend.Device, &SemaphoreInfo, nullptr, &RenderBackend.ImageAvailableSemaphores[Index]) != VK_SUCCESS ||
            vkCreateSemaphore(RenderBackend.Device, &SemaphoreInfo, nullptr, &RenderBackend.RenderFinishedSemaphores[Index]) != VK_SUCCESS ||
            vkCreateFence(RenderBackend.Device, &FenceInfo, nullptr, &RenderBackend.InFlightFences[Index]) != VK_SUCCESS)
        {
            printf("Failed to create semaphores!\n");
        }
    }
}

void Render()
{
    vkWaitForFences(RenderBackend.Device, 1, &RenderBackend.InFlightFences[CurrentFrame], VK_TRUE, UINT64_MAX);

    //TODO(Lyubomir): Handle SwapChain recreation when the window size changes!!!

    uint32_t ImageIndex;
    vkAcquireNextImageKHR(RenderBackend.Device, RenderBackend.SwapChain, UINT64_MAX, RenderBackend.ImageAvailableSemaphores[CurrentFrame], 0, &ImageIndex);

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Update Uniform Buffer
    static auto StartTime = std::chrono::high_resolution_clock::now();
    auto CurrentTime = std::chrono::high_resolution_clock::now();
    float Time = std::chrono::duration<float, std::chrono::seconds::period>(CurrentTime - StartTime).count();

    glm::mat4 ModelMatrix = glm::mat4(1.0f);
    ModelMatrix = glm::translate(ModelMatrix, glm::vec3(0.0f, 0.0f, 0.0f)); // position
    ModelMatrix = glm::rotate(ModelMatrix, Time * glm::radians(90.0f), glm::vec3(0.0f, 5.0f, 1.0f)); // rotation
    ModelMatrix = glm::scale(ModelMatrix, glm::vec3(1.3f, 1.3f, 1.3f)); // scale

    camera Camera = {};
    Camera.Position = glm::vec3(2.0f, 2.0f, 10.0f);
    Camera.Target = glm::vec3(0.0f, 0.0f, 0.0f);
    Camera.Up = glm::vec3(0.0f, 0.0f, 1.0f);
    Camera.AspectRatio = 45.0f;
    Camera.NearPlane = 0.1f;
    Camera.FarPlane = 100.0f;

    uniform_buffer UniformBuffer = {};
    UniformBuffer.ModelMatrix = ModelMatrix;
    UniformBuffer.ViewMatrix = glm::lookAt(Camera.Position, Camera.Target, Camera.Up);
    UniformBuffer.ProjectionMatrix = glm::perspective(glm::radians(Camera.AspectRatio),
                                     (float) RenderBackend.SwapChainExtent.width /
                                     (float) RenderBackend.SwapChainExtent.height,
                                     Camera.NearPlane, Camera.FarPlane);
    UniformBuffer.ProjectionMatrix[1][1] *= -1;

    memcpy(RenderBackend.UniformBuffersMapped[CurrentFrame], &UniformBuffer, sizeof(UniformBuffer));

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Reset Fences And Command Buffer

    vkResetFences(RenderBackend.Device, 1, &RenderBackend.InFlightFences[CurrentFrame]);

    vkResetCommandBuffer(RenderBackend.CommandBuffers[CurrentFrame], 0);

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Record Command Buffer
    VkCommandBufferBeginInfo BeginInfo = {};
    BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    BeginInfo.flags = 0; // Optional
    BeginInfo.pInheritanceInfo = nullptr; // Optional

    if (vkBeginCommandBuffer(RenderBackend.CommandBuffers[CurrentFrame], &BeginInfo) != VK_SUCCESS)
    {
        printf("Failed to begin recording command buffer!\n");
    }

    VkRenderPassBeginInfo RenderPassInfo = {};
    RenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    RenderPassInfo.renderPass = RenderBackend.RenderPass;
    RenderPassInfo.framebuffer = RenderBackend.SwapChainFramebuffers[ImageIndex];
    RenderPassInfo.renderArea.offset.x = 0;
    RenderPassInfo.renderArea.offset.x = 0;
    RenderPassInfo.renderArea.extent = RenderBackend.SwapChainExtent;
    VkClearValue ClearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    RenderPassInfo.clearValueCount = 1;
    RenderPassInfo.pClearValues = &ClearColor;

    vkCmdBeginRenderPass(RenderBackend.CommandBuffers[CurrentFrame], &RenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(RenderBackend.CommandBuffers[CurrentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, RenderBackend.GraphicsPipeline);

    VkViewport Viewport = {};
    Viewport.x = 0.0f;
    Viewport.y = 0.0f;
    Viewport.width = (float) RenderBackend.SwapChainExtent.width;
    Viewport.height = (float) RenderBackend.SwapChainExtent.height;
    Viewport.minDepth = 0.0f;
    Viewport.maxDepth = 1.0f;

    vkCmdSetViewport(RenderBackend.CommandBuffers[CurrentFrame], 0, 1, &Viewport);

    VkRect2D Scissor = {};
    Scissor.offset.x = 0;
    Scissor.offset.y = 0;
    Scissor.extent = RenderBackend.SwapChainExtent;

    vkCmdSetScissor(RenderBackend.CommandBuffers[CurrentFrame], 0, 1, &Scissor);

    VkBuffer VertexBuffers[] = {RenderBackend.VertexBuffer};
    VkDeviceSize Offsets[] = {0};
    vkCmdBindVertexBuffers(RenderBackend.CommandBuffers[CurrentFrame], 0, 1, VertexBuffers, Offsets);

    vkCmdBindIndexBuffer(RenderBackend.CommandBuffers[CurrentFrame], RenderBackend.IndexBuffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(RenderBackend.CommandBuffers[CurrentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            RenderBackend.PipelineLayout, 0, 1, &RenderBackend.DescriptorSets[CurrentFrame], 0, nullptr);

    vkCmdDrawIndexed(RenderBackend.CommandBuffers[CurrentFrame], NumIndices, 1, 0, 0, 0);

    //////////////////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Draw Second Cube

    glm::mat4 ModelMatrix2 = glm::mat4(1.0f);
    ModelMatrix2 = glm::translate(ModelMatrix2, glm::vec3(4.0f, 0.0f, 0.0f)); // position
    ModelMatrix2 = glm::rotate(ModelMatrix2, Time * glm::radians(90.0f), glm::vec3(2.0f, 0.0f, 5.0f)); // rotation
    ModelMatrix2 = glm::scale(ModelMatrix2, glm::vec3(1.0f, 1.0f, 1.0f)); // scale

    uniform_buffer UniformBuffer2 = {};
    UniformBuffer2.ModelMatrix = ModelMatrix2;
    UniformBuffer2.ViewMatrix = glm::lookAt(Camera.Position, Camera.Target, Camera.Up);
    UniformBuffer2.ProjectionMatrix = glm::perspective(glm::radians(Camera.AspectRatio),
                                     (float) RenderBackend.SwapChainExtent.width /
                                     (float) RenderBackend.SwapChainExtent.height,
                                     Camera.NearPlane, Camera.FarPlane);
    UniformBuffer2.ProjectionMatrix[1][1] *= -1;

    memcpy(RenderBackend.UniformBuffersMapped2[CurrentFrame], &UniformBuffer2, sizeof(UniformBuffer2));

    vkCmdBindDescriptorSets(RenderBackend.CommandBuffers[CurrentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, RenderBackend.PipelineLayout, 0, 1, &RenderBackend.DescriptorSets2[CurrentFrame], 0, nullptr);

    vkCmdDrawIndexed(RenderBackend.CommandBuffers[CurrentFrame], NumIndices, 1, 0, 0, 0);

    vkCmdEndRenderPass(RenderBackend.CommandBuffers[CurrentFrame]);

    if (vkEndCommandBuffer(RenderBackend.CommandBuffers[CurrentFrame]) != VK_SUCCESS)
    {
        printf("Failed to record command buffer!\n");
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Submit Frame
    VkSubmitInfo SubmitInfo = {};
    SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore WaitSemaphores[] = { RenderBackend.ImageAvailableSemaphores[CurrentFrame] };
    VkPipelineStageFlags WaitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    SubmitInfo.waitSemaphoreCount = 1;
    SubmitInfo.pWaitSemaphores = WaitSemaphores;
    SubmitInfo.pWaitDstStageMask = WaitStages;
    SubmitInfo.commandBufferCount = 1;
    SubmitInfo.pCommandBuffers = &RenderBackend.CommandBuffers[CurrentFrame];

    VkSemaphore SignalSemaphores[] = { RenderBackend.RenderFinishedSemaphores[CurrentFrame] };
    SubmitInfo.signalSemaphoreCount = 1;
    SubmitInfo.pSignalSemaphores = SignalSemaphores;

    if (vkQueueSubmit(RenderBackend.GraphicsQueue, 1, &SubmitInfo, RenderBackend.InFlightFences[CurrentFrame]) != VK_SUCCESS)
    {
        printf("Failed to submit draw command buffer!\n");
    }

    VkPresentInfoKHR PresentInfo = {};
    PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    PresentInfo.waitSemaphoreCount = 1;
    PresentInfo.pWaitSemaphores = SignalSemaphores;

    VkSwapchainKHR swapChains[] = {RenderBackend.SwapChain};
    PresentInfo.swapchainCount = 1;
    PresentInfo.pSwapchains = swapChains;
    PresentInfo.pImageIndices = &ImageIndex;
    PresentInfo.pResults = nullptr; // Optional

    vkQueuePresentKHR(RenderBackend.PresentQueue, &PresentInfo);

    CurrentFrame = (CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void ShutdownRenderBackend()
{
    vkDeviceWaitIdle(RenderBackend.Device);

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Clean Up
    for (uint32 Index = 0; Index < MAX_FRAMES_IN_FLIGHT; ++Index)
    {
        vkDestroySemaphore(RenderBackend.Device, RenderBackend.ImageAvailableSemaphores[Index], nullptr);
        vkDestroySemaphore(RenderBackend.Device, RenderBackend.RenderFinishedSemaphores[Index], nullptr);
        vkDestroyFence(RenderBackend.Device, RenderBackend.InFlightFences[Index], nullptr);
    }
    vkDestroyCommandPool(RenderBackend.Device, RenderBackend.CommandPool, nullptr);

    for (uint32 Index = 0;Index < RenderBackend.SwapChainFramebuffers.size(); ++Index)
    {
        vkDestroyFramebuffer(RenderBackend.Device, RenderBackend.SwapChainFramebuffers[Index], nullptr);
    }

    vkDestroyPipeline(RenderBackend.Device, RenderBackend.GraphicsPipeline, nullptr);

    vkDestroyPipelineLayout(RenderBackend.Device, RenderBackend.PipelineLayout, nullptr);

    vkDestroyRenderPass(RenderBackend.Device, RenderBackend.RenderPass, nullptr);

    for (uint32 Index; Index < RenderBackend.SwapChainImageViews.size(); ++Index)
    {
        vkDestroyImageView(RenderBackend.Device, RenderBackend.SwapChainImageViews[Index], nullptr);
    }

    vkDestroySwapchainKHR(RenderBackend.Device, RenderBackend.SwapChain, nullptr);

    for (uint32 Index = 0; Index < MAX_FRAMES_IN_FLIGHT; ++Index)
    {
        vkDestroyBuffer(RenderBackend.Device, RenderBackend.UniformBuffers[Index], nullptr);
        vkFreeMemory(RenderBackend.Device, RenderBackend.UniformBuffersMemory[Index], nullptr);
    }

    vkDestroyDescriptorPool(RenderBackend.Device, RenderBackend.DescriptorPool, nullptr);

    vkDestroyDescriptorSetLayout(RenderBackend.Device, RenderBackend.DescriptorSetLayout, nullptr);

    vkDestroyBuffer(RenderBackend.Device, RenderBackend.IndexBuffer, nullptr);

    vkFreeMemory(RenderBackend.Device, RenderBackend.IndexBufferMemory, nullptr);

    vkDestroyBuffer(RenderBackend.Device, RenderBackend.VertexBuffer, nullptr);

    vkFreeMemory(RenderBackend.Device, RenderBackend.VertexBufferMemory, nullptr);

    vkDestroyDevice(RenderBackend.Device, nullptr);

    if (RenderBackend.EnableValidationLayers)
    {
        PFN_vkDestroyDebugUtilsMessengerEXT DestroyDebugUtilsMessengerEXT =
            (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(RenderBackend.Instance, "vkDestroyDebugUtilsMessengerEXT");
        if (DestroyDebugUtilsMessengerEXT != nullptr)
        {
            DestroyDebugUtilsMessengerEXT(RenderBackend.Instance, RenderBackend.DebugMessenger, nullptr);
        }
    }

    vkDestroySurfaceKHR(RenderBackend.Instance, RenderBackend.Surface, nullptr);
    vkDestroyInstance(RenderBackend.Instance, nullptr);
}
