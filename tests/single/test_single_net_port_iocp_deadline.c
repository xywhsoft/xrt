#define XRT_MODULE_NET_PORT_IOCP
#include <xrt.h>
#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <windows.h>
#include <string.h>
static unsigned testEarlyTimeouts, testFallbackSelected;
static FARPROC WINAPI testTimerGetProc(HMODULE Module,LPCSTR Name)
{
    if(!strcmp(Name,"NtCreateWaitCompletionPacket")){++testFallbackSelected;return NULL;}
    return GetProcAddress(Module,Name);
}
static BOOL WINAPI testTimerQueued(HANDLE Port,LPDWORD Bytes,PULONG_PTR Key,LPOVERLAPPED* Overlapped,DWORD Timeout)
{
    if(Timeout&&Timeout!=INFINITE&&testEarlyTimeouts<2){++testEarlyTimeouts;*Bytes=0;*Key=0;*Overlapped=NULL;SetLastError(WAIT_TIMEOUT);return FALSE;}
    return GetQueuedCompletionStatus(Port,Bytes,Key,Overlapped,Timeout);
}
/* Isolated native fault injection: no production hook or SDK modification. */
#define GetProcAddress testTimerGetProc
#define GetQueuedCompletionStatus testTimerQueued
#endif
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#undef GetProcAddress
#undef GetQueuedCompletionStatus
#include "../test.h"
int main(void)
{
#if defined(_WIN32) || defined(_WIN64)
    xnetportconfig Config;xrtNetPortConfigInit(&Config);Config.Backend=XNET_PORT_IOCP;
    xnetport* Port=xrtNetPortCreate(&Config);testRequire(Port!=NULL,"fallback deadline port");
    xnetportevent Event;size_t Count=7;double Began=xrtTimer();
    testRequire(xrtNetPortWait(Port,&Event,1,100,&Count)==XNET_RESULT_TIMEOUT&&!Count&&xrtTimer()-Began>=.1,
        "coarse native timeout bypassed original monotonic deadline");
    testRequire(testEarlyTimeouts==2&&testFallbackSelected,"fallback/early native timeout injection not exercised");
    testRequire(xrtNetPortDestroy(Port),"fallback deadline port close");
    puts("[PASS] IOCP fallback waits preserve original deadline after early native timeout");
#else
    puts("[SKIP] IOCP fallback injection requires Windows");
#endif
    return 0;
}
