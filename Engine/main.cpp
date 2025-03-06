#include "render_backend.cpp"
#include "../game/game.h"
#include "render_backend.h"
#include <sys/mman.h>
#include <dlfcn.h>
#include <unistd.h>
#include <cstdio>

game_memory TestGameMemory;

//TODO(Lyubomir): Math Library and API Types

void SleepSeconds(int seconds)
{
    sleep(seconds);
}

int main(int argc, char *argv[])
{
    //LoadGameCode();

    //TODO(Lyubomir): Its Time To Learn LLDB ;D

    //bin folder -> lldb app
    // run / r
    // set break point -> break set -f main.cpp 42 / b main.cpp : 42
    // set break point when a function gets called -> b Initialize()
    // run / r
    // continue -> c
    // step over -> next / n
    // step into -> step / s
    // list breakpoints -> break list / br list
    // delete a breakpoint number 4 -> br del 4
    // delete all -> br del
    // print var content -> p VarName
    // frame variables -> frame variable / fr v
    // go to current frame -> frame select / fr s
    // backtrace -> bt
    // inspect other frame variables - > frame select 1 / f 1 -> fr v to inspect the variables
    // set watchpoint(break when variable changes or reads from, program must be running to set)
    // global variable watchpoint -> watchpoint set variable globalVariableName / w s v globalVariableName
    // global variable watchpoint -> watchpoint set variable -w read | write | read_write globalVariableName
    // member variable -> w s v object.memberVarName -w read
    // terminating / kill process -> kill
    // exit lldb -> quit / Ctrl + D
    game_memory GameMemory;
    GameMemory.r = 0.0f;
    GameMemory.g = 0.0f;
    GameMemory.b = 0.0f;

    GameMemory.PermanentStorageSize = Gigabytes(1);
    GameMemory.PermanentStorage = mmap(NULL, GameMemory.PermanentStorageSize,
                                       PROT_READ | PROT_WRITE,
                                       MAP_PRIVATE | MAP_ANONYMOUS,
                                       -1, 0);

    Initialize();

    InitializeRenderBackend(&GameMemory);

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Render Loop

    const char* LibraryPath = "./libgame.dylib";
    const char* FunctionName = "TestAddressFunction";

    void* LibHandle = nullptr;
    typedef void (*TestFunction)(game_memory&);
    TestFunction TestFunctionPtr = nullptr;

    while (!ShouldClose())
    {
        if (LibHandle)
        {
            dlclose(LibHandle);
        }

        LibHandle = dlopen(LibraryPath, RTLD_NOW);
        if (!LibHandle)
        {
            fprintf(stderr, "Failed to load library: %s\n", dlerror());
            break;
        }

        // Get the address of the function
        TestFunctionPtr = (TestFunction)dlsym(LibHandle, FunctionName);
        if (!TestFunctionPtr)
        {
            fprintf(stderr, "Failed to load function: %s\n", dlerror());
            break;
        }

        //printf("Address of TestFunction: %p\n", (void*)TestFunctionPtr);

        if (TestFunctionPtr)
        {
            (*TestFunctionPtr)(GameMemory);
        }

        PollEvents();

        Render(&GameMemory);
    }
    if (LibHandle)
    {
        dlclose(LibHandle);
    }


    ShutdownRenderBackend();

    Shutdown();

    munmap(GameMemory.PermanentStorage, GameMemory.PermanentStorageSize);
}
