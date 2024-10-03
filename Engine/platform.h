#pragma once

#include <QuartzCore/QuartzCore.h>
#include <cstdio>
#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_metal.h>
#include <vector>

#include <fstream>
#include <cstdint>
#include <stdint.h>

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef float float32;
typedef double float64;
typedef int32 bool32;

struct internal_platform_window_state;

typedef int32 WINDOW_STYLE_FLAGS;
enum : int32
{
    WindowStyleMaskBorderless = 0,
    WindowStyleMaskTilted = 1 << 0,
    WindowStyleMaskClosable = 1 << 1,
    //TODO(Lyubomir): Add Window Flags
    WindowStyleDefault = 15
};

typedef struct window
{
    const char* Name;
    int16 Width;
    int16 Height;

    struct internal_platform_window_state* State;
}window;

bool32 Initialize(void);
void Shutdown(void);
bool32 PollEvents(void);
bool32 ShouldClose();
CAMetalLayer* GetInternalStateMetalLayer();

bool32 CreateWindow(int16 Width, int16 Height, const char* Name, WINDOW_STYLE_FLAGS Flags);
void DestroyWindow(window* Window);
void ShowWindow(window* Window);
void HideWindow(window* Window);

void ProcessKey(void);
void ProcessButton(void);
void ProcessMouseMove(void);
void ProcessMouseWheel(void);


//TODO(Lyubomir): Platform layer platform handling
static std::vector<char> ReadFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("failed to open file!\n");
    }

    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}
