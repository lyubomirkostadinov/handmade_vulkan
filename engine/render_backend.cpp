#include "render_backend.h"
#include "memory_arena.h"
#include "platform.h"
#include "vulkan/vulkan_core.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../libs/tiny_gltf.h"

buffer_group* GetModelBufferGroup(model_type ModelType)
{
    //TODO(Lyubomir): What do we want to do here ???
    buffer_group* Result = RenderBackend.BufferGroups[ModelType];

    switch(ModelType)
    {
        case TRIANGLE:

            break;
        case CUBE:

            break;
        case SUZANNE:
            Result->VertexBuffer = &RenderBackend.VertexBuffer;
            Result->IndexBuffer = &RenderBackend.IndexBuffer;
            break;
        default:

            break;
    }
    Assert(Result->VertexBuffer != nullptr);
    return Result;
}

model* CreateModel(memory_arena* Arena, model_type ModelType, glm::vec3 Position, glm::vec3 Rotation, glm::vec3 Scale)
{
    model* Result = nullptr;

    model TempModel;
    TempModel.ModelType = ModelType;
    TempModel.Position = Position;
    TempModel.Rotation = Rotation;
    TempModel.Scale = Scale;
    TempModel.ModelBuffers = GetModelBufferGroup(ModelType);

    Result = PushStruct(Arena, model);
    memcpy(Result, &TempModel, sizeof(model));

    CreateFrameUniformBuffers(&RenderBackend, &Result->UniformBuffers, &Result->UniformBuffersMemory, &Result->UniformBuffersMapped);

    return Result;
}

void UpdateModel(model* Model, camera* Camera)
{
    glm::mat4 ModelMatrix = glm::mat4(1.0f);
    ModelMatrix = glm::translate(ModelMatrix, Model->Position); // position
    //ModelMatrix = glm::rotate(ModelMatrix, glm::radians(90.0f), Model->Rotation); // rotation
    ModelMatrix = glm::scale(ModelMatrix, Model->Scale); // scale

    glm::vec3 CameraFrontDirection = Camera->Position + Camera->Front;

    uniform_buffer UniformBuffer = {};
    UniformBuffer.ModelMatrix = ModelMatrix;
    UniformBuffer.ViewMatrix = glm::lookAt(Camera->Position, CameraFrontDirection, Camera->Up);
    UniformBuffer.ProjectionMatrix = glm::perspective(glm::radians(Camera->AspectRatio),
                                     (float) RenderBackend.SwapChainExtent.width /
                                     (float) RenderBackend.SwapChainExtent.height,
                                     Camera->NearPlane, Camera->FarPlane);
    UniformBuffer.ProjectionMatrix[1][1] *= -1;

    memcpy(Model->UniformBuffersMapped[CurrentFrame], &UniformBuffer, sizeof(UniformBuffer));
}

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

        for (uint32 Index = 0; Index < MemoryProperties.memoryTypeCount; ++Index)
        {
            if ((TypeFilter & (1 << Index)) && (MemoryProperties.memoryTypes[Index].propertyFlags & Properties) == Properties)
            {
                return Index;
            }
        }

        printf("Failed to find suitable memory type!\n");
        return MemoryProperties.memoryTypeCount + 1;
}

VkCommandBuffer BeginSingleCommandBuffer()
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

    return CommandBuffer;
}

void EndSingleCommandBuffer(VkCommandBuffer CommandBuffer)
{
    vkEndCommandBuffer(CommandBuffer);

    VkSubmitInfo SubmitInfo = {};
    SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfo.commandBufferCount = 1;
    SubmitInfo.pCommandBuffers = &CommandBuffer;

    vkQueueSubmit(RenderBackend.GraphicsQueue, 1, &SubmitInfo, 0);
    vkQueueWaitIdle(RenderBackend.GraphicsQueue);

    vkFreeCommandBuffers(RenderBackend.Device, RenderBackend.CommandPool, 1, &CommandBuffer);
}

void TransitionImageLayout(VkImage Image, VkFormat Format, VkImageLayout OldLayout, VkImageLayout NewLayout)
{
    VkCommandBuffer CommandBuffer = BeginSingleCommandBuffer();

    VkImageMemoryBarrier Barrier = {};
    Barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    Barrier.oldLayout = OldLayout;
    Barrier.newLayout = NewLayout;
    Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    Barrier.image = Image;
    Barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    Barrier.subresourceRange.baseMipLevel = 0;
    Barrier.subresourceRange.levelCount = 1;
    Barrier.subresourceRange.baseArrayLayer = 0;
    Barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags SourceStage;
    VkPipelineStageFlags DestinationStage;

    if (OldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        NewLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        Barrier.srcAccessMask = 0;
        Barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        SourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        DestinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (OldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             NewLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        Barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        Barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        SourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        DestinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
    {
        printf("Unsupported layout transition!\n");
    }

    vkCmdPipelineBarrier(CommandBuffer, SourceStage, DestinationStage, 0, 0, nullptr, 0, nullptr, 1, &Barrier);

    EndSingleCommandBuffer(CommandBuffer);
}

void CreateImage(uint32 TextureWidth, uint32 TextureHeight, VkFormat Format,
                 VkImageTiling Tiling, VkImageUsageFlags Usage,
                 VkMemoryPropertyFlags Properties, VkImage& Image,
                 VkDeviceMemory& ImageMemory)
{
    VkImageCreateInfo ImageInfo = {};
    ImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ImageInfo.imageType = VK_IMAGE_TYPE_2D;
    ImageInfo.extent.width = TextureWidth;
    ImageInfo.extent.height = TextureHeight;
    ImageInfo.extent.depth = 1;
    ImageInfo.mipLevels = 1;
    ImageInfo.arrayLayers = 1;
    ImageInfo.format = Format;
    ImageInfo.tiling = Tiling;
    ImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ImageInfo.usage = Usage;
    ImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    ImageInfo.flags = 0; // Optional

    if (vkCreateImage(RenderBackend.Device, &ImageInfo, nullptr, &Image) != VK_SUCCESS)
    {
        printf("Failed to create image!\n");
    }

    VkMemoryRequirements MemoryRequirements;
    vkGetImageMemoryRequirements(RenderBackend.Device, Image, &MemoryRequirements);

    VkMemoryAllocateInfo AllocateInfo = {};
    AllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    AllocateInfo.allocationSize = MemoryRequirements.size;
    AllocateInfo.memoryTypeIndex = FindMemoryType(MemoryRequirements.memoryTypeBits, Properties);

    if (vkAllocateMemory(RenderBackend.Device, &AllocateInfo, nullptr, &ImageMemory) != VK_SUCCESS)
    {
        printf("Failed to allocate image memory!\n");
    }

    vkBindImageMemory(RenderBackend.Device, Image, ImageMemory, 0);
}

VkImageView CreateImageView(VkImage Image, VkFormat Format, VkImageAspectFlags AspectFlags)
{
    VkImageViewCreateInfo TextureViewInfo = {};
    TextureViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    TextureViewInfo.image = Image;
    TextureViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    TextureViewInfo.format = Format;
    TextureViewInfo.subresourceRange.aspectMask = AspectFlags;
    TextureViewInfo.subresourceRange.baseMipLevel = 0;
    TextureViewInfo.subresourceRange.levelCount = 1;
    TextureViewInfo.subresourceRange.baseArrayLayer = 0;
    TextureViewInfo.subresourceRange.layerCount = 1;

    VkImageView ImageView;
    if (vkCreateImageView(RenderBackend.Device, &TextureViewInfo, nullptr, &ImageView) != VK_SUCCESS)
    {
        printf("Failed to create texture image view!\n");
    }

    return ImageView;
}

void CopyBufferToImage(VkBuffer Buffer, VkImage Image, uint32 ImageWidth, uint32 ImageHeight)
{
    VkCommandBuffer CommandBuffer = BeginSingleCommandBuffer();

    VkOffset3D ImageOffset = {0, 0, 0};
    VkExtent3D ImageExtent = {ImageWidth, ImageHeight, 1};

    VkBufferImageCopy BufferRegion = {};
    BufferRegion.bufferOffset = 0;
    BufferRegion.bufferRowLength = 0;
    BufferRegion.bufferImageHeight = 0;
    BufferRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    BufferRegion.imageSubresource.mipLevel = 0;
    BufferRegion.imageSubresource.baseArrayLayer = 0;
    BufferRegion.imageSubresource.layerCount = 1;
    BufferRegion.imageOffset = ImageOffset;
    BufferRegion.imageExtent = ImageExtent;

    vkCmdCopyBufferToImage(CommandBuffer, Buffer, Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &BufferRegion);

    EndSingleCommandBuffer(CommandBuffer);
}

void CreateBuffer(VkDeviceSize Size, VkBufferUsageFlags Usage,
                  VkMemoryPropertyFlags MemoryProperties,
                  VkBuffer& Buffer, VkDeviceMemory& BufferMemory)
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
    VkCommandBuffer CommandBuffer = BeginSingleCommandBuffer();

    VkBufferCopy CopyRegion = {};
    CopyRegion.size = Size;
    vkCmdCopyBuffer(CommandBuffer, SourceBuffer, DestinationBuffer, 1, &CopyRegion);

    EndSingleCommandBuffer(CommandBuffer);
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

        VkDescriptorImageInfo DescriptorImageInfo = {};
        DescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        DescriptorImageInfo.imageView = RenderBackend->TextureImageView;
        DescriptorImageInfo.sampler = RenderBackend->TextureSampler;

        VkWriteDescriptorSet DescriptorWrite[2] = {};
        DescriptorWrite[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        DescriptorWrite[0].dstSet = (*DescriptorSets)[Index];
        DescriptorWrite[0].dstBinding = 0;
        DescriptorWrite[0].dstArrayElement = 0;
        DescriptorWrite[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        DescriptorWrite[0].descriptorCount = 1;
        DescriptorWrite[0].pBufferInfo = &DescriptorBufferInfo;

        DescriptorWrite[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        DescriptorWrite[1].dstSet = (*DescriptorSets)[Index];
        DescriptorWrite[1].dstBinding = 1;
        DescriptorWrite[1].dstArrayElement = 0;
        DescriptorWrite[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        DescriptorWrite[1].descriptorCount = 1;
        DescriptorWrite[1].pImageInfo = &DescriptorImageInfo;

        vkUpdateDescriptorSets(RenderBackend->Device, ArrayCount(DescriptorWrite), DescriptorWrite, 0, nullptr);
    }
}

void InitializeRenderBackend(game_memory* GameMemory)
{
    InitializeArena(&RenderBackend.GraphicsArena, GameMemory->PermanentStorageSize - sizeof(game_memory),
                    (uint8 *)GameMemory->PermanentStorage + sizeof(game_memory));

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Load  GLTF Model
    tinygltf::Model SponzaGLTFModel;

    tinygltf::TinyGLTF Loader;
    std::string Error;
    std::string Warning;

    //bool Result = Loader.LoadASCIIFromFile(&SuzanneModel, &Error, &Warning, "../resources/models/suzanne/Suzanne.gltf");
    bool Result = Loader.LoadASCIIFromFile(&SponzaGLTFModel, &Error, &Warning, "../resources/models/sponza/Sponza.gltf");

    if (!Warning.empty())
    {
      printf("Warning %s\n", Warning.c_str());
    }

    if (!Error.empty())
    {
      printf("Error: %s\n", Error.c_str());
    }

    if (!Result)
    {
      printf("Failed to parse glTF\n");
    }

    size_t TotalVertexCount = 0;
    size_t TotalIndexCount = 0;

    //NOTE(Lyubomir): Calculate Vertex Count
    for (const tinygltf::Mesh& Mesh : SponzaGLTFModel.meshes)
    {
        for (const tinygltf::Primitive& Primitive : Mesh.primitives)
        {
            std::map<std::string, int>::const_iterator PositionIterator = Primitive.attributes.find("POSITION");
            if (PositionIterator != Primitive.attributes.end())
            {
                const tinygltf::Accessor& PosAccessor = SponzaGLTFModel.accessors[PositionIterator->second];
                TotalVertexCount += PosAccessor.count;
            }

            const tinygltf::Accessor& IndexAccessor = SponzaGLTFModel.accessors[Primitive.indices];
            TotalIndexCount += IndexAccessor.count;
        }
    }

    vertex *Vertices = PushArray(&RenderBackend.GraphicsArena, TotalVertexCount ,vertex);
    uint32 *Indices = PushArray(&RenderBackend.GraphicsArena, TotalIndexCount ,uint32);

    size_t VertexOffset = 0;
    size_t IndexOffset = 0;

    //NOTE(Lyubomir): Fill data into arrays
    for (const tinygltf::Mesh& Mesh : SponzaGLTFModel.meshes)
    {
        for (const tinygltf::Primitive& Primitive : Mesh.primitives)
        {
            //NOTE(Lyubimir): Access position
            std::map<std::string, int>::const_iterator PositionIterator = Primitive.attributes.find("POSITION");
            if (PositionIterator == Primitive.attributes.end())
            {
                printf("POSITION attribute not found\n");
                continue;
            }
            const tinygltf::Accessor& PositionAccessor = SponzaGLTFModel.accessors[PositionIterator->second];
            const tinygltf::BufferView& PositionBufferView = SponzaGLTFModel.bufferViews[PositionAccessor.bufferView];
            const tinygltf::Buffer& PositionBuffer = SponzaGLTFModel.buffers[PositionBufferView.buffer];
            const float* PositionData = reinterpret_cast<const float*>(&PositionBuffer.data[PositionBufferView.byteOffset + PositionAccessor.byteOffset]);

            //NOTE(Lyubimir): Access texture coordinates
            std::map<std::string, int>::const_iterator TextureCoordinateIterator = Primitive.attributes.find("TEXCOORD_0");
            const float* TextureCoordinateData = nullptr;
            if (TextureCoordinateIterator != Primitive.attributes.end())
            {
                const tinygltf::Accessor& TextureCoordinateAccessor = SponzaGLTFModel.accessors[TextureCoordinateIterator->second];
                const tinygltf::BufferView& TextureCoordinateBufferView = SponzaGLTFModel.bufferViews[TextureCoordinateAccessor.bufferView];
                const tinygltf::Buffer& TextureCoordinateBuffer = SponzaGLTFModel.buffers[TextureCoordinateBufferView.buffer];
                TextureCoordinateData = reinterpret_cast<const float*>(&TextureCoordinateBuffer.data[TextureCoordinateBufferView.byteOffset + TextureCoordinateAccessor.byteOffset]);
            }

            //NOTE(Lyubimir): Fill vertices
            for (size_t Index = 0; Index < PositionAccessor.count; ++Index)
            {
                Vertices[VertexOffset + Index].VertexPosition = glm::vec3(PositionData[Index * 3], PositionData[Index * 3 + 1], PositionData[Index * 3 + 2]);
                Vertices[VertexOffset + Index].VertexColor = glm::vec3(1.0, 0, 0);
                if (TextureCoordinateData)
                {
                    Vertices[VertexOffset + Index].TextureCoordinate = glm::vec2(TextureCoordinateData[Index * 2], TextureCoordinateData[Index * 2 + 1]);
                }
            }

            //NOTE(Lyubimir): Access and fill indices
            const tinygltf::Accessor& IndexAccessor = SponzaGLTFModel.accessors[Primitive.indices];
            const tinygltf::BufferView& IndexBufferView = SponzaGLTFModel.bufferViews[IndexAccessor.bufferView];
            const tinygltf::Buffer& IndexBuffer = SponzaGLTFModel.buffers[IndexBufferView.buffer];

            if (IndexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
            {
                const uint32* IndexData = reinterpret_cast<const uint32*>(&IndexBuffer.data[IndexBufferView.byteOffset + IndexAccessor.byteOffset]);
                for (uint32 Index = 0; Index < IndexAccessor.count; ++Index)
                {
                    Indices[IndexOffset + Index] = IndexData[Index] + VertexOffset;
                }
            }
            else if (IndexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
            {
                const uint16* IndexData = reinterpret_cast<const uint16*>(&IndexBuffer.data[IndexBufferView.byteOffset + IndexAccessor.byteOffset]);
                for (uint32 Index = 0; Index < IndexAccessor.count; ++Index)
                {
                    Indices[IndexOffset + Index] = static_cast<uint32>(IndexData[Index]) + VertexOffset;
                }
            }
            else
            {
                printf("Unsupported index component type: %d\n", IndexAccessor.componentType);
                continue;
            }

            mesh_primitive MeshPrimitive;
            MeshPrimitive.IndexOffset = IndexOffset;
            MeshPrimitive.IndexCount = IndexAccessor.count;
            MeshPrimitive.VertexOffset = VertexOffset;
            MeshPrimitive.VertexCount = PositionAccessor.count;
            MeshPrimitive.MaterialIndex = Primitive.material;
            MeshPrimitive.TextureIndex = 1;

            if(Primitive.material >= 0)
            {
                const auto& Material = SponzaGLTFModel.materials[Primitive.material];
                if(Material.pbrMetallicRoughness.baseColorTexture.index >= 0)
                {
                    int32 TextureIndex = Material.pbrMetallicRoughness.baseColorTexture.index;
                    int32 ImageIndex = SponzaGLTFModel.textures[TextureIndex].source;
                    MeshPrimitive.TextureIndex = ImageIndex;
                }
            }
            RenderBackend.SponzaSegments.push_back(MeshPrimitive);

            VertexOffset += PositionAccessor.count;
            IndexOffset += IndexAccessor.count;
        }
    }

    NumVertices = TotalVertexCount;
    NumIndices = TotalIndexCount;

    VkVertexInputBindingDescription BindingDescription = {};
    BindingDescription.binding = 0;
    BindingDescription.stride = sizeof(vertex);
    BindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription AttributeDescriptions[3] = {};
    AttributeDescriptions[0].binding = 0;
    AttributeDescriptions[0].location = 0;
    AttributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    AttributeDescriptions[0].offset = offsetof(vertex, VertexPosition);

    AttributeDescriptions[1].binding = 0;
    AttributeDescriptions[1].location = 1;
    AttributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    AttributeDescriptions[1].offset = offsetof(vertex, VertexColor);

    AttributeDescriptions[2].binding = 0;
    AttributeDescriptions[2].location = 2;
    AttributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    AttributeDescriptions[2].offset = offsetof(vertex, TextureCoordinate);

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
    uint32 DeviceCount = 0;
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
    DeviceFeatures.samplerAnisotropy = VK_TRUE;

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

    for (uint32 Index = 0; Index < RenderBackend.SwapChainImages.size(); ++Index)
    {
        RenderBackend.SwapChainImageViews[Index] = CreateImageView(RenderBackend.SwapChainImages[Index], RenderBackend.SwapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Render Pass
    VkAttachmentDescription DepthAttachment = {};
    DepthAttachment.format = VK_FORMAT_D32_SFLOAT;
    DepthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    DepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    DepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    DepthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    DepthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    DepthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    DepthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference DepthAttachmentRef = {};
    DepthAttachmentRef.attachment = 1;
    DepthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

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
    SubPass.pDepthStencilAttachment = &DepthAttachmentRef;

    VkSubpassDependency Dependency = {};
    Dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    Dependency.dstSubpass = 0;
    Dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    Dependency.srcAccessMask = 0;
    Dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    Dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription RenderPassAttachments[2] = {ColorAttachment, DepthAttachment};

    VkRenderPassCreateInfo RenderPassInfo = {};
    RenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    RenderPassInfo.attachmentCount = ArrayCount(RenderPassAttachments);
    RenderPassInfo.pAttachments = RenderPassAttachments;
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

    VkDescriptorSetLayoutBinding SamplerLayoutBinding = {};
    SamplerLayoutBinding.binding = 1;
    SamplerLayoutBinding.descriptorCount = 1;
    SamplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    SamplerLayoutBinding.pImmutableSamplers = nullptr;
    SamplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding Bindings[2] = {UniformBufferLayoutBinding, SamplerLayoutBinding};

    VkDescriptorSetLayoutCreateInfo UniformDescriptorLayoutInfo = {};
    UniformDescriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    UniformDescriptorLayoutInfo.bindingCount = ArrayCount(Bindings);
    UniformDescriptorLayoutInfo.pBindings = Bindings;

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
    VertexInputInfo.vertexAttributeDescriptionCount = 3;
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

    VkPipelineDepthStencilStateCreateInfo DepthStencilState = {};
    DepthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    DepthStencilState.depthTestEnable = VK_TRUE;
    DepthStencilState.depthWriteEnable = VK_TRUE;
    DepthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;
    DepthStencilState.depthBoundsTestEnable = VK_FALSE;
    DepthStencilState.minDepthBounds = 0.0f; // Optional
    DepthStencilState.maxDepthBounds = 1.0f; // Optional
    DepthStencilState.stencilTestEnable = VK_FALSE;
    //DepthStencilStateCreateInfo.front = {}; // Optional
    //DepthStencilStateCreateInfo.back = {}; // Optional

    VkGraphicsPipelineCreateInfo PipelineInfo = {};
    PipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    PipelineInfo.stageCount = 2;
    PipelineInfo.pStages = ShaderStages;
    PipelineInfo.pVertexInputState = &VertexInputInfo;
    PipelineInfo.pInputAssemblyState = &InputAssembly;
    PipelineInfo.pViewportState = &ViewportState;
    PipelineInfo.pRasterizationState = &Rasterizer;
    PipelineInfo.pMultisampleState = &MultiSampling;
    PipelineInfo.pDepthStencilState = &DepthStencilState;
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
    //NOTE(Lyubomir): Create Depth Resources

    //TODO(Lyubomir): Choose the best format on the device
    VkFormat DepthFormat = VK_FORMAT_D32_SFLOAT;

    CreateImage(RenderBackend.SwapChainExtent.width, RenderBackend.SwapChainExtent.height, DepthFormat,
                VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, RenderBackend.DepthImage, RenderBackend.DepthImageMemory);

    RenderBackend.DepthImageView = CreateImageView(RenderBackend.DepthImage, DepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Frame Buffers
    RenderBackend.SwapChainFramebuffers.resize(RenderBackend.SwapChainImageViews.size());

    for (int Index = 0; Index < RenderBackend.SwapChainImageViews.size(); ++Index)
    {
        VkImageView Attachments[] =
        {
            RenderBackend.SwapChainImageViews[Index],
            RenderBackend.DepthImageView
        };

        VkFramebufferCreateInfo FramebufferInfo = {};
        FramebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        FramebufferInfo.renderPass = RenderBackend.RenderPass;
        FramebufferInfo.attachmentCount = ArrayCount(Attachments);
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
    //NOTE(Lyubomir): Create Texture Image
    texture TestTexture;
    stbi_uc* Pixels = stbi_load("../resources/models/suzanne/Suzanne_BaseColor.png", &TestTexture.TextureWidth, &TestTexture.TextureHeight, &TestTexture.TextureChannels, STBI_rgb_alpha);
    VkDeviceSize ImageSize = TestTexture.TextureWidth * TestTexture.TextureHeight * 4;

    if (!Pixels)
    {
        printf("Failed to load texture image!\n");
    }

    CreateBuffer(ImageSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 TestTexture.StagingTextureBuffer, TestTexture.StagingTextureBufferMemory);

    vkMapMemory(RenderBackend.Device, TestTexture.StagingTextureBufferMemory, 0, ImageSize, 0, &TestTexture.TextureData);
    memcpy(TestTexture.TextureData, Pixels, static_cast<size_t>(ImageSize));
    vkUnmapMemory(RenderBackend.Device, TestTexture.StagingTextureBufferMemory);

    stbi_image_free(Pixels);

    CreateImage(TestTexture.TextureWidth, TestTexture.TextureHeight,
                VK_FORMAT_R8G8B8A8_SRGB,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                RenderBackend.TextureImage, RenderBackend.TextureImageMemory);

    TransitionImageLayout(RenderBackend.TextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    CopyBufferToImage(TestTexture.StagingTextureBuffer, RenderBackend.TextureImage, static_cast<uint32>(TestTexture.TextureWidth), static_cast<uint32>(TestTexture.TextureHeight));
    TransitionImageLayout(RenderBackend.TextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(RenderBackend.Device, TestTexture.StagingTextureBuffer, nullptr);
    vkFreeMemory(RenderBackend.Device, TestTexture.StagingTextureBufferMemory, nullptr);

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Texture Image View
    RenderBackend.TextureImageView = CreateImageView(RenderBackend.TextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Texture Sampler
    VkPhysicalDeviceProperties DeviceProperties = {};
    vkGetPhysicalDeviceProperties(RenderBackend.PhysicalDevice, &DeviceProperties);

    VkSamplerCreateInfo TextureSamplerInfo = {};
    TextureSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    TextureSamplerInfo.magFilter = VK_FILTER_LINEAR;
    TextureSamplerInfo.minFilter = VK_FILTER_LINEAR;
    TextureSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    TextureSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    TextureSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    TextureSamplerInfo.anisotropyEnable = VK_TRUE;
    TextureSamplerInfo.maxAnisotropy = DeviceProperties.limits.maxSamplerAnisotropy;
    TextureSamplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    TextureSamplerInfo.unnormalizedCoordinates = VK_FALSE;
    TextureSamplerInfo.compareEnable = VK_FALSE;
    TextureSamplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    TextureSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    TextureSamplerInfo.mipLodBias = 0.0f;
    TextureSamplerInfo.minLod = 0.0f;
    TextureSamplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(RenderBackend.Device, &TextureSamplerInfo, nullptr, &RenderBackend.TextureSampler) != VK_SUCCESS)
    {
        printf("Failed to create texture sampler!\n");
    }

    ////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create All Sponza Textures

    std::vector<texture> SponzaTextures(SponzaGLTFModel.images.size());

    for (uint32 Index = 0; Index < SponzaGLTFModel.images.size(); ++Index)
    {
         auto& Texture = SponzaTextures[Index];
         auto& Image = SponzaGLTFModel.images[Index];
         int TextureWidth = 0, TextureHeight = 0, TextureChannels = 0;

         unsigned char* Pixels = nullptr;
         if (!Image.uri.empty())
         {
             std::string FullPath = std::string("../resources/models/sponza/") + Image.uri;
             Pixels = stbi_load(FullPath.c_str(), &TextureWidth, &TextureHeight, &TextureChannels, STBI_rgb_alpha);
         }
         else if (!Image.image.empty())
         {
             TextureWidth = Image.width;
             TextureHeight = Image.height;
             TextureChannels = Image.component;
             Pixels = (unsigned char*)malloc(TextureWidth * TextureHeight * 4);
             memcpy(Pixels, Image.image.data(), TextureWidth * TextureHeight * 4);
         }

         if (!Pixels)
         {
             printf("Failed to load texture image %s!\n", Image.uri.c_str());
             continue;
         }

         VkDeviceSize ImageSize = TextureWidth * TextureHeight * 4;

         //NOTE(Lyubomir): Create staging buffer, copy in pixels
         VkBuffer StagingBuffer;
         VkDeviceMemory StagingBufferMemory;
         CreateBuffer(ImageSize,
                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      StagingBuffer, StagingBufferMemory);

         void* PixelData;
         vkMapMemory(RenderBackend.Device, StagingBufferMemory, 0, ImageSize, 0, &PixelData);
         memcpy(PixelData, Pixels, ImageSize);
         vkUnmapMemory(RenderBackend.Device, StagingBufferMemory);

         //NOTE(Lyubomir): Create Image (VK_FORMAT_R8G8B8A8_SRGB matches most Sponza images)
         CreateImage(TextureWidth, TextureHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, Texture.Image, Texture.Memory);

         //NOTE(Lyubomir): Copy buffer data into image (transition + copy)
         TransitionImageLayout(Texture.Image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
         CopyBufferToImage(StagingBuffer, Texture.Image, TextureWidth, TextureHeight);
         TransitionImageLayout(Texture.Image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

         //NOTE(Lyubomir): Create view
         Texture.View = CreateImageView(Texture.Image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);

         Texture.Sampler = RenderBackend.TextureSampler;
         Texture.TextureWidth = TextureWidth;
         Texture.TextureHeight = TextureHeight;

         vkDestroyBuffer(RenderBackend.Device, StagingBuffer, nullptr);
         vkFreeMemory(RenderBackend.Device, StagingBufferMemory, nullptr);

         stbi_image_free(Pixels);
     }

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Vertex Buffer
    for(uint32 Index; Index < MAX_MODEL_TYPE; ++Index)
    {
        RenderBackend.BufferGroups[Index] = PushStruct(&RenderBackend.GraphicsArena, buffer_group);
    }

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
    RenderBackend.SponzaModel = CreateModel(&RenderBackend.GraphicsArena, SUZANNE, glm::vec3(0.0f, 0.0f, -10.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.2f, 0.2f, 0.2f));

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Descriptor Pool
    VkDescriptorPoolSize DescriptorPoolSize[2] = {};
    DescriptorPoolSize[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    DescriptorPoolSize[0].descriptorCount = static_cast<uint32>(MAX_FRAMES_IN_FLIGHT * RenderBackend.SponzaSegments.size() + MAX_FRAMES_IN_FLIGHT * 2);
    DescriptorPoolSize[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    DescriptorPoolSize[1].descriptorCount = static_cast<uint32>(MAX_FRAMES_IN_FLIGHT * RenderBackend.SponzaSegments.size() + MAX_FRAMES_IN_FLIGHT * 2);

    VkDescriptorPoolCreateInfo DescriptorPoolInfo = {};
    DescriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    DescriptorPoolInfo.poolSizeCount = ArrayCount(DescriptorPoolSize);
    DescriptorPoolInfo.pPoolSizes = DescriptorPoolSize;
    //TODO(Lyubomir): Dynamic Binding for uniform buffers so we can reuse 1 descriptor set layout for many uniform buffers?
    DescriptorPoolInfo.maxSets = static_cast<uint32>(MAX_FRAMES_IN_FLIGHT * RenderBackend.SponzaSegments.size() + MAX_FRAMES_IN_FLIGHT * 2);

    if (vkCreateDescriptorPool(RenderBackend.Device, &DescriptorPoolInfo, nullptr, &RenderBackend.DescriptorPool) != VK_SUCCESS)
    {
        printf("Failed to create descriptor pool!\n");
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Create Descriptor Sets
    RenderBackend.SponzaModel->DescriptorSetLayouts.resize(MAX_FRAMES_IN_FLIGHT);
    for(int32 Index = 0; Index < MAX_FRAMES_IN_FLIGHT; ++Index)
    {
        RenderBackend.SponzaModel->DescriptorSetLayouts[Index] = RenderBackend.DescriptorSetLayout;
    }

    model* SponzaModel = RenderBackend.SponzaModel;
    CreateDescriptorSets(&RenderBackend, &SponzaModel->DescriptorSetLayouts, &SponzaModel->DescriptorSets, &RenderBackend.DescriptorPool, &SponzaModel->UniformBuffers);

    RenderBackend.SegmentDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    for (uint32 Frame = 0; Frame < MAX_FRAMES_IN_FLIGHT; ++Frame)
    {
        RenderBackend.SegmentDescriptorSets[Frame].resize(RenderBackend.SponzaSegments.size());
        std::vector<VkDescriptorSetLayout> Layouts(RenderBackend.SponzaSegments.size(), RenderBackend.DescriptorSetLayout);
        VkDescriptorSetAllocateInfo AllocInfo = {};
        AllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        AllocInfo.descriptorPool = RenderBackend.DescriptorPool;
        AllocInfo.descriptorSetCount = (uint32)Layouts.size();
        AllocInfo.pSetLayouts = Layouts.data();

        VkResult Result = vkAllocateDescriptorSets(RenderBackend.Device, &AllocInfo, RenderBackend.SegmentDescriptorSets[Frame].data());
        if (Result != VK_SUCCESS)
        {
            printf("Failed to allocate Sponza segment descriptor sets!\n");
        }

        for (uint32 Segment = 0; Segment < RenderBackend.SponzaSegments.size(); ++Segment)
        {
            mesh_primitive& MeshPrimitive = RenderBackend.SponzaSegments[Segment];
            int32 ImageIndex = MeshPrimitive.TextureIndex;

            VkDescriptorBufferInfo BufferInfo = {};
            BufferInfo.buffer = RenderBackend.SponzaModel->UniformBuffers[Frame];
            BufferInfo.offset = 0;
            BufferInfo.range = sizeof(uniform_buffer);

            VkDescriptorImageInfo ImageInfo = {};
            ImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            ImageInfo.imageView = SponzaTextures[ImageIndex].View;
            ImageInfo.sampler = SponzaTextures[ImageIndex].Sampler;

            VkWriteDescriptorSet DescWrites[2] = {};
            //NOTE(Lyubomir): UBO
            DescWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            DescWrites[0].dstSet = RenderBackend.SegmentDescriptorSets[Frame][Segment];
            DescWrites[0].dstBinding = 0;
            DescWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            DescWrites[0].descriptorCount = 1;
            DescWrites[0].pBufferInfo = &BufferInfo;
            //NOTE(Lyubomir): Sampler
            DescWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            DescWrites[1].dstSet = RenderBackend.SegmentDescriptorSets[Frame][Segment];
            DescWrites[1].dstBinding = 1;
            DescWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            DescWrites[1].descriptorCount = 1;
            DescWrites[1].pImageInfo = &ImageInfo;

            vkUpdateDescriptorSets(RenderBackend.Device, 2, DescWrites, 0, nullptr);
        }
    }

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

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Initialize Camera
    RenderBackend.Camera = PushStruct(&RenderBackend.GraphicsArena, camera);
    RenderBackend.Camera->Position = glm::vec3(0.0f, 0.0f, 0.0f);
    RenderBackend.Camera->Front = glm::vec3(0.0f, 0.0f, -1.0f);
    RenderBackend.Camera->Up = glm::vec3(0.0f, 1.0f, 0.0f);
    RenderBackend.Camera->Right = glm::normalize(glm::cross(RenderBackend.Camera->Front, RenderBackend.Camera->Up));
    RenderBackend.Camera->AspectRatio = 45.0f;
    RenderBackend.Camera->NearPlane = 0.1f;
    RenderBackend.Camera->FarPlane = 1000.0f;
    RenderBackend.Camera->Yaw = -90.0f;
    RenderBackend.Camera->Pitch = 0.0f;
    RenderBackend.Camera->Sensitivity = 0.1f;
}

void UpdateCameraVectors(camera* Camera)
{
    Camera->Front.x = cos(glm::radians(Camera->Yaw)) * cos(glm::radians(Camera->Pitch));
    Camera->Front.y = sin(glm::radians(Camera->Pitch));
    Camera->Front.z = sin(glm::radians(Camera->Yaw)) * cos(glm::radians(Camera->Pitch));

    Camera->Front = glm::normalize(Camera->Front);
    Camera->Right = glm::normalize(cross(Camera->Front, {0.0f, 1.0f, 0.0f}));
    Camera->Up = glm::cross(Camera->Right, Camera->Front);
}

void UpdateCamera(camera *Camera)
{
    float DeltaX, DeltaY;
    ProcessMouseMove(&DeltaX, &DeltaY);

    DeltaX *= Camera->Sensitivity;
    DeltaY *= Camera->Sensitivity;

    Camera->Yaw += DeltaX;
    Camera->Pitch -= DeltaY;

    if (Camera->Pitch > 89.0f)
    {
        Camera->Pitch = 89.0f;
    }
    if (Camera->Pitch < -89.0f)
    {
        Camera->Pitch = -89.0f;
    }

    UpdateCameraVectors(Camera);

    int32 WPressed = ProcessKey(KeyW);
    int32 APressed = ProcessKey(KeyA);
    int32 SPressed = ProcessKey(KeyS);
    int32 DPressed = ProcessKey(KeyD);
    int32 SpacePressed = ProcessKey(KeySpace);
    int32 ShiftPressed = ProcessKey(KeyShift);

    float Speed = 18.0f;

    if(WPressed > -1)
    {
        Camera->Position += Camera->Front * (Speed * RenderBackend.DeltaTime);
    }
    if(SPressed > -1)
    {
        Camera->Position -= Camera->Front * (Speed * RenderBackend.DeltaTime);
    }
    if(APressed > -1)
    {
        Camera->Position -= Camera->Right * (Speed * RenderBackend.DeltaTime);
    }
    if(DPressed > -1)
    {
        Camera->Position += Camera->Right * (Speed * RenderBackend.DeltaTime);
    }
    if(SpacePressed > -1)
    {
        Camera->Position += Camera->Up * (Speed * RenderBackend.DeltaTime);
    }
    if(ShiftPressed > -1)
    {
        Camera->Position -= Camera->Up * (Speed * RenderBackend.DeltaTime);
    }
}

void Render(game_memory* GameMemory)
{
    UpdateCamera(RenderBackend.Camera);

    vkWaitForFences(RenderBackend.Device, 1, &RenderBackend.InFlightFences[CurrentFrame], VK_TRUE, UINT64_MAX);

    //TODO(Lyubomir): Handle SwapChain recreation when the window size changes!!!

    uint32_t ImageIndex;
    vkAcquireNextImageKHR(RenderBackend.Device, RenderBackend.SwapChain, UINT64_MAX, RenderBackend.ImageAvailableSemaphores[CurrentFrame], 0, &ImageIndex);

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Update Uniform Buffers

    UpdateModel(RenderBackend.SponzaModel, RenderBackend.Camera);

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Reset Fences And Command Buffer

    vkResetFences(RenderBackend.Device, 1, &RenderBackend.InFlightFences[CurrentFrame]);

    vkResetCommandBuffer(RenderBackend.CommandBuffers[CurrentFrame], 0);

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Record Command Buffer

    float r = GameMemory->r;
    float g = GameMemory->g;
    float b = GameMemory->b;
    VkClearColorValue ClearColorValue = {r, g, b, 1.0f};
    VkClearDepthStencilValue ClearDepthStencilValue = {1.0f, 0};

    VkClearValue ClearValues[2] = {};
    ClearValues[0].color = ClearColorValue;
    ClearValues[1].depthStencil = ClearDepthStencilValue;

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
    RenderPassInfo.clearValueCount = ArrayCount(ClearValues);
    RenderPassInfo.pClearValues = ClearValues;

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

    for (uint32 Segment = 0; Segment < RenderBackend.SponzaSegments.size(); ++Segment)
    {
        mesh_primitive& Primitive = RenderBackend.SponzaSegments[Segment];

        vkCmdBindDescriptorSets(RenderBackend.CommandBuffers[CurrentFrame],
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                RenderBackend.PipelineLayout,
                                0, 1,
                                &RenderBackend.SegmentDescriptorSets[CurrentFrame][Segment],
                                0, nullptr);

        vkCmdDrawIndexed(RenderBackend.CommandBuffers[CurrentFrame],
                         Primitive.IndexCount,
                         1,
                         Primitive.IndexOffset,
                         0,
                         0);
    }

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

    vkDestroyImageView(RenderBackend.Device, RenderBackend.DepthImageView, nullptr);

    vkDestroyImage(RenderBackend.Device, RenderBackend.DepthImage, nullptr);

    vkFreeMemory(RenderBackend.Device, RenderBackend.DepthImageMemory, nullptr);

    for (uint32 Index; Index < RenderBackend.SwapChainImageViews.size(); ++Index)
    {
        vkDestroyImageView(RenderBackend.Device, RenderBackend.SwapChainImageViews[Index], nullptr);
    }

    vkDestroySwapchainKHR(RenderBackend.Device, RenderBackend.SwapChain, nullptr);

    vkDestroySampler(RenderBackend.Device, RenderBackend.TextureSampler, nullptr);
    vkDestroyImageView(RenderBackend.Device, RenderBackend.TextureImageView, nullptr);

    vkDestroyImage(RenderBackend.Device, RenderBackend.TextureImage, nullptr);
    vkFreeMemory(RenderBackend.Device, RenderBackend.TextureImageMemory, nullptr);

    //TODO(Lyubomir): Destroy Sponza Textures!

    for (uint32 Index = 0; Index < MAX_FRAMES_IN_FLIGHT; ++Index)
    {
        vkDestroyBuffer(RenderBackend.Device, RenderBackend.SponzaModel->UniformBuffers[Index], nullptr);
        vkFreeMemory(RenderBackend.Device, RenderBackend.SponzaModel->UniformBuffersMemory[Index], nullptr);
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
