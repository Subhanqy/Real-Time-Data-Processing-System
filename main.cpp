#include <windows.h>
#include "gui.hpp"

// The one-and-only WinMain
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    return runGUI(hInstance, nCmdShow);
}
