#ifndef XREDIS_TEST_CLUSTER_CLIENT_PEER_H
#define XREDIS_TEST_CLUSTER_CLIENT_PEER_H
#include "cluster_refresh_peer.h"
static inline xredisclusterclient* cfCreate(rffixture* F,xredisclusterclientconfig* Config)
{
    xredisclusterclientconfig K;if(Config)K=*Config;else xredisClusterClientConfigInit(&K);K.RefreshMin=1;K.RefreshRetryMax=5;K.Refresh.RetryDelay=1;K.Route.RetryDelay=1;
    xredisclusteraddress A=nnAddress(F->Base,0);A.Slot=0;xredisclusterclient* C=NULL;
    redisPeerRequire(xredisClusterClientCreate(F->Base->Peers[0].Engine,1,F->Base->Cache,&A,1,&K,&C)==XREDIS_ADMISSION_ACCEPTED,"managed Cluster client create");return C;
}
static inline xredisclusterclientinfo cfInfo(xredisclusterclient* C)
{xredisclusterclientinfo I={0};I.Size=sizeof(I);redisPeerRequire(xredisClusterClientInfo(C,&I),"managed Cluster info");return I;}
static inline const xredisclusterclientresult* cfWait(xfuture* F)
{redisPeerRequire(F && xrtFutureWaitFor(F,15000)==XWAIT_OK && xrtFutureValue(F),"managed Cluster automatic progress");return xrtFutureValue(F);}
static inline xfuture* cfEcho(xredisclusterclient* C,int64 Timeout)
{xbytesview A[]={XRT_BYTES_LITERAL("ECHO"),XRT_BYTES_LITERAL("managed\0route")};xfuture* F=NULL;redisPeerRequire(xredisClusterClientCall(C,1,A,2,Timeout,&F)==XREDIS_ADMISSION_ACCEPTED,"managed Cluster command admission");return F;}
static inline void cfReady(xredisclusterclient* C)
{xfuture* F=xredisClusterClientReady(C);redisPeerRequire(xrtFutureWaitFor(F,15000)==XWAIT_OK && ((const xredisclusterclientready*)xrtFutureValue(F))->Status==XREDIS_CLUSTER_CLIENT_COMPLETE,"managed Cluster bootstrap");xrtFutureDestroy(F);}
static inline void cfClose(xredisclusterclient* C)
{xfuture* F=xredisClusterClientClosed(C);redisPeerRequire(xredisClusterClientClose(C) && xrtFutureWaitFor(F,15000)==XWAIT_OK && cfInfo(C).Closed && !cfInfo(C).Requests && !cfInfo(C).APIs && !cfInfo(C).CommandBytes && !cfInfo(C).StorageBytes && !cfInfo(C).HeapEntries,"managed Cluster close duties");xrtFutureDestroy(F);}
static inline void cfStop(rffixture* F,xredisclusterclient* C)
{cfClose(C);xredisClusterClientRelease(C);nnClose(F->Base);xredisClusterNodesRelease(F->Base->Cache);for(size_t I=0;I<3;++I)xredisClientRelease(xrtAtomicPtrLoad(&F->Nodes[I].Client,XMEMORY_ACQUIRE));nnStop(F->Base);xrtFree(F);}
#endif
