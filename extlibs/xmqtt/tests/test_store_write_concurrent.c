#include "support/store_write_peer.h"
#include <xrt/thread.h>
enum {SWC_COUNT=64};
typedef struct swccontext {xmqttstorewrite* Writer;xmqttversion Version;xatomic32 Start,Next,Turn;} swccontext;
static int32 swcProduce(ptr Data)
{
    swccontext* X=Data;jwAwait(&X->Start);
    for(unsigned I=0;I<SWC_COUNT/4;++I){uint32 Key=xrtAtomic32FetchAdd(&X->Next,1,XMEMORY_ACQ_REL)+1;double End=xrtTimer()+15;
        while(xrtAtomic32Load(&X->Turn,XMEMORY_ACQUIRE)!=Key){testRequire(xrtTimer()<End,"ordered key allocation admission turn");xrtSleep(1);}
        unsigned char Wire[128];size_t N=0;uint64 Payload=Key;xmqttpacket P;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);P.Body.Publish.Topic=XRT_STR_LITERAL("parallel");P.Body.Publish.Payload=(xbytesview){(const unsigned char*)&Payload,sizeof(Payload)};
        testRequire(X->Version==XMQTT_VERSION_5?xmqttPacketWrite5(Wire,sizeof(Wire),&P,XMQTT_SERVER_TO_CLIENT,NULL,&N):xmqttPacketWrite311(Wire,sizeof(Wire),&P,XMQTT_SERVER_TO_CLIENT,NULL,&N),"producer canonical message");
        xmqttstoredeltaconfig DC=sdConfig(X->Version);xmqttstoredelta O={.Type=XMQTT_STORE_IN_ACCEPT,.Epoch=1,.Key=Key,.Data={Wire,N}};xfuture* Future=swOps(X->Writer,&DC,&O,1);xrtAtomic32Store(&X->Turn,Key+1,XMEMORY_RELEASE);memset(Wire,0xcc,sizeof(Wire));
        const xmqttcommitresult* R=jwOutcome(Future);testRequire(R->Status==XMQTT_COMMIT_OK&&R->Sequence==Key+1&&R->Effect==XMQTT_JOURNAL_COMMITTED,"every ordered producer admitted once and physically committed");xrtFutureDestroy(Future);
    }return 0;
}
static int32 swcRead(ptr Data)
{
    swccontext* X=Data;jwAwait(&X->Start);xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget)&&xrtAllocationBudgetEnter(Budget,&Scope),"read/Info actual one-byte allocation budget");
    for(uint64 Key=1;Key<=SWC_COUNT;++Key){unsigned char Buffer[128];xmqttstoreview View;size_t N=0;xmqttstorestatus Status;double End=xrtTimer()+15;
        do{Status=xmqttStoreWriteRead(X->Writer,Key,Buffer,sizeof(Buffer),&View,&N);testRequire((Status==XMQTT_STORE_DONE||Status==XMQTT_STORE_EMPTY)&&xrtTimer()<End,"bounded committed read while native writer advances");if(Status==XMQTT_STORE_EMPTY)xrtSleep(1);}while(Status==XMQTT_STORE_EMPTY);
        xmqttproperty Properties[4];xmqttcodecbuffers B={Properties,4,NULL,0};xmqttpacket P;xmqttcodecissue Issue;xmqttcodecstatus S=X->Version==XMQTT_VERSION_5?xmqttPacketRead5((xbytesview){Buffer,N},XMQTT_SERVER_TO_CLIENT,NULL,&B,&P,&Issue):xmqttPacketRead311((xbytesview){Buffer,N},XMQTT_SERVER_TO_CLIENT,NULL,&B,&P,&Issue);uint64 Payload=0;if(S==XMQTT_CODEC_DONE&&P.Body.Publish.Payload.Size==sizeof(Payload))memcpy(&Payload,P.Body.Publish.Payload.Data,sizeof(Payload));
        testRequire(S==XMQTT_CODEC_DONE&&Payload==Key&&View.Key==Key&&View.InboxPending&&View.Wire.Data==Buffer&&swInfo(X->Writer).State.Inbox>=Key,"read copies one complete immutable committed payload with no torn metadata");
    }
    testRequire(xrtAllocationBudgetLeave(&Scope),"reader budget leaves");xallocationbudgetinfo I={.Size=sizeof(I)};testRequire(xrtAllocationBudgetInfo(Budget,&I)&&!I.Rejections&&!I.LiveBytes&&!I.LiveCount,"concurrent getters and native reapers allocate nothing on reader");xrtAllocationBudgetFree(Budget);return 0;
}
static void swcCase(xmqttversion Version)
{
    jwdrive D={0};jpfile F=jwFile(&D);xmqttjournalconfig JC=stJournalConfig(Version);xmqttjournal* J=jpCreate(F,&JC);xmqttstorestateconfig SC=stConfig(Version);SC.WallMilliseconds=0;SC.MaxInbox=SWC_COUNT;xmqttstorestate* State=NULL;stStatus(xmqttStoreStateRecover(J,&SC,&State),XMQTT_STORE_DONE,"parallel empty source");xtaskpool* Pool=jwPool(4);xmqttstorewriteconfig C;xmqttStoreWriteConfigInit(&C);xmqttstorewrite* W=NULL;testRequire(xmqttStoreWriteCreate(Pool,J,State,&C,&W),"parallel semantic owner");sdsource Source;sdSource(Version,&Source);xfuture* Future=swOps(W,&SC.Delta,Source.Ops,2);testRequire(jwOutcome(Future)->Status==XMQTT_COMMIT_OK,"actual initial BIND/OPEN");xrtFutureDestroy(Future);
    swccontext X={.Writer=W,.Version=Version};xrtAtomic32Init(&X.Turn,1);xthread* Threads[6];for(unsigned I=0;I<6;++I){Threads[I]=xrtThreadCreate(I<4?swcProduce:swcRead,&X,0);testRequire(Threads[I]!=NULL,"native producer/reader thread");}xrtAtomic32Store(&X.Start,1,XMEMORY_RELEASE);
    for(unsigned I=0;I<6;++I){testRequire(xrtThreadWaitFor(Threads[I],20000)==XWAIT_OK&&!xrtThreadExitCode(Threads[I]),"physical producer/reader join");xrtThreadDestroy(Threads[I]);}
    testRequire(xmqttStoreWriteClose(W)&&xmqttStoreWriteWaitFor(W,15000)==XWAIT_OK,"all serialized native data duties retired");xmqttstorewriteinfo Info=swInfo(W);testRequire(Info.State.Inbox==SWC_COUNT&&Info.State.LastKey==SWC_COUNT&&Info.State.Transactions==SWC_COUNT+1&&!Info.Worker.Objects&&!Info.Worker.Requests&&!Info.Worker.Tasks&&xrtAtomic32Load(&D.MaxActive,XMEMORY_ACQUIRE)==1,"exact model and full temporary/retained/native refunds");testRequire(xmqttStoreWriteDestroy(W),"parallel owner destroyed");jwPoolDone(Pool);
    J=jpReopen(F,&JC);State=NULL;SC.WallMilliseconds=100000;stStatus(xmqttStoreStateRecover(J,&SC,&State),XMQTT_STORE_DONE,"fresh parallel full journal recovery");testRequire(stInfo(State).Inbox==SWC_COUNT&&stInfo(State).LastKey==SWC_COUNT&&stInfo(State).Transactions==SWC_COUNT+1,"fresh immutable model contains every producer delivery once");xmqttStoreStateDestroy(State);testRequire(xmqttJournalDestroy(J),"reader journal retired");jpFileClose(F);
}
int main(void){swcCase(XMQTT_VERSION_5);swcCase(XMQTT_VERSION_311);puts("[PASS] four producers,two allocation-free readers,four host workers,128 exact typed commits and fresh full replay");return 0;}
