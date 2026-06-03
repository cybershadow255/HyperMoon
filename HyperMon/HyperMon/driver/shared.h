#pragma once

// ================================================================
//  HyperMon - Shared definitions (driver <-> usermode)
//  Include this in both the kernel driver AND the usermode app.
// ================================================================

#define HYPERMON_DEVICE_NAME  L"\\Device\\HyperMon"
#define HYPERMON_SYM_LINK     L"\\DosDevices\\HyperMon"
#define HYPERMON_USER_PATH    L"\\\\.\\HyperMon"

// IOCTL codes
#define HYPERMON_IOCTL(code) \
    CTL_CODE(FILE_DEVICE_UNKNOWN, (code), METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_HM_GET_EVENTS   HYPERMON_IOCTL(0x800)  // Read pending events
#define IOCTL_HM_CLEAR_EVENTS HYPERMON_IOCTL(0x801)  // Flush event queue
#define IOCTL_HM_GET_COUNT    HYPERMON_IOCTL(0x802)  // How many events queued?

#define HM_MAX_PATH     260
#define HM_QUEUE_SIZE  4096   // Circular buffer capacity

// ----- Event types -----------------------------------------------
typedef enum _HM_EVENT_TYPE : unsigned int {
    HM_PROCESS_CREATE    = 1,
    HM_PROCESS_TERMINATE = 2,
    HM_THREAD_CREATE     = 3,
    HM_THREAD_TERMINATE  = 4,
    HM_IMAGE_LOAD        = 5,
    HM_REG_PRE_SET       = 6,
    HM_REG_CREATE        = 7,
    HM_REG_DELETE        = 8,
} HM_EVENT_TYPE;

// ----- One captured event ----------------------------------------
#pragma pack(push, 1)
typedef struct _HM_EVENT {
    HM_EVENT_TYPE Type;
    unsigned int  ProcessId;
    unsigned int  ParentProcessId;  // valid for PROCESS_CREATE
    unsigned int  ThreadId;         // valid for THREAD events
    long long     Timestamp;        // FILETIME (100-ns intervals since 1601-01-01)
    wchar_t       ImagePath[HM_MAX_PATH];   // exe / DLL path
    wchar_t       Data[HM_MAX_PATH];        // registry key or extra info
} HM_EVENT;
#pragma pack(pop)
