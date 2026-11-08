#include "test.h"
#include <xrt/thread.h>
#include <xrt/atomic.h>
#include <xrt/memory.h>
typedef struct selectedexeccontext {
    xcronexecutor* Adapter;xatomic32 Started,Gate,NotifyGate,NotifyEntered,Notified,Reentry;
    xatomic64 Calls;int Mode;bool Block,BlockNotify,Probe,ProbeNotify,Plain,Real;
    xcronexecutionlabel Expected;
} selectedexeccontext;
static void seWait(const xatomic32* V,uint32 N,cstr Text)
{double End=xrtTimer()+10;while(xrtAtomic32Load(V,XMEMORY_ACQUIRE)<N){testRequire(xrtTimer()<End,Text);xrtThreadYield();}}
static xexecutor* sePool(uint32 Threads,size_t Queue)
{xexecutorconfig C={Threads,Queue,0};xexecutor* E=xrtExecutorCreate(&C);testRequire(E!=NULL,"selected execution pool");return E;}
static void sePoolEnd(xexecutor* E)
{testRequire(xrtExecutorClose(E) && xrtExecutorWaitFor(E,10000)==XWAIT_OK && xrtExecutorDestroy(E),"selected execution pool physical join");}
static xcronselectedrun seLease(uint64 Id,uint64 Job)
{
    xcronselectedrun R={.Run={.Id=Id,.Job=Job,.ScheduledAt=(xtime)Id*1000,.Through=(xtime)Id*1000,.Observed={(xtime)Id*1000,Id}},.Candidates=1+Id%258};
    xrtTimeSplit(R.Run.ScheduledAt,&R.Local);R.Local.Offset=0;R.Local.IsDST=-1;return R;
}
static void seLabel(const xcronrun* R,const xcronexecutionlabel* L)
{
    xdatetime D;xrtTimeSplit(R->ScheduledAt,&D);D.Offset=0;D.IsDST=-1;
    testRequire(L && L->Candidates==1+R->Id%258 && !memcmp(&D,&L->Local,sizeof(D)),"worker reads the immutable complete nominal snapshot for its own lease");
}
static xcronrunresult seProc(const xcronrun* R,const xcronexecution* T,ptr Raw)
{
    selectedexeccontext* C=Raw;const xcronexecutionlabel* L=xcronExecutionSelected(T);
    if(C->Plain)testRequire(!L,"ordinary callback has no selected view");
    else if(C->Real)testRequire(L && !memcmp(L,&C->Expected,sizeof(*L)) && R->Data && *(int*)R->Data==42,"actual selected lease metadata and borrowed Data reach the worker");
    else seLabel(R,L);
    xrtAtomic64FetchAdd(&C->Calls,1,XMEMORY_RELAXED);xrtAtomic32FetchAdd(&C->Started,1,XMEMORY_RELEASE);
    if(C->Probe){xcronexecutorstats S;xcronexecutorissue I;testRequire(!xcronExecutorInspect(C->Adapter,&S,&I) && I.Code==XCRON_EXECUTOR_THREAD,"selected worker cannot enter owner APIs");xrtClearError();}
    if(C->Block)seWait(&C->Gate,1,"selected worker released");
    if(C->Real)testRequire(!memcmp(L,&C->Expected,sizeof(*L)) && *(int*)R->Data==42,"actual selected view/Data remain live through callback tails");
    else if(!C->Plain)seLabel(R,L);
    if(C->Mode==1){xrtSetErrorInfo(XERR_IO,"test.cron.selected.worker",27,"selected worker failure");return XCRON_RUN_FAILED;}
    if(C->Mode==2)return (xcronrunresult)99;
    return C->Mode==3 && xcronExecutionCancelled(T)?XCRON_RUN_CANCELLED:XCRON_RUN_SUCCEEDED;
}
static void seNotify(ptr Raw)
{
    selectedexeccontext* C=Raw;xrtAtomic32FetchAdd(&C->Notified,1,XMEMORY_RELEASE);xrtAtomic32Store(&C->NotifyEntered,1,XMEMORY_RELEASE);
    if(C->ProbeNotify){xcronexecutorstats S;xcronexecutorissue I;testRequire(!xcronExecutorInspect(C->Adapter,&S,&I) && I.Code==XCRON_EXECUTOR_REENTRY,"selected owner notification reentry rejected");xrtClearError();xcronselectedrun R=seLease(1000001,1);
        testRequire(!xcronExecutorSubmitBatchSelected(C->Adapter,&R,1,&I) && I.Code==XCRON_EXECUTOR_REENTRY,"selected batch owner Notify reentry rejected");xrtClearError();xrtAtomic32FetchAdd(&C->Reentry,1,XMEMORY_RELEASE);}
    if(C->BlockNotify)seWait(&C->NotifyGate,1,"selected notification tail released");
}
static xcronexecutor* seAdapter(xexecutor* E,uint32 Slots,selectedexeccontext* C,bool Notify)
{xcronexecutor* B=NULL;xcronexecutorconfig Config={E,seProc,C,Notify?seNotify:NULL,C,Slots,0,SIZE_MAX};testRequire(xcronExecutorCreateSelected(&Config,&B,NULL),"selected adapter factory");C->Adapter=B;return B;}
static void seEnd(xcronexecutor* B)
{xcronexecutorstats S;double End=xrtTimer()+10;do{testRequire(xcronExecutorInspect(B,&S,NULL) && !S.Active,"selected adapter fully drained");if(S.Producers){testRequire(xrtTimer()<End,"selected publisher tails finish");xrtThreadYield();}}while(S.Producers);testRequire(xcronExecutorDestroy(B,NULL),"selected adapter destroyed after tails");}
static xcronselectedcompletion seReceipt(xcronexecutor* B)
{xcronselectedcompletion C;size_t N;double End=xrtTimer()+10;do{testRequire(xcronExecutorDrainSelected(B,&C,1,&N,NULL),"selected receipt drain");if(!N){testRequire(xrtTimer()<End,"selected receipt deadline");xrtThreadYield();}}while(!N);return C;}
static void seSame(const xcronselectedrun* A,const xcronselectedrun* B)
{testRequire(A->Run.Id==B->Run.Id && A->Run.Job==B->Run.Job && A->Run.Data==B->Run.Data && A->Run.ScheduledAt==B->Run.ScheduledAt && A->Run.Through==B->Run.Through && A->Run.Observed.Wall==B->Run.Observed.Wall && A->Run.Observed.MonotonicMilliseconds==B->Run.Observed.MonotonicMilliseconds && A->Run.Flags==B->Run.Flags && A->Candidates==B->Candidates && !memcmp(&A->Local,&B->Local,sizeof(A->Local)),"all semantic run and nominal receipt fields survive copying and slot reuse");}
static void testSESnapshotAndResults(void)
{
    testRequire(!xcronExecutionSelected(NULL),"NULL selected view");xexecutor* E=sePool(1,4);selectedexeccontext C={.Block=true,.Probe=true};xcronexecutor* B=seAdapter(E,1,&C,false);
    for(unsigned Mode=0;Mode<3;++Mode){C.Mode=Mode;xrtAtomic32Store(&C.Gate,0,XMEMORY_RELAXED);xcronselectedrun R=seLease(Mode+1,7),Original=R;int Data=41;R.Run.Data=Original.Run.Data=&Data;
        xrtSetErrorInfo(XERR_STATE,"selected.owner",3,"preserve owner error");const xerror* Ambient=xrtGetError();testRequire(xcronExecutorSubmitSelected(B,&R,NULL),"selected callback submission");seWait(&C.Started,Mode+1,"selected callback entered");memset(&R,0xa5,sizeof(R));xrtAtomic32Store(&C.Gate,1,XMEMORY_RELEASE);
        xcronselectedcompletion D=seReceipt(B);seSame(&D.Run,&Original);testRequire(xrtGetError()==Ambient,"selected worker error does not consume owner ambient error");
        if(!Mode)testRequire(D.Result==XCRON_RUN_SUCCEEDED && D.Code==XCRON_EXECUTOR_OK && !D.Error,"selected success completion");
        else if(Mode==1)testRequire(D.Result==XCRON_RUN_FAILED && D.Code==XCRON_EXECUTOR_OK && xrtErrorCode(D.Error)==27 && !strcmp(xrtErrorDomain(D.Error),"test.cron.selected.worker"),"selected failure transfers worker error ownership");
        else testRequire(D.Result==XCRON_RUN_FAILED && D.Code==XCRON_EXECUTOR_RESULT && !D.Error,"invalid selected result does not borrow another worker error");
        xrtErrorFree(D.Error);xrtClearError();
    }
    seEnd(B);C=(selectedexeccontext){.Block=true};B=seAdapter(E,1,&C,false);xcronselectedrun R=seLease(30,7);testRequire(xcronExecutorSubmitSelected(B,&R,NULL),"active selected close fixture");seWait(&C.Started,1,"active selected close body entered");testRequire(xcronExecutorClose(B,true,NULL),"selected close retains active label storage");xcronexecutorissue I;testRequire(!xcronExecutorDestroy(B,&I) && I.Code==XCRON_EXECUTOR_BUSY,"active selected close cannot destroy borrowed view");xrtClearError();xrtAtomic32Store(&C.Gate,1,XMEMORY_RELEASE);xcronselectedcompletion Closed=seReceipt(B);seSame(&Closed.Run,&R);testRequire(Closed.Result==XCRON_RUN_SUCCEEDED && !Closed.Error,"late cancellation does not rewrite a selected success");seEnd(B);
    C=(selectedexeccontext){.Plain=true};xcronexecutorconfig Config={E,seProc,&C,NULL,NULL,1,0,SIZE_MAX};testRequire(xcronExecutorCreate(&Config,&B,NULL),"ordinary adapter under extended build");C.Adapter=B;R=seLease(4,7);memset(&I,0x5a,sizeof(I));xcronexecutorissue Before=I;size_t Count=77;xcronselectedcompletion Wrong;
    testRequire(!xcronExecutorSubmitSelected(B,&R,&I) && !memcmp(&I,&Before,sizeof(I)),"selected submit on ordinary adapter preserves outputs");xrtClearError();testRequire(!xcronExecutorDrainSelected(B,&Wrong,1,&Count,&I) && Count==77 && !memcmp(&I,&Before,sizeof(I)),"selected drain on ordinary adapter preserves outputs");xrtClearError();
    testRequire(xcronExecutorSubmit(B,&R.Run,NULL),"ordinary submit retains its callback contract");xcroncompletion D;size_t N=0;while(!N)testRequire(xcronExecutorDrain(B,&D,1,&N,NULL),"ordinary extended-build drain");testRequire(D.Result==XCRON_RUN_SUCCEEDED && !D.Error,"ordinary callback remained unaffected");seEnd(B);sePoolEnd(E);
}
#include "executor_selected_cases.h"
int main(void)
{testSESnapshotAndResults();testSEQueueAndTail();testSEOwnerAndStress();testSEKernelAndLimits();testSEBatches();puts("[PASS] selected executor: immutable views, physical receipts, cancellation/rejection/notify reuse,16384 concurrent labels, kernel/OOM and4096 no-heap lifecycles; atomic64-lease batches, prefix rollback and32768 warm batch receipts");return 0;}
