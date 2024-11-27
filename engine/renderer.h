#pragma once

#include "render_backend.h"
#include <ImageIO/ImageIO.h>

struct memory_arena
{
    //TODO(Lyubomir): Memory Allocator
};

struct buffer_group
{
    VkBuffer* VertexBuffer;
    VkBuffer* IndexBuffer;
};

enum model_type
{
    Triangle,
    Cube,
    //TODO: Model Types
};

struct model
{
    glm::vec3 Position;
    glm::vec3 Rotation;
    glm::vec3 Scale;

    model_type ModelType;
    buffer_group* ModelBuffers;
    VkBuffer* UniformBuffer;
};


buffer_group* GetModelBufferGroup(model_type ModelType);
model* CreateModel(memory_arena* Arena, model_type ModelType,  glm::vec3 Position, glm::vec3 Rotation, glm::vec3 Scale);
