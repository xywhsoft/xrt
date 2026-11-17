#include <xmqtt/store_state.h>
#include <xrt/memory.h>
#include <stdio.h>
int main(int Argc,char** Argv)
{
    if(Argc>2)return 2;
    str Temporary=NULL;xfile F=Argc==2?xrtOpen(Argv[1],XFILE_READ|XFILE_WRITE):xrtFileTemp(NULL,"xmqtt-state-",NULL,&Temporary);if(!F)return 1;
    xmqttstorestateconfig C;xmqttStoreStateConfigInit(&C);C.Binding=XRT_BYTES_LITERAL("demo/broker/principal/slot");C.Generation=1;C.ClientId=XRT_STR_LITERAL("store-demo");C.WallMilliseconds=1003;
    xmqttjournalconfig JC;xmqttJournalConfigInit(&JC);JC.Binding=C.Binding;JC.Generation=C.Generation;JC.PayloadSchema=XMQTT_STORE_DELTA_SCHEMA;
    uint64 Size=0;bool OK=xrtFileSize(F,&Size);xmqttjournal* J=NULL;xmqttstorestate* State=NULL;
    if(OK&&!Size){OK=xmqttJournalCreate(F,&JC,&J)==XMQTT_JOURNAL_DONE;
        unsigned char Wire[128],Batch[512];size_t N=0,Bytes=0;xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=4;Q.Identifier=17;Q.Body.Publish.Topic=XRT_STR_LITERAL("demo/incoming");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("accepted hello");
        if(OK)OK=xmqttPacketWrite5(Wire,sizeof(Wire),&Q,XMQTT_SERVER_TO_CLIENT,&C.Delta.Codec,&N);
        xmqttstoredelta Ops[4]={{.Type=XMQTT_STORE_BIND,.Epoch=1,.WallMilliseconds=1000,.Data=XRT_BYTES_LITERAL("store-demo")},
            {.Type=XMQTT_STORE_OPEN,.Flags=1,.Expiry=60,.Epoch=1,.WallMilliseconds=1001},
            {.Type=XMQTT_STORE_IN_ACCEPT,.Identifier=17,.Epoch=1,.Key=1,.Data={Wire,N}},
            {.Type=XMQTT_STORE_END,.Flags=1,.Epoch=1,.WallMilliseconds=1002}};
        xmqttproperty Scratch[4];if(OK)OK=xmqttStoreDeltaWrite(&C.Delta,Ops,4,Scratch,4,Batch,sizeof(Batch),&Bytes)==XMQTT_STORE_DELTA_DONE;
        uint64 Seq=0;if(OK)OK=xmqttJournalAppend(J,(xbytesview){Batch,Bytes},&Seq)==XMQTT_JOURNAL_DONE;
        if(J&&!xmqttJournalDestroy(J))OK=false;
        J=NULL;
    }
    if(OK)OK=xmqttJournalOpen(F,&JC,&J)==XMQTT_JOURNAL_DONE&&xmqttStoreStateRecover(J,&C,&State)==XMQTT_STORE_DONE;
    xmqttstorestateinfo I;xmqttstoreview V;if(OK)OK=xmqttStoreStateInfo(State,&I)&&!I.HasSession&&!I.Incoming&&I.Inbox==1&&xmqttStoreStateView(State,XMQTT_STORE_INBOX,0,&V)==XMQTT_STORE_DONE;
    if(J&&!xmqttJournalDestroy(J))OK=false;
    if(!xrtClose(F))OK=false;
    if(OK)printf("recovered Inbox key%llu: %zu owned wire bytes after protocol expiry and native file close\n",(unsigned long long)V.Key,V.Wire.Size);
    xmqttStoreStateDestroy(State);if(Temporary){if(!xrtFileDelete(Temporary))OK=false;xrtFree(Temporary);}
    return OK?0:1;
}
