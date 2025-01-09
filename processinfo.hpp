#ifndef PROCESSINFO_HPP
#define PROCESSINFO_HPP

#include <string>
#include <vector>
#include <windows.h>

// Holds info about a single process
struct ProcessData
{
    std::wstring name;    // e.g. "services.exe"
    DWORD        processId;
    DWORDLONG    memoryMB;  // working set in MB
    ULONGLONG    uptimeSec; // how long the process has run
};

// Enumerates processes, returns a list
std::vector<ProcessData> getProcessList();

#endif // PROCESSINFO_HPP
