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
#define XMQTT_MODULE_XMQTT_STORE_STATE
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
#include "../support/store_state_peer.h"
static jpdrive Drive;
static unsigned Notices;
static void ssNotice(const xerror* E,ptr P){(void)E;(void)P;++Notices;}
static void ssDisarm(void){NativeCountdown=0;PersistentFault=false;KeepFailing=false;}
static unsigned ssAttempt(xmqttversion V,unsigned Target,bool Ambient,bool Persistent)
{
    memset(&Drive,0,sizeof(Drive));jpfile F=jpFile(&Drive);stsource Source;stSource(V,&Source);xmqttjournal* J=stWrite(F,V,Source.Ops,16,true);xmqttstorestateconfig C=stConfig(V);
    xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(C.MaxMemoryBytes,&Budget)&&xrtAllocationBudgetEnter(Budget,&Scope),"error failure ownership ledger");
    if(Ambient)xrtSetErrorInfo(XERR_AGAIN,"store.ambient",1,"caller owner");
    const xerror* Old=xrtGetError();Notices=0;xrtSetErrorHandler(ssNotice,NULL);unsigned Start=NativeWrites,Faults=NativeFaults;NativeCountdown=Target;PersistentFault=Persistent;
    xmqttstorestate* S=(xmqttstorestate*)(uintptr_t)1;xmqttstorestatus Result=xmqttStoreStateRecover(J,&C,&S);unsigned Writes=NativeWrites-Start;
    if(Target){testRequire(Result==XMQTT_STORE_ERROR&&S==(xmqttstorestate*)(uintptr_t)1&&Writes==Target&&NativeFaults==Faults+1&&!NativeCountdown,"every actual error-key write can fail once/persistently without publication or another setter attempt");}
    else {stStatus(Result,XMQTT_STORE_DONE,"native shim control");testRequire(xrtGetError()==Old&&Writes>30,"quiet full replay control");xmqttStoreStateDestroy(S);}
    testRequire(!Notices,"no repeated ambient or synthetic errors through failed slot");ssDisarm();xrtSetErrorHandler(NULL,NULL);xrtClearError();testRequire(xrtAllocationBudgetLeave(&Scope),"error ledger leave");xallocationbudgetinfo I={.Size=sizeof(I)};testRequire(xrtAllocationBudgetInfo(Budget,&I)&&!I.LiveBytes&&!I.LiveCount&&!I.Rejections,"all saved errors and partial replay payloads physically refunded");xrtAllocationBudgetFree(Budget);testRequire(xmqttJournalDestroy(J),"native-key journal released");jpFileClose(F);return Writes;
}
static void ssBound(bool Ambient)
{
    memset(&Drive,0,sizeof(Drive));jpfile F=jpFile(&Drive);stsource Source;stSource(XMQTT_VERSION_5,&Source);xmqttjournal* J=stWrite(F,XMQTT_VERSION_5,Source.Ops,16,true);xmqttstorestateconfig C=stConfig(XMQTT_VERSION_5);
    xrt_error_context Context={0};xrt_error_context* Prior=__xrtErrorContextSwap(&Context);if(Ambient)xrtSetErrorInfo(XERR_AGAIN,"store.ambient",1,"bound owner");const xerror* Old=xrtGetError();unsigned Writes=NativeWrites,Faults=NativeFaults;
    NativeCountdown=1;PersistentFault=true;xmqttstorestate* S=NULL;stStatus(xmqttStoreStateRecover(J,&C,&S),XMQTT_STORE_DONE,"bound context recovery");testRequire(NativeWrites==Writes&&NativeFaults==Faults&&NativeCountdown==1&&xrtGetError()==Old,"existing execution-context storage needs no native error-key setter");
    ssDisarm();xrtClearError();testRequire(__xrtErrorContextSwap(Prior)==&Context&&!Context.Error&&!xrtGetError(),"outer context restored");xmqttStoreStateDestroy(S);testRequire(xmqttJournalDestroy(J),"bound journal released");jpFileClose(F);
}
int main(void)
{
    xerror* Old=NULL;LearnErrorKey=true;testRequire(xrtTrySwapErrorTake(NULL,&Old)&&HaveErrorKey&&!Old,"learn only actual native error key");unsigned Cases=0;
    for(unsigned V=0;V<2;++V){xmqttversion Version=V?XMQTT_VERSION_5:XMQTT_VERSION_311;unsigned Writes=ssAttempt(Version,0,true,false);
        for(unsigned Point=1;Point<=Writes;++Point)for(unsigned A=0;A<2;++A)for(unsigned P=0;P<2;++P){ssAttempt(Version,Point,A!=0,P!=0);++Cases;}}
    ssBound(false);ssBound(true);printf("[PASS] %u actual whole-recovery native error-key failures and2 bound contexts\n",Cases);return 0;
}
