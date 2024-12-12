#include "renderer.h"
#include "memory_arena.h"
#include "render_backend.h"

buffer_group* GetModelBufferGroup(model_type ModelType)
{
    //TODO(Lyubomir): Do we want to store these in the Arena?
    buffer_group* Result = PushStruct(&RenderBackend.GraphicsArena, buffer_group);

    switch(ModelType)
    {
        case TRIANGLE:

            break;
        case CUBE:
            Result->VertexBuffer = &RenderBackend.VertexBuffer;
            Result->IndexBuffer = &RenderBackend.IndexBuffer;
            break;
        default:

            break;
    }

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
