#include "processinfo.hpp"
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>   // for GetProcessMemoryInfo
#include <string>
#include <vector>
#include <chrono>
#include <cstdlib>  // For std::mbstowcs
#include <string>

std::wstring ConvertToWString(const char* ansiStr)
{
    if (ansiStr == nullptr)
        return L"";

    // Determine the length required for the wide string
    size_t len = std::mbstowcs(nullptr, ansiStr, 0);
    if (len == static_cast<size_t>(-1))
        return L"";

    // Allocate a buffer for the wide string
    std::wstring wideStr(len, L'\0');

    // Perform the conversion
    std::mbstowcs(&wideStr[0], ansiStr, len);

    return wideStr;
}



std::vector<ProcessData> getProcessList()
{
    std::vector<ProcessData> result;

    // Take a snapshot of all processes
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
        return result;

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(pe32);

    if (!Process32First(hSnapshot, &pe32))
    {
        CloseHandle(hSnapshot);
        return result;
    }

    do
    {
        // We'll fill a ProcessData
        ProcessData pd;
        pd.processId = pe32.th32ProcessID;
        pd.name = ConvertToWString(pe32.szExeFile);


        // Try to open process for query
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                      FALSE,
                                      pd.processId);
        if (hProcess)
        {
            // 1) Memory usage
            PROCESS_MEMORY_COUNTERS pmc;
            if (GetProcessMemoryInfo(hProcess, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)))
            {
                DWORDLONG workingSet = pmc.WorkingSetSize; // in bytes
                pd.memoryMB = workingSet / (1024ULL * 1024ULL);
            }
            else
            {
                pd.memoryMB = 0;
            }

            // 2) Uptime: get creation time from GetProcessTimes
            FILETIME ftCreate, ftExit, ftKernel, ftUser;
            if (GetProcessTimes(hProcess, &ftCreate, &ftExit, &ftKernel, &ftUser))
            {
                // Convert ftCreate (FILETIME) to ULONGLONG of 100-nanosecs since 1601
                ULONGLONG create64 = ((ULONGLONG)ftCreate.dwHighDateTime << 32)
                                   | (ULONGLONG)ftCreate.dwLowDateTime;

                // Current system time in same format
                FILETIME ftNow;
                GetSystemTimeAsFileTime(&ftNow);
                ULONGLONG now64 = ((ULONGLONG)ftNow.dwHighDateTime << 32)
                                | (ULONGLONG)ftNow.dwLowDateTime;

                // difference in 100-nanosecs
                ULONGLONG diff = (now64 - create64);

                // convert to seconds
                // 1 second = 10,000,000 (100-nanosec units)
                pd.uptimeSec = diff / 10000000ULL;
            }
            else
            {
                pd.uptimeSec = 0;
            }

            CloseHandle(hProcess);
        }
        else
        {
            // Could not open process
            pd.memoryMB  = 0;
            pd.uptimeSec = 0;
        }

        // Add to result
        result.push_back(pd);

    } while (Process32Next(hSnapshot, &pe32));

    CloseHandle(hSnapshot);
    return result;
}
