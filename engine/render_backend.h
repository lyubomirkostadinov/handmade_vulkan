#pragma once

//TODO(Lyubomir) Extract stuff to Defines.h
#include "platform.h"
#include "vulkan/vulkan_core.h"

#define GLM_FORCE_RADIANS
#include "../libs/glm/glm.hpp"
#include "../libs/glm/gtc/matrix_transform.hpp"

//TODO(Lyubomir): Stay away from STD!
#include <chrono>
#include <vector>

#define MAX_FRAMES_IN_FLIGHT 2
uint32 CurrentFrame = 0;

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
};

struct uniform_buffer
{
    //TODO(Lyubomir): Camera
    glm::mat4 ModelMatrix;
    glm::mat4 ViewMatrix;
    glm::mat4 ProjectionMatrix;
};

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
} RenderBackend;

uint32 FindMemoryType(uint32 TypeFilter, VkMemoryPropertyFlags Properties);

void CreateBuffer(VkDeviceSize Size, VkBufferUsageFlags Usage, VkMemoryPropertyFlags MemoryProperties, VkBuffer& Buffer, VkDeviceMemory& BufferMemory);

void CopyBuffer(VkBuffer SourceBuffer, VkBuffer DestinationBuffer, VkDeviceSize Size);
