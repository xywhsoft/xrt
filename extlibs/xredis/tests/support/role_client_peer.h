#ifndef XREDIS_ROLE_CLIENT_PEER_H
#define XREDIS_ROLE_CLIENT_PEER_H
#include "client_init_peer.h"
#include <xredis/role_client.h>
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
#include "dial_peer.h"
#endif
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
#include "tls_peer.h"
#define ROLE_PEER_TLS 1
#else
#define ROLE_PEER_TLS 0
#endif
#define ROLE_MASTER_REPLY "*3\r\n+master\r\n:0\r\n*0\r\n"
#define ROLE_REPLICA_REPLY "*5\r\n+slave\r\n+127.0.0.1\r\n:6379\r\n+connected\r\n:0\r\n"
#define ROLE_SENTINEL_REPLY "*2\r\n+sentinel\r\n*1\r\n+cache\r\n"
typedef struct roleclientpeer {
    redisinitpeer Plain;
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
    redisdialpeer Dial;
#endif
#if ROLE_PEER_TLS
    redistlspeer TLS;
#endif
    bool Secure,WithDial;
} roleclientpeer;
static inline redisinitpeer* roleClientPeerInit(roleclientpeer* P)
{
#if ROLE_PEER_TLS
    if(P->Secure)return &P->TLS.Dial.Init;
#endif
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
    if(P->WithDial)return &P->Dial.Init;
#endif
    return &P->Plain;
}
static inline void roleClientPeerStart(roleclientpeer* P,bool Secure,bool Dial,const redisinitpeerstep* Steps,size_t Count)
{
    memset(P,0,sizeof(*P));P->Secure=Secure;P->WithDial=Dial;
#if ROLE_PEER_TLS
    if(Secure){redisTlsPeerStart(&P->TLS,false,false,0);redisinitpeer* I=&P->TLS.Dial.Init;I->Steps=Steps;I->Count=Count;I->Index=0;xrtAtomic32Store(&I->StepsSeen,0,XMEMORY_RELAXED);xrtAtomicPtrStore(&I->Peer.Hook,&I->Hook,XMEMORY_RELEASE);return;}
#endif
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
    if(Dial){redisDialPeerStartScript(&P->Dial,0,0,0,Steps,Count);return;}
#endif
    redisPeerRequire(!Secure && !Dial,"unavailable role fixture transport");redisInitPeerStart(&P->Plain,Steps,Count);
}
static inline bool roleClientPeerConnect(roleclientpeer* P,unsigned Factory,const xredisclientconfig* Config,const xredisclientinitconfig* Init,xredisclient** Output)
{
    redisinitpeer* I=roleClientPeerInit(P);
#if ROLE_PEER_TLS
    if(P->Secure){xredisclienttlsconfig T;redisTlsPeerConfig(&P->TLS,&T);T.WriteBytes=3;
        if(!Factory)return xredisClientTlsConnect(I->Peer.Engine,&I->Peer.Address,0,Config,Init,&T,Output);
        if(Factory==1)return xredisClientTlsDial(I->Peer.Engine,P->TLS.Dial.Resolver,XRT_STR_LITERAL("redis.fixture"),I->Peer.Address.Port,0,Config,Init,&T,Output);
        xnetaddr Addresses[]={P->TLS.Dial.RefusedAddress,I->Peer.Address};xnetaddrlist* List=xrtNetAddrListCreate(Addresses,2);redisPeerRequire(List!=NULL,"role TLS address list");bool OK=xredisClientTlsDialAddresses(I->Peer.Engine,List,0,Config,Init,&T,Output);xrtNetAddrListDestroy(List);return OK;
    }
#endif
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
    if(Factory==1)return xredisClientDial(I->Peer.Engine,P->Dial.Resolver,XRT_STR_LITERAL("redis.fixture"),I->Peer.Address.Port,0,Config,Init,NULL,Output);
    if(Factory==2){xnetaddr Addresses[]={P->Dial.RefusedAddress,I->Peer.Address};xnetaddrlist* List=xrtNetAddrListCreate(Addresses,2);redisPeerRequire(List!=NULL,"role address list");bool OK=xredisClientDialAddresses(I->Peer.Engine,List,0,Config,Init,NULL,Output);xrtNetAddrListDestroy(List);return OK;}
#endif
    redisPeerRequire(Factory==0,"unavailable role fixture factory");return xredisClientConnectInit(I->Peer.Engine,&I->Peer.Address,0,Config,Init,Output);
}
static inline void roleClientPeerStop(roleclientpeer* P)
{
#if ROLE_PEER_TLS
    if(P->Secure){redisTlsPeerStop(&P->TLS);return;}
#endif
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
    if(P->WithDial){redisDialPeerStop(&P->Dial);return;}
#endif
    redisPeerStop(&P->Plain.Peer);
}
static inline const void* roleClientWait(xfuture* F)
{redisPeerRequire(F && xrtFutureWaitFor(F,10000)==XWAIT_OK,"role future");xfutureresult R;redisPeerRequire(xrtFutureResult(F,&R) && R.State==XFUTURE_RESOLVED && R.Value,"role typed future");return R.Value;}
static inline xfuture* roleClientPing(xredisclient* C,int64 Timeout)
{xbytesview Ping=XRT_BYTES_LITERAL("PING");xfuture* F=NULL;redisPeerRequire(xredisClientCallAsync(C,&Ping,1,Timeout,&F)==XREDIS_ADMISSION_ACCEPTED,"role queued ping");return F;}
static inline void roleClientClose(roleclientpeer* P,xredisclient* C)
{
    xfuture* Closed=xredisClientClosed(C);redisPeerRequire(xredisClientClose(C) && xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"role physical close");xredisclientinfo I={0};I.Size=sizeof(I);
    redisPeerRequire(xredisClientInfo(C,&I) && I.State==XREDIS_CLIENT_CLOSED && !I.InitBytes && !I.Requests && !I.CommandBytes && I.Accepted==I.Completed,"role setup/request physical retirement");xrtFutureDestroy(Closed);xredisClientRelease(C);roleClientPeerStop(P);
}
#endif
