#ifndef XMQTT_TEST_JOURNAL_WORKER_PEER_H
#define XMQTT_TEST_JOURNAL_WORKER_PEER_H
#include "journal_peer.h"
#include <xmqtt/journal_worker.h>
#include <xrt/atomic.h>
#include <xrt/time.h>
typedef struct jwdrive {
    jpdrive Base;xatomic32 Entered,Release,Active,MaxActive;unsigned Hold;
    xmqttjournalworker* Worker;xtaskpool* Pool;bool CheckWorker;unsigned Checked;
} jwdrive;
static inline void jwBarrier(jwdrive* D)
{
    uint32 Active=xrtAtomic32FetchAdd(&D->Active,1,XMEMORY_ACQ_REL)+1,Max=xrtAtomic32Load(&D->MaxActive,XMEMORY_ACQUIRE);
    while(Active>Max&&!xrtAtomic32CompareExchange(&D->MaxActive,&Max,Active,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE)){}
    testRequire(Active==1,"no overlapping Journal IO on a multi-thread pool");
    if(D->CheckWorker){testRequire(xrtTaskPoolIsCurrentWorkerV1(D->Pool),"native writes/sync only on borrowed host pool");xmqttjournalworkerinfo I;
        testRequire(xmqttJournalWorkerInfo(D->Worker,&I)&&I.Busy&&xmqttJournalWorkerWaitFor(D->Worker,0)==XWAIT_ERROR&&!xmqttJournalWorkerDestroy(D->Worker),"backend Info reentry allowed; blocking self wait/destroy refused");xrtClearError();++D->Checked;}
    if(D->Hold&&D->Base.Calls+1==D->Hold){xrtAtomic32Store(&D->Entered,1,XMEMORY_RELEASE);double End=xrtTimer()+15;while(!xrtAtomic32Load(&D->Release,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"bounded injected native IO barrier");xrtSleep(1);}}
}
static inline bool jwWrite(void* State,uint64 At,const void* B,size_t N,size_t* Done)
{jwdrive* D=State;jwBarrier(D);bool R=jpWrite(&D->Base,At,B,N,Done);xrtAtomic32FetchSub(&D->Active,1,XMEMORY_RELEASE);return R;}
static inline bool jwFlush(void* State)
{jwdrive* D=State;jwBarrier(D);bool R=jpFlush(&D->Base);xrtAtomic32FetchSub(&D->Active,1,XMEMORY_RELEASE);return R;}
static const xvfsfileops_v1 jwOps={sizeof(xvfsfileops_v1),XRT_VFS_FILE_OPS_VERSION,XVFS_FILE_READ_AT|XVFS_FILE_WRITE_AT|XVFS_FILE_STAT|XVFS_FILE_RESIZE|XVFS_FILE_FLUSH,NULL,NULL,jpRead,jwWrite,NULL,jpStat,jpResize,jwFlush,jpClose};
static inline xvfslookup jwOpen(void* C,xvfscase Case,xstrview Path,const xfileoptions* O,xvfsfile_v1* F)
{(void)Case;(void)Path;*F=(xvfsfile_v1){&jwOps,C,O->Flags,0};return XVFS_LOOKUP_OPENED;}
static const xvfsprovider_v1 jwProvider={sizeof(xvfsprovider_v1),XRT_VFS_PROVIDER_VERSION,0,jpRetain,jpRelease,jwOpen,NULL,NULL,NULL};
static inline jpfile jwFile(jwdrive* D)
{jpfile F={0};F.Vfs=xrtVfsCreate();testRequire(F.Vfs!=NULL,"worker test VFS");F.Mount=xrtVfsMount(F.Vfs,"/",0,XVFS_CASE_SENSITIVE,&jwProvider,D,0);testRequire(F.Mount!=NULL,"worker test mount");xfileoptions O;xrtFileOptionsInit(&O);O.Flags=XFILE_READ|XFILE_WRITE;F.File=xrtVfsOpen(F.Vfs,"/journal",&O);testRequire(F.File!=NULL,"worker test absolute file");return F;}
static inline xmqttjournalworkerinfo jwSnapshot(xmqttjournalworker* W)
{xmqttjournalworkerinfo I={0};testRequire(xmqttJournalWorkerInfo(W,&I),"worker snapshot");return I;}
static inline const xmqttcommitresult* jwOutcome(xfuture* F)
{testRequire(F&&xrtFutureWaitFor(F,15000)==XWAIT_OK&&xrtFutureState(F)==XFUTURE_RESOLVED,"worker always typed outcome");return xrtFutureValue(F);}
static inline void jwAwait(xatomic32* V)
{double End=xrtTimer()+15;while(!xrtAtomic32Load(V,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"worker fixture deadline");xrtSleep(1);}}
static inline xtaskpool* jwPool(unsigned Threads)
{xtaskpoolconfig C={.Threads=Threads,.QueueLimit=32};xtaskpool* P=xrtTaskPoolCreate(&C);testRequire(P!=NULL,"real host storage pool");return P;}
static inline xmqttjournalworker* jwCreate(jpfile F,jwdrive* D,xtaskpool* P,const xmqttjournalworkerconfig* C)
{xmqttjournalconfig JC=jpConfig();xmqttjournal* J=jpCreate(F,&JC);xmqttjournalworker* W=NULL;testRequire(xmqttJournalWorkerCreate(P,J,C,&W),"worker takes ready Journal");D->Worker=W;D->Pool=P;D->Base.Calls=0;return W;}
static inline void jwPoolDone(xtaskpool* P)
{testRequire(xrtTaskPoolClose(P)&&xrtTaskPoolWaitFor(P,15000)==XWAIT_OK&&xrtTaskPoolDestroy(P),"host pool physical join");}
typedef struct jwblock {xatomic32 Started,Released;} jwblock;
static inline xtaskoutcome jwBlock(xcancel* C,ptr Data,xtaskvalue* V)
{(void)V;jwblock* B=Data;xrtAtomic32Store(&B->Started,1,XMEMORY_RELEASE);while(!xrtAtomic32Load(&B->Released,XMEMORY_ACQUIRE)&&!xrtCancelRequested(C))xrtSleep(1);return XTASK_SUCCESS;}
#endif
