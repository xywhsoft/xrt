#include "../internal/xrpc_result_quota.h"
#include <xrt/atomic.h>
#include <xrt/sync.h>
#include <xrt/memory.h>
#include <stddef.h>
#if defined(XRPC_FEATURE_XRPC_RESULT_QUOTA)
struct xrpcresultquota {
    xatomic32 References;
    xmutex Gate;
    size_t MaxBlocks,MaxBytes,Blocks,Bytes;
    uint64 Rejected;
};
typedef union xrpcresultallocation {
    struct {xrpcresultquota* Quota;size_t Bytes;} Meta;
    max_align_t Alignment;
} xrpcresultallocation;
bool xrpcResultQuotaCreate(size_t Blocks,size_t Bytes,xrpcresultquota** Result)
{
    xrpcresultquota* Q=xrtCalloc(1,sizeof(*Q));if(!Q)return false;
    if(!xrtMutexInit(&Q->Gate)){xrtFree(Q);return false;}
    xrtAtomic32Init(&Q->References,1);Q->MaxBlocks=Blocks;Q->MaxBytes=Bytes;*Result=Q;return true;
}
void xrpcResultQuotaRelease(xrpcresultquota* Q)
{if(Q && xrtAtomic32FetchSub(&Q->References,1,XMEMORY_ACQ_REL)==1){xrtMutexUnit(&Q->Gate);xrtFree(Q);}}
void xrpcResultQuotaInfo(xrpcresultquota* Q,size_t* Blocks,size_t* Bytes,uint64* Rejected)
{xrtMutexLock(&Q->Gate);*Blocks=Q->Blocks;*Bytes=Q->Bytes;*Rejected=Q->Rejected;xrtMutexUnlock(&Q->Gate);}
ptr xrpcResultQuotaAlloc(xrpcresultquota* Q,size_t Bytes,bool* Limited)
{
    *Limited=false;
    if(Bytes>SIZE_MAX-sizeof(xrpcresultallocation)){xrtMutexLock(&Q->Gate);++Q->Rejected;xrtMutexUnlock(&Q->Gate);*Limited=true;return NULL;}
    Bytes+=sizeof(xrpcresultallocation);xrtMutexLock(&Q->Gate);
    bool OK=Q->Blocks<Q->MaxBlocks && Bytes<=Q->MaxBytes-Q->Bytes;
    if(OK){++Q->Blocks;Q->Bytes+=Bytes;xrtAtomic32FetchAdd(&Q->References,1,XMEMORY_RELAXED);}
    else ++Q->Rejected;
    xrtMutexUnlock(&Q->Gate);if(!OK){*Limited=true;return NULL;}
    xrpcresultallocation* A=xrtMalloc(Bytes);
    if(!A){xrtMutexLock(&Q->Gate);--Q->Blocks;Q->Bytes-=Bytes;xrtMutexUnlock(&Q->Gate);xrpcResultQuotaRelease(Q);return NULL;}
    A->Meta.Quota=Q;A->Meta.Bytes=Bytes;return A+1;
}
void xrpcResultQuotaFree(ptr Value)
{
    if(!Value)return;
    xrpcresultallocation* A=(xrpcresultallocation*)Value-1;xrpcresultquota* Q=A->Meta.Quota;size_t Bytes=A->Meta.Bytes;
    xrtFree(A);xrtMutexLock(&Q->Gate);--Q->Blocks;Q->Bytes-=Bytes;xrtMutexUnlock(&Q->Gate);xrpcResultQuotaRelease(Q);
}
#endif
