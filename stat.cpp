#include "stat.hpp"
#include <windows.h>

// Static variables for CPU usage calc
static ULONGLONG g_prevIdle   = 0;
static ULONGLONG g_prevKernel = 0;
static ULONGLONG g_prevUser   = 0;

double getCPUUsage()
{
    FILETIME idleTime, kernelTime, userTime;
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime))
        return 0.0;

    // Convert FILETIME to 64-bit
    ULONGLONG idle   = ((ULONGLONG)idleTime.dwHighDateTime   << 32) | idleTime.dwLowDateTime;
    ULONGLONG kernel = ((ULONGLONG)kernelTime.dwHighDateTime << 32) | kernelTime.dwLowDateTime;
    ULONGLONG user   = ((ULONGLONG)userTime.dwHighDateTime   << 32) | userTime.dwLowDateTime;

    // Calculate deltas
    ULONGLONG idleDelta   = idle   - g_prevIdle;
    ULONGLONG kernelDelta = kernel - g_prevKernel;
    ULONGLONG userDelta   = user   - g_prevUser;

    // Store for next call
    g_prevIdle   = idle;
    g_prevKernel = kernel;
    g_prevUser   = user;

    // kernel includes idle on Windows
    ULONGLONG total = kernelDelta + userDelta;
    ULONGLONG busy  = (kernelDelta - idleDelta) + userDelta;

    double usage = 0.0;
    if (total > 0)
        usage = (busy * 100.0) / total;

    return usage;
}

