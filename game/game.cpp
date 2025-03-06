#include "game.h"
#include <cstdio>

extern "C"
{
    void TestAddressFunction(game_memory* GameMemory)
    {
        GameMemory->r = 0.f;
        GameMemory->g = 1.f;
        GameMemory->b = 0.f;
    }
}

//////////////////////////////////////////////
int main()
{


}
