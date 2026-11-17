#if !defined(_WIN32) && !defined(_WIN64) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include <stdbool.h>
static bool LearnErrorKey,HaveErrorKey;
static _Thread_local unsigned NativeCountdown,NativeFaults,NativeWrites;
static _Thread_local bool PersistentFault,KeepFailing;
static _Thread_local bool ArmAfterReader;
static _Thread_local unsigned RestoreWriteMark;
#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <windows.h>
static DWORD ErrorKey;
static BOOL WINAPI teNativeSet(DWORD Key,PVOID Value)
{
    if(LearnErrorKey){ErrorKey=Key;HaveErrorKey=true;LearnErrorKey=false;}
    if(HaveErrorKey&&Key==ErrorKey){++NativeWrites;if(KeepFailing||(NativeCountdown&&!--NativeCountdown)){++NativeFaults;KeepFailing=PersistentFault;return FALSE;}}
    return FlsSetValue(Key,Value);
}
#define FlsSetValue teNativeSet
#else
#include <pthread.h>
#include <errno.h>
static pthread_key_t ErrorKey;
static int teNativeSet(pthread_key_t Key,const void* Value)
{
    if(LearnErrorKey){ErrorKey=Key;HaveErrorKey=true;LearnErrorKey=false;}
    if(HaveErrorKey&&Key==ErrorKey){++NativeWrites;if(KeepFailing||(NativeCountdown&&!--NativeCountdown)){++NativeFaults;KeepFailing=PersistentFault;return ENOMEM;}}
    return pthread_setspecific(Key,Value);
}
#define pthread_setspecific teNativeSet
#endif
#define XCRON_MODULE_XCRON_TZDB_FILE
#define XRT_MODULE_MEMORY_STATS
#define XRT_MODULE_THREAD
#include <xcron/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#if defined(_WIN32) || defined(_WIN64)
#undef FlsSetValue
#else
#undef pthread_setspecific
#endif
#define XCRON_IMPLEMENTATION
#include "../../../../single/extlibs/xcron.h"
#define main teNormalMain
#include "../test_tzdb_errors.c"
#undef main
static void teDisarm(void){NativeCountdown=0;PersistentFault=false;KeepFailing=false;ArmAfterReader=false;}
static bool teFailureRead(ptr Raw,xstrview Name,xbytesview* Data,ptr* Owned,xcrontzdbissue* Work)
{
    bool OK=teRead(Raw,Name,Data,Owned,Work);
    /* A diagnostic handler also restores its own protected error reference.
     * Arm only after that complete callback boundary, not at a guessed write. */
    if(OK&&ArmAfterReader){ArmAfterReader=false;RestoreWriteMark=NativeWrites;NativeCountdown=1;}
    return OK;
}
static bool teAttempt(xcrontzdb* D,unsigned Kind,xroot Root,uint64 Expected,uint64* R,xcrontzdbissue* I,tereader* Reader)
{
    xcrontzdbentry E={XRT_STR_LITERAL("UTC"),{TeUTC,sizeof(TeUTC)}};
    if(Kind<2)return xcronTzdbLoad(D,XRT_STR_LITERAL("candidate"),Kind?NULL:&E,Kind?0:1,Expected,R,I);
    xstrview Name=XRT_STR_LITERAL("UTC");if(Kind==2)return xcronTzdbLoadRoot(D,Root,XRT_STR_LITERAL("candidate"),&Name,1,Expected,R,I);
    xcrontzdbsource Source={.Names=&Name,.Count=1,.Read=teFailureRead,.Context=Reader};return xcronTzdbLoadSource(D,XRT_STR_LITERAL("candidate"),&Source,Expected,R,I);
}
static void teFailure(unsigned Kind,bool Initial,bool HasAmbient,unsigned Point,bool Persistent,bool HasIssue)
{
    xcrontzdb* D=teCreate(2);uint64 R=777;xcrontzdbentry E={XRT_STR_LITERAL("UTC"),{TeUTC,sizeof(TeUTC)}};xcrontzdbsnapshot* Old=NULL;
    if(Initial){testRequire(xcronTzdbLoad(D,XRT_STR_LITERAL("before"),&E,1,0,&R,NULL)&&R==1,"preexisting generation");Old=teAcquire(D);}
    tefiles Files={0};if(Kind==2)Files=teFiles();xcrontzdbstats Before=teStats(D);tereader Reader={.Notice=true};
    xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(65536,&Budget)&&xrtAllocationBudgetEnter(Budget,&Scope),"caller-owned ambient error allocation ledger");
    teobserver O={.DB=D,.Caller=xrtThreadCurrentId()};if(HasAmbient)teAmbient(&O,D);else xrtSetErrorHandler(teNotice,&O);
    xcrontzdbissue I,Keep;memset(&I,0x5a,sizeof(I));Keep=I;R=777;unsigned Faults=NativeFaults,Writes=NativeWrites;
    unsigned ExpectedWrites=Point==1?1:Kind==3?0:2;PersistentFault=Persistent;NativeCountdown=ExpectedWrites;ArmAfterReader=Point==2&&Kind==3;
    bool OK=teAttempt(D,Kind,Files.Root,Initial?1:0,&R,HasIssue?&I:NULL,&Reader);
    if(Point==2&&Kind==3)ExpectedWrites=RestoreWriteMark-Writes+1;
    if(OK||R!=777||NativeFaults!=Faults+1||NativeWrites!=Writes+ExpectedWrites||NativeCountdown){fprintf(stderr,"native case kind%u initial%u ambient%u point%u persistent%u issue%u: ok%u revision%llu faults%u writes%u expected%u remaining%u code%d\n",Kind,Initial,HasAmbient,Point,Persistent,HasIssue,OK,(unsigned long long)R,NativeFaults-Faults,NativeWrites-Writes,ExpectedWrites,NativeCountdown,HasIssue?(int)I.Code:-1);}
    testRequire(!OK&&R==777&&NativeFaults==Faults+1&&NativeWrites==Writes+ExpectedWrites&&!NativeCountdown&&!ArmAfterReader,"actual native error-slot failure yields no success, output publication or recursive setter retry");
    if(HasIssue)testRequire(I.Code==XCRON_TZDB_SYSTEM&&I.Index==XCRON_TZDB_NO_INDEX&&!I.Zone.Code&&!I.Cause&&!I.SystemCode,"typed local SYSTEM without invented parser/IO cause");else testRequire(!memcmp(&I,&Keep,sizeof(I)),"optional issue remains untouched");
    xcrontzdbstats After=teStats(D);testRequire(After.Revision==Before.Revision&&After.Names==Before.Names&&!After.Loading,"failed entry/restore never publishes or consumes a builder credit");
    testRequire(!O.Old&&O.Fresh==(Point==2&&Kind==3?1u:0u),"native slot failure never repeats old or publishes a synthetic error");
    if(Point==1)testRequire(xrtGetError()==O.Ambient&&!Reader.Calls&&After.LiveBytes==Before.LiveBytes,"failed scope entry precedes source work and retains exact slot owner");
    else if(Kind==3)testRequire(Reader.Calls==1&&xrtErrorFind(xrtGetError(),"cron.reader.notice",1),"failed restoration keeps current provisional diagnostic owned");
    else testRequire(!xrtGetError(),"restoration failure may leave no diagnostic");
    xallocationbudgetinfo BI={.Size=sizeof(BI)};testRequire(xrtAllocationBudgetInfo(Budget,&BI)&&BI.LiveCount==(Point==1&&HasAmbient?1u:0u),"saved ambient owner is retained on entry failure and freed on exit failure");
    teDisarm();xrtSetErrorHandler(NULL,NULL);xrtClearError();testRequire(xrtAllocationBudgetLeave(&Scope)&&xrtAllocationBudgetInfo(Budget,&BI)&&!BI.LiveCount&&!BI.LiveBytes&&!BI.Rejections,"every ambient error reference physically refunded");xrtAllocationBudgetFree(Budget);
    testRequire(teStats(D).LiveBytes==Before.LiveBytes,"candidate, file buffer and provisional diagnostic fully refunded");
    if(Old){xcrontzdbsnapshot* Current=teAcquire(D);testRequire(Current==Old&&teOffset(Old)==0,"failed reload keeps the identical retained snapshot");xcronTzdbSnapshotRelease(Current);}
    testRequire(teAttempt(D,Kind,Files.Root,Initial?1:0,&R,&I,&Reader)&&R==(Initial?2u:1u),"retry on same manager succeeds with no skipped revision or leaked load slot");
    teEnd(D);if(Old){testRequire(teOffset(Old)==0,"old generation survives later retry and manager destruction");xcronTzdbSnapshotRelease(Old);}if(Kind==2)teFilesEnd(Files);xrtClearError();
}
static void teBound(bool HasAmbient)
{
    xcrontzdb* D=teCreate(1);xrt_error_context Context={0};xrt_error_context* Prior=__xrtErrorContextSwap(&Context);testRequire(!xrtGetError(),"fresh existing scheduler-style bound context");
    teobserver O={.DB=D,.Caller=xrtThreadCurrentId()};if(HasAmbient)teAmbient(&O,D);else xrtSetErrorHandler(teNotice,&O);uint64 R=777;unsigned Writes=NativeWrites,Faults=NativeFaults;NativeCountdown=1;PersistentFault=true;
    testRequire(teAttempt(D,0,NULL,0,&R,NULL,NULL)&&R==1&&NativeWrites==Writes&&NativeFaults==Faults&&NativeCountdown==1&&xrtGetError()==O.Ambient&&!O.Old&&!O.Fresh,"bound-context reload needs no native error-slot setter and stays quiet");
    teDisarm();xrtSetErrorHandler(NULL,NULL);xrtClearError();testRequire(__xrtErrorContextSwap(Prior)==&Context&&!Context.Error&&!xrtGetError(),"restore outer execution context without borrowed references");teEnd(D);
}
typedef struct tegate {xcrontzdb* D;xatomic32 Entered,Go;bool Fault,OK;uint64 Revision;xcrontzdbissue Issue;} tegate;
static void teWait(xatomic32* A)
{double End=xrtTimer()+15;while(!xrtAtomic32Load(A,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"controlled loader rendezvous");xrtThreadYield();}}
static bool teGateRead(ptr Raw,xstrview Name,xbytesview* Data,ptr* Owned,xcrontzdbissue* Work)
{(void)Name;(void)Work;tegate* G=Raw;xrtAtomic32Store(&G->Entered,1,XMEMORY_RELEASE);teWait(&G->Go);*Data=(xbytesview){TeUTC,sizeof(TeUTC)};*Owned=NULL;return true;}
static int32 teWorker(ptr Raw)
{
    tegate* G=Raw;xrtSetErrorInfo(XERR_STATE,"worker.ambient",1,"loader owner");xstrview Name=XRT_STR_LITERAL("UTC");xcrontzdbsource Source={.Names=&Name,.Count=1,.Read=teGateRead,.Context=G};unsigned Faults=NativeFaults;
    if(G->Fault){NativeCountdown=2;PersistentFault=true;}G->OK=xcronTzdbLoadSource(G->D,XRT_STR_LITERAL("gated"),&Source,1,&G->Revision,&G->Issue);
    if(G->Fault){testRequire(!G->OK&&G->Issue.Code==XCRON_TZDB_SYSTEM&&NativeFaults==Faults+1,"concurrent candidate actually failed its native restoration");}
    teDisarm();xrtClearError();return 0;
}
static void teJoin(xthread* T)
{testRequire(xrtThreadWaitFor(T,15000)==XWAIT_OK&&!xrtThreadExitCode(T),"physical loader join");xrtThreadDestroy(T);}
static void teConcurrent(bool Close)
{
    xcrontzdb* D=teCreate(2);xcrontzdbentry E={XRT_STR_LITERAL("UTC"),{TeUTC,sizeof(TeUTC)}};uint64 R=0;testRequire(xcronTzdbLoad(D,XRT_STR_LITERAL("before"),&E,1,0,&R,NULL),"concurrent initial revision");xcrontzdbsnapshot* Old=teAcquire(D);
    tegate A={.D=D,.Revision=777},B={.D=D,.Revision=777,.Fault=true};xrtAtomic32Init(&A.Entered,0);xrtAtomic32Init(&A.Go,0);xrtAtomic32Init(&B.Entered,0);xrtAtomic32Init(&B.Go,0);
    xthread* TA=xrtThreadCreate(teWorker,&A,0),*TB=xrtThreadCreate(teWorker,&B,0);testRequire(TA&&TB,"two admitted builders");teWait(&A.Entered);teWait(&B.Entered);testRequire(teStats(D).Loading==2,"both cold sources outside publication lock");
    uint64 Keep=888;xcrontzdbissue I;testRequire(!xcronTzdbLoad(D,XRT_STR_LITERAL("full"),&E,1,1,&Keep,&I)&&I.Code==XCRON_TZDB_LIMIT&&Keep==888,"no queued third builder");xrtClearError();xrtAtomic32Store(&B.Go,1,XMEMORY_RELEASE);teJoin(TB);
    testRequire(B.Revision==777&&teStats(D).Revision==1&&teStats(D).Loading==1&&teOffset(Old)==0,"failed restoration refunds only its own credit and leaves current publication readable");
    if(Close){testRequire(xcronTzdbClose(D,NULL)&&!xcronTzdbDestroy(D,&I)&&I.Code==XCRON_TZDB_BUSY,"close still waits for surviving physical builder");xrtClearError();}
    else testRequire(xcronTzdbLoad(D,XRT_STR_LITERAL("winner"),&E,1,1,&R,NULL)&&R==2,"freed failed-builder slot admits an actual CAS winner");
    xrtAtomic32Store(&A.Go,1,XMEMORY_RELEASE);teJoin(TA);testRequire(!A.OK&&A.Revision==777&&A.Issue.Code==(Close?XCRON_TZDB_CLOSED:XCRON_TZDB_STALE)&&!teStats(D).Loading,"surviving builder observes real close/CAS order without stale overwrite");
    if(Close)testRequire(xcronTzdbDestroy(D,NULL),"destroy only after all physical joins");else teEnd(D);testRequire(teOffset(Old)==0,"old reader survives all concurrent outcomes");xcronTzdbSnapshotRelease(Old);xrtClearError();
}
int main(void)
{
    xerror* Old=NULL;LearnErrorKey=true;testRequire(xrtTrySwapErrorTake(NULL,&Old)&&HaveErrorKey&&!LearnErrorKey&&!Old,"learn only the real native error key, excluding heap/budget/context keys");
    testRequire(!teNormalMain(),"normal cases also pass with native shim inactive");unsigned Cases=0;
    for(unsigned Kind=0;Kind<4;++Kind)for(unsigned Initial=0;Initial<2;++Initial)for(unsigned Ambient=0;Ambient<2;++Ambient)for(unsigned Point=1;Point<=2;++Point)for(unsigned Persistent=0;Persistent<2;++Persistent)for(unsigned Issue=0;Issue<2;++Issue){teFailure(Kind,Initial!=0,Ambient!=0,Point,Persistent!=0,Issue!=0);++Cases;}
    teBound(false);teBound(true);teConcurrent(false);teConcurrent(true);printf("[PASS] %u native error-key setter failure cases plus2 bound contexts and2 actual concurrent rollback/CAS/close races\n",Cases);return 0;
}
