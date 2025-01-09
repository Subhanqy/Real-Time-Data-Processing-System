#ifndef MEMINFO_HPP
#define MEMINFO_HPP

#include <windows.h>

// Writes total MB to totalMB, free MB to freeMB
void getMemoryUsage(DWORDLONG& totalMB, DWORDLONG& freeMB);

#endif // MEMINFO_HPP
