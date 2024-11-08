#include "render_backend.h"

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
