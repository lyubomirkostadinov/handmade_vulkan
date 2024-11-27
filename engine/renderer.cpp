#include "renderer.h"

buffer_group* GetModelBufferGroup(model_type ModelType)
{
    buffer_group* Result = nullptr;

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

    //TODO(Lyubomir): MemCopy TempModel to memory_arena and return the model pointer.

    return Result;
}
