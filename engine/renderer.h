#include "render_backend.h"

struct model
{
    glm::vec3 Position;
    glm::vec3 Rotation;
    glm::vec3 Scale;

    VkBuffer* VertexBuffer;
    VkBuffer* UniformBuffer;
};
