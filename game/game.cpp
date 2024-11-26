#include "game.h"
#include <cstdio>

extern "C"
{
    void TestAddressFunction(game_memory* GameMemory)
    {
        printf("CALLING TestAddressFunction\n");
        GameMemory->r = 1.f;
        GameMemory->g = 0.f;
        GameMemory->b = 0.f;
    }
}
