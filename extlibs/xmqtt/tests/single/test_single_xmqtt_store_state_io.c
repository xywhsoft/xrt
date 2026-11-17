#define XMQTT_MODULE_XMQTT_STORE_STATE
#define XRT_MODULE_VFS
#define XRT_MODULE_MEMORY_STATS
#include <xmqtt/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
static bool ReadArmed;
static unsigned Reads,FailRead;
static bool siReadFull(xfile File,uint64 At,ptr Data,size_t Bytes,size_t* Done)
{
    if(ReadArmed&&++Reads==FailRead){xrtSetErrorInfo(XERR_IO,"store.read.fault",1,"injected public read boundary failure");return false;}
    return xrtReadAtFull(File,At,Data,Bytes,Done);
}
#define xrtReadAtFull siReadFull
#define XMQTT_IMPLEMENTATION
#include "../../../../single/extlibs/xmqtt.h"
#undef xrtReadAtFull
#include "../support/store_state_peer.h"
static jpdrive Drive;
static unsigned siFailure(unsigned Target,unsigned SuffixFailure)
{
    memset(&Drive,0,sizeof(Drive));jpfile F=jpFile(&Drive);stsource Source;stSource(XMQTT_VERSION_5,&Source);xmqttjournal* J=stWrite(F,XMQTT_VERSION_5,Source.Ops,16,true);
    if(SuffixFailure){testRequire(xmqttJournalDestroy(J),"suffix fixture opener released");memset(Drive.Bytes+Drive.Size,0xee,20);Drive.Size+=20;xmqttjournalconfig JC=stJournalConfig(XMQTT_VERSION_5);J=jpReopen(F,&JC);Drive.Calls=0;Drive.Fail=SuffixFailure;}
    xmqttstorestateconfig C=stConfig(XMQTT_VERSION_5);xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(C.MaxMemoryBytes,&Budget)&&xrtAllocationBudgetEnter(Budget,&Scope),"IO candidate ownership ledger");
    Reads=0;FailRead=Target;ReadArmed=true;xmqttstorestate* S=(xmqttstorestate*)(uintptr_t)1;xmqttstorestatus Status=xmqttStoreStateRecover(J,&C,&S);ReadArmed=false;unsigned Observed=Reads;
    if(Target||SuffixFailure){stStatus(Status,XMQTT_STORE_IO,"read or final suffix retirement failure is typed IO");testRequire(S==(xmqttstorestate*)(uintptr_t)1&&(SuffixFailure||Observed==Target),"IO never publishes earlier valid recovered state or retries a failed read");}
    else {stStatus(Status,XMQTT_STORE_DONE,"full read control");xmqttStoreStateDestroy(S);}
    xrtClearError();testRequire(xrtAllocationBudgetLeave(&Scope),"IO ledger leave");xallocationbudgetinfo I={.Size=sizeof(I)};testRequire(xrtAllocationBudgetInfo(Budget,&I)&&!I.LiveBytes&&!I.LiveCount&&!I.Rejections,"all earlier accepted payload/error owners refunded on IO failure");xrtAllocationBudgetFree(Budget);testRequire(xmqttJournalDestroy(J),"IO journal released");
    if(SuffixFailure){Drive.Fail=0;xmqttjournalconfig JC=stJournalConfig(XMQTT_VERSION_5);J=jpReopen(F,&JC);S=NULL;stStatus(xmqttStoreStateRecover(J,&C,&S),XMQTT_STORE_DONE,"fresh recovery resolves earlier suffix IO failure");testRequire(stInfo(S).Inbox==3&&Drive.Size==jpInfo(J).CommittedEnd,"complete prefix only after physical suffix retirement");xmqttStoreStateDestroy(S);testRequire(xmqttJournalDestroy(J),"retried suffix reader released");}
    jpFileClose(F);return Observed;
}
int main(void)
{unsigned Total=siFailure(0,0);testRequire(Total==48,"three actual public read boundaries per committed transaction");for(unsigned I=1;I<=Total;++I)siFailure(I,0);siFailure(0,1);siFailure(0,2);printf("[PASS] %u public-read boundary failures and2 final suffix resize/sync failures; no early recovered-state publication\n",Total);return 0;}
