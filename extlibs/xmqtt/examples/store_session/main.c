#include <xmqtt/store_session.h>
#include <xrt/memory.h>
#include <xrt/time.h>
#include <stdio.h>
int main(int Argc,char** Argv)
{
    if(Argc>2)return 2;
    str Temporary=NULL;xfile F=Argc==2?xrtOpen(Argv[1],XFILE_READ|XFILE_WRITE):xrtFileTemp(NULL,"xmqtt-import-",NULL,&Temporary);if(!F)return 1;
    xmqttstorestateconfig RC;xmqttStoreStateConfigInit(&RC);RC.Binding=XRT_BYTES_LITERAL("demo/broker/principal/slot");RC.Generation=1;RC.ClientId=XRT_STR_LITERAL("import-demo");RC.WallMilliseconds=1003;
    xmqttjournalconfig JC;xmqttJournalConfigInit(&JC);JC.Binding=RC.Binding;JC.Generation=RC.Generation;JC.PayloadSchema=XMQTT_STORE_DELTA_SCHEMA;uint64 Size=0;bool OK=xrtFileSize(F,&Size);xmqttjournal* J=NULL;xmqttstorestate* State=NULL;xmqttsession* Session=NULL;
    if(OK&&!Size){OK=xmqttJournalCreate(F,&JC,&J)==XMQTT_JOURNAL_DONE;unsigned char Wire[128],Batch[1024];size_t N=0,Bytes=0;xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=4;Q.Identifier=17;Q.Body.Publish.Topic=XRT_STR_LITERAL("demo/recovery");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("hello\0world");if(OK)OK=xmqttPacketWrite5(Wire,sizeof(Wire),&Q,XMQTT_CLIENT_TO_SERVER,&RC.Delta.Codec,&N);
        xmqttstoredelta Ops[5]={{.Type=XMQTT_STORE_BIND,.Epoch=1,.WallMilliseconds=1000,.Data=XRT_BYTES_LITERAL("import-demo")},{.Type=XMQTT_STORE_OPEN,.Flags=1,.Expiry=60,.Epoch=1,.WallMilliseconds=1001},{.Type=XMQTT_STORE_OUT_PUT,.Flags=3,.Identifier=17,.Epoch=1,.Key=1,.Data={Wire,N}},{.Type=XMQTT_STORE_OUT_PUBREL,.Identifier=17,.Epoch=1,.Key=1},{.Type=XMQTT_STORE_END,.Epoch=1,.Expiry=60,.WallMilliseconds=1002}};
        xmqttproperty Scratch[4];if(OK)OK=xmqttStoreDeltaWrite(&RC.Delta,Ops,5,Scratch,4,Batch,sizeof(Batch),&Bytes)==XMQTT_STORE_DELTA_DONE;uint64 Seq=0;if(OK)OK=xmqttJournalAppend(J,(xbytesview){Batch,Bytes},&Seq)==XMQTT_JOURNAL_DONE;if(J&&!xmqttJournalDestroy(J))OK=false;
        J=NULL;}
    if(OK)OK=xmqttJournalOpen(F,&JC,&J)==XMQTT_JOURNAL_DONE&&xmqttStoreStateRecover(J,&RC,&State)==XMQTT_STORE_DONE;
    if(J&&!xmqttJournalDestroy(J))OK=false;
    if(!xrtClose(F))OK=false;
    xmqttsessionconfig SK;xmqttSessionConfigInit(&SK);SK.ClientId=RC.ClientId;xmqttstoresessionconfig IC;xmqttStoreSessionConfigInit(&IC);IC.Binding=RC.Binding;IC.Generation=RC.Generation;IC.WallMilliseconds=1004;IC.MonotonicTime=xrtTimer();if(OK)OK=xmqttStoreSessionRestore(State,&SK,&IC,&Session)==XMQTT_SESSION_DONE;
    xmqttStoreStateDestroy(State);xmqttsessioninfo I={0};if(OK)OK=xmqttSessionInfo(Session,&I)&&!I.Connected&&I.HasState&&I.ActiveOutgoing==1&&!I.Generation;if(OK)printf("independent disconnected Session: %zu outgoing obligation; authenticated Client/CONNACK required before PUBREL replay\n",I.ActiveOutgoing);if(Session&&!xmqttSessionDestroy(Session))OK=false;
    if(Temporary){if(!xrtFileDelete(Temporary))OK=false;xrtFree(Temporary);}return OK?0:1;
}
