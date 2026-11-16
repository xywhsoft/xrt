#include <xmqtt/journal.h>
#include <stdio.h>
/* Caller supplies a pre-existing exclusively owned file whose namespace has
 * already been durably published. Empty initializes; nonempty recovers. */
int main(int Argc,char** Argv)
{
    if(Argc!=2){puts("usage: journal-example existing-file (see README)");return 0;}
    xfile F=xrtOpen(Argv[1],XFILE_READ|XFILE_WRITE);if(!F)return 1;
    xmqttjournalconfig C;xmqttJournalConfigInit(&C);C.Binding=XRT_BYTES_LITERAL("demo/broker/principal/slot");C.Generation=1;
    uint64 Size=0,Sequence=0;xmqttjournal* J=NULL;bool OK=xrtFileSize(F,&Size);
    if(OK)OK=(Size?xmqttJournalOpen(F,&C,&J):xmqttJournalCreate(F,&C,&J))==XMQTT_JOURNAL_DONE;
    xmqttjournalstatus S=XMQTT_JOURNAL_EMPTY;xmqttjournalrecord R;
    if(OK&&Size){while((S=xmqttJournalNext(J,&R))==XMQTT_JOURNAL_DONE)printf("transaction%llu: %zu bytes\n",(unsigned long long)R.Sequence,R.Data.Size);OK=S==XMQTT_JOURNAL_EMPTY;}
    if(OK){S=xmqttJournalAppend(J,XRT_BYTES_LITERAL("opaque demo transaction"),&Sequence);OK=S==XMQTT_JOURNAL_DONE;if(OK)printf("committed%llu after both sync barriers\n",(unsigned long long)Sequence);else printf("append status%d; reopen and recover before retry\n",S);}
    if(J&&!xmqttJournalDestroy(J)){OK=false;}if(!xrtClose(F)){OK=false;}return OK?0:2;
}
