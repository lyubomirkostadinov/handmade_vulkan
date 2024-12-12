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

void UpdateModel(model* Model, camera* Camera)
{
    static auto StartTime = std::chrono::high_resolution_clock::now();
    auto CurrentTime = std::chrono::high_resolution_clock::now();
    float Time = std::chrono::duration<float, std::chrono::seconds::period>(CurrentTime - StartTime).count();

    glm::mat4 ModelMatrix = glm::mat4(1.0f);
    ModelMatrix = glm::translate(ModelMatrix, Model->Position); // position
    ModelMatrix = glm::rotate(ModelMatrix, Time * glm::radians(90.0f), Model->Rotation); // rotation
    ModelMatrix = glm::scale(ModelMatrix, Model->Scale); // scale

    uniform_buffer UniformBuffer = {};
    UniformBuffer.ModelMatrix = ModelMatrix;
    UniformBuffer.ViewMatrix = glm::lookAt(Camera->Position, Camera->Target, Camera->Up);
    UniformBuffer.ProjectionMatrix = glm::perspective(glm::radians(Camera->AspectRatio),
                                     (float) RenderBackend.SwapChainExtent.width /
                                     (float) RenderBackend.SwapChainExtent.height,
                                     Camera->NearPlane, Camera->FarPlane);
    UniformBuffer.ProjectionMatrix[1][1] *= -1;

    memcpy(Model->UniformBuffersMapped[CurrentFrame], &UniformBuffer, sizeof(UniformBuffer));
}
