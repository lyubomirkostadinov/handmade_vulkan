
#pragma once

//TODO(Lyubomir) Extract stuff to Defines.h
#include "memory_arena.h"
#include "platform.h"
#include "vulkan/vulkan_core.h"
#include <cstddef>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "../libs/glm/glm.hpp"
#include "../libs/glm/gtc/matrix_transform.hpp"

#include "memory_arena.h"
#include <CoreVideo/CoreVideo.h>
#include <chrono>
#include <vector>
//TODO(Lyubomir): Stay away from STD!

#define MAX_FRAMES_IN_FLIGHT 2
uint32 CurrentFrame = 0;

struct buffer_group
{
    VkBuffer* VertexBuffer;
    VkBuffer* IndexBuffer;
};

struct texture
{
    int32 TextureWidth;
    int32 TextureHeight;
    int32 TextureChannels;

    VkBuffer StagingTextureBuffer;
    VkDeviceMemory StagingTextureBufferMemory;
    void* TextureData;

    VkImage Image;
    VkDeviceMemory Memory;
    VkImageView View;
    VkSampler Sampler;
};

enum model_type
{
    TRIANGLE,
    CUBE,
    SUZANNE,
    //TODO(Lyubomir): Model Types
    MAX_MODEL_TYPE
};

struct model
{
    glm::vec3 Position;
    glm::vec3 Rotation;
    glm::vec3 Scale;

    model_type ModelType;
    buffer_group* ModelBuffers;

    std::vector<VkBuffer> UniformBuffers;
    std::vector<VkDeviceMemory> UniformBuffersMemory;
    std::vector<void*> UniformBuffersMapped;

    std::vector<VkDescriptorSet> DescriptorSets;
    std::vector<VkDescriptorSetLayout> DescriptorSetLayouts;
};

struct camera
{
    glm::vec3 Position;
    glm::vec3 Front;
     glm::vec3 Right;
    glm::vec3 Up;
    float Fov;
    float AspectRatio;
    float NearPlane;
    float FarPlane;
    float Yaw;
    float Pitch;
    float Sensitivity;
};

struct mesh_primitive
{
    size_t IndexOffset;
    size_t IndexCount;
    size_t VertexOffset;
    size_t VertexCount;
    int32 MaterialIndex;
    int32 TextureIndex;
};

struct vertex
{
    glm::vec3 VertexPosition;
    glm::vec3 VertexColor;
    glm::vec2 TextureCoordinate;
};

struct uniform_buffer
{
    //TODO(Lyubomir): Camera
    glm::mat4 ModelMatrix;
    glm::mat4 ViewMatrix;
    glm::mat4 ProjectionMatrix;
};

uint32 NumVertices = 8;
uint32 NumIndices = 36;

struct render_backend
{
    std::vector<mesh_primitive> SponzaSegments;

    VkInstance Instance;
    VkPhysicalDevice PhysicalDevice;
    VkDevice Device;
    VkCommandPool CommandPool;
    VkSurfaceKHR Surface;
    VkQueue GraphicsQueue;
    VkQueue PresentQueue;

    VkBuffer VertexBuffer;
    VkDeviceMemory VertexBufferMemory;
    VkBuffer IndexBuffer;
    VkDeviceMemory IndexBufferMemory;

    VkRenderPass RenderPass;
    VkDebugUtilsMessengerEXT DebugMessenger;
    std::vector<VkImageView> SwapChainImageViews;
    std::vector<VkImage> SwapChainImages;
    VkFormat SwapChainImageFormat;
    VkExtent2D SwapChainExtent;
    VkSwapchainKHR SwapChain;
    bool32 EnableValidationLayers;
    std::vector<VkSemaphore> ImageAvailableSemaphores;
    std::vector<VkSemaphore> RenderFinishedSemaphores;
    std::vector<VkFence> InFlightFences;
    std::vector<VkCommandBuffer> CommandBuffers;
    std::vector<VkFramebuffer> SwapChainFramebuffers;

    VkPipeline GraphicsPipeline;
    VkPipelineLayout PipelineLayout;

    VkDescriptorPool DescriptorPool;
    VkDescriptorSetLayout DescriptorSetLayout;

    std::vector<std::vector<VkDescriptorSet>> SegmentDescriptorSets;

    VkImage TextureImage;
    VkDeviceMemory TextureImageMemory;
    VkImageView TextureImageView;
    VkSampler TextureSampler;

    VkImage DepthImage;
    VkDeviceMemory DepthImageMemory;
    VkImageView DepthImageView;

    buffer_group* BufferGroups[MAX_MODEL_TYPE];

    memory_arena GraphicsArena;
    model* SponzaModel;

    camera* Camera;
    float DeltaTime;
} RenderBackend;

buffer_group* GetModelBufferGroup(model_type ModelType);

model* CreateModel(memory_arena* Arena, model_type ModelType,  glm::vec3 Position, glm::vec3 Rotation, glm::vec3 Scale);

void UpdateModel(model* Model, camera* Camera);

uint32 FindMemoryType(uint32 TypeFilter, VkMemoryPropertyFlags Properties);

VkCommandBuffer BeginSingleCommandBuffer();

void EndSingleCommandBuffer(VkCommandBuffer CommandBuffer);

void TransitionImageLayout(VkImage Image, VkFormat Format, VkImageLayout OldLayout, VkImageLayout NewLayout);

void CreateImage(uint32_t width, uint32_t height, VkFormat format,
                 VkImageTiling tiling, VkImageUsageFlags usage,
                 VkMemoryPropertyFlags properties, VkImage& image,
                 VkDeviceMemory& imageMemory);

VkImageView CreateImageView(VkImage Image, VkFormat Format);

void CopyBufferToImage(VkBuffer Buffer, VkImage Image, uint32 ImageWidth, uint32 ImageHeight);

void CreateBuffer(VkDeviceSize Size, VkBufferUsageFlags Usage,
                  VkMemoryPropertyFlags MemoryProperties,
                  VkBuffer& Buffer, VkDeviceMemory& BufferMemory);

void CopyBuffer(VkBuffer SourceBuffer, VkBuffer DestinationBuffer, VkDeviceSize Size);

void CreateFrameUniformBuffers(render_backend* RenderBackend,
                               std::vector<VkBuffer>* UniformBuffers,
                               std::vector<VkDeviceMemory>* UniformBuffersMemory,
                               std::vector<void*>* UniformBuffersMapped);

void CreateDescriptorSets(render_backend* RenderBackend,
                          std::vector<VkDescriptorSetLayout>* DescriptorSetLayouts,
                          std::vector<VkDescriptorSet>* DescriptorSets,
                          VkDescriptorPool* DescriptorPool,
                          std::vector<VkBuffer>* UniformBuffers);

void InitializeRenderBackend(game_memory* GameMemory);

void Render(game_memory* GameMemory);

void UpdateCamera();

void ShutdownRenderBackend();
