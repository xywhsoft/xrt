#include "support/journal_peer.h"
static jpdrive Drive;static unsigned OldNotifications,NewNotifications;static const xerror* Ambient;static bool BackendNotice;
static xvfsfileops_v1 ErrorOps;
static void jeHandler(const xerror* Error,ptr Context)
{
    (void)Context;if(Error==Ambient)++OldNotifications;else ++NewNotifications;
    xerror* Taken=NULL;testRequire(xrtTrySwapErrorTake(NULL,&Taken)&&Taken==Error,"handler sees owning current error");
    testRequire(xrtTrySwapErrorTake(Taken,&Taken)&&!Taken,"handler reentrant quiet restore");
}
static bool jeFlush(void* State)
{
    if(BackendNotice){BackendNotice=false;xrtSetErrorInfo(XERR_STATE,"journal.backend.notice",1,"successful provider diagnostic");}
    return jpFlush(State);
}
static xvfslookup jeOpen(void* C,xvfscase Case,xstrview Path,const xfileoptions* O,xvfsfile_v1* F)
{xvfslookup R=jpOpen(C,Case,Path,O,F);if(R==XVFS_LOOKUP_OPENED)F->Ops=&ErrorOps;return R;}
static jpfile jeFile(void)
{
    ErrorOps=jpOps;ErrorOps.Flush=jeFlush;xvfsprovider_v1 P=jpProvider;P.Open=jeOpen;
    jpfile F={0};F.Vfs=xrtVfsCreate();testRequire(F.Vfs!=NULL,"error scope VFS");F.Mount=xrtVfsMount(F.Vfs,"/",0,XVFS_CASE_SENSITIVE,&P,&Drive,0);testRequire(F.Mount!=NULL,"error scope mount");
    xfileoptions O;xrtFileOptionsInit(&O);O.Flags=XFILE_READ|XFILE_WRITE;F.File=xrtVfsOpen(F.Vfs,"/journal",&O);testRequire(F.File!=NULL,"error scope file");return F;
}
static void jeQuiet(void)
{testRequire(xrtGetError()==Ambient,"successful operation restores exact old error identity");}
static void jeCase(xmqttversion V)
{
    memset(&Drive,0,sizeof(Drive));OldNotifications=NewNotifications=0;BackendNotice=false;jpfile F=jeFile();xmqttjournalconfig C=jpConfig();C.Version=V;
    xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(65536,&B)&&xrtAllocationBudgetEnter(B,&Scope),"error ownership budget");
    Ambient=xrtErrorCreate(XERR_STATE,"journal.errors.ambient",17,"old diagnostic");testRequire(Ambient!=NULL,"old error allocated");xrtSetErrorTake((xerror*)Ambient);xrtSetErrorHandler(jeHandler,NULL);
    BackendNotice=true;xmqttjournal* J=jpCreate(F,&C);jeQuiet();testRequire(NewNotifications==1,"new backend diagnostic notified exactly once");
    jpAppend(J,"first",1);jeQuiet();jpAppend(J,"second",2);jeQuiet();xmqttjournalrecord R={99,{0}};
    jpStatus(xmqttJournalNext(J,&R),XMQTT_JOURNAL_EMPTY,"ready journal empty read");testRequire(R.Sequence==99,"EMPTY output untouched");jeQuiet();
    uint64 Seq=777;unsigned char Large[1025]={0};unsigned Calls=Drive.Calls;
    jpStatus(xmqttJournalAppend(J,(xbytesview){Large,sizeof(Large)},&Seq),XMQTT_JOURNAL_LIMIT,"normal quota refusal");testRequire(Seq==777&&Calls==Drive.Calls,"quota refusal before IO/output");jeQuiet();
    testRequire(xmqttJournalDestroy(J),"first owner freed");jeQuiet();J=jpReopen(F,&C);jeQuiet();
    jpStatus(xmqttJournalNext(J,&R),XMQTT_JOURNAL_DONE,"first scoped replay");testRequire(R.Sequence==1&&R.Data.Size==5&&!memcmp(R.Data.Data,"first",5),"first replay exact");jeQuiet();
    jpStatus(xmqttJournalNext(J,&R),XMQTT_JOURNAL_DONE,"second scoped replay");testRequire(R.Sequence==2&&R.Data.Size==6&&!memcmp(R.Data.Data,"second",6),"second replay exact");jeQuiet();
    jpStatus(xmqttJournalNext(J,&R),XMQTT_JOURNAL_EMPTY,"scoped final replay");jeQuiet();testRequire(xmqttJournalDestroy(J),"recovery owner freed");
    xmqttjournalconfig Small=C;Small.MaxTransactionBytes=4;J=jpReopen(F,&Small);jeQuiet();R.Sequence=99;
    jpStatus(xmqttJournalNext(J,&R),XMQTT_JOURNAL_LIMIT,"local replay capacity refusal");testRequire(R.Sequence==99&&jpInfo(J).State==XMQTT_JOURNAL_RECOVERING,"normal replay refusal atomic");jeQuiet();testRequire(xmqttJournalDestroy(J),"small owner freed");
    Small=C;Small.MaxFileBytes=12288;J=(xmqttjournal*)(uintptr_t)1;
    jpStatus(xmqttJournalOpen(F.File,&Small,&J),XMQTT_JOURNAL_LIMIT,"constructor physical quota refusal");testRequire(J==(xmqttjournal*)(uintptr_t)1,"constructor LIMIT output untouched");jeQuiet();
    printf("[NOTIFICATIONS] MQTT%d old=%u new=%u\n",(int)V,OldNotifications,NewNotifications);
    testRequire(!OldNotifications&&NewNotifications==1,"success and normal refusals never notify an old error again");
    /* A false-with-no-error backend failure must still publish a real IO error. */
    J=jpReopen(F,&C);testRequire(jpReplay(J)==2,"failure scope recovered prefix");Drive.Fail=Drive.Calls+1;
    jpStatus(xmqttJournalAppend(J,XRT_BYTES_LITERAL("third"),&Seq),XMQTT_JOURNAL_IO,"fresh IO diagnostic");
    testRequire(Seq==777&&NewNotifications==2&&!OldNotifications&&!xrtErrorFind(xrtGetError(),"journal.errors.ambient",17),"failed IO does not reuse or re-notify stale cause");
    testRequire(xmqttJournalDestroy(J),"failed owner freed");xrtSetErrorHandler(NULL,NULL);Ambient=NULL;xrtClearError();
    testRequire(xrtAllocationBudgetLeave(&Scope),"error budget leave");xallocationbudgetinfo I={.Size=sizeof(I)};testRequire(xrtAllocationBudgetInfo(B,&I)&&!I.LiveCount&&!I.LiveBytes,"saved and discarded errors physically refunded");xrtAllocationBudgetFree(B);jpFileClose(F);
}
int main(void)
{jeCase(XMQTT_VERSION_5);jeCase(XMQTT_VERSION_311);puts("[PASS] quiet journal success/refusals, backend diagnostics, handler reentry and error ownership refunds");return 0;}
