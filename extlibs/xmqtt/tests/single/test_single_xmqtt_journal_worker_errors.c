#if !defined(_WIN32) && !defined(_WIN64) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include <stdbool.h>
static bool LearnErrorKey, HaveErrorKey;
typedef struct poststats {unsigned Target,Writes,Faults;} poststats;
static _Thread_local poststats* PostStats;
static void postRestore(void);
#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <windows.h>
static DWORD ErrorKey;
static BOOL WINAPI ssNativeSet(DWORD Key, PVOID Value)
{
    if (LearnErrorKey) { ErrorKey=Key; HaveErrorKey=true; LearnErrorKey=false; }
    if (HaveErrorKey && Key==ErrorKey) {
        if(PostStats){++PostStats->Writes;if(PostStats->Writes==PostStats->Target){++PostStats->Faults;postRestore();
#if defined(_WIN32) || defined(_WIN64)
            return FALSE;
#else
            return ENOMEM;
#endif
        }}
    }
    return FlsSetValue(Key,Value);
}
#define FlsSetValue ssNativeSet
#else
#include <pthread.h>
#include <errno.h>
static pthread_key_t ErrorKey;
static int ssNativeSet(pthread_key_t Key, const void* Value)
{
    if (LearnErrorKey) { ErrorKey=Key; HaveErrorKey=true; LearnErrorKey=false; }
    if (HaveErrorKey && Key==ErrorKey) {
        if(PostStats){++PostStats->Writes;if(PostStats->Writes==PostStats->Target){++PostStats->Faults;postRestore();return ENOMEM;}}
    }
    return pthread_setspecific(Key,Value);
}
#define pthread_setspecific ssNativeSet
#endif
#define XMQTT_MODULE_XMQTT_JOURNAL_WORKER
#define XMQTT_MODULE_XMQTT_STORE_STATE
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
#include "../support/store_state_peer.h"
#include "../support/journal_worker_peer.h"
static _Thread_local xrt_error_context* PostContext;
static void postRestore(void)
{testRequire(PostContext&&__xrtErrorContextSwap(PostContext)==NULL,"restore actual Core task context immediately after injected native error-key failure");PostContext=NULL;PostStats=NULL;}
typedef struct postdrive {jpdrive Base;poststats Stats;} postdrive;
static bool postFlush(void* State)
{
    postdrive* D=State;bool OK=jpFlush(State);
    if(OK&&D->Base.Calls==7){PostContext=__xrtErrorContextSwap(NULL);testRequire(PostContext!=NULL,"ordinary host task has an actual bound Core error context");PostStats=&D->Stats;}
    return OK;
}
static const xvfsfileops_v1 postOps={sizeof(xvfsfileops_v1),XRT_VFS_FILE_OPS_VERSION,XVFS_FILE_READ_AT|XVFS_FILE_WRITE_AT|XVFS_FILE_STAT|XVFS_FILE_RESIZE|XVFS_FILE_FLUSH,NULL,NULL,jpRead,jpWrite,NULL,jpStat,jpResize,postFlush,jpClose};
static xvfslookup postOpen(void* C,xvfscase Case,xstrview Path,const xfileoptions* O,xvfsfile_v1* F)
{(void)Case;(void)Path;*F=(xvfsfile_v1){&postOps,C,O->Flags,0};return XVFS_LOOKUP_OPENED;}
static const xvfsprovider_v1 postProvider={sizeof(xvfsprovider_v1),XRT_VFS_PROVIDER_VERSION,0,jpRetain,jpRelease,postOpen,NULL,NULL,NULL};
static void postCase(xmqttversion V,unsigned Target)
{
    postdrive D={0};jpfile F={0};F.Vfs=xrtVfsCreate();testRequire(F.Vfs!=NULL,"post-sync test VFS");F.Mount=xrtVfsMount(F.Vfs,"/",0,XVFS_CASE_SENSITIVE,&postProvider,&D,0);testRequire(F.Mount!=NULL,"post-sync provider");xfileoptions O;xrtFileOptionsInit(&O);O.Flags=XFILE_READ|XFILE_WRITE;F.File=xrtVfsOpen(F.Vfs,"/journal",&O);testRequire(F.File!=NULL,"post-sync file");xmqttjournalconfig JC=stJournalConfig(V);xmqttjournal* J=jpCreate(F,&JC);D.Base.Calls=0;D.Stats.Target=Target;
    stsource Source;stSource(V,&Source);xmqttstoredeltaconfig C=sdConfig(V);xmqttproperty P[4];unsigned char Buffer[SD_BUFFER];size_t N=0;sdStatus(xmqttStoreDeltaWrite(&C,Source.Ops,16,P,4,Buffer,sizeof(Buffer),&N),XMQTT_STORE_DELTA_DONE,"real typed post-sync candidate");xtaskpool* Pool=jwPool(1);xmqttjournalworkerconfig WC;xmqttJournalWorkerConfigInit(&WC);xmqttjournalworker* W=NULL;testRequire(xmqttJournalWorkerCreate(Pool,J,&WC,&W),"post-sync owner transfer");xfuture* Future=NULL;testRequire(xmqttJournalWorkerAppendAsync(W,(xbytesview){Buffer,N},10000,&Future)==XMQTT_JWORK_ACCEPTED,"actual async commit before native setter fault");const xmqttcommitresult* R=jwOutcome(Future);
    testRequire(R->Status==(Target==1?XMQTT_COMMIT_STORAGE:XMQTT_COMMIT_INITIALIZATION)&&R->AppendStatus==(Target==1?XMQTT_JOURNAL_ERROR:XMQTT_JOURNAL_DONE)&&R->Effect==XMQTT_JOURNAL_COMMITTED&&R->Sequence==1&&R->AttemptSequence==1&&!R->Cause,"both sync barriers physically committed even when Journal/worker local restoration fails");testRequire(xmqttJournalWorkerClose(W)&&xmqttJournalWorkerWaitFor(W,10000)==XWAIT_OK&&D.Stats.Writes==Target&&D.Stats.Faults==1&&jwSnapshot(W).Failed&&xmqttJournalWorkerDestroy(W),"actual native failure once,checked ownership,stop and physical retirement");jwPoolDone(Pool);xrtFutureDestroy(Future);D.Stats.Target=0;D.Base.Calls=0;
    J=jpReopen(F,&JC);xmqttstorestateconfig RC=stConfig(V);xmqttstorestate* State=NULL;stStatus(xmqttStoreStateRecover(J,&RC,&State),XMQTT_STORE_DONE,"fresh complete-prefix recovery after local post-commit failure");testRequire(stInfo(State).Transactions==1&&stInfo(State).Operations==16&&xmqttJournalDestroy(J),"physical commit survives the caller's local failure");xmqttStoreStateDestroy(State);jpFileClose(F);
}
int main(void)
{
    xerror* Old=NULL;LearnErrorKey=true;testRequire(xrtTrySwapErrorTake(NULL,&Old)&&HaveErrorKey&&!Old,"learn only actual native error-key identity");for(unsigned V=0;V<2;++V)for(unsigned Target=1;Target<=2;++Target)postCase(V?XMQTT_VERSION_5:XMQTT_VERSION_311,Target);puts("[PASS]4 actual Journal/worker post-sync native error-key failures preserve COMMITTED and fresh typed recovery; normal host context restored");return 0;
}
