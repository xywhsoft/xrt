#include <xmqtt/store_delta.h>
#include <xmqtt/journal.h>
#include <stdio.h>
int main(int Argc,char** Argv)
{
    if(Argc>2)return 2;
    xmqttstoredeltaconfig C;xmqttStoreDeltaConfigInit(&C);xmqttproperty Scratch[4];unsigned char Packet[256],Batch[1024];size_t N=0,Bytes=0;
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=4;Q.Identifier=17;Q.Body.Publish.Topic=XRT_STR_LITERAL("demo/incoming");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("hello");
    if(!xmqttPacketWrite5(Packet,sizeof(Packet),&Q,XMQTT_SERVER_TO_CLIENT,&C.Codec,&N))return 1;
    /* Illustrative accepted connection + first receive admission. No broker
     * connection, authentication proof, ACK output or business effect is run. */
    xmqttstoredelta Ops[3]={{.Type=XMQTT_STORE_BIND,.Epoch=1,.WallMilliseconds=1000,.Data=XRT_BYTES_LITERAL("store-demo")},
        {.Type=XMQTT_STORE_OPEN,.Flags=XMQTT_STORE_DELTA_CLEAN_START,.Expiry=3600,.Epoch=1,.WallMilliseconds=1001},
        {.Type=XMQTT_STORE_IN_ACCEPT,.Identifier=17,.Epoch=1,.Key=1,.Data={Packet,N}}};
    if(xmqttStoreDeltaWrite(&C,Ops,3,Scratch,4,Batch,sizeof(Batch),&Bytes)!=XMQTT_STORE_DELTA_DONE)return 1;
    xmqttstoredelta Parsed[16];xmqttstoredeltadocument Doc;
    if(xmqttStoreDeltaRead(&C,(xbytesview){Batch,Bytes},Parsed,16,Scratch,4,&Doc)!=XMQTT_STORE_DELTA_DONE||Doc.Count!=3)return 1;
    printf("typed batch: %zu records, %zu bytes; payload and QoS2 identity share acceptance\n",Doc.Count,Bytes);
    if(Argc==1){puts("optional: pass an exclusively owned existing file (see README)");return 0;}
    xfile F=xrtOpen(Argv[1],XFILE_READ|XFILE_WRITE);if(!F)return 1;uint64 Size=0,Seq=0;xmqttjournal* J=NULL;bool OK=xrtFileSize(F,&Size);
    xmqttjournalconfig JC;xmqttJournalConfigInit(&JC);JC.Generation=1;JC.Binding=XRT_BYTES_LITERAL("demo/broker/principal/slot");JC.PayloadSchema=XMQTT_STORE_DELTA_SCHEMA;
    if(OK)OK=(Size?xmqttJournalOpen(F,&JC,&J):xmqttJournalCreate(F,&JC,&J))==XMQTT_JOURNAL_DONE;
    if(OK&&!Size){OK=xmqttJournalAppend(J,(xbytesview){Batch,Bytes},&Seq)==XMQTT_JOURNAL_DONE;if(OK)printf("synced one transaction: %llu\n",(unsigned long long)Seq);}
    else if(OK){xmqttjournalrecord R;xmqttjournalstatus S;while((S=xmqttJournalNext(J,&R))==XMQTT_JOURNAL_DONE){if(xmqttStoreDeltaRead(&C,R.Data,Parsed,16,Scratch,4,&Doc)!=XMQTT_STORE_DELTA_DONE){OK=false;break;}printf("recovered transaction%llu: %zu typed records\n",(unsigned long long)R.Sequence,Doc.Count);}if(S!=XMQTT_JOURNAL_EMPTY)OK=false;}
    if(J&&!xmqttJournalDestroy(J)){OK=false;}if(!xrtClose(F)){OK=false;}return OK?0:1;
}
