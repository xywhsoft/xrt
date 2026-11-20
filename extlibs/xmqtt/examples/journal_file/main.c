#include "../common/journal_files.h"
#include <stdio.h>
#include <string.h>

int main(int Argc,char** Argv)
{
    if(Argc==1){
        puts("Usage: examples_journal_file_main create|open DIRECTORY EXISTING_FILE");
        return 0;
    }
    if(Argc!=4)return 2;
    bool Create=!strcmp(Argv[1],"create");
    if(!Create&&strcmp(Argv[1],"open"))return 2;
    xroot Root=xrtRootOpen(Argv[2]);
    if(!Root)return 1;
    xmqttjournalconfig C;
    xmqttJournalConfigInit(&C);
    C.Binding=XRT_BYTES_LITERAL("example/broker/principal/slot");
    C.Generation=1;
    xfile LockedFile=NULL;
    xmqttjournal* Journal=NULL;
    xmqttjournalstatus Status=mqttExampleOpenNativeJournal(Root,Argv[3],&C,Create,&LockedFile,&Journal);
    bool RootClosed=xrtRootClose(Root);
    if(Status==XMQTT_JOURNAL_BUSY){
        puts("Journal already has an owner");
        return RootClosed?3:1;
    }
    if(Status!=XMQTT_JOURNAL_DONE)return 1;
    bool OK=RootClosed;
    if(OK&&!Create){
        xmqttjournalrecord Record;
        while((Status=xmqttJournalNext(Journal,&Record))==XMQTT_JOURNAL_DONE)
            printf("Recovered transaction %llu\n",(unsigned long long)Record.Sequence);
        OK=Status==XMQTT_JOURNAL_EMPTY;
    }
    if(OK){
        uint64 Sequence=0;
        OK=xmqttJournalAppend(Journal,XRT_BYTES_LITERAL("opaque demo transaction"),&Sequence)==XMQTT_JOURNAL_DONE;
        if(OK)printf("Committed transaction %llu\n",(unsigned long long)Sequence);
    }
    if(!mqttExampleCloseNativeJournal(Journal,&LockedFile))return 1;
    return OK?0:1;
}
