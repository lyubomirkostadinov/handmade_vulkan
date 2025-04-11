#include "renderer.h"
#include "memory_arena.h"
#include "render_backend.h"

buffer_group* GetModelBufferGroup(model_type ModelType)
{
    buffer_group* Result = RenderBackend.BufferGroups[ModelType];

    switch(ModelType)
    {
        case TRIANGLE:

            break;
        case CUBE:

            break;
        case SUSANNE:
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
