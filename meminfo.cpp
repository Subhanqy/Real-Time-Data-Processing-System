#include "meminfo.hpp"
#include <windows.h>

void getMemoryUsage(DWORDLONG& totalMB, DWORDLONG& freeMB)
{
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);

    if (GlobalMemoryStatusEx(&memStatus))
    {
        DWORDLONG totalPhys = memStatus.ullTotalPhys;
        DWORDLONG availPhys = memStatus.ullAvailPhys;

        totalMB = totalPhys / (1024ULL * 1024ULL);
        freeMB  = availPhys / (1024ULL * 1024ULL);
    }
    else
    {
        totalMB = 0;
        freeMB  = 0;
    }
}
