#include "../common/journal_files.h"
#include <stdio.h>
#include <string.h>

/* The demo format uses complete opaque snapshots. Only the final snapshot
 * carries the demo's state. An MQTT application supplies its own codec. */
int main(int Argc,char** Argv)
{
    if(Argc==1){
        puts("Usage: examples_journal_slot_main create|read|rotate DIRECTORY");
        return 0;
    }
    if(Argc!=3)return 2;
    bool Create=!strcmp(Argv[1],"create"),Rotate=!strcmp(Argv[1],"rotate");
    if(!Create&&!Rotate&&strcmp(Argv[1],"read"))return 2;
    xroot Root=xrtRootOpen(Argv[2]);
    if(!Root)return 1;
    xmqttjournalconfig C;
    xmqttJournalConfigInit(&C);
    C.Generation=1;
    C.Binding=XRT_BYTES_LITERAL("example/complete-snapshots");
    mqttExampleJournalSlotFiles Files={0};
    mqttExampleJournalSlot* Slot=NULL;
    xmqttjournal* J=NULL;
    bool OK=false;
    xmqttjournalstatus Status=mqttExampleOpenNativeSlot(Root,&C,Create,&Files,&Slot);
    if(Status!=XMQTT_JOURNAL_DONE)goto Done;
    if(mqttExampleJournalSlotOpenCurrent(Slot,&J)!=XMQTT_JOURNAL_DONE)goto Done;
    unsigned char Snapshot[64];
    size_t Bytes=0;
    xmqttjournalrecord Record;
    while((Status=xmqttJournalNext(J,&Record))==XMQTT_JOURNAL_DONE){
        if(Record.Data.Size>sizeof(Snapshot))goto Done;
        Bytes=Record.Data.Size;
        memcpy(Snapshot,Record.Data.Data,Bytes);
    }
    if(Status!=XMQTT_JOURNAL_EMPTY)goto Done;
    uint64 Sequence=0;
    if(Create){
        if(mqttExampleJournalSlotAppend(Slot,J,XRT_BYTES_LITERAL("state=1"),&Sequence)!=XMQTT_JOURNAL_DONE||
           mqttExampleJournalSlotAppend(Slot,J,XRT_BYTES_LITERAL("state=2"),&Sequence)!=XMQTT_JOURNAL_DONE)goto Done;
    }else if(Rotate){
        if(!Bytes||!mqttExampleJournalSlotClose(Slot,J))goto Done;
        J=NULL;
        if(mqttExampleJournalSlotPrepare(Slot,&J)!=XMQTT_JOURNAL_DONE||
           mqttExampleJournalSlotAppend(Slot,J,(xbytesview){Snapshot,Bytes},&Sequence)!=XMQTT_JOURNAL_DONE||
           mqttExampleJournalSlotPublish(Slot,J)!=XMQTT_JOURNAL_DONE)goto Done;
    }
    mqttExampleJournalSlotInfo Info={0};
    if(!mqttExampleJournalSlotGetInfo(Slot,&Info))goto Done;
    printf("Selected generation %llu\n",(unsigned long long)Info.Generation);
    if(Bytes)printf("Snapshot %.*s\n",(int)Bytes,(const char*)Snapshot);
    OK=true;

Done:
    if(!mqttExampleJournalSlotClose(Slot,J)){
        (void)mqttExampleJournalSlotClose(Slot,J);
        OK=false;
    }
    if(!mqttExampleCloseNativeSlot(Slot,&Files))OK=false;
    if(!xrtRootClose(Root))OK=false;
    return OK?0:1;
}
