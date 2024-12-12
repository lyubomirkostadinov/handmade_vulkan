#pragma once

#include "render_backend.h"
#include "memory_arena.h"
#include <vector>

struct buffer_group
{
    VkBuffer* VertexBuffer;
    VkBuffer* IndexBuffer;
};

enum model_type
{
    TRIANGLE,
    CUBE,
    //TODO: Model Types
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

buffer_group* GetModelBufferGroup(model_type ModelType);
model* CreateModel(memory_arena* Arena, model_type ModelType,  glm::vec3 Position, glm::vec3 Rotation, glm::vec3 Scale);

class camera;
void UpdateModel(model* Model, camera* Camera);
