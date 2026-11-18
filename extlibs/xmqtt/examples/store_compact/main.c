#include <xmqtt/store_compact.h>
#include <stdio.h>
#include <string.h>

static bool seed(xmqttjournalslot* Slot,const xmqttstorestateconfig* Config)
{
    xmqttjournal* J=NULL;
    if(xmqttJournalSlotOpenCurrent(Slot,&J)!=XMQTT_JOURNAL_DONE)return false;
    const unsigned char Message[]={0x30,0x05,0,1,'t',0,'a'};
    const xmqttstoredelta Operations[]={
        {.Type=XMQTT_STORE_BIND,.Epoch=1,.Data=XRT_BYTES_LITERAL("example")},
        {.Type=XMQTT_STORE_OPEN,.Epoch=1,.WallMilliseconds=1,.Expiry=60},
        {.Type=XMQTT_STORE_IN_ACCEPT,.Epoch=1,.Key=1,.Data={Message,sizeof(Message)}},
        {.Type=XMQTT_STORE_END,.Epoch=1,.WallMilliseconds=2,.Expiry=60},
    };
    unsigned char Buffer[192];
    size_t Bytes;
    uint64 Sequence=0;
    xmqttjournalrecord R;
    bool Good=xmqttJournalNext(J,&R)==XMQTT_JOURNAL_EMPTY;
    for(size_t I=0;Good&&I<sizeof(Operations)/sizeof(Operations[0]);++I){
        Good=xmqttStoreDeltaWrite(&Config->Delta,Operations+I,1,NULL,0,Buffer,sizeof(Buffer),&Bytes)==XMQTT_STORE_DELTA_DONE&&
             xmqttJournalAppend(J,(xbytesview){Buffer,Bytes},&Sequence)==XMQTT_JOURNAL_DONE;
    }
    if(!xmqttJournalDestroy(J)){(void)xmqttJournalDestroy(J);return false;}
    return Good;
}
static bool show(xmqttjournalslot* Slot,xmqttstorestateconfig* Config)
{
    xmqttjournalslotinfo SI;
    if(!xmqttJournalSlotInfo(Slot,&SI))return false;
    Config->Generation=SI.Generation;
    xmqttjournal* J=NULL;
    if(xmqttJournalSlotOpenCurrent(Slot,&J)!=XMQTT_JOURNAL_DONE)return false;
    xmqttstorestate* State=NULL;
    xmqttstorestateinfo Info;
    bool Good=xmqttStoreStateRecover(J,Config,&State)==XMQTT_STORE_DONE&&xmqttStoreStateInfo(State,&Info);
    if(Good)printf("Generation %llu, transactions %llu, pending Inbox %zu, highest key %llu\n",
        (unsigned long long)SI.Generation,(unsigned long long)Info.Transactions,
        Info.Inbox,(unsigned long long)Info.LastKey);
    xmqttStoreStateDestroy(State);
    if(!xmqttJournalDestroy(J)){(void)xmqttJournalDestroy(J);return false;}
    return Good;
}

int main(int Argc,char** Argv)
{
    if(Argc==1){puts("Usage: examples_store_compact_main create|compact|read DIRECTORY");return 0;}
    if(Argc!=3)return 2;
    bool Create=!strcmp(Argv[1],"create"),Compact=!strcmp(Argv[1],"compact");
    if(!Create&&!Compact&&strcmp(Argv[1],"read"))return 2;
    xroot Root=xrtRootOpen(Argv[2]);
    if(!Root)return 1;
    xmqttjournalconfig JC;
    xmqttJournalConfigInit(&JC);
    JC.Generation=1;JC.Binding=XRT_BYTES_LITERAL("example/typed-checkpoint");
    JC.PayloadSchema=XMQTT_STORE_DELTA_SCHEMA;
    xmqttstorestateconfig C;
    xmqttStoreStateConfigInit(&C);
    C.Binding=JC.Binding;C.ClientId=XRT_STR_LITERAL("example");C.WallMilliseconds=1000;
    xmqttjournalslot* Slot=NULL;
    bool OK=(Create?xmqttJournalSlotCreate(Root,&JC,&Slot):xmqttJournalSlotOpen(Root,&JC,&Slot))==XMQTT_JOURNAL_DONE;
    if(OK&&Create)OK=seed(Slot,&C);
    if(OK&&Compact){
        xmqttjournalslotinfo I;
        OK=xmqttJournalSlotInfo(Slot,&I);
        if(OK){
            C.Generation=I.Generation;
            OK=xmqttStoreCompact(Slot,&C,256)==XMQTT_JOURNAL_DONE;
        }
    }
    if(OK)OK=show(Slot,&C);
    if(!xmqttJournalSlotDestroy(Slot)){(void)xmqttJournalSlotDestroy(Slot);OK=false;}
    if(!xrtRootClose(Root))OK=false;
    return OK?0:1;
}
