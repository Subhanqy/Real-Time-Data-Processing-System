#include "gui.hpp"
#include <tchar.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <algorithm>

#include "stat.hpp"        // getCPUUsage()
#include "meminfo.hpp"     // getMemoryUsage(...)
#include "uptime.hpp"      // getUptimeSeconds()
#include "processinfo.hpp" // getProcessList()

// Forward declarations
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// We'll store system data in a struct
struct SystemData
{
    double    cpuUsage;      // CPU usage in %
    DWORDLONG totalMemMB;    // Total physical memory in MB
    DWORDLONG freeMemMB;     // Free physical memory in MB
    ULONGLONG uptimeSec;     // System uptime (seconds)
};

// A global or static instance for system-wide stats
static SystemData g_systemData = { 0.0, 0, 0, 0 };

// We'll store the process list as well
static std::vector<ProcessData> g_processList;

// Global HFONT for bigger text
static HFONT g_hFont = nullptr;

// Timer constants
static const UINT_PTR TIMER_ID = 1;
static const UINT TIMER_INTERVAL_MS = 2000;  // Refresh every 2s

// Helpers
void updateSystemData(SystemData& data);
void createBigFont();
void destroyBigFont();

//----------------------------------------------
// The function that WinMain calls
//----------------------------------------------
int runGUI(HINSTANCE hInstance, int nCmdShow)
{
    const TCHAR CLASS_NAME[] = _T("RealTimeDataMonitorClass");

    WNDCLASSEX wc = { 0 };
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);

    if (!RegisterClassEx(&wc))
    {
        MessageBox(NULL, _T("RegisterClassEx failed!"), _T("Error"), MB_ICONERROR);
        return 1;
    }

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        _T("Real-Time Data Monitor (Win32 GUI)"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,  // width=800, height=600 for more room
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd)
    {
        MessageBox(NULL, _T("CreateWindowEx failed!"), _T("Error"), MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Main message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

//----------------------------------------------
// Window Procedure
//----------------------------------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        {
            createBigFont();

            // Refresh data every 2 seconds
            SetTimer(hwnd, TIMER_ID, TIMER_INTERVAL_MS, NULL);

            // Update once immediately
            updateSystemData(g_systemData);
            g_processList = getProcessList(); // from processinfo.cpp
        }
        return 0;

    case WM_TIMER:
        if (wParam == TIMER_ID)
        {
            // Refresh system data
            updateSystemData(g_systemData);

            // Refresh process list
            g_processList = getProcessList();

            // Force a repaint
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // Create offscreen DC & bitmap for double-buffer
            int width  = ps.rcPaint.right - ps.rcPaint.left;
            int height = ps.rcPaint.bottom - ps.rcPaint.top;
            HDC memDC  = CreateCompatibleDC(hdc);
            HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
            HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

            // Fill background with light gray
            HBRUSH bgBrush = CreateSolidBrush(RGB(240, 240, 240));
            RECT memRect{0,0,width,height};
            FillRect(memDC, &memRect, bgBrush);
            DeleteObject(bgBrush);

            // Select our custom font
            HFONT oldFont = (HFONT)SelectObject(memDC, g_hFont);
            SetBkMode(memDC, TRANSPARENT);
            SetTextColor(memDC, RGB(0,0,0));

            //------------------------------------------
            // 1) Display system-wide stats at top
            //------------------------------------------
            std::wstringstream wss;
            wss << std::fixed << std::setprecision(1);

            // CPU usage
            wss << L"CPU Usage: " << g_systemData.cpuUsage << L"%";
            std::wstring strCPU = wss.str();

            wss.str(L"");
            wss.clear();

            // Memory
            wss << L"Memory: " << g_systemData.freeMemMB << L" MB free / "
                << g_systemData.totalMemMB << L" MB total";
            std::wstring strMem = wss.str();

            wss.str(L"");
            wss.clear();

            // Uptime
            ULONGLONG sec = g_systemData.uptimeSec;
            int hours = (int)(sec / 3600ULL);
            int mins  = (int)((sec % 3600ULL) / 60ULL);
            int s     = (int)((sec % 3600ULL) % 60ULL);

            wss << L"Uptime: " << hours << L"h " << mins << L"m " << s << L"s";
            std::wstring strUptime = wss.str();

            // Draw top lines
            int x = 20;
            int y = 20;
            TextOutW(memDC, x, y, strCPU.c_str(),    (int)strCPU.size());    y += 30;
            TextOutW(memDC, x, y, strMem.c_str(),    (int)strMem.size());    y += 30;
            TextOutW(memDC, x, y, strUptime.c_str(), (int)strUptime.size()); y += 40;

            //------------------------------------------
            // 2) Display processes / services
            //    Sort by memory usage descending, for example
            //------------------------------------------
            // (We already have them in g_processList)
            // Let's sort by memory usage (descending)
            std::sort(g_processList.begin(), g_processList.end(),
                      [](const ProcessData& a, const ProcessData& b){
                          return b.memoryMB < a.memoryMB; // so bigger first
                      });

            // We'll only display the top ~10 for neatness
            int displayCount = std::min<int>((int)g_processList.size(), 10);

            // Print a header row
            // Declare 'header' once
            std::wstring header;

        // Set the header row
        header = std::wstring(L"PROCESS NAME            ") +  // 24 characters
                 L"| RAM (MB)   "                           // 12 characters
                 L"| UPTIME      ";                         // 12 characters
        TextOutW(memDC, x, y, header.c_str(), (int)header.size());
        y += 30;

        for (int i = 0; i < displayCount; i++) {
            const ProcessData& pd = g_processList[i];

            // Build a formatted line for each process
            wss.str(L"");
            wss.clear();

            wss << std::left << std::setw(24) << pd.name.substr(0, 23); // Truncate name to 23 chars
                //<< L" | "
                //<< std::right << std::setw(10) << pd.memoryMB         // Right-align memory value
               // << L" | ";

            // Convert uptime to h,m,s
            ULONGLONG procSec = pd.uptimeSec;
            int pHours = (int)(procSec / 3600ULL);
            int pMins  = (int)((procSec % 3600ULL) / 60ULL);
            int pSecs  = (int)((procSec % 3600ULL) % 60ULL);
          //  wss << std::right << std::setw(3) << pHours << L"h "
          //      << std::setw(2) << pMins << L"m "
          //      << std::setw(2) << pSecs << L"s";                   // Format uptime neatly

            std::wstring line = wss.str();
            TextOutW(memDC, x, y, line.c_str(), (int)line.size());
            y += 28;  // Move to the next line
        }

            //------------------------------------------

            // Blit offscreen to main DC
            BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

            // Cleanup
            SelectObject(memDC, oldFont);
            SelectObject(memDC, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDC);

            EndPaint(hwnd, &ps);
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_ID);
        destroyBigFont();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

//----------------------------------------------
// Update CPU, Mem, Uptime
//----------------------------------------------
void updateSystemData(SystemData& data)
{
    // CPU usage
    data.cpuUsage = getCPUUsage();

    // Memory usage
    DWORDLONG totalMB = 0, freeMB = 0;
    getMemoryUsage(totalMB, freeMB);
    data.totalMemMB = totalMB;
    data.freeMemMB  = freeMB;

    // Uptime
    data.uptimeSec = getUptimeSeconds();
}

//----------------------------------------------
// Create a bigger HFONT for text
//----------------------------------------------
void createBigFont()
{
    if (!g_hFont)
    {
        LOGFONT lf = { 0 };
        lf.lfHeight = -20;       // ~20 px
        lf.lfWeight = FW_SEMIBOLD;
        _tcscpy_s(lf.lfFaceName, LF_FACESIZE, _T("Segoe UI"));

        g_hFont = CreateFontIndirect(&lf);
    }
}

//----------------------------------------------
// Destroy the big font if it exists
//----------------------------------------------
void destroyBigFont()
{
    if (g_hFont)
    {
        DeleteObject(g_hFont);
        g_hFont = nullptr;
    }
}
