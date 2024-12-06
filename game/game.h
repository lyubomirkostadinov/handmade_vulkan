#pragma once

#include <cstddef>
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
typedef size_t memory_index;

#define DEBUG_BUILD 1

#if DEBUG_BUILD
#define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
#else
#define Assert(Expression)
#endif

#define InvalidCodePath Assert(!"InvalidCodePath")

#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) (Kilobytes(Value)*1024LL)
#define Gigabytes(Value) (Megabytes(Value)*1024LL)
#define Terabytes(Value) (Gigabytes(Value)*1024LL)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))
// TODO(Lyubomir): swap, min, max ... macros???

#define Align16(Value) ((Value + 15) & ~15)


struct game_memory
{
    bool32 IsInitialized;
    int64 PermanentStorageSize;
    void *PermanentStorage;

    float r;
    float g;
    float b;
};

extern "C"
{
    void TestAddressFunction(game_memory* GameMemory);
}
