#include "support/journal_peer.h"
static jpdrive Drive;
static void journalBasic(xmqttversion V)
{
    memset(&Drive,0,sizeof(Drive));jpfile F=jpFile(&Drive);xmqttjournalconfig C=jpConfig();C.Version=V;xmqttjournal* J=jpCreate(F,&C);
    testRequire(Drive.Size==12288&&Drive.Flushes==1,"native-public-operation prototype initialized and synced");
    xrtSetErrorInfo(XERR_STATE,"journal.ambient",1,"preserve");const xerror* Error=xrtGetError();Drive.Reenter=true;Drive.Journal=J;
    jpAppend(J,"first",1);
    xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B)&&xrtAllocationBudgetEnter(B,&Scope),"allocation-free append budget");
    uint64 Seq=0;jpStatus(xmqttJournalAppend(J,(xbytesview){0},&Seq),XMQTT_JOURNAL_DONE,"empty transaction");testRequire(Seq==2&&xrtGetError()==Error,"append preserves ambient error identity");
    testRequire(xrtAllocationBudgetLeave(&Scope),"append budget leave");xallocationbudgetinfo BI={.Size=sizeof(BI)};testRequire(xrtAllocationBudgetInfo(B,&BI)&&!BI.Rejections&&!BI.PeakBytes,"zero append heap allocations ");xrtAllocationBudgetFree(B);
    testRequire(Drive.Calls==18&&Drive.Flushes==5,"create5 operations; first7 empty6, two sync barriers each");testRequire(xmqttJournalDestroy(J)&&!Drive.Closes,"journal destroy retains borrowed file");Drive.Journal=NULL;xrtClearError();
    J=jpReopen(F,&C);Seq=999;jpStatus(xmqttJournalAppend(J,XRT_BYTES_LITERAL("bad"),&Seq),XMQTT_JOURNAL_STATE,"no writes before complete replay");testRequire(Seq==999,"state rejection output");xrtClearError();xmqttjournalrecord R;
    jpStatus(xmqttJournalNext(J,&R),XMQTT_JOURNAL_DONE,"first replay");testRequire(R.Sequence==1&&R.Data.Size==5&&!memcmp(R.Data.Data,"first",5),"exact replay payload");
    uint64* Overlap=(uint64*)(void*)R.Data.Data;jpStatus(xmqttJournalAppend(J,XRT_BYTES_LITERAL("x"),Overlap),XMQTT_JOURNAL_ERROR,"library replay storage output excluded");xrtClearError();
    jpStatus(xmqttJournalNext(J,&R),XMQTT_JOURNAL_DONE,"zero payload replay");testRequire(!R.Data.Size&&R.Sequence==2,"empty transaction data");jpStatus(xmqttJournalNext(J,&R),XMQTT_JOURNAL_EMPTY,"full replay readiness");
    unsigned Calls=Drive.Calls;C=jpConfig();C.Version=V;Seq=777;unsigned char Large[1025]={0};jpStatus(xmqttJournalAppend(J,(xbytesview){Large,sizeof(Large)},&Seq),XMQTT_JOURNAL_LIMIT,"transaction limit");testRequire(Seq==777&&Calls==Drive.Calls,"capacity refusal no I/O/output");
    Drive.Chunk=7;jpAppend(J,"second",3);testRequire(xmqttJournalDestroy(J),"destroy after short I/O");J=jpReopen(F,&C);testRequire(jpReplay(J)==3,"short reads and writes completed by existing XRT");testRequire(xmqttJournalDestroy(J),"roundtrip stop");jpFileClose(F);testRequire(Drive.Closes==1,"borrowed backend closes exactly once");xrtClearError();
}
static int journalNative(int Argc,char** Argv)
{
    testRequire(Argc==3||Argc==4,"native fixture/crash CLI");bool Create=strcmp(Argv[1],"--verify")!=0;
    xfile Native=xrtOpen(Argv[2],XFILE_READ|XFILE_WRITE|(Create?(XFILE_CREATE|XFILE_TRUNCATE):0));testRequire(Native!=NULL,"native XRT file open");xmqttjournalconfig C=jpConfig();xmqttjournal* J=NULL;
    if(!Create){xfile ReadOnly=xrtOpen(Argv[2],XFILE_READ);testRequire(ReadOnly!=NULL,"native readonly capability probe");xmqttjournal* Probe=(xmqttjournal*)(uintptr_t)1;jpStatus(xmqttJournalOpen(ReadOnly,&C,&Probe),XMQTT_JOURNAL_ERROR,"readonly file cannot claim journal sync/write ownership");testRequire(Probe==(xmqttjournal*)(uintptr_t)1&&xrtClose(ReadOnly),"readonly output and file ownership");xfile Append=xrtOpen(Argv[2],XFILE_READ|XFILE_WRITE|XFILE_APPEND);testRequire(Append!=NULL,"native append-mode probe");jpStatus(xmqttJournalOpen(Append,&C,&Probe),XMQTT_JOURNAL_ERROR,"absolute journal refuses append mode");testRequire(Probe==(xmqttjournal*)(uintptr_t)1&&xrtClose(Append),"append-mode borrowed ownership");xrtClearError();}
    if(Create){jpStatus(xmqttJournalCreate(Native,&C,&J),XMQTT_JOURNAL_DONE,"native initialize");jpAppend(J,"first",1);testRequire(xmqttJournalDestroy(J),"initial owner destroyed");}
    if(!strcmp(Argv[1],"--crash")){
        memset(&Drive,0,sizeof(Drive));Drive.Native=Native;jpfile F=jpFile(&Drive);J=jpReopen(F,&C);testRequire(jpReplay(J)==1,"old native prefix recovered");Drive.Calls=0;Drive.KillAt=(unsigned)atoi(Argv[3]);jpAppend(J,"second",2);testRequire(xmqttJournalDestroy(J),"child normal end");jpFileClose(F);
    }else if(!strcmp(Argv[1],"--emit")){jpStatus(xmqttJournalOpen(Native,&C,&J),XMQTT_JOURNAL_DONE,"native reopen emit");testRequire(jpReplay(J)==1,"emit prefix");jpAppend(J,"second",2);testRequire(xmqttJournalDestroy(J),"emit stop");}
    else {jpStatus(xmqttJournalOpen(Native,&C,&J),XMQTT_JOURNAL_DONE,"fresh native reader");unsigned Count=0;xmqttjournalrecord R;xmqttjournalstatus S;while((S=xmqttJournalNext(J,&R))==XMQTT_JOURNAL_DONE){++Count;const char* Want=Count==1?"first":"second";testRequire(R.Sequence==Count&&Count<=2&&R.Data.Size==strlen(Want)&&!memcmp(R.Data.Data,Want,R.Data.Size),"fresh-process exact payload");}jpStatus(S,XMQTT_JOURNAL_EMPTY,"fresh native validated end");printf("[RECOVERED] %u\n",Count);if(Argc==4)testRequire(Count==(unsigned)atoi(Argv[3]),"expected post-kill committed sequence");testRequire(xmqttJournalDestroy(J),"native reader owner stop");}
    testRequire(xrtClose(Native),"native file closed by actual owner");puts("[PASS] native file journal");return 0;
}
int main(int Argc,char** Argv)
{if(Argc>1)return journalNative(Argc,Argv);journalBasic(XMQTT_VERSION_5);journalBasic(XMQTT_VERSION_311);puts("[PASS] public XRT native/virtual I/O prototype, journal replay, ownership, bounds, no hot allocation");return 0;}
