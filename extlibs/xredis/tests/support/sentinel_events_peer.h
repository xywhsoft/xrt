#ifndef XREDIS_SENTINEL_EVENTS_PEER_H
#define XREDIS_SENTINEL_EVENTS_PEER_H
#include "sentinel_pool_peer.h"
#include <xredis/sentinel_events.h>

typedef struct sefixture {
    spfixture Pool;
    spnode Backup;
    xatomicptr Wire[2];
    xatomic32 Subscribes, Mode;
    bool HasBackup;
} sefixture;
static bool seCommand(redispeer* Peer,xredisreply* Reply,ptr Data)
{
    spnode* Node=Data;
    xredisreplyvalue Root={0},Name={0},Channel={0};
    redisPeerRequire(xredisReplyGet(Reply,0,&Root) && xredisReplyGet(Reply,Root.FirstChild,&Name),"listener command");
    if(!redisPeerName(Name,"SUBSCRIBE"))return spCommand(Peer,Reply,Data);
    sefixture* F=(sefixture*)Node->Root;
    redisPeerRequire(Root.ChildCount==2 && xredisReplyGet(Reply,Name.Next,&Channel) &&
        redisPeerName(Channel,"+switch-master"),"exact Sentinel event subscription");
    unsigned Index=Node==&F->Backup?1:0;
    xrtAtomicPtrStore(&F->Wire[Index],Peer,XMEMORY_RELEASE);
    uint32 Mode=xrtAtomic32Load(&F->Mode,XMEMORY_ACQUIRE);
    xrtAtomic32FetchAdd(&F->Subscribes,1,XMEMORY_RELEASE);
    if(Mode==1){redisPeerSend(Peer,"-NOPERM synthetic\r\n",19);return true;}
    if(Mode==2)return true;
    static const char Ack[]="3\r\n+subscribe\r\n$14\r\n+switch-master\r\n:1\r\n";
    char Frame[sizeof(Ack)];
    Frame[0]=F->Pool.Resp2?'*':'>';
    memcpy(Frame+1,Ack,sizeof(Ack)-1);
    redisPeerSend(Peer,Frame,sizeof(Frame));
    return true;
}
static void seStart(sefixture* F,bool Replica,bool Secure,bool Resp2,bool Backup)
{
    memset(F,0,sizeof(*F));spStart(&F->Pool,0,Secure,Resp2);
    F->Pool.Replicas=Replica;F->HasBackup=Backup;
    F->Pool.Seed.Base.Hook=(redispeerhook){seCommand,&F->Pool.Seed};
    if(Backup){
        redisConnectPeerStart(&F->Backup.Base);
        F->Backup.Root=&F->Pool;F->Backup.Base.Hook=(redispeerhook){seCommand,&F->Backup};
        F->Pool.SeedPeer=&F->Backup;
#if SP_TLS
        if(Secure)F->Backup.Base.TLS=&F->Pool.TLS.Options;
#endif
    }
}
static xredispool* seCreate(sefixture* F,size_t Count)
{
    xredissentinelpoolconfig K;spConfig(&F->Pool,&K);
    xredispoolconfig Options;xredisPoolConfigInit(&Options);Options.WorkItems=1;
    if(!F->HasBackup)return spCreate(&F->Pool,Count,&Options,&K);
    xredissentinelpoolseedconfig Seeds;xredisSentinelPoolSeedConfigInit(&Seeds);
    xredissentinelendpoint Seed={XRT_BYTES_LITERAL("127.0.0.1"),F->Pool.Seed.Base.Address.Port};
    Seeds.Seeds=&Seed;Seeds.Connect=spSeedEndpoint;Seeds.MaxSeeds=2;Seeds.PeerTimeout=500;
    K.MaxConnecting=1;
    xredispool* P=NULL;
    xnetengine* Engine=F->Pool.Seed.Base.Engine;
    xredisadmission A=F->Pool.Replicas?xredisSentinelReplicaPoolCreateWithSeeds(Engine,0,Count,&Options,&K,&Seeds,spMaster,8,&P):
        xredisSentinelPoolCreateWithSeeds(Engine,0,Count,&Options,&K,&Seeds,&P);
    redisPeerRequire(A==XREDIS_ADMISSION_ACCEPTED,"seeded event pool");
    xrtAtomicPtrStore(&F->Pool.Pool,P,XMEMORY_RELEASE);
    return P;
}
static xredissentineleventinfo seInfo(xredispool* P)
{
    xredissentineleventinfo I={.Size=sizeof(I)};
    redisPeerRequire(xredisSentinelPoolEventInfo(P,&I),"Sentinel event snapshot");
    return I;
}
static void seListen(xredispool* P,int64 Interval)
{
    xredissentineleventconfig K;
    redisPeerRequire(xredisSentinelEventConfigInit(&K),"listener defaults");
    K.AttemptTimeout=2000;K.RetryMin=5;K.RetryMax=20;K.WorkMessages=2;K.HintInterval=Interval;
    redisPeerRequire(xredisSentinelPoolListen(P,&K)==XREDIS_ADMISSION_ACCEPTED,"start Sentinel listener");
}
typedef struct sewrite {
    sefixture* Fixture;
    cstr Payload;
    unsigned Seed,Count; /* zero Count disconnects */
    xatomic32 Done;
} sewrite;
static void seWrite(xnetworker* Worker,ptr Data)
{
    (void)Worker;
    sewrite* W=Data;
    redispeer* Peer=xrtAtomicPtrLoad(&W->Fixture->Wire[W->Seed],XMEMORY_ACQUIRE);
    redisPeerRequire(Peer && !xrtAtomic32Load(&Peer->Closed,XMEMORY_ACQUIRE),"active event wire");
    if(!W->Count)xrtNetStreamAbort(Peer->Stream);
    else{
        char Frame[512];
        int N=snprintf(Frame,sizeof(Frame),"%c3\r\n+message\r\n$14\r\n+switch-master\r\n$%zu\r\n%s\r\n",
            W->Fixture->Pool.Resp2?'*':'>',strlen(W->Payload),W->Payload);
        redisPeerRequire(N>0 && (size_t)N<sizeof(Frame),"event frame cap");
        for(unsigned I=0;I<W->Count;++I)redisPeerSend(Peer,Frame,(size_t)N);
    }
    xrtAtomic32Store(&W->Done,1,XMEMORY_RELEASE);
}
static void seSend(sefixture* F,unsigned Seed,cstr Payload,unsigned Count)
{
    sewrite W={.Fixture=F,.Payload=Payload,.Seed=Seed,.Count=Count};
    xnetengine* Engine=Seed?F->Backup.Base.Engine:F->Pool.Seed.Base.Engine;
    redisPeerRequire(xrtNetEnginePost(Engine,1,seWrite,&W),"send Sentinel fixture event");
    redisPeerWait(&W.Done,1);
}
static void seStop(sefixture* F,xredispool* P)
{
    spClose(&F->Pool,P);
    if(F->HasBackup)redisConnectPeerStop(&F->Backup.Base);
    spStop(&F->Pool);
    redisPeerRequire(seInfo(P).Session.State==XREDIS_SESSION_CLOSED,"closed event snapshot");
    xredisPoolRelease(P);
}
#endif
