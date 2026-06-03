// ================================================================
//  HyperMon - Usermode Console Monitor
//  Compile with: cl monitor.cpp /Fe:monitor.exe /W4
//  (No WDK needed - plain Win32 / MSVC)
// ================================================================

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdio>
#include <string>
#include <atomic>
#include <algorithm>

// shared.h lives in ../driver/ - adjust path if needed
#include "../driver/shared.h"

// ----------------------------------------------------------------
//  ANSI colors (Windows 10+ supports VT sequences)
// ----------------------------------------------------------------
#define CLR_RESET    "\033[0m"
#define CLR_BOLD     "\033[1m"
#define CLR_DIM      "\033[2m"
#define CLR_RED      "\033[91m"
#define CLR_GREEN    "\033[92m"
#define CLR_YELLOW   "\033[93m"
#define CLR_BLUE     "\033[94m"
#define CLR_MAGENTA  "\033[95m"
#define CLR_CYAN     "\033[96m"
#define CLR_WHITE    "\033[97m"
#define CLR_GRAY     "\033[90m"

static std::atomic<bool> g_Running(true);

// ----------------------------------------------------------------
//  Helpers
// ----------------------------------------------------------------

static void EnableAnsiColors()
{
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD  m = 0;
    if (GetConsoleMode(h, &m))
        SetConsoleMode(h, m | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

// Convert FILETIME quad to "HH:MM:SS.mmm" local time string
static void FormatTimestamp(long long ts, char* buf, size_t bufLen)
{
    FILETIME   ft;
    SYSTEMTIME st;
    ft.dwLowDateTime  = static_cast<DWORD>(ts & 0xFFFFFFFF);
    ft.dwHighDateTime = static_cast<DWORD>(ts >> 32);
    FileTimeToLocalFileTime(&ft, &ft);
    FileTimeToSystemTime(&ft, &st);
    _snprintf_s(buf, bufLen, bufLen - 1,
        "%02d:%02d:%02d.%03d",
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}

// Truncate a wide path to the last N chars for display
static std::wstring ShortPath(const wchar_t* path, int maxLen = 55)
{
    std::wstring s(path);
    if (static_cast<int>(s.size()) > maxLen)
        s = L"..." + s.substr(s.size() - maxLen + 3);
    return s;
}

// ----------------------------------------------------------------
//  Event pretty-print
// ----------------------------------------------------------------

struct EventFmt { const char* tag; const char* color; };

static EventFmt GetFmt(HM_EVENT_TYPE t)
{
    switch (t) {
    case HM_PROCESS_CREATE:    return { "PROC +  ", CLR_GREEN   };
    case HM_PROCESS_TERMINATE: return { "PROC -  ", CLR_RED     };
    case HM_THREAD_CREATE:     return { "THR  +  ", CLR_CYAN    };
    case HM_THREAD_TERMINATE:  return { "THR  -  ", CLR_BLUE    };
    case HM_IMAGE_LOAD:        return { "DLL/EXE ", CLR_YELLOW  };
    case HM_REG_PRE_SET:       return { "REG SET ", CLR_MAGENTA };
    case HM_REG_CREATE:        return { "REG NEW ", CLR_MAGENTA };
    case HM_REG_DELETE:        return { "REG DEL ", CLR_RED     };
    default:                   return { "???     ", CLR_WHITE   };
    }
}

static void PrintEvent(const HM_EVENT& ev, unsigned long long seq)
{
    char ts[32] = {};
    FormatTimestamp(ev.Timestamp, ts, sizeof(ts));

    auto fmt = GetFmt(ev.Type);

    // Sequence number (dim)
    printf(CLR_GRAY "#%-6llu " CLR_RESET, seq);

    // Timestamp
    printf(CLR_DIM "%s " CLR_RESET, ts);

    // Event type tag (colored)
    printf("%s" CLR_BOLD "%-8s" CLR_RESET " ", fmt.color, fmt.tag);

    // PID
    printf(CLR_WHITE "PID" CLR_GRAY ":%5u" CLR_RESET "  ", ev.ProcessId);

    // Optional PPID
    if (ev.ParentProcessId)
        printf(CLR_DIM "PPID:%5u" CLR_RESET "  ", ev.ParentProcessId);
    else if (ev.ThreadId)
        printf(CLR_DIM "TID :%5u" CLR_RESET "  ", ev.ThreadId);
    else
        printf("             ");

    // Path or data
    const wchar_t* info = ev.ImagePath[0] ? ev.ImagePath : ev.Data;
    if (info[0]) {
        auto s = ShortPath(info);
        printf(CLR_DIM "%ls" CLR_RESET, s.c_str());
    }

    printf("\n");
}

// ----------------------------------------------------------------
//  Ctrl-C handler
// ----------------------------------------------------------------

static BOOL WINAPI CtrlHandler(DWORD)
{
    g_Running = false;
    return TRUE;
}

// ----------------------------------------------------------------
//  Main
// ----------------------------------------------------------------

int main()
{
    EnableAnsiColors();
    SetConsoleTitleA("HyperMon - Kernel Monitor");

    // Banner
    printf(CLR_CYAN CLR_BOLD
        "\n"
        "  +------------------------------------------+\n"
        "  |   HyperMon  -  Windows Kernel Monitor    |\n"
        "  |   Ring 0 callbacks  |  Real-time view    |\n"
        "  +------------------------------------------+\n"
        CLR_RESET "\n");

    // Open the kernel device
    HANDLE hDev = CreateFileW(
        HYPERMON_USER_PATH,
        GENERIC_READ | GENERIC_WRITE,
        0, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hDev == INVALID_HANDLE_VALUE) {
        printf(CLR_RED
            "[-] Cannot open HyperMon device (error %lu).\n"
            "    Make sure the driver is loaded:\n"
            "       powershell -ExecutionPolicy Bypass -File setup.ps1\n"
            CLR_RESET, GetLastError());
        printf("\nPress Enter to exit...");
        getchar();
        return 1;
    }

    printf(CLR_GREEN "[+] Connected to driver.\n" CLR_RESET);
    printf(CLR_GRAY "    Press Ctrl+C to stop.\n\n" CLR_RESET);

    // Column header
    printf(CLR_BOLD CLR_GRAY
        "%-7s %-12s %-8s %-8s %-13s  %s\n",
        "#", "TIME", "EVENT", "PID", "PPID/TID", "PATH / DATA");
    printf("%s\n" CLR_RESET, std::string(100, '-').c_str());

    SetConsoleCtrlHandler(CtrlHandler, TRUE);

    // Polling loop
    constexpr ULONG BATCH = 64;
    HM_EVENT buf[BATCH];
    unsigned long long seq = 0;

    while (g_Running) {
        DWORD bytesRet = 0;
        BOOL  ok = DeviceIoControl(
            hDev,
            IOCTL_HM_GET_EVENTS,
            nullptr, 0,
            buf, sizeof(buf),
            &bytesRet, nullptr);

        if (ok && bytesRet >= sizeof(HM_EVENT)) {
            ULONG n = bytesRet / sizeof(HM_EVENT);
            for (ULONG i = 0; i < n; ++i)
                PrintEvent(buf[i], ++seq);
        }

        Sleep(50);  // ~20 polls/sec — low CPU, low latency
    }

    printf("\n" CLR_CYAN "[*] Stopped. %llu events captured.\n" CLR_RESET, seq);
    CloseHandle(hDev);
    return 0;
}
