#include <xmqtt/store_inbox.h>
#include <xrt/memory.h>
#include <stdio.h>

int main(void)
{
    str Path=NULL;
    xfile File=xrtFileTemp(NULL,"xmqtt-delivery-",NULL,&Path);
    if(!File)return 1;

    xmqttjournalconfig JC;
    xmqttJournalConfigInit(&JC);
    JC.Binding=XRT_BYTES_LITERAL("demo/broker/slot");
    JC.Generation=1;
    JC.PayloadSchema=XMQTT_STORE_DELTA_SCHEMA;
    xmqttstorestateconfig SC;
    xmqttStoreStateConfigInit(&SC);
    SC.Binding=JC.Binding;
    SC.Generation=JC.Generation;
    SC.ClientId=XRT_STR_LITERAL("inbox-demo");

    xmqttjournal* Journal=NULL;
    xmqttstorestate* State=NULL;
    xmqttstorewrite* Writer=NULL;
    xfuture* Future=NULL;
    xtaskpool* Pool=NULL;
    bool OK=xmqttJournalCreate(File,&JC,&Journal)==XMQTT_JOURNAL_DONE;
    if(OK)OK=xmqttStoreStateRecover(Journal,&SC,&State)==XMQTT_STORE_DONE;
    if(OK){xtaskpoolconfig PC={.Threads=1,.QueueLimit=8};Pool=xrtTaskPoolCreate(&PC);OK=Pool!=NULL;}
    if(OK){
        xmqttstorewriteconfig WC;
        xmqttStoreWriteConfigInit(&WC);
        OK=xmqttStoreInboxCreate(Pool,Journal,State,&WC,&Writer);
        if(OK){Journal=NULL;State=NULL;}
    }

    /* Stand in for an already authenticated Client and an accepted packet. */
    const unsigned char Wire[]={0x30,5,0,1,'t',0,'x'};
    xmqttstoredelta Ops[]={
        {.Type=XMQTT_STORE_BIND,.Epoch=1,.WallMilliseconds=1,.Data=XRT_BYTES_LITERAL("inbox-demo")},
        {.Type=XMQTT_STORE_OPEN,.Epoch=1,.WallMilliseconds=2},
        {.Type=XMQTT_STORE_IN_ACCEPT,.Epoch=1,.Key=1,.Data={Wire,sizeof(Wire)}},
        {.Type=XMQTT_STORE_END,.Epoch=1,.WallMilliseconds=3}
    };
    unsigned char Buffer[512];
    size_t Bytes=0;
    if(OK)OK=xmqttStoreDeltaWrite(&SC.Delta,Ops,4,NULL,0,Buffer,sizeof(Buffer),&Bytes)==XMQTT_STORE_DELTA_DONE&&
             xmqttStoreWriteAppendAsync(Writer,(xbytesview){Buffer,Bytes},10000,&Future)==XMQTT_JWORK_ACCEPTED&&
             xrtFutureWaitFor(Future,10000)==XWAIT_OK&&xrtFutureState(Future)==XFUTURE_RESOLVED&&
             ((const xmqttcommitresult*)xrtFutureValue(Future))->Effect==XMQTT_JOURNAL_COMMITTED;
    xrtFutureDestroy(Future);
    Future=NULL;

    xmqttstorelease Lease;
    xmqttstoreview Delivery;
    if(OK)OK=xmqttStoreInboxTake(Writer,Buffer,sizeof(Buffer),&Lease,&Delivery,&Bytes)==XMQTT_STORE_DONE;
    if(OK){
        printf("Processing delivery %llu (%zu bytes)\n",(unsigned long long)Delivery.Key,Bytes);
        OK=xmqttStoreInboxAckAsync(Writer,Lease,10000,&Future)==XMQTT_JWORK_ACCEPTED&&
           xrtFutureWaitFor(Future,10000)==XWAIT_OK&&xrtFutureState(Future)==XFUTURE_RESOLVED&&
           ((const xmqttcommitresult*)xrtFutureValue(Future))->Effect==XMQTT_JOURNAL_COMMITTED;
    }

    if(Writer&&!xmqttStoreWriteDestroy(Writer))return 1;
    if(Pool&&(!xrtTaskPoolClose(Pool)||xrtTaskPoolWaitFor(Pool,10000)!=XWAIT_OK||!xrtTaskPoolDestroy(Pool)))return 1;
    if(Journal){if(!xmqttJournalDestroy(Journal))OK=false;Journal=NULL;}
    xmqttStoreStateDestroy(State);
    State=NULL;
    if(OK){
        xmqttstorestateinfo Info;
        SC.WallMilliseconds=3;
        OK=xmqttJournalOpen(File,&JC,&Journal)==XMQTT_JOURNAL_DONE&&
           xmqttStoreStateRecover(Journal,&SC,&State)==XMQTT_STORE_DONE&&
           xmqttStoreStateInfo(State,&Info)&&Info.Inbox==0;
        if(OK)puts("Reopened journal has no pending deliveries.");
    }
    xmqttStoreStateDestroy(State);
    if(Journal&&!xmqttJournalDestroy(Journal))OK=false;
    xrtFutureDestroy(Future);
    if(!xrtClose(File)||!xrtFileDelete(Path))OK=false;
    xrtFree(Path);
    return OK?0:1;
}
