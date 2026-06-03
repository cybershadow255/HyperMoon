// ================================================================
//  HyperMon - Windows Kernel Monitoring Driver
//  Compiler: MSVC (WDK)   Target: Windows 10/11 x64
//
//  Uses only official, documented Windows Kernel Callback APIs:
//  - PsSetCreateProcessNotifyRoutineEx  (process start/stop)
//  - PsSetCreateThreadNotifyRoutine     (thread create/destroy)
//  - PsSetLoadImageNotifyRoutine        (DLL / exe loads)
//  - CmRegisterCallback                 (registry operations)
//
//  Communication with usermode via IOCTL over a named device.
// ================================================================

#include <ntddk.h>
#include <wdm.h>
#include "shared.h"

// ----------------------------------------------------------------
//  Globals
// ----------------------------------------------------------------

static PDEVICE_OBJECT g_Device     = nullptr;
static LARGE_INTEGER  g_CmCookie   = {};
static BOOLEAN        g_Registered = FALSE;

// Circular event queue
static HM_EVENT   g_Queue[HM_QUEUE_SIZE] = {};
static ULONG      g_Head  = 0;   // next write slot
static ULONG      g_Tail  = 0;   // next read slot
static ULONG      g_Count = 0;   // current item count
static KSPIN_LOCK g_Lock;        // protects Head/Tail/Count

// ----------------------------------------------------------------
//  Queue helpers  (safe at any IRQL <= DISPATCH_LEVEL)
// ----------------------------------------------------------------

static void QueuePush(const HM_EVENT* ev)
{
    KIRQL irql;
    KeAcquireSpinLock(&g_Lock, &irql);

    if (g_Count < HM_QUEUE_SIZE) {
        RtlCopyMemory(&g_Queue[g_Head], ev, sizeof(HM_EVENT));
        g_Head = (g_Head + 1) % HM_QUEUE_SIZE;
        ++g_Count;
    }
    // Queue full -> silently drop (oldest events stay)

    KeReleaseSpinLock(&g_Lock, irql);
}

// Returns number of events actually copied into `buf`.
static ULONG QueuePop(HM_EVENT* buf, ULONG maxCount)
{
    KIRQL irql;
    KeAcquireSpinLock(&g_Lock, &irql);

    ULONG n = min(g_Count, maxCount);
    for (ULONG i = 0; i < n; ++i) {
        RtlCopyMemory(&buf[i], &g_Queue[g_Tail], sizeof(HM_EVENT));
        g_Tail = (g_Tail + 1) % HM_QUEUE_SIZE;
    }
    g_Count -= n;

    KeReleaseSpinLock(&g_Lock, irql);
    return n;
}

static void QueueClear()
{
    KIRQL irql;
    KeAcquireSpinLock(&g_Lock, &irql);
    g_Head = g_Tail = g_Count = 0;
    KeReleaseSpinLock(&g_Lock, irql);
}

// ----------------------------------------------------------------
//  Helper: safe wide-string copy from UNICODE_STRING
// ----------------------------------------------------------------

static void CopyUStr(wchar_t* dst, const UNICODE_STRING* src)
{
    if (!src || src->Length == 0 || !src->Buffer) return;
    SIZE_T bytes = min((SIZE_T)src->Length, (HM_MAX_PATH - 1) * sizeof(wchar_t));
    RtlCopyMemory(dst, src->Buffer, bytes);
    dst[bytes / sizeof(wchar_t)] = L'\0';
}

// ----------------------------------------------------------------
//  Kernel Callbacks
// ----------------------------------------------------------------

// Called on every process creation and termination.
// Runs at PASSIVE_LEVEL inside the creating/exiting thread.
static VOID CB_Process(
    _Inout_     PEPROCESS            Process,
    _In_        HANDLE               ProcessId,
    _In_opt_    PPS_CREATE_NOTIFY_INFO Info)
{
    UNREFERENCED_PARAMETER(Process);

    HM_EVENT ev = {};
    KeQuerySystemTime(reinterpret_cast<PLARGE_INTEGER>(&ev.Timestamp));
    ev.ProcessId = HandleToUlong(ProcessId);

    if (Info) {
        ev.Type           = HM_PROCESS_CREATE;
        ev.ParentProcessId = HandleToUlong(Info->ParentProcessId);
        CopyUStr(ev.ImagePath, Info->ImageFileName);
    } else {
        ev.Type = HM_PROCESS_TERMINATE;
    }

    QueuePush(&ev);
}

// Called on every thread creation and termination.
// Runs at PASSIVE_LEVEL or APC_LEVEL.
static VOID CB_Thread(
    _In_ HANDLE ProcessId,
    _In_ HANDLE ThreadId,
    _In_ BOOLEAN Create)
{
    HM_EVENT ev = {};
    KeQuerySystemTime(reinterpret_cast<PLARGE_INTEGER>(&ev.Timestamp));
    ev.ProcessId = HandleToUlong(ProcessId);
    ev.ThreadId  = HandleToUlong(ThreadId);
    ev.Type      = Create ? HM_THREAD_CREATE : HM_THREAD_TERMINATE;
    QueuePush(&ev);
}

// Called when any executable image (EXE, DLL, driver) is mapped.
// Runs at PASSIVE_LEVEL.
static VOID CB_ImageLoad(
    _In_opt_ PUNICODE_STRING FullImageName,
    _In_     HANDLE          ProcessId,
    _In_     PIMAGE_INFO     ImageInfo)
{
    UNREFERENCED_PARAMETER(ImageInfo);

    HM_EVENT ev = {};
    KeQuerySystemTime(reinterpret_cast<PLARGE_INTEGER>(&ev.Timestamp));
    ev.Type      = HM_IMAGE_LOAD;
    ev.ProcessId = HandleToUlong(ProcessId);
    CopyUStr(ev.ImagePath, FullImageName);
    QueuePush(&ev);
}

// Called before/after registry operations.
// Runs at PASSIVE_LEVEL.
static NTSTATUS CB_Registry(
    _In_opt_ PVOID CallbackContext,
    _In_opt_ PVOID Argument1,
    _In_opt_ PVOID Argument2)
{
    UNREFERENCED_PARAMETER(CallbackContext);

    auto cls = static_cast<REG_NOTIFY_CLASS>(reinterpret_cast<ULONG_PTR>(Argument1));

    HM_EVENT ev = {};
    KeQuerySystemTime(reinterpret_cast<PLARGE_INTEGER>(&ev.Timestamp));
    ev.ProcessId = HandleToUlong(PsGetCurrentProcessId());

    switch (cls) {
    case RegNtPreSetValueKey: {
        auto* i = static_cast<PREG_SET_VALUE_KEY_INFORMATION>(Argument2);
        ev.Type = HM_REG_PRE_SET;
        if (i) CopyUStr(ev.Data, i->ValueName);
        QueuePush(&ev);
        break;
    }
    case RegNtPreCreateKey: {
        auto* i = static_cast<PREG_CREATE_KEY_INFORMATION>(Argument2);
        ev.Type = HM_REG_CREATE;
        if (i) CopyUStr(ev.Data, i->CompleteName);
        QueuePush(&ev);
        break;
    }
    case RegNtPreDeleteKey:
    case RegNtPreDeleteValueKey: {
        ev.Type = HM_REG_DELETE;
        QueuePush(&ev);
        break;
    }
    default:
        break;
    }

    return STATUS_SUCCESS;
}

// ----------------------------------------------------------------
//  IRP Dispatch
// ----------------------------------------------------------------

static NTSTATUS Dispatch_CreateClose(PDEVICE_OBJECT, PIRP Irp)
{
    Irp->IoStatus.Status      = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

static NTSTATUS Dispatch_IoControl(PDEVICE_OBJECT, PIRP Irp)
{
    auto*    stack  = IoGetCurrentIrpStackLocation(Irp);
    ULONG    code   = stack->Parameters.DeviceIoControl.IoControlCode;
    NTSTATUS status = STATUS_SUCCESS;
    ULONG_PTR info  = 0;

    switch (code) {

    // ---- IOCTL_HM_GET_COUNT ----
    case IOCTL_HM_GET_COUNT: {
        if (stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(ULONG)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        KIRQL irql;
        KeAcquireSpinLock(&g_Lock, &irql);
        ULONG cnt = g_Count;
        KeReleaseSpinLock(&g_Lock, irql);
        *static_cast<ULONG*>(Irp->AssociatedIrp.SystemBuffer) = cnt;
        info = sizeof(ULONG);
        break;
    }

    // ---- IOCTL_HM_GET_EVENTS ----
    case IOCTL_HM_GET_EVENTS: {
        ULONG outBytes = stack->Parameters.DeviceIoControl.OutputBufferLength;
        ULONG maxEvts  = outBytes / sizeof(HM_EVENT);
        if (maxEvts == 0) { status = STATUS_BUFFER_TOO_SMALL; break; }

        ULONG got = QueuePop(
            static_cast<HM_EVENT*>(Irp->AssociatedIrp.SystemBuffer),
            maxEvts);
        info = got * sizeof(HM_EVENT);
        break;
    }

    // ---- IOCTL_HM_CLEAR_EVENTS ----
    case IOCTL_HM_CLEAR_EVENTS: {
        QueueClear();
        break;
    }

    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    Irp->IoStatus.Status      = status;
    Irp->IoStatus.Information = info;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

// ----------------------------------------------------------------
//  DriverUnload  – deregister callbacks BEFORE removing device!
// ----------------------------------------------------------------

static VOID DriverUnload(PDRIVER_OBJECT DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);

    KdPrint(("[HyperMon] Unloading...\n"));

    if (g_Registered) {
        // Order matters: unregister, then wait for callbacks to drain.
        PsSetCreateProcessNotifyRoutineEx(CB_Process, TRUE);
        PsRemoveCreateThreadNotifyRoutine(CB_Thread);
        PsRemoveLoadImageNotifyRoutine(CB_ImageLoad);
        CmUnRegisterCallback(g_CmCookie);
    }

    UNICODE_STRING symLink = RTL_CONSTANT_STRING(HYPERMON_SYM_LINK);
    IoDeleteSymbolicLink(&symLink);

    if (g_Device)
        IoDeleteDevice(g_Device);

    KdPrint(("[HyperMon] Unloaded cleanly.\n"));
}

// ----------------------------------------------------------------
//  DriverEntry
// ----------------------------------------------------------------

extern "C"
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);
    KdPrint(("[HyperMon] Loading...\n"));

    KeInitializeSpinLock(&g_Lock);
    DriverObject->DriverUnload = DriverUnload;

    // Dispatch table
    DriverObject->MajorFunction[IRP_MJ_CREATE]         = Dispatch_CreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]          = Dispatch_CreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = Dispatch_IoControl;

    // ----- Create kernel device -----
    UNICODE_STRING devName = RTL_CONSTANT_STRING(HYPERMON_DEVICE_NAME);
    NTSTATUS status = IoCreateDevice(
        DriverObject, 0, &devName,
        FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN,
        FALSE, &g_Device);
    if (!NT_SUCCESS(status)) {
        KdPrint(("[HyperMon] IoCreateDevice failed: 0x%08X\n", status));
        return status;
    }
    g_Device->Flags |= DO_BUFFERED_IO;

    // ----- Symbolic link for usermode access -----
    UNICODE_STRING symLink = RTL_CONSTANT_STRING(HYPERMON_SYM_LINK);
    status = IoCreateSymbolicLink(&symLink, &devName);
    if (!NT_SUCCESS(status)) {
        KdPrint(("[HyperMon] IoCreateSymbolicLink failed: 0x%08X\n", status));
        IoDeleteDevice(g_Device);
        return status;
    }

    // ----- Register process callback -----
    status = PsSetCreateProcessNotifyRoutineEx(CB_Process, FALSE);
    if (!NT_SUCCESS(status)) {
        KdPrint(("[HyperMon] PsSetCreateProcessNotifyRoutineEx failed: 0x%08X\n", status));
        goto Cleanup;
    }

    // ----- Register thread callback -----
    status = PsSetCreateThreadNotifyRoutine(CB_Thread);
    if (!NT_SUCCESS(status)) {
        KdPrint(("[HyperMon] PsSetCreateThreadNotifyRoutine failed: 0x%08X\n", status));
        PsSetCreateProcessNotifyRoutineEx(CB_Process, TRUE);
        goto Cleanup;
    }

    // ----- Register image-load callback -----
    status = PsSetLoadImageNotifyRoutine(CB_ImageLoad);
    if (!NT_SUCCESS(status)) {
        KdPrint(("[HyperMon] PsSetLoadImageNotifyRoutine failed: 0x%08X\n", status));
        PsSetCreateProcessNotifyRoutineEx(CB_Process, TRUE);
        PsRemoveCreateThreadNotifyRoutine(CB_Thread);
        goto Cleanup;
    }

    // ----- Register registry callback -----
    status = CmRegisterCallback(CB_Registry, nullptr, &g_CmCookie);
    if (!NT_SUCCESS(status)) {
        KdPrint(("[HyperMon] CmRegisterCallback failed: 0x%08X\n", status));
        PsSetCreateProcessNotifyRoutineEx(CB_Process, TRUE);
        PsRemoveCreateThreadNotifyRoutine(CB_Thread);
        PsRemoveLoadImageNotifyRoutine(CB_ImageLoad);
        goto Cleanup;
    }

    g_Registered = TRUE;
    g_Device->Flags &= ~DO_DEVICE_INITIALIZING;
    KdPrint(("[HyperMon] Loaded successfully.\n"));
    return STATUS_SUCCESS;

Cleanup:
    IoDeleteSymbolicLink(&symLink);
    IoDeleteDevice(g_Device);
    g_Device = nullptr;
    return status;
}
