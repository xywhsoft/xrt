#if !defined(_WIN32) && !defined(_WIN64) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include <stdbool.h>
static bool LearnErrorKey, HaveErrorKey;
static _Thread_local unsigned NativeCountdown, NativeFaults, NativeWrites;
static _Thread_local bool PersistentFault, KeepFailing;
#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <windows.h>
static DWORD ErrorKey;
static BOOL WINAPI ssNativeSet(DWORD Key, PVOID Value)
{
    if (LearnErrorKey) { ErrorKey=Key; HaveErrorKey=true; LearnErrorKey=false; }
    if (HaveErrorKey && Key==ErrorKey) {
        ++NativeWrites;
        if (KeepFailing || (NativeCountdown && !--NativeCountdown)) {
            ++NativeFaults; KeepFailing=PersistentFault; return FALSE;
        }
    }
    return FlsSetValue(Key,Value);
}
#define FlsSetValue ssNativeSet
#else
#include <pthread.h>
#include <errno.h>
static pthread_key_t ErrorKey;
static int ssNativeSet(pthread_key_t Key, const void* Value)
{
    if (LearnErrorKey) { ErrorKey=Key; HaveErrorKey=true; LearnErrorKey=false; }
    if (HaveErrorKey && Key==ErrorKey) {
        ++NativeWrites;
        if (KeepFailing || (NativeCountdown && !--NativeCountdown)) {
            ++NativeFaults; KeepFailing=PersistentFault; return ENOMEM;
        }
    }
    return pthread_setspecific(Key,Value);
}
#define pthread_setspecific ssNativeSet
#endif
#define XMQTT_MODULE_XMQTT_STORE_SESSION
#define XRT_MODULE_VFS
#define XRT_MODULE_MEMORY_STATS
#include <xmqtt/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#if defined(_WIN32) || defined(_WIN64)
#undef FlsSetValue
#else
#undef pthread_setspecific
#endif
#define XMQTT_IMPLEMENTATION
#include "../../../../single/extlibs/xmqtt.h"
#include "../support/store_session_peer.h"
static unsigned Notices;
static void seNotice(const xerror* E,ptr P){(void)E;(void)P;++Notices;}
static void seDisarm(void){NativeCountdown=0;PersistentFault=false;KeepFailing=false;}
static void seAttempt(xmqttversion V,unsigned Target,bool Ambient,bool Persistent,bool Assigned)
{
    xmqttstorestate* State=ssRecover(V,true,60);xmqttsessionconfig K=ssSessionConfig(V);if(Assigned)K.ClientId=(xstrview){0};xmqttstoresessionconfig C=ssConfig(10,4000);
    xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1048576,&Budget)&&xrtAllocationBudgetEnter(Budget,&Scope),"native error failure ledger");
    if(Ambient)xrtSetErrorInfo(XERR_AGAIN,"import.ambient",1,"caller owner");
    const xerror* Old=xrtGetError();Notices=0;xrtSetErrorHandler(seNotice,NULL);unsigned Start=NativeWrites,Faults=NativeFaults;NativeCountdown=Target;PersistentFault=Persistent;xmqttsession* S=(xmqttsession*)(uintptr_t)1;
    xmqttsessionstatus R=Assigned?xmqttStoreSessionRestoreAssigned(State,&K,&C,&S):xmqttStoreSessionRestore(State,&K,&C,&S);unsigned Writes=NativeWrites-Start;
    if(Target)testRequire(R==XMQTT_SESSION_ERROR&&S==(xmqttsession*)(uintptr_t)1&&Writes==Target&&NativeFaults==Faults+1&&!NativeCountdown,"checked entry/final setter failure cannot publish partial Session or retry native setter");
    else {ssStatus(R,XMQTT_SESSION_DONE,"native key control");testRequire(Writes==2&&xrtGetError()==Old,"two quiet ownership transfers preserve exact ambient owner");testRequire(xmqttSessionDestroy(S),"control native-key candidate retired");}
    testRequire(!Notices,"no repeated ambient/synthetic error notices");seDisarm();xrtSetErrorHandler(NULL,NULL);xrtClearError();testRequire(xrtAllocationBudgetLeave(&Scope),"native error ledger leave");xallocationbudgetinfo I={.Size=sizeof(I)};testRequire(xrtAllocationBudgetInfo(Budget,&I)&&!I.LiveBytes&&!I.LiveCount&&!I.Rejections,"saved errors/Session/scratch/packets refunded physically");xrtAllocationBudgetFree(Budget);testRequire(stInfo(State).Outgoing==3&&stInfo(State).Inbox==2,"source immutable after native setter failure");xmqttStoreStateDestroy(State);
}
static void seBound(bool Ambient)
{
    xmqttstorestate* State=ssRecover(XMQTT_VERSION_5,true,60);xmqttsessionconfig K=ssSessionConfig(XMQTT_VERSION_5);xmqttstoresessionconfig C=ssConfig(10,4000);xrt_error_context Context={0};xrt_error_context* Prior=__xrtErrorContextSwap(&Context);if(Ambient)xrtSetErrorInfo(XERR_AGAIN,"import.ambient",1,"bound owner");const xerror* Old=xrtGetError();unsigned Start=NativeWrites;NativeCountdown=1;PersistentFault=true;xmqttsession* S=NULL;ssStatus(xmqttStoreSessionRestore(State,&K,&C,&S),XMQTT_SESSION_DONE,"bound context import");testRequire(NativeWrites==Start&&NativeCountdown==1&&xrtGetError()==Old,"bound Core execution context needs no native error setter");seDisarm();xrtClearError();testRequire(__xrtErrorContextSwap(Prior)==&Context&&!Context.Error&&!xrtGetError(),"outer context restored");testRequire(xmqttSessionDestroy(S),"bound imported Session retired");xmqttStoreStateDestroy(State);
}
int main(void)
{
    xerror* Old=NULL;LearnErrorKey=true;testRequire(xrtTrySwapErrorTake(NULL,&Old)&&HaveErrorKey&&!Old,"learn actual native error key only");unsigned Cases=0;
    for(unsigned V=0;V<3;++V){xmqttversion Level=V?XMQTT_VERSION_5:XMQTT_VERSION_311;bool Assigned=V==2;seAttempt(Level,0,true,false,Assigned);for(unsigned Target=1;Target<=2;++Target)for(unsigned A=0;A<2;++A)for(unsigned P=0;P<2;++P){seAttempt(Level,Target,A!=0,P!=0,Assigned);++Cases;}}
    seBound(false);seBound(true);printf("[PASS] %u actual native entry/final error-slot failures and2 bound contexts\n",Cases);return 0;
}
