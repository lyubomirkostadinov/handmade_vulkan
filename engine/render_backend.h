
#pragma once

//TODO(Lyubomir) Extract stuff to Defines.h
#include "memory_arena.h"
#include "platform.h"
#include "vulkan/vulkan_core.h"

#define GLM_FORCE_RADIANS
#include "../libs/glm/glm.hpp"
#include "../libs/glm/gtc/matrix_transform.hpp"

#include "renderer.h"

//TODO(Lyubomir): Stay away from STD!
#include <chrono>
#include <vector>

#define MAX_FRAMES_IN_FLIGHT 2
uint32 CurrentFrame = 0;

struct model;

struct camera
{
    glm::vec3 Position;
    glm::vec3 Target;
    glm::vec3 Up;
    float Fov;
    float AspectRatio;
    float NearPlane;
    float FarPlane;
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

const uint32 NumVertices = 8;
const uint32 NumIndices = 36;

struct render_backend
{
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

    VkImage TextureImage;
    VkDeviceMemory TextureImageMemory;
    VkImageView TextureImageView;
    VkSampler TextureSampler;

    memory_arena GraphicsArena;
    model* CubeModel;
    model* CubeModel2;
} RenderBackend;

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

void Render();

void ShutdownRenderBackend();
