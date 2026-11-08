#ifndef XRT_NET_IOCP_TIMER_H
#define XRT_NET_IOCP_TIMER_H
#include "xrt_net_port.h"
#if defined(XRT_FEATURE_NET_PORT_IOCP) && (defined(_WIN32) || defined(_WIN64))
/* Documented ntdll entry points have no SDK import library/header. Keep the
 * exact Windows ABI and resolve them dynamically; older systems retain GQCS.
 * No APC, thread-pool callback, helper thread or process timer-resolution change.
 * https://learn.microsoft.com/windows/win32/devnotes/ntassociatewaitcompletionpacket */
struct _OBJECT_ATTRIBUTES;
typedef HANDLE (WINAPI *__xrt_net_iocp_create_timer)(LPSECURITY_ATTRIBUTES, LPCWSTR, DWORD, DWORD);
typedef LONG (NTAPI *__xrt_net_iocp_create_packet)(PHANDLE, ACCESS_MASK, struct _OBJECT_ATTRIBUTES*);
typedef LONG (NTAPI *__xrt_net_iocp_associate_packet)(HANDLE, HANDLE, HANDLE, PVOID, PVOID, LONG, ULONG_PTR, PBOOLEAN);
typedef LONG (NTAPI *__xrt_net_iocp_cancel_packet)(HANDLE, BOOLEAN);
typedef struct __xrt_net_iocp_timer_api {
    __xrt_net_iocp_create_timer CreateTimer;
    __xrt_net_iocp_create_packet CreatePacket;
    __xrt_net_iocp_associate_packet Associate;
    __xrt_net_iocp_cancel_packet Cancel;
} __xrt_net_iocp_timer_api;
typedef struct __xrt_net_iocp_timer {
    HANDLE Timer, Packet;
    __xrt_net_iocp_associate_packet Associate;
    __xrt_net_iocp_cancel_packet Cancel;
    OVERLAPPED Wake;
    double Deadline;
    bool Enabled, Associated, Scheduled;
} __xrt_net_iocp_timer;
typedef char __xrt_net_iocp_timer_function_size[
    sizeof(__xrt_net_iocp_create_timer) == sizeof(FARPROC) &&
    sizeof(__xrt_net_iocp_create_packet) == sizeof(FARPROC) &&
    sizeof(__xrt_net_iocp_associate_packet) == sizeof(FARPROC) &&
    sizeof(__xrt_net_iocp_cancel_packet) == sizeof(FARPROC) ? 1 : -1];

static bool __xrtNetIOCPTimerAPI(__xrt_net_iocp_timer_api* pAPI)
{
    HMODULE hKernel = GetModuleHandleW(L"kernel32.dll"), hNative = GetModuleHandleW(L"ntdll.dll");
    FARPROC Proc;
    memset(pAPI, 0, sizeof(*pAPI)); if (!hKernel || !hNative) return false;
    Proc = GetProcAddress(hKernel, "CreateWaitableTimerExW"); memcpy(&pAPI->CreateTimer, &Proc, sizeof(Proc));
    Proc = GetProcAddress(hNative, "NtCreateWaitCompletionPacket"); memcpy(&pAPI->CreatePacket, &Proc, sizeof(Proc));
    Proc = GetProcAddress(hNative, "NtAssociateWaitCompletionPacket"); memcpy(&pAPI->Associate, &Proc, sizeof(Proc));
    Proc = GetProcAddress(hNative, "NtCancelWaitCompletionPacket"); memcpy(&pAPI->Cancel, &Proc, sizeof(Proc));
    return pAPI->CreateTimer && pAPI->CreatePacket && pAPI->Associate && pAPI->Cancel;
}
/* Private injectable constructor also lets tests exercise optional API/handle
 * failures without new public hooks or a modified platform runtime. */
static bool __xrtNetIOCPTimerCreate(__xrt_net_iocp_timer* pTimer, const __xrt_net_iocp_timer_api* pAPI)
{
    memset(pTimer, 0, sizeof(*pTimer));
    if (!pAPI->CreateTimer || !pAPI->CreatePacket || !pAPI->Associate || !pAPI->Cancel) return false;
    /* CREATE_WAITABLE_TIMER_HIGH_RESOLUTION is supported since Windows 10 1803.
     * Use the documented value even with SDKs which do not declare its name. */
    pTimer->Timer = pAPI->CreateTimer(NULL, NULL, 0x00000002u, TIMER_MODIFY_STATE | SYNCHRONIZE);
    if (!pTimer->Timer) return false;
    if (pAPI->CreatePacket(&pTimer->Packet, MAXIMUM_ALLOWED, NULL) < 0 || !pTimer->Packet) {
        CloseHandle(pTimer->Timer); pTimer->Timer = NULL; return false;
    }
    pTimer->Associate = pAPI->Associate; pTimer->Cancel = pAPI->Cancel; pTimer->Enabled = true; return true;
}
static void __xrtNetIOCPTimerInit(__xrt_net_iocp_timer* pTimer)
{
    __xrt_net_iocp_timer_api API;
    memset(pTimer, 0, sizeof(*pTimer));
    if (__xrtNetIOCPTimerAPI(&API)) (void)__xrtNetIOCPTimerCreate(pTimer, &API);
}
/* STATUS_PENDING explicitly forbids immediate packet reuse. Keep its fixed
 * sentinel alive; a subsequent wait/cancel can retire it without spinning. */
static bool __xrtNetIOCPTimerCancel(__xrt_net_iocp_timer* pTimer)
{
    if (pTimer->Scheduled) { CancelWaitableTimer(pTimer->Timer); pTimer->Scheduled = false; }
    if (pTimer->Associated) {
        LONG Status = pTimer->Cancel(pTimer->Packet, TRUE);
        if (Status == 0 || (ULONG)Status == 0xc0000120u) pTimer->Associated = false; /* SUCCESS / CANCELLED */
        else if (Status != 0x103) pTimer->Enabled = false; /* PENDING, or optional path failure. */
    }
    return !pTimer->Associated;
}
/* Port owner only. Retain an already scheduled EARLIER wake across I/O returns.
 * This avoids arm/cancel syscalls per hot event. Private timer packets always
 * mean an internal wake; the outer port loop checks its ORIGINAL deadline.
 * Thus a former wait's earlier timer never produces an early public timeout. */
static bool __xrtNetIOCPTimerEnsure(__xrt_net_iocp_timer* pTimer, HANDLE hPort, int64 iTimeout)
{
    if (!pTimer->Enabled || iTimeout <= 0) return false;
    double Deadline = xrtTimer() + (double)iTimeout * 0.001;
    if (pTimer->Associated && pTimer->Scheduled && pTimer->Deadline <= Deadline) return true;
    if (!__xrtNetIOCPTimerCancel(pTimer)) return false;
    LARGE_INTEGER Due;
    Due.QuadPart = -(iTimeout > INT64_MAX / 10000 ? INT64_MAX : iTimeout * 10000);
    /* Setting first resets any former signal. An expiry before association is
     * still queued by the native API's already-signaled-object contract. */
    if (!SetWaitableTimer(pTimer->Timer, &Due, 0, NULL, NULL, FALSE)) { pTimer->Enabled = false; return false; }
    LONG Status = pTimer->Associate(pTimer->Packet, hPort, pTimer->Timer, NULL, &pTimer->Wake, 0, 0, NULL);
    if (Status < 0) {
        CancelWaitableTimer(pTimer->Timer); pTimer->Enabled = false; return false;
    }
    pTimer->Deadline = Deadline; pTimer->Associated = pTimer->Scheduled = true; return true;
}
static void __xrtNetIOCPTimerConsumed(__xrt_net_iocp_timer* pTimer)
{ pTimer->Associated = pTimer->Scheduled = false; }
static void __xrtNetIOCPTimerUnit(__xrt_net_iocp_timer* pTimer)
{
    (void)__xrtNetIOCPTimerCancel(pTimer);
    if (pTimer->Packet) CloseHandle(pTimer->Packet);
    if (pTimer->Timer) CloseHandle(pTimer->Timer);
    pTimer->Packet = pTimer->Timer = NULL;
    pTimer->Enabled = pTimer->Associated = pTimer->Scheduled = false;
}
#endif
#endif
