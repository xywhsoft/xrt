"""Exercise production Client construction/free and sequence boundaries, no installed hooks."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,sys
ROOT=Path(__file__).resolve().parents[4]
DRIVER=r'''
#include <xredis.h>
#include <xrt.h>
#include <stdio.h>
static void* boundaryCalloc(size_t Count,size_t Bytes);
static void boundaryFree(void* Pointer);
#define xrtCalloc boundaryCalloc
#define xrtFree boundaryFree
#include "@CLIENT@"
#undef xrtCalloc
#undef xrtFree
#define XREDIS_TRACKING_TEST_MAIN trackingBoundaryBaseTests
#include "@TEST@"
typedef struct boundaryProbe {
    xatomic32 Armed,Entered,FreeEntered; xatomicptr Allocation;
    xmutex Allocate,Free;size_t Bytes;bool HoldFree;
} boundaryProbe;
static boundaryProbe Probe;
static void* boundaryCalloc(size_t Count,size_t Bytes)
{
    if(xrtAtomic32Load(&Probe.Armed,XMEMORY_ACQUIRE) && Count==1 && Bytes==Probe.Bytes){
        xrtAtomic32Store(&Probe.Entered,1,XMEMORY_RELEASE);xrtMutexLock(&Probe.Allocate);xrtMutexUnlock(&Probe.Allocate);
        void* P=xrtCalloc(Count,Bytes);xrtAtomicPtrStore(&Probe.Allocation,P,XMEMORY_RELEASE);return P;
    }
    return xrtCalloc(Count,Bytes);
}
static void boundaryFree(void* P)
{
    if(P && xrtAtomic32Load(&Probe.Armed,XMEMORY_ACQUIRE) && Probe.HoldFree && P==xrtAtomicPtrLoad(&Probe.Allocation,XMEMORY_ACQUIRE)){
        xrtAtomic32Store(&Probe.FreeEntered,1,XMEMORY_RELEASE);xrtMutexLock(&Probe.Free);xrtMutexUnlock(&Probe.Free);
    }
    xrtFree(P);
}
typedef struct boundaryInstall {xredisclient* Client;xallocationbudget* Budget;xredisadmission Admission;} boundaryInstall;
static int32 boundaryInstaller(ptr Data)
{
    boundaryInstall* I=Data;xallocationscope Scope={0};testRequire(xrtAllocationBudgetEnter(I->Budget,&Scope),"installation budget enter");
    I->Admission=xredisClientTrackingObserve(I->Client,0);testRequire(xrtAllocationBudgetLeave(&Scope),"installation budget leave");xrtClearError();xredisClientRelease(I->Client);return 0;
}
typedef struct boundarySnapshot {xredisclient* Client;xatomic32 Done,Quiescent;} boundarySnapshot;
static void boundaryCheck(xnetworker* Worker,ptr Data)
{
    (void)Worker;boundarySnapshot* S=Data;xredisclient* C=S->Client;xrtMutexLock(&C->Gate);
    bool OtherDone=C->TransportClosed && !C->Info.Requests && !C->Info.InitBytes && !C->InitTimer && !C->WakeQueued;
    xrtMutexUnlock(&C->Gate);xrtAtomic32Store(&S->Quiescent,OtherDone,XMEMORY_RELEASE);xrtAtomic32FetchAdd(&S->Done,1,XMEMORY_RELEASE);
}
static void boundaryClose(bool OOM)
{
    redispeer P;redisPeerStart(&P);xredisclient* C=testClientConnect(&P,NULL);xfuture* Ready=xredisClientReady(C);testRequire(xrtFutureWaitFor(Ready,5000)==XWAIT_OK,"boundary ready");xrtFutureDestroy(Ready);
    Probe.Bytes=sizeof(xredisobservation)+C->Config.MaxPushes*sizeof(uint64);Probe.HoldFree=!OOM;
    xrtAtomic32Store(&Probe.Entered,0,XMEMORY_RELAXED);xrtAtomic32Store(&Probe.FreeEntered,0,XMEMORY_RELAXED);xrtAtomicPtrStore(&Probe.Allocation,NULL,XMEMORY_RELAXED);
    testRequire(xrtMutexLock(&Probe.Allocate),"hold real Client allocation");xrtAtomic32Store(&Probe.Armed,1,XMEMORY_RELEASE);
    boundaryInstall I={C,NULL,XREDIS_ADMISSION_ACCEPTED};testRequire(xrtAllocationBudgetCreate(OOM?1:65536,&I.Budget) && xredisClientRetain(C),"installation owner");xthread* T=xrtThreadCreate(boundaryInstaller,&I,0);testRequire(T!=NULL,"installation thread");redisPeerWait(&Probe.Entered,1);
    xredistrackinginfo Info={0};Info.Size=sizeof(Info);testRequire(xredisClientTrackingInfo(C,&Info) && Info.Preparing && Info.Reserved==Probe.Bytes && !Info.Bytes,"reserved allocation precedes allocation/publication");
    xrtSetErrorKind(XERR_MEMORY);const xerror* Error=xrtGetError();testRequire(xredisClientTrackingObserve(C,0)==XREDIS_ADMISSION_OVERLOADED && xrtGetError()==Error,"concurrent installation is counted and never allocates a second block");xrtClearError();
    xfuture* Closed=xredisClientClosed(C);testRequire(xredisClientClose(C),"close during real allocation");
    boundarySnapshot S={C,{0},{0}};xrtAtomic32Init(&S.Done,0);xrtAtomic32Init(&S.Quiescent,0);xnetpost* Posts=calloc(500,sizeof(*Posts));testRequire(Posts!=NULL,"snapshot posts");
    for(unsigned N=0;N<500 && !xrtAtomic32Load(&S.Quiescent,XMEMORY_ACQUIRE);++N){testRequire(!xrtFutureDone(Closed),"physical close cannot overtake an in-progress allocation");
        testRequire(xrtNetPostInit(Posts+N) && xrtNetPost(xrtNetEngineWorker(P.Engine,0),Posts+N,boundaryCheck,&S),"owner snapshot");redisPeerWait(&S.Done,N+1);if(!xrtAtomic32Load(&S.Quiescent,XMEMORY_ACQUIRE))xrtSleep(10);}
    testRequire(xrtAtomic32Load(&S.Quiescent,XMEMORY_ACQUIRE) && xrtFutureWaitFor(Closed,20)==XWAIT_TIMEOUT,"only observation construction keeps physical close pending");
    if(!OOM)testRequire(xrtMutexLock(&Probe.Free),"hold actual free after all Client lock queries");
    xrtMutexUnlock(&Probe.Allocate);
    if(!OOM){redisPeerWait(&Probe.FreeEntered,1);testRequire(xrtFutureWaitFor(Closed,20)==XWAIT_TIMEOUT,"unpublished allocation must be freed before physical close");xrtMutexUnlock(&Probe.Free);}
    testRequire(xrtThreadWaitFor(T,5000)==XWAIT_OK,"installer join");xrtThreadDestroy(T);testRequire(I.Admission==(OOM?XREDIS_ADMISSION_ERROR:XREDIS_ADMISSION_CLOSED),"exact terminal installation outcome");
    testRequire(xrtFutureWaitFor(Closed,5000)==XWAIT_OK && xredisClientTrackingInfo(C,&Info) && !Info.Enabled && !Info.Preparing && !Info.Reserved && !Info.Bytes,"all construction duties retired");xrtAtomic32Store(&Probe.Armed,0,XMEMORY_RELEASE);
    testClientClose(&P,C);xrtFutureDestroy(Closed);free(Posts);xallocationbudgetinfo B={0};B.Size=sizeof(B);testRequire(xrtAllocationBudgetInfo(I.Budget,&B) && !B.LiveBytes && !B.LiveCount,"construction/free scopes return to zero");xrtAllocationBudgetFree(I.Budget);
}
static void boundaryCounter(void)
{
    redispeer P;redisPeerStart(&P);xredisclient* C=testClientConnect(&P,NULL);testRequire(xredisClientTrackingObserve(C,0)==XREDIS_ADMISSION_ACCEPTED,"counter observer");
    xredisobservation* O=xrtAtomicPtrLoad(&C->Observation,XMEMORY_ACQUIRE);xbytesview A=XRT_BYTES_LITERAL("PING");
    testRequire(xredisClientTrackingCallAsync(C,&A,1,100,(xfuture**)O)==XREDIS_ADMISSION_ERROR && !xrtAtomic64Load(&O->Sequence,XMEMORY_ACQUIRE),"observed output cannot overwrite private allocation");xrtClearError();
    trackingGate G={0};xrtAtomic32Init(&G.Entered,0);xrtAtomic32Init(&G.Left,0);testRequire(xrtMutexInit(&G.Gate) && xrtMutexLock(&G.Gate) && xrtNetPostInit(&G.Post) && xrtNetPost(xrtNetEngineWorker(P.Engine,0),&G.Post,trackingBlocked,&G),"hold counter writer");redisPeerWait(&G.Entered,1);
    xrtAtomic64Store(&O->Sequence,UINT64_MAX-1,XMEMORY_RELEASE);xfuture* Last=trackingSubmit(C,"PING",5000);xrtMutexUnlock(&G.Gate);redisPeerWait(&G.Left,1);xrtMutexUnit(&G.Gate);
    testRequire(trackingResult(Last)->Request.Status==XREDIS_REQUEST_OK && trackingResult(Last)->Sequence==UINT64_MAX,"last sequence remains representable");xfuture* Overflow=trackingSubmit(C,"PING",5000);
    testRequire(trackingResult(Overflow)->Request.Status==XREDIS_REQUEST_LIMIT && !trackingResult(Overflow)->Sequence,"counter exhaustion cannot publish an unordered reply");
    testRequire(xrtAtomic64Load(&O->Sequence,XMEMORY_ACQUIRE)==UINT64_MAX,"counter never wraps");testClientClose(&P,C);xrtFutureDestroy(Last);xrtFutureDestroy(Overflow);
}
int main(void)
{
    xrtAtomic32Init(&Probe.Armed,0);xrtAtomic32Init(&Probe.Entered,0);xrtAtomic32Init(&Probe.FreeEntered,0);xrtAtomicPtrInit(&Probe.Allocation,NULL);
    testRequire(xrtMutexInit(&Probe.Allocate) && xrtMutexInit(&Probe.Free),"probe gates");trackingBoundaryBaseTests();boundaryClose(false);boundaryClose(true);boundaryCounter();
    xrtMutexUnit(&Probe.Allocate);xrtMutexUnit(&Probe.Free);
    printf("layout_client: %zu\nlayout_request: %zu\nlayout_wire_offset: %zu\nlayout_observer_header: %zu\nlayout_observed_result: %zu\n",sizeof(xredisclient),sizeof(xredisrequest),offsetof(xredisrequest,Wire),sizeof(xredisobservation),sizeof(xredistrackingresult));
    puts("[PASS] production allocation/free close barriers, real scoped OOM, private output aliases and uint64 exhaustion");return 0;
}
'''
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--compiler',default='gcc');parser.add_argument('--out',type=Path,required=True);parser.add_argument('--sanitize');args=parser.parse_args()
 sys.path.insert(0,str(ROOT/'tools'));import build
 out=args.out.absolute();out.mkdir(parents=True,exist_ok=True);arch='native' if os.name=='nt' else 'x64'
 sources,_,_,_,defines,links,_,_,includes,headers=build._load_modules('xredis_tracking,memory_stats,time',[ROOT/'extlibs/xredis/config/modules.json'])
 flags=['-O2']
 for name in includes:flags+=['-I',str(ROOT/name)]
 if args.sanitize:
  flags+=['-g','-fno-omit-frame-pointer','-fsanitize='+args.sanitize]
  if args.sanitize=='thread':flags+=['-Wno-error=tsan']
 objects=build._compile_objects(args.compiler,arch,sources,defines,out/'obj',False,flags,headers)
 source=ROOT/'extlibs/xredis/src/client/client.c';unit=ROOT/'extlibs/xredis/tests/test_tracking.c';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();before=sha(source)
 driver=out/'driver.c';driver.write_text(DRIVER.replace('@CLIENT@',source.as_posix()).replace('@TEST@',unit.as_posix()),encoding='utf-8')
 binary=out/('driver.exe' if os.name=='nt' else 'driver');objs=[o for s,o in zip(sources,objects) if s!='extlibs/xredis/src/client/client.c']
 command=[args.compiler,*build._compile_options(args.compiler,arch,defines,flags),str(driver),*[str(o) for o in objs],*['-l'+n for n in links],'-o',str(binary)]
 if args.sanitize:command+=['-fsanitize='+args.sanitize]
 with (out/'build.log').open('w',encoding='utf-8') as stream:subprocess.run(command,stdout=stream,stderr=subprocess.STDOUT,check=True,cwd=ROOT,timeout=1800)
 env=dict(os.environ)
 if args.sanitize=='address,undefined':env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
 if args.sanitize=='thread':env.update(TSAN_OPTIONS='halt_on_error=1')
 with (out/'run.log').open('w',encoding='utf-8') as stream:p=subprocess.run([str(binary)],stdout=stream,stderr=subprocess.STDOUT,env=env,cwd=ROOT,timeout=120)
 assert p.returncode==0,(out/'run.log').read_text(encoding='utf-8',errors='replace')[-3500:]
 assert sha(source)==before
 (out/'result.json').write_text(json.dumps({'passed':True,'command':command,'source_sha256':before,'driver_sha256':sha(driver),'binary_sha256':sha(binary),'run_log_sha256':sha(out/'run.log'),'sanitize':args.sanitize,'scope':'actual production Client TU with test-only allocation/free wrappers; actual other runtime objects; initialized counter near exhaustion with its writer held'},indent=2)+'\n',encoding='utf-8');print('production boundary checks passed',flush=True)
if __name__=='__main__':main()
