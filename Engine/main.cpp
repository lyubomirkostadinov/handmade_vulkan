
#include "render_backend.cpp"
#include "../game/game.h"
#include "render_backend.h"
#include <sys/mman.h>

game_memory TestGameMemory;

//TODO(Lyubomir): Math Library and API Types

void LoadGameCode()
{
        typedef void (*TestFunction)(game_memory&);

        TestFunction TestFunctionPtr = NULL;

        TestFunctionPtr = (TestFunction)MyNSGLGetProcAddress("TestAddressFunction");

        printf("Address of TestFunction: %p\n", (void*)TestFunctionPtr);

        if (!TestFunctionPtr)
        {
            printf("Failed to load function\n");
        }

        if (TestFunctionPtr)
        {
            //NOTE(Lyubomir): Call the function
            (*TestFunctionPtr)(TestGameMemory);
        }
}

int main(int argc, char *argv[])
{
    LoadGameCode();

    //TODO(Lyubomir): Its Time To Learn LLDB ;D

    game_memory GameMemory;
    GameMemory.PermanentStorageSize = Gigabytes(1);
    GameMemory.PermanentStorage = mmap(NULL, GameMemory.PermanentStorageSize,
                                       PROT_READ | PROT_WRITE,
                                       MAP_PRIVATE | MAP_ANONYMOUS,
                                       -1, 0);

    Initialize();

    InitializeRenderBackend(&GameMemory);

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Render Loop
    while (!ShouldClose())
    {
        PollEvents();

        Render();
    }
    ShutdownRenderBackend();

    Shutdown();

    munmap(GameMemory.PermanentStorage, GameMemory.PermanentStorageSize);
}
