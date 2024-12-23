#version 450

layout(binding = 0) uniform uniform_buffer
{
    mat4 ModelMatrix;
    mat4 ViewMatrix;
    mat4 ProjectionMatrix;
} UniformBuffer;

layout(location = 0) in vec3 Position;
layout(location = 1) in vec3 Color;
layout(location = 2) in vec2 TextureCoordinate;

layout(location = 0) out vec3 FragmentColor;
layout(location = 1) out vec2 FragmentTextureCoordinate;

void main()
{
    gl_Position = UniformBuffer.ProjectionMatrix * UniformBuffer.ViewMatrix * UniformBuffer.ModelMatrix * vec4(Position, 1.0);
    FragmentColor = Color;
    FragmentTextureCoordinate = TextureCoordinate;
}
