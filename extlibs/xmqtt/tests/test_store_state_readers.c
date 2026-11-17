#include "support/store_state_peer.h"
#include <xrt/thread.h>
static jpdrive Drive;
static int32 srRead(ptr Raw)
{
    const xmqttstorestate* S=Raw;xrtSetErrorInfo(XERR_AGAIN,"store.reader",1,"independent reader diagnostic");const xerror* Old=xrtGetError();
    xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget)&&xrtAllocationBudgetEnter(Budget,&Scope),"reader one-byte allocation cap");
    for(unsigned I=0;I<1000;++I){xmqttstorestateinfo Info={0};testRequire(xmqttStoreStateInfo(S,&Info)&&Info.Inbox==3&&Info.Epoch==2,"concurrent immutable info");
        for(size_t K=0;K<3;++K){xmqttstoreview V={0};stStatus(xmqttStoreStateView(S,XMQTT_STORE_INBOX,K,&V),XMQTT_STORE_DONE,"concurrent ordered payload");testRequire(V.Key==21+K&&V.Wire.Data[0]>>4==3,"exact shared immutable wire");}
        testRequire(xrtGetError()==Old,"reader diagnostics remain independent");}
    testRequire(xrtAllocationBudgetLeave(&Scope),"reader budget leave");xallocationbudgetinfo I={.Size=sizeof(I)};testRequire(xrtAllocationBudgetInfo(Budget,&I)&&!I.PeakBytes&&!I.Rejections,"getter path attempts no allocation");xrtAllocationBudgetFree(Budget);xrtClearError();return 0;
}
int main(void)
{
    memset(&Drive,0,sizeof(Drive));jpfile F=jpFile(&Drive);stsource Source;stSource(XMQTT_VERSION_5,&Source);xmqttjournal* J=stWrite(F,XMQTT_VERSION_5,Source.Ops,16,true);xmqttstorestateconfig C=stConfig(XMQTT_VERSION_5);xmqttstorestate* S=NULL;
    stStatus(xmqttStoreStateRecover(J,&C,&S),XMQTT_STORE_DONE,"immutable view published once");testRequire(xmqttJournalDestroy(J),"reader journal released");jpFileClose(F);memset(&Source,0xcc,sizeof(Source));memset(&Drive,0xcc,sizeof(Drive));
    xthread* Threads[8];for(unsigned I=0;I<8;++I){Threads[I]=xrtThreadCreate(srRead,S,0);testRequire(Threads[I]!=NULL,"physical concurrent reader");}
    for(unsigned I=0;I<8;++I){testRequire(xrtThreadWaitFor(Threads[I],30000)==XWAIT_OK&&!xrtThreadExitCode(Threads[I]),"readers physically joined before unique-owner destruction");xrtThreadDestroy(Threads[I]);}
    xmqttStoreStateDestroy(S);puts("[PASS]8 concurrent readers,8000 info+24000 payload views after Journal/file release,zero-heap getters and physical joins");return 0;
}
