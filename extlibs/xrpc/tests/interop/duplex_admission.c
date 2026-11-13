/* Compile this production TU probe WITHOUT the ordinary duplex.c object.
 * Interpose public encoder/atomic calls only; production has no test hooks. */
#include "../test.h"
#include <xrpc/duplex.h>
#include <xrt/thread.h>
#include <xrt/atomic.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
static bool admissionProbeWrite(ptr,size_t,const xrpcoutgoing*,size_t,bool,const xrpcencodeconfig*,size_t*);
static uint32 admissionProbeSubtract(xatomic32*,uint32,xmemoryorder);
#define xrpcEncodeWrite admissionProbeWrite
#define xrtAtomic32FetchSub admissionProbeSubtract
#ifndef XRPC_DUPLEX_PROBE_SOURCE
#define XRPC_DUPLEX_PROBE_SOURCE "../../src/duplex/duplex.c"
#endif
#include XRPC_DUPLEX_PROBE_SOURCE
#undef xrpcEncodeWrite
#undef xrtAtomic32FetchSub
typedef struct admissionProbeState {
    xrpcduplex* D;
    uint64 ProducerId;
    xatomic32 Entered,Permit,RetireEntered,RetirePermit,OtherStarted,OtherDone,OwnerStarted,OwnerDone,Wakes;
    bool FreezeWrite,FreezeRetire,FailWrite;
    int64 Timeout;
    xrpcduplexadmission Admission,OtherAdmission;
    xfuture *Future,*OtherFuture;
    xrpcduplexinfo Observed;
    xerrkind Failure;
} admissionProbeState;
static admissionProbeState AdmissionProbe;
static bool admissionProbeAwait(xatomic32* Flag,int64 Milliseconds)
{double Until=xrtTimer()+(double)Milliseconds*.001;while(!xrtAtomic32Load(Flag,XMEMORY_ACQUIRE)){if(xrtTimer()>=Until)return false;xrtSleep(0);}return true;}
static void admissionProbeInit(void)
{
    memset(&AdmissionProbe,0,sizeof(AdmissionProbe));
    xatomic32* Flags[]={&AdmissionProbe.Entered,&AdmissionProbe.Permit,&AdmissionProbe.RetireEntered,&AdmissionProbe.RetirePermit,&AdmissionProbe.OtherStarted,&AdmissionProbe.OtherDone,&AdmissionProbe.OwnerStarted,&AdmissionProbe.OwnerDone,&AdmissionProbe.Wakes};
    for(size_t N=0;N<sizeof(Flags)/sizeof(Flags[0]);++N)xrtAtomic32Init(Flags[N],0);
    AdmissionProbe.OtherAdmission=XRPC_DUPLEX_ERROR;AdmissionProbe.Timeout=XRT_WAIT_FOREVER;AdmissionProbe.Future=(xfuture*)(uintptr_t)1;AdmissionProbe.OtherFuture=(xfuture*)(uintptr_t)1;
}
static bool admissionProbeWrite(ptr Out,size_t Capacity,const xrpcoutgoing* Items,size_t Count,bool Batch,const xrpcencodeconfig* Config,size_t* Written)
{
    if(AdmissionProbe.FreezeWrite && xrtThreadCurrentId()==AdmissionProbe.ProducerId){
        xrtAtomic32Store(&AdmissionProbe.Entered,1,XMEMORY_RELEASE);
        testRequire(admissionProbeAwait(&AdmissionProbe.Permit,10000),"encoder probe release timed out");
    }
    bool OK=xrpcEncodeWrite(Out,Capacity,Items,Count,Batch,Config,Written);
    if(OK && AdmissionProbe.FailWrite && xrtThreadCurrentId()==AdmissionProbe.ProducerId){
        xrtSetErrorInfo(XERR_MEMORY,"xrt.test",1,"injected encoder refusal");return false;}
    return OK;
}
static uint32 admissionProbeSubtract(xatomic32* A,uint32 Value,xmemoryorder Order)
{
    if(AdmissionProbe.FreezeRetire && AdmissionProbe.D && A==&AdmissionProbe.D->References && xrtThreadCurrentId()==AdmissionProbe.ProducerId){
#if !defined(XRPC_DUPLEX_PROBE_BASELINE)
        xrtMutexLock(&AdmissionProbe.D->Gate);bool Constructed=AdmissionProbe.D->Constructing==0;xrtMutexUnlock(&AdmissionProbe.D->Gate);
        if(Constructed){xrtAtomic32Store(&AdmissionProbe.RetireEntered,1,XMEMORY_RELEASE);testRequire(admissionProbeAwait(&AdmissionProbe.RetirePermit,10000),"constructor retirement probe release timed out");}
#endif
    }
    return xrtAtomic32FetchSub(A,Value,Order);
}
static void admissionProbeWake(ptr User)
{(void)User;xrtAtomic32FetchAdd(&AdmissionProbe.Wakes,1,XMEMORY_RELAXED);}
static int32 admissionProbeProduce(ptr User)
{
    (void)User;AdmissionProbe.ProducerId=xrtThreadCurrentId();
    AdmissionProbe.Admission=xrpcDuplexCall(AdmissionProbe.D,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[1]"),AdmissionProbe.Timeout,&AdmissionProbe.Future);
    if(AdmissionProbe.Admission==XRPC_DUPLEX_ERROR)AdmissionProbe.Failure=xrtErrorKind(xrtGetError());
    return 0;
}
static int32 admissionProbeOther(ptr User)
{
    (void)User;xrtAtomic32Store(&AdmissionProbe.OtherStarted,1,XMEMORY_RELEASE);
    AdmissionProbe.OtherAdmission=xrpcDuplexCall(AdmissionProbe.D,XRT_STR_LITERAL("other"),XRT_STR_LITERAL("[2]"),XRT_WAIT_FOREVER,&AdmissionProbe.OtherFuture);
    xrtAtomic32Store(&AdmissionProbe.OtherDone,1,XMEMORY_RELEASE);return 0;
}
static int32 admissionProbeOwner(ptr User)
{
    (void)User;xrtAtomic32Store(&AdmissionProbe.OwnerStarted,1,XMEMORY_RELEASE);
    testRequire(xrpcDuplexClose(AdmissionProbe.D,XRPC_CALL_CLOSED) && xrpcDuplexStep(AdmissionProbe.D),"owner blocked/protocol close");
    AdmissionProbe.Observed.Size=sizeof(AdmissionProbe.Observed);testRequire(xrpcDuplexInfo(AdmissionProbe.D,&AdmissionProbe.Observed),"construction observation");
    xrtAtomic32Store(&AdmissionProbe.OwnerDone,1,XMEMORY_RELEASE);return 0;
}
static void admissionProbeJoin(xthread* Thread)
{testRequire(xrtThreadWaitFor(Thread,10000)==XWAIT_OK,"probe thread physical retirement");xrtThreadDestroy(Thread);}
static void admissionProbeFinish(void)
{
    testRequire(xrpcDuplexClose(AdmissionProbe.D,XRPC_CALL_CLOSED),"probe final close");
    double End=xrtTimer()+10;xrpcduplexinfo I={0};I.Size=sizeof(I);
    do {testRequire(xrpcDuplexStep(AdmissionProbe.D) && xrpcDuplexInfo(AdmissionProbe.D,&I) && xrtTimer()<End,"probe final drain");}while(!I.Drained);
    testRequire(!I.Operations && !I.ControlBlocks && I.Accepted==I.Completed,"probe physical accounting");
#if !defined(XRPC_DUPLEX_PROBE_BASELINE)
    testRequire(!I.Constructing && !I.TemporaryBytes && I.PeakTemporaryBytes==sizeof(xrpcoutgoing)+24u,"aggregate workspace accounting");
#endif
    xrpcduplex* D=AdmissionProbe.D;AdmissionProbe.D=NULL;testRequire(xrpcDuplexDestroy(D),"probe destroy");
    if(AdmissionProbe.Admission==XRPC_DUPLEX_ACCEPTED){
        xfutureresult R;testRequire(xrtFutureResult(AdmissionProbe.Future,&R) && R.State==XFUTURE_RESOLVED && R.Value,"reserved constructor future");
        const xrpccallresult* V=R.Value;testRequire(V->Status==(AdmissionProbe.Timeout==XRT_WAIT_FOREVER?XRPC_CALL_CLOSED:XRPC_CALL_TIMEOUT) && V->Delivery==XRPC_NOT_SENT,"entry deadline/close linearization");
        xrtFutureDestroy(AdmissionProbe.Future);
    }else testRequire(AdmissionProbe.Admission==XRPC_DUPLEX_ERROR && AdmissionProbe.Future==(xfuture*)(uintptr_t)1 && AdmissionProbe.Failure==XERR_MEMORY,"rollback lost output/diagnostic");
    if(AdmissionProbe.OtherAdmission==XRPC_DUPLEX_ACCEPTED)xrtFutureDestroy(AdmissionProbe.OtherFuture);
    xrtClearError();
}
static void admissionProbeEncoding(bool ExpectedProgress,bool Fail,bool Timeout)
{
    admissionProbeInit();AdmissionProbe.FreezeWrite=true;AdmissionProbe.FailWrite=Fail;
    if(Timeout)AdmissionProbe.Timeout=1;
    xrpcduplexconfig C;xrpcDuplexConfigInit(&C);C.MaxTemporaryBytes=sizeof(xrpcoutgoing)+24u;C.Wake=admissionProbeWake;
    testRequire(xrpcDuplexCreate(&C,&AdmissionProbe.D),"probe create");
    xthread* Producer=xrtThreadCreate(admissionProbeProduce,NULL,0);testRequire(Producer && admissionProbeAwait(&AdmissionProbe.Entered,5000),"constructor did not enter real encoder");
    if(Timeout)xrtSleep(20);
    xthread* Other=xrtThreadCreate(admissionProbeOther,NULL,0);testRequire(Other && admissionProbeAwait(&AdmissionProbe.OtherStarted,5000),"other producer start");
    bool OtherProgress=admissionProbeAwait(&AdmissionProbe.OtherDone,ExpectedProgress?5000:200);
    xthread* Owner=xrtThreadCreate(admissionProbeOwner,NULL,0);testRequire(Owner && admissionProbeAwait(&AdmissionProbe.OwnerStarted,5000),"owner probe start");
    bool OwnerProgress=admissionProbeAwait(&AdmissionProbe.OwnerDone,ExpectedProgress?5000:200);
    testRequire(OtherProgress==ExpectedProgress && OwnerProgress==ExpectedProgress,"encoding still owns the admission gate");
#if !defined(XRPC_DUPLEX_PROBE_BASELINE)
    testRequire(AdmissionProbe.OtherAdmission==XRPC_DUPLEX_OVERLOADED && AdmissionProbe.OtherFuture==(xfuture*)(uintptr_t)1,"aggregate concurrent workspace was not bounded");
    testRequire(AdmissionProbe.Observed.Constructing==1 && AdmissionProbe.Observed.Operations==1 && !AdmissionProbe.Observed.Drained && AdmissionProbe.Observed.TemporaryBytes==sizeof(xrpcoutgoing)+24u,"close forgot a reserved constructor");
#endif
    xrtAtomic32Store(&AdmissionProbe.Permit,1,XMEMORY_RELEASE);
    admissionProbeJoin(Producer);admissionProbeJoin(Other);admissionProbeJoin(Owner);
    printf("encoder_owner_progress: %u\nencoder_other_progress: %u\n",OwnerProgress?1u:0u,OtherProgress?1u:0u);admissionProbeFinish();
}
#if !defined(XRPC_DUPLEX_PROBE_BASELINE)
static void admissionProbeRetirement(void)
{
    admissionProbeInit();AdmissionProbe.FreezeRetire=true;
    xrpcduplexconfig C;xrpcDuplexConfigInit(&C);C.MaxTemporaryBytes=sizeof(xrpcoutgoing)+24u;C.Wake=admissionProbeWake;
    testRequire(xrpcDuplexCreate(&C,&AdmissionProbe.D),"retirement create");
    xthread* Producer=xrtThreadCreate(admissionProbeProduce,NULL,0);testRequire(Producer && admissionProbeAwait(&AdmissionProbe.RetireEntered,5000),"constructor did not reach post-wake physical pin");
    xthread* Owner=xrtThreadCreate(admissionProbeOwner,NULL,0);testRequire(Owner && admissionProbeAwait(&AdmissionProbe.OwnerDone,5000),"retirement owner progress");admissionProbeJoin(Owner);
    testRequire(!AdmissionProbe.Observed.Operations && !AdmissionProbe.Observed.Constructing && !AdmissionProbe.Observed.TemporaryBytes && !AdmissionProbe.Observed.Drained && isfinite(AdmissionProbe.Observed.NextDeadline) && AdmissionProbe.Observed.NextDeadline<=xrtTimer()+.01,"last physical callback pin can lose the only wake");
    xrtAtomic32Store(&AdmissionProbe.RetirePermit,1,XMEMORY_RELEASE);admissionProbeJoin(Producer);admissionProbeFinish();puts("post_wake_retirement_retry: 1");
}
#endif
#if !defined(XRPC_DUPLEX_PROBE_BASELINE)
static void admissionProbeSerialBoundary(void)
{
    admissionProbeInit();xrpcduplexconfig C;xrpcDuplexConfigInit(&C);C.MaxTemporaryBytes=sizeof(xrpcoutgoing)+24u;
    testRequire(xrpcDuplexCreate(&C,&AdmissionProbe.D),"serial boundary create");
    AdmissionProbe.D->Serial=UINT64_MAX-1u;xfuture* F=NULL;
    testRequire(xrpcDuplexCall(AdmissionProbe.D,XRT_STR_LITERAL("last"),(xstrview){0},XRT_WAIT_FOREVER,&F)==XRPC_DUPLEX_ACCEPTED && xrpcDuplexStep(AdmissionProbe.D),"last uint64 ID admission");
    xrpcduplexoutput* O=NULL;uint64 Token=0;testRequire(xrpcDuplexNext(AdmissionProbe.D,&O,&Token)==1 && xrpcDuplexStarted(AdmissionProbe.D,Token) && xrpcDuplexSent(AdmissionProbe.D,Token),"last uint64 ID send");
    testRequire(xrpcDuplexReceive(AdmissionProbe.D,XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"id\":\"18446744073709551615\",\"result\":true}"),(xbytesview){0})==XRPC_DUPLEX_ACCEPTED,"21-byte key cannot match maximum local ID");
    xfutureresult R;testRequire(xrtFutureResult(F,&R) && R.State==XFUTURE_RESOLVED && R.Value && ((const xrpccallresult*)R.Value)->Status==XRPC_CALL_OK,"maximum ID result");xrtFutureDestroy(F);xrpcDuplexOutputRelease(O);
    F=(xfuture*)(uintptr_t)1;testRequire(xrpcDuplexCall(AdmissionProbe.D,XRT_STR_LITERAL("wrap"),(xstrview){0},XRT_WAIT_FOREVER,&F)==XRPC_DUPLEX_OVERLOADED && F==(xfuture*)(uintptr_t)1,"wire ID serial wrapped");
    testRequire(xrpcDuplexNotify(AdmissionProbe.D,XRT_STR_LITERAL("note"),(xstrview){0},XRT_WAIT_FOREVER,&F)==XRPC_DUPLEX_ACCEPTED && xrpcDuplexStep(AdmissionProbe.D),"notification needs no ID credit");
    testRequire(xrpcDuplexNext(AdmissionProbe.D,&O,&Token)==1 && xrpcDuplexStarted(AdmissionProbe.D,Token) && xrpcDuplexSent(AdmissionProbe.D,Token),"notification after ID exhaustion");xrtFutureDestroy(F);xrpcDuplexOutputRelease(O);
    testRequire(xrpcDuplexClose(AdmissionProbe.D,XRPC_CALL_CLOSED) && xrpcDuplexStep(AdmissionProbe.D) && xrpcDuplexDestroy(AdmissionProbe.D),"serial boundary retirement");AdmissionProbe.D=NULL;puts("uint64_id_boundary: 1");
}
#endif
int main(int Argc,char** Argv)
{
    bool Expected=Argc>1 && atoi(Argv[1])!=0;admissionProbeEncoding(Expected,false,false);
#if !defined(XRPC_DUPLEX_PROBE_BASELINE)
    admissionProbeEncoding(Expected,true,false);admissionProbeEncoding(Expected,false,true);admissionProbeRetirement();admissionProbeSerialBoundary();
#endif
    puts("[PASS] duplex actual-TU encoder/owner/aggregate-reservation/close/rollback/deadline/physical-retirement probe");return 0;
}
