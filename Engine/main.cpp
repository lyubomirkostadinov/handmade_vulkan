
#include "render_backend.cpp"
#include "../game/game.h"
#include "render_backend.h"

game_memory GameMemory;

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
            (*TestFunctionPtr)(GameMemory);
        }
}

int main(int argc, char *argv[])
{
    LoadGameCode();

    //TODO(Lyubomir): Its Time To Learn LLDB ;D

    Initialize();

    InitializeRenderBackend();

    //////////////////////////////////////////////////////////////////////////////////////////
    //NOTE(Lyubomir): Render Loop
    while (!ShouldClose())
    {
        PollEvents();

        Render();
    }
    ShutdownRenderBackend();

    Shutdown();
}
