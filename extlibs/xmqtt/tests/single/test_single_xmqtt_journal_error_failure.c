#if !defined(_WIN32) && !defined(_WIN64) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include <stdbool.h>
static unsigned NativeCountdown,NativeFailures;
#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <windows.h>
static BOOL WINAPI jfNativeSet(DWORD Key,PVOID Value)
{if(NativeCountdown&&!--NativeCountdown){++NativeFailures;return FALSE;}return FlsSetValue(Key,Value);}
#define FlsSetValue jfNativeSet
#else
#include <pthread.h>
#include <errno.h>
static int jfNativeSet(pthread_key_t Key,const void* Value)
{if(NativeCountdown&&!--NativeCountdown){++NativeFailures;return ENOMEM;}return pthread_setspecific(Key,Value);}
#define pthread_setspecific jfNativeSet
#endif
#define XMQTT_MODULE_XMQTT_JOURNAL
#define XRT_MODULE_VFS
#define XRT_MODULE_MEMORY_STATS
#include <xmqtt/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#if defined(_WIN32) || defined(_WIN64)
#undef FlsSetValue
#else
#undef pthread_setspecific
#endif
#define XMQTT_IMPLEMENTATION
#include "../../../../single/extlibs/xmqtt.h"
#include "../support/journal_peer.h"
static jpdrive Drive;static unsigned IOCalls,Notifications;static xvfsfileops_v1 FaultOps;
static void jfHandler(const xerror* Error,ptr Context)
{(void)Error;(void)Context;++Notifications;}
static bool jfRead(void* S,uint64 At,void* B,size_t N,size_t* Done)
{++IOCalls;return jpRead(S,At,B,N,Done);}
static bool jfWrite(void* S,uint64 At,const void* B,size_t N,size_t* Done)
{++IOCalls;return jpWrite(S,At,B,N,Done);}
static bool jfStat(void* S,xfileinfo* I){++IOCalls;return jpStat(S,I);}
static bool jfResize(void* S,uint64 N){++IOCalls;return jpResize(S,N);}
static bool jfFlush(void* S){++IOCalls;return jpFlush(S);}
static xvfslookup jfOpen(void* C,xvfscase Case,xstrview Path,const xfileoptions* O,xvfsfile_v1* F)
{xvfslookup R=jpOpen(C,Case,Path,O,F);if(R==XVFS_LOOKUP_OPENED)F->Ops=&FaultOps;return R;}
static jpfile jfFile(void)
{
    FaultOps=jpOps;FaultOps.ReadAt=jfRead;FaultOps.WriteAt=jfWrite;FaultOps.Stat=jfStat;FaultOps.Resize=jfResize;FaultOps.Flush=jfFlush;
    xvfsprovider_v1 P=jpProvider;P.Open=jfOpen;jpfile F={0};F.Vfs=xrtVfsCreate();testRequire(F.Vfs!=NULL,"native-slot VFS");F.Mount=xrtVfsMount(F.Vfs,"/",0,XVFS_CASE_SENSITIVE,&P,&Drive,0);testRequire(F.Mount!=NULL,"native-slot mount");
    xfileoptions O;xrtFileOptionsInit(&O);O.Flags=XFILE_READ|XFILE_WRITE;F.File=xrtVfsOpen(F.Vfs,"/journal",&O);testRequire(F.File!=NULL,"native-slot file");return F;
}
/* Fail the actual platform error-slot setter at entry or restoration, with
 * both empty and populated ambient slots. No production hook is compiled. */
static void jfCase(xmqttversion V,unsigned Kind,unsigned Point,bool HasAmbient)
{
    memset(&Drive,0,sizeof(Drive));jpfile F=jfFile();xmqttjournalconfig C=jpConfig();C.Version=V;
    xmqttjournal* J=(xmqttjournal*)(uintptr_t)1;
    if(Kind){J=jpCreate(F,&C);jpAppend(J,"first",1);testRequire(xmqttJournalDestroy(J),"native-slot baseline freed");
        if(Kind==3){memset(Drive.Bytes+Drive.Size,0xee,8);Drive.Size+=8;}
        if(Kind==4)C.MaxTransactionBytes=4;
        if(Kind>1){J=jpReopen(F,&C);if(Kind==3){xmqttjournalrecord R;jpStatus(xmqttJournalNext(J,&R),XMQTT_JOURNAL_DONE,"prefix before suffix retirement");}
            if(Kind==5)testRequire(jpReplay(J)==1,"append baseline ready");}
        else J=(xmqttjournal*)(uintptr_t)1;}
    xmqttjournalinfo Before={0};if(Kind>1)Before=jpInfo(J);
    xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(32768,&Budget)&&xrtAllocationBudgetEnter(Budget,&Scope),"native-slot failure error budget");
    xerror* Ambient=HasAmbient?xrtErrorCreate(XERR_STATE,"journal.native.ambient",37,"must not be a new failure"):NULL;testRequire(!HasAmbient||Ambient,"native-slot ambient error");
    xerror* Old=NULL;testRequire(xrtTrySwapErrorTake(Ambient,&Old)&&!Old,"native-slot install baseline");Notifications=0;xrtSetErrorHandler(jfHandler,NULL);
    unsigned Calls=IOCalls,Failures=NativeFailures;uint64 Sequence=999;xmqttjournalrecord R={777,{(const unsigned char*)(uintptr_t)1,33}},Original=R;
    NativeCountdown=Point;xmqttjournalstatus S;
    if(!Kind)S=xmqttJournalCreate(F.File,&C,&J);
    else if(Kind==1)S=xmqttJournalOpen(F.File,&C,&J);
    else if(Kind==5)S=xmqttJournalAppend(J,XRT_BYTES_LITERAL("second"),&Sequence);
    else S=xmqttJournalNext(J,&R);
    testRequire(!NativeCountdown&&NativeFailures==Failures+1,"exact actual native TLS setter failure reached");
    jpStatus(S,XMQTT_JOURNAL_ERROR,"typed local error-slot failure");testRequire(Sequence==999&&!memcmp(&R,&Original,sizeof(R)),"all failed caller outputs unchanged");
    testRequire(!Notifications,"native-slot failure never re-notifies old diagnostics");
    if(Point==1){testRequire(IOCalls==Calls&&xrtGetError()==Ambient,"entry failure before IO preserves exact slot owner");
        if(Kind>1){xmqttjournalinfo After=jpInfo(J);testRequire(After.State==Before.State&&After.Sequence==Before.Sequence&&After.Effect==Before.Effect&&After.AttemptSequence==Before.AttemptSequence,"entry failure leaves cached state untouched");}}
    else {testRequire(IOCalls>Calls&&!xrtGetError(),"exit failure frees saved owner, does not invent a setter diagnostic");
        if(Kind>1){xmqttjournalinfo After=jpInfo(J);testRequire(After.State==XMQTT_JOURNAL_POISONED,"local exit failure poisons existing owner");
            if(Kind==5)testRequire(After.Effect==XMQTT_JOURNAL_COMMITTED&&After.Sequence==2&&After.AttemptSequence==2&&After.CommittedEnd==Drive.StableSize,"fully committed append retains exact effect despite local ERROR");
            if(Kind==3)testRequire(Drive.Size==Before.CommittedEnd&&Drive.StableSize==Drive.Size,"suffix retirement remains physically synced despite local ERROR");
            jpStatus(xmqttJournalNext(J,&R),XMQTT_JOURNAL_STATE,"poisoned owner requires fresh reopen");}}
    xrtSetErrorHandler(NULL,NULL);xrtClearError();if(Kind<2)testRequire(J==(xmqttjournal*)(uintptr_t)1,"failed constructor owner output untouched");else testRequire(xmqttJournalDestroy(J),"faulted owner freed");
    testRequire(xrtAllocationBudgetLeave(&Scope),"native-slot failure budget leave");xallocationbudgetinfo I={.Size=sizeof(I)};testRequire(xrtAllocationBudgetInfo(Budget,&I)&&!I.LiveCount&&!I.LiveBytes,"failed native setter loses no owning error or constructor allocation");xrtAllocationBudgetFree(Budget);
    /* A local exit failure cannot erase already synced file effects. */
    if(Kind||Point==2){C.MaxTransactionBytes=1024;J=jpReopen(F,&C);unsigned Want=!Kind?0:Kind==5&&Point==2?2:1;testRequire(jpReplay(J)==Want,"fresh owner recovers exact physical history after local bookkeeping failure");testRequire(xmqttJournalDestroy(J),"fresh recovery owner freed");}
    else testRequire(!Drive.Size,"entry failure cannot initialize any file bytes");
    jpFileClose(F);xrtClearError();
}
static int jfNative(int Argc,char** Argv)
{
    testRequire(Argc==4,"native slot failure CLI");bool Verify=!strcmp(Argv[1],"--native-verify");
    testRequire(Verify||!strcmp(Argv[1],"--native-slot-failure"),"native slot failure mode");
    xmqttjournalconfig C=jpConfig();C.Version=!strcmp(Argv[3],"311")?XMQTT_VERSION_311:XMQTT_VERSION_5;
    xfile F=xrtOpen(Argv[2],XFILE_READ|XFILE_WRITE|(Verify?0:XFILE_CREATE|XFILE_TRUNCATE));testRequire(F!=NULL,"native file open");xmqttjournal* J=NULL;
    if(Verify){jpStatus(xmqttJournalOpen(F,&C,&J),XMQTT_JOURNAL_DONE,"fresh native recovery");xmqttjournalrecord R;unsigned Count=0;xmqttjournalstatus S;
        while((S=xmqttJournalNext(J,&R))==XMQTT_JOURNAL_DONE){const char* Want=++Count==1?"first":"second";testRequire(Count<=2&&R.Sequence==Count&&R.Data.Size==strlen(Want)&&!memcmp(R.Data.Data,Want,R.Data.Size),"fresh process exact native payloads");}
        jpStatus(S,XMQTT_JOURNAL_EMPTY,"fresh native complete prefix");testRequire(Count==2,"native local ERROR did not lose fully committed transaction");}
    else {jpStatus(xmqttJournalCreate(F,&C,&J),XMQTT_JOURNAL_DONE,"native failure fixture create");jpAppend(J,"first",1);
        xerror* A=xrtErrorCreate(XERR_STATE,"native.file.ambient",21,"old caller diagnostic");testRequire(A!=NULL,"native old error");xrtSetErrorTake(A);Notifications=0;xrtSetErrorHandler(jfHandler,NULL);
        uint64 Sequence=999;unsigned Before=NativeFailures;NativeCountdown=2;xmqttjournalstatus S=xmqttJournalAppend(J,XRT_BYTES_LITERAL("second"),&Sequence);
        testRequire(!NativeCountdown&&NativeFailures==Before+1,"native file actual TLS restoration failure");jpStatus(S,XMQTT_JOURNAL_ERROR,"native committed transaction with local ERROR");
        xmqttjournalinfo I=jpInfo(J);testRequire(Sequence==999&&!Notifications&&!xrtGetError()&&I.State==XMQTT_JOURNAL_POISONED&&I.Effect==XMQTT_JOURNAL_COMMITTED&&I.Sequence==2,"native commit effect, atomic output and quiet handler");xrtSetErrorHandler(NULL,NULL);}
    testRequire(xmqttJournalDestroy(J)&&xrtClose(F),"native journal borrowed file cleanup");puts("[PASS] native synced transaction survives error-slot restoration failure and fresh-process recovery");return 0;
}
int main(int Argc,char** Argv)
{
    if(Argc>1)return jfNative(Argc,Argv);
    unsigned Cases=0;for(unsigned Version=0;Version<2;++Version)for(unsigned Kind=0;Kind<6;++Kind)for(unsigned Point=1;Point<=2;++Point)for(unsigned Ambient=0;Ambient<2;++Ambient){jfCase(Version?XMQTT_VERSION_311:XMQTT_VERSION_5,Kind,Point,Ambient!=0);++Cases;}
    printf("[PASS] %u actual native TLS setter failures; pre-IO entry, atomic outputs, committed effects, suffix sync, fresh recovery and physical refunds\n",Cases);return 0;
}
