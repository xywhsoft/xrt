#include "../test.h"
#include "../../src/internal/xrt_net_iocp_timer.h"
#if defined(_WIN32) || defined(_WIN64)
static __xrt_net_iocp_timer_api testTimerAPI;
static size_t testTimerAssociations, testTimerCancels;
static bool testTimerPending, testTimerCancelError;
static LONG NTAPI testTimerAssociate(HANDLE Packet,HANDLE Port,HANDLE Timer,PVOID Key,PVOID Context,LONG Status,ULONG_PTR Bytes,PBOOLEAN Signaled)
{
    ++testTimerAssociations;
    return testTimerAPI.Associate(Packet,Port,Timer,Key,Context,Status,Bytes,Signaled);
}
static LONG NTAPI testTimerCancel(HANDLE Packet,BOOLEAN Remove)
{
    ++testTimerCancels; LONG Status=testTimerAPI.Cancel(Packet,Remove);
    if(testTimerPending){testTimerPending=false;return 0x103;}
    return testTimerCancelError?(LONG)0xc0000001u:Status;
}
static HANDLE WINAPI testTimerCreateFail(LPSECURITY_ATTRIBUTES Attributes,LPCWSTR Name,DWORD Flags,DWORD Access)
{(void)Attributes;(void)Name;(void)Flags;(void)Access;SetLastError(ERROR_NOT_ENOUGH_MEMORY);return NULL;}
static LONG NTAPI testTimerPacketFail(PHANDLE Packet,ACCESS_MASK Access,struct _OBJECT_ATTRIBUTES* Attributes)
{(void)Packet;(void)Access;(void)Attributes;return (LONG)0xc0000017u;}
static LONG NTAPI testTimerAssociateFail(HANDLE Packet,HANDLE Port,HANDLE Timer,PVOID Key,PVOID Context,LONG Status,ULONG_PTR Bytes,PBOOLEAN Signaled)
{(void)Packet;(void)Port;(void)Timer;(void)Key;(void)Context;(void)Status;(void)Bytes;(void)Signaled;return (LONG)0xc0000017u;}
static DWORD testTimerHandles(void)
{DWORD Count=0;testRequire(GetProcessHandleCount(GetCurrentProcess(),&Count)!=0,"query timer handles");return Count;}
static void testTimerPacket(__xrt_net_iocp_timer* Timer,HANDLE Port)
{
    DWORD Bytes=1;ULONG_PTR Key=1;OVERLAPPED* Overlapped=NULL;
    testRequire(GetQueuedCompletionStatus(Port,&Bytes,&Key,&Overlapped,5000)&&Overlapped==&Timer->Wake&&Bytes==0,"native timer packet identity");
    __xrtNetIOCPTimerConsumed(Timer);testRequire(!Timer->Associated&&!Timer->Scheduled,"consumed packet remains associated");
}
static void testTimerHelpers(void)
{
    __xrt_net_iocp_timer Timer;__xrtNetIOCPTimerInit(&Timer);
    if(!Timer.Enabled){puts("[SKIP] optional native high resolution timer API unavailable");__xrtNetIOCPTimerUnit(&Timer);return;}
    __xrtNetIOCPTimerUnit(&Timer);testRequire(__xrtNetIOCPTimerAPI(&testTimerAPI),"native timer API load");
    HANDLE Port=CreateIoCompletionPort(INVALID_HANDLE_VALUE,NULL,0,0);testRequire(Port!=NULL,"timer helper IOCP");
    DWORD Baseline=testTimerHandles();__xrt_net_iocp_timer_api API=testTimerAPI;
    API.CreateTimer=NULL;testRequire(!__xrtNetIOCPTimerCreate(&Timer,&API),"missing optional entry point");__xrtNetIOCPTimerUnit(&Timer);
    API=testTimerAPI;API.CreateTimer=testTimerCreateFail;testRequire(!__xrtNetIOCPTimerCreate(&Timer,&API),"native timer creation failure");__xrtNetIOCPTimerUnit(&Timer);
    API=testTimerAPI;API.CreatePacket=testTimerPacketFail;testRequire(!__xrtNetIOCPTimerCreate(&Timer,&API),"native packet creation failure");__xrtNetIOCPTimerUnit(&Timer);
    testRequire(testTimerHandles()==Baseline,"partial timer constructor leaked handles");
    API=testTimerAPI;API.Associate=testTimerAssociateFail;testRequire(__xrtNetIOCPTimerCreate(&Timer,&API),"association failure fixture");
    testRequire(!__xrtNetIOCPTimerEnsure(&Timer,Port,1)&&!Timer.Enabled,"association failure did not select fallback");__xrtNetIOCPTimerUnit(&Timer);
    testRequire(__xrtNetIOCPTimerCreate(&Timer,&testTimerAPI),"native set failure fixture");
    DWORD Flags=1;testRequire(GetHandleInformation(Timer.Timer,&Flags)&&!(Flags&HANDLE_FLAG_INHERIT)&&
        GetHandleInformation(Timer.Packet,&Flags)&&!(Flags&HANDLE_FLAG_INHERIT),"timer handles inherited");
    testRequire(CloseHandle(Timer.Timer)!=0,"invalidate private timer");Timer.Timer=NULL;
    testRequire(!__xrtNetIOCPTimerEnsure(&Timer,Port,1)&&!Timer.Enabled,"native set failure did not select fallback");
    __xrtNetIOCPTimerUnit(&Timer);__xrtNetIOCPTimerUnit(&Timer);
    API=testTimerAPI;API.Associate=testTimerAssociate;API.Cancel=testTimerCancel;
    testRequire(__xrtNetIOCPTimerCreate(&Timer,&API),"counted timer fixture");testTimerAssociations=testTimerCancels=0;
    testRequire(__xrtNetIOCPTimerEnsure(&Timer,Port,1000),"initial timer association");double Due=Timer.Deadline;
    for(unsigned Index=0;Index<1000;++Index)testRequire(__xrtNetIOCPTimerEnsure(&Timer,Port,2000),"reuse earlier wake");
    testRequire(testTimerAssociations==1&&!testTimerCancels&&Timer.Deadline==Due,"hot reuse rearmed kernel timer");
    testTimerPending=true;testRequire(!__xrtNetIOCPTimerEnsure(&Timer,Port,1)&&Timer.Associated&&!Timer.Scheduled&&testTimerAssociations==1,"PENDING packet immediately reused");
    testRequire(__xrtNetIOCPTimerEnsure(&Timer,Port,1)&&testTimerAssociations==2,"retired PENDING packet not reusable");testTimerPacket(&Timer,Port);
    testRequire(__xrtNetIOCPTimerEnsure(&Timer,Port,1000),"cancel error fixture");testTimerCancelError=true;
    testRequire(!__xrtNetIOCPTimerEnsure(&Timer,Port,1)&&!Timer.Enabled,"cancel error did not select fallback");testTimerCancelError=false;__xrtNetIOCPTimerUnit(&Timer);
    /* Associate an already signaled auto-reset timer; the packet must still
     * arrive exactly once without a user callback or an extra thread. */
    testRequire(__xrtNetIOCPTimerCreate(&Timer,&testTimerAPI),"signaled timer fixture");LARGE_INTEGER DueTime;DueTime.QuadPart=-1;
    testRequire(SetWaitableTimer(Timer.Timer,&DueTime,0,NULL,NULL,FALSE)!=0,"expire before association");xrtSleep(20);
    testRequire(testTimerAPI.Associate(Timer.Packet,Port,Timer.Timer,NULL,&Timer.Wake,0,0,NULL)==0,"already signaled association");
    Timer.Associated=Timer.Scheduled=true;testTimerPacket(&Timer,Port);__xrtNetIOCPTimerUnit(&Timer);
    for(unsigned Index=0;Index<2000;++Index){testRequire(__xrtNetIOCPTimerCreate(&Timer,&testTimerAPI),"timer lifetime loop");
        testRequire(__xrtNetIOCPTimerEnsure(&Timer,Port,1),"timer lifetime association");__xrtNetIOCPTimerUnit(&Timer);}
    testRequire(testTimerHandles()==Baseline,"native timer lifetime handle leak");testRequire(CloseHandle(Port)!=0,"timer helper port close");
}
typedef struct testtimerpost {xnetport* Port;} testtimerpost;
static DWORD WINAPI testTimerPost(LPVOID Data)
{testtimerpost* State=Data;xrtSleep(2);testRequire(xrtNetPortPost(State->Port,42,NULL),"post during timer wait");return 0;}
static void testTimerPublic(void)
{
    DWORD Baseline=testTimerHandles();xnetportconfig Config;xrtNetPortConfigInit(&Config);Config.Backend=XNET_PORT_IOCP;
    xnetport* Port=xrtNetPortCreate(&Config);testRequire(Port!=NULL,"timer public port");
    testtimerpost State={Port};HANDLE Thread=CreateThread(NULL,0,testTimerPost,&State,0,NULL);testRequire(Thread!=NULL,"timer posting thread");
    xnetportevent Event;size_t Count=0;
    testRequire(xrtNetPortWait(Port,&Event,1,1000,&Count)==XNET_RESULT_OK&&Count==1&&Event.Id==42,"timer swallowed real event");
    DWORD ExitCode=1;testRequire(WaitForSingleObject(Thread,INFINITE)==WAIT_OBJECT_0&&GetExitCodeThread(Thread,&ExitCode)&&!ExitCode,"timer post join");CloseHandle(Thread);
    double Began=xrtTimer();
    testRequire(xrtNetPortWait(Port,&Event,1,1500,&Count)==XNET_RESULT_TIMEOUT&&!Count&&xrtTimer()-Began>=1.5,"old timer caused early public timeout");
    for(unsigned Index=0;Index<100;++Index){Began=xrtTimer();testRequire(xrtNetPortWait(Port,&Event,1,1,&Count)==XNET_RESULT_TIMEOUT&&!Count&&xrtTimer()-Began>=.001,"timer deadline or sentinel leaked");}
    testRequire(xrtNetPortDestroy(Port),"timer public port destruction");testRequire(testTimerHandles()==Baseline,"public timer port handle leak");
}
#endif
int main(void)
{
#if defined(_WIN32) || defined(_WIN64)
    testTimerHelpers();testTimerPublic();puts("[PASS] IOCP high resolution waits: packet reuse, PENDING cancellation, failures, stale wakes and native handle retirement");
#else
    puts("[SKIP] IOCP timer tests require Windows");
#endif
    return 0;
}
