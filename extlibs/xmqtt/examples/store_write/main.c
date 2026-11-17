#include <xmqtt/store_write.h>
#include <xmqtt/store_state.h>
#include <xrt/memory.h>
#include <stdio.h>
#include <string.h>
int main(int Argc,char** Argv)
{
    if(Argc>2)return 2;
    str Path=NULL;xfile File=Argc==2?xrtOpen(Argv[1],XFILE_READ|XFILE_WRITE):xrtFileTemp(NULL,"xmqtt-commit-",NULL,&Path);if(!File)return 1;
    xmqttstorestateconfig RC;xmqttStoreStateConfigInit(&RC);RC.Binding=XRT_BYTES_LITERAL("demo/broker/principal/slot");RC.Generation=1;RC.ClientId=XRT_STR_LITERAL("commit-demo");RC.WallMilliseconds=1004;
    xmqttjournalconfig JC;xmqttJournalConfigInit(&JC);JC.Binding=RC.Binding;JC.Generation=RC.Generation;JC.PayloadSchema=XMQTT_STORE_DELTA_SCHEMA;xmqttjournal* J=NULL;xmqttstorestate* State=NULL;xmqttstorewrite* W=NULL;xtaskpool* Pool=NULL;xfuture* Future=NULL;uint64 Size=0;bool OK=xrtFileSize(File,&Size);
    if(OK&&!Size){OK=xmqttJournalCreate(File,&JC,&J)==XMQTT_JOURNAL_DONE;unsigned char Wire[128],Batch[512];size_t N=0,Bytes=0;xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Identifier=17;Q.Body.Publish.Topic=XRT_STR_LITERAL("demo/accepted");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("application work");if(OK)OK=xmqttPacketWrite5(Wire,sizeof(Wire),&Q,XMQTT_SERVER_TO_CLIENT,NULL,&N);
        xmqttstoredelta Ops[4]={{.Type=XMQTT_STORE_BIND,.Epoch=1,.WallMilliseconds=1000,.Data=XRT_BYTES_LITERAL("commit-demo")},{.Type=XMQTT_STORE_OPEN,.Flags=1,.Epoch=1,.WallMilliseconds=1001},{.Type=XMQTT_STORE_IN_ACCEPT,.Identifier=17,.Epoch=1,.Key=1,.Data={Wire,N}},{.Type=XMQTT_STORE_END,.Flags=1,.Epoch=1,.WallMilliseconds=1002}};xmqttproperty P[4];if(OK)OK=xmqttStoreDeltaWrite(&RC.Delta,Ops,4,P,4,Batch,sizeof(Batch),&Bytes)==XMQTT_STORE_DELTA_DONE;uint64 Seq=0;if(OK)OK=xmqttJournalAppend(J,(xbytesview){Batch,Bytes},&Seq)==XMQTT_JOURNAL_DONE;if(J&&!xmqttJournalDestroy(J))OK=false;J=NULL;}
    if(OK)OK=xmqttJournalOpen(File,&JC,&J)==XMQTT_JOURNAL_DONE&&xmqttStoreStateRecover(J,&RC,&State)==XMQTT_STORE_DONE;
    xmqttstorestateinfo I={0};if(OK)OK=xmqttStoreStateInfo(State,&I);
    if(OK&&I.Inbox){xmqttstoreview Delivery={0};OK=xmqttStoreStateView(State,XMQTT_STORE_INBOX,0,&Delivery)==XMQTT_STORE_DONE;
        /* Application effects must finish BEFORE constructing its delivery ACK.
         * This passive demo only prints that work; no business atomicity claim. */
        if(OK)printf("accepted delivery%llu: %zu immutable wire bytes\n",(unsigned long long)Delivery.Key,Delivery.Wire.Size);
        xmqttstoredelta Ack={.Type=XMQTT_STORE_INBOX_ACK,.Epoch=Delivery.Epoch,.Key=Delivery.Key};unsigned char Buffer[128];size_t N=0;xmqttproperty P[4];if(OK)OK=xmqttStoreDeltaWrite(&RC.Delta,&Ack,1,P,4,Buffer,sizeof(Buffer),&N)==XMQTT_STORE_DELTA_DONE;
        xtaskpoolconfig PC={.Threads=2,.QueueLimit=8};if(OK){Pool=xrtTaskPoolCreate(&PC);OK=Pool!=NULL;}xmqttstorewriteconfig C;xmqttStoreWriteConfigInit(&C);if(OK){OK=xmqttStoreWriteCreate(Pool,J,State,&C,&W);if(OK){J=NULL;State=NULL;}}
        if(OK)OK=xmqttStoreWriteAppendAsync(W,(xbytesview){Buffer,N},10000,&Future)==XMQTT_JWORK_ACCEPTED;
        if(OK){memset(Buffer,0xcc,sizeof(Buffer));OK=xrtFutureWaitFor(Future,10000)==XWAIT_OK&&xrtFutureState(Future)==XFUTURE_RESOLVED;if(OK){const xmqttcommitresult* R=xrtFutureValue(Future);printf("physical commit status%d effect%d sequence%llu\n",R->Status,R->Effect,(unsigned long long)R->Sequence);OK=R->Status==XMQTT_COMMIT_OK&&R->Effect==XMQTT_JOURNAL_COMMITTED;}}
    }
    if(W&&!xmqttStoreWriteDestroy(W))OK=false;
    if(Pool){if(!xrtTaskPoolClose(Pool)||xrtTaskPoolWaitFor(Pool,10000)!=XWAIT_OK||!xrtTaskPoolDestroy(Pool))OK=false;}
    if(J&&!xmqttJournalDestroy(J))OK=false;
    J=NULL;xmqttStoreStateDestroy(State);State=NULL;
    if(OK)OK=xmqttJournalOpen(File,&JC,&J)==XMQTT_JOURNAL_DONE&&xmqttStoreStateRecover(J,&RC,&State)==XMQTT_STORE_DONE&&xmqttStoreStateInfo(State,&I);
    if(OK)printf("fresh recovery pending Inbox: %zu\n",I.Inbox);
    if(J&&!xmqttJournalDestroy(J))OK=false;
    xmqttStoreStateDestroy(State);xrtFutureDestroy(Future);if(!xrtClose(File))OK=false;if(Path){if(!xrtFileDelete(Path))OK=false;xrtFree(Path);}return OK?0:1;
}
