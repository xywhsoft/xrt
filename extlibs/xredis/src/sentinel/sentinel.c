#include <xredis/sentinel.h>
#include "../internal/sentinel_config.h"
#include "../internal/sentinel_client.h"
#include <xrt/memory.h>
#include <math.h>
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL)
enum {XS_SEED,XS_SEED_READY,XS_QUERY,XS_PEERS,XS_SEED_DRAIN,XS_MASTER,XS_MASTER_READY,XS_ROLE,XS_MASTER_DRAIN,XS_BACKOFF,XS_SUCCESS,XS_STOP,XS_DONE};
struct xredissentineldiscovery {
    xnetengine* Engine;xredissentinelconfig Config;xredissentinelresult Result;
    xredisclient* Client;xfuture* Pending;xfuture* Closed;
    double Deadline,SeedDue,ReplicaDue,RetryDue,NextWake;
    size_t State,SeedOffset,RoundNull;bool Taken,Stepping;
    xredissentinelstatus AfterDrain,ReplicaFailure;
    xredisreply* ReplicaReply;
    size_t ReplicaLimit,ReplicaCount,ReplicaIndex;
    xredissentinelendpoint PreferredReplica,Promoted;
    xredissentinelpeerdiscovery Peers;
    xredisreply* PeerReply;
    size_t Seed, PeerCount;
    double PeerDue;
    bool PeerAttempted;
    /* Replica/peer descriptors, service bytes, selected and preferred hosts follow. */
};
static bool xredisSentinelArgument(cstr Text)
{xrtSetErrorInfo(XERR_ARGUMENT,"xrt.redis",XREDIS_CLIENT_ARGUMENT,Text);return false;}
static xredissentinelendpoint* xredisSentinelReplicas(xredissentineldiscovery* D)
{return (xredissentinelendpoint*)(D+1);}
static xredissentinelendpoint* xredisSentinelPeerEntries(xredissentineldiscovery* D)
{return xredisSentinelReplicas(D)+D->ReplicaLimit;}
static unsigned char* xredisSentinelData(xredissentineldiscovery* D)
{return (unsigned char*)(xredisSentinelPeerEntries(D)+D->Peers.MaxRows);}
static size_t xredisSentinelBytes(const xredissentinelconfig* K,size_t Endpoints,size_t PreferredBytes)
{return sizeof(xredissentineldiscovery)+Endpoints*sizeof(xredissentinelendpoint)+K->Service.Size+K->MaxHostBytes+PreferredBytes;}
static void xredisSentinelDropPeers(xredissentineldiscovery* D)
{
    xredisReplyRelease(D->PeerReply);
    D->PeerReply=NULL;D->PeerCount=0;
}
static void xredisSentinelDropReplicas(xredissentineldiscovery* D)
{
    xredisReplyRelease(D->ReplicaReply);
    D->ReplicaReply=NULL;D->ReplicaCount=D->ReplicaIndex=0;D->Promoted=(xredissentinelendpoint){0};
}
static bool xredisSentinelDisjoint(xredissentineldiscovery* D,ptr P,size_t N)
{return P && xrtMemRangeValid(P,N) && !xrtMemRangesOverlap(D,xredisSentinelBytes(&D->Config,D->ReplicaLimit+D->Peers.MaxRows,D->PreferredReplica.Host.Size),P,N);}
static double xredisSentinelDue(double Now,int64 Timeout)
{return nextafter(Now+(double)Timeout*.001,INFINITY);}
static int64 xredisSentinelRemaining(double Due)
{double N=ceil((Due-xrtTimer())*1000.0);return N>0?(int64)N:0;}
static void xredisSentinelDropPending(xredissentineldiscovery* D)
{if(D->Pending){xrtFutureDestroy(D->Pending);D->Pending=NULL;}}
static void xredisSentinelStop(xredissentineldiscovery* D,xredissentinelstatus Status)
{if(D->PeerReply)xredisSentinelDropPeers(D);if(D->ReplicaLimit){xredisSentinelDropReplicas(D);D->Result.Endpoint=(xredissentinelendpoint){0};}
 D->Result.Status=Status;D->State=XS_STOP;if(D->Client)(void)xredisClientClose(D->Client);}
static void xredisSentinelFinish(xredissentineldiscovery* D)
{xredisSentinelDropReplicas(D);D->State=XS_DONE;D->NextWake=0;if(D->Engine){(void)xrtNetEngineUnpin(D->Engine);D->Engine=NULL;}}
static xredissentinelstatus xredisSentinelReadyClass(xredisreadystatus S)
{
    switch(S){case XREDIS_READY_TIMEOUT:return XREDIS_SENTINEL_TIMEOUT;
    case XREDIS_READY_DISCONNECTED:case XREDIS_READY_CLOSED:return XREDIS_SENTINEL_UNREACHABLE;
    case XREDIS_READY_AUTH_FAILED:return XREDIS_SENTINEL_AUTH_FAILED;
    case XREDIS_READY_SERVER_ERROR:return XREDIS_SENTINEL_SERVER_ERROR;
    case XREDIS_READY_LIMIT:return XREDIS_SENTINEL_LIMIT;case XREDIS_READY_MEMORY:return XREDIS_SENTINEL_MEMORY;
    case XREDIS_READY_ROLE_MISMATCH:return XREDIS_SENTINEL_STALE_ROLE;default:return XREDIS_SENTINEL_PROTOCOL;}
}
static xredissentinelstatus xredisSentinelRequestClass(xredisrequeststatus S)
{
    switch(S){case XREDIS_REQUEST_TIMEOUT:return XREDIS_SENTINEL_TIMEOUT;
    case XREDIS_REQUEST_DISCONNECTED:case XREDIS_REQUEST_CLOSED:return XREDIS_SENTINEL_UNREACHABLE;
    case XREDIS_REQUEST_CANCELLED:return XREDIS_SENTINEL_CANCELLED;
    case XREDIS_REQUEST_SERVER_ERROR:return XREDIS_SENTINEL_SERVER_ERROR;
    case XREDIS_REQUEST_MEMORY:return XREDIS_SENTINEL_MEMORY;case XREDIS_REQUEST_LIMIT:return XREDIS_SENTINEL_LIMIT;
    default:return XREDIS_SENTINEL_PROTOCOL;}
}
static bool xredisSentinelTransient(xredissentinelstatus S)
{return S==XREDIS_SENTINEL_TIMEOUT || S==XREDIS_SENTINEL_UNREACHABLE || S==XREDIS_SENTINEL_STALE_ROLE;}
static void xredisSentinelDrainStart(xredissentineldiscovery* D,bool Seed,xredissentinelstatus Why)
{D->AfterDrain=Why;D->State=Seed?XS_SEED_DRAIN:XS_MASTER_DRAIN;if(D->Client)(void)xredisClientClose(D->Client);}
static void xredisSentinelCandidateFailure(xredissentineldiscovery* D,bool Seed,xredissentinelstatus Why)
{
    /* A learned seed can become stale, untrusted or incompatible. It cannot
     * close a pool on remote seed errors while other seeds remain eligible. */
    bool Learned=Seed && D->Peers.Connect && D->Seed>=D->Peers.Configured;
    if(xredisSentinelTransient(Why) || (Learned &&
       (Why==XREDIS_SENTINEL_AUTH_FAILED || Why==XREDIS_SENTINEL_SERVER_ERROR ||
        Why==XREDIS_SENTINEL_PROTOCOL || Why==XREDIS_SENTINEL_LIMIT))){
        xredisSentinelDrainStart(D,Seed,Why);
    }else xredisSentinelStop(D,Why);
}
static bool xredisSentinelDrain(xredissentineldiscovery* D)
{
    xredisSentinelDropPending(D);
    if(!D->Client)return true;
    (void)xredisClientClose(D->Client);
    if(!D->Closed)D->Closed=xredisClientClosed(D->Client);
    if(!D->Closed || !xrtFutureDone(D->Closed))return false;
    xredisclientinfo I={0};I.Size=sizeof(I);(void)xredisClientInfo(D->Client,&I);
    if(I.Requests || I.InitBytes){D->NextWake=xredisSentinelDue(xrtTimer(),1);return false;}
    xrtFutureDestroy(D->Closed);D->Closed=NULL;xredisClientRelease(D->Client);D->Client=NULL;return true;
}
static void xredisSentinelBackoff(xredissentineldiscovery* D,xredissentinelstatus Last)
{
    if(D->Result.Rounds>=D->Config.MaxRounds){xredisSentinelStop(D,Last);return;}
    D->RetryDue=xredisSentinelDue(xrtTimer(),D->Config.RetryDelay);D->State=XS_BACKOFF;
}
static void xredisSentinelNextSeed(xredissentineldiscovery* D)
{
    if(D->ReplicaLimit){
        xredisSentinelDropReplicas(D);D->Result.Endpoint=(xredissentinelendpoint){0};
    }
    if(++D->SeedOffset<D->Config.SeedCount){D->State=XS_SEED;return;}
    xredissentinelstatus Why=D->RoundNull==D->Config.SeedCount?XREDIS_SENTINEL_UNKNOWN_SERVICE:
        D->ReplicaFailure?D->ReplicaFailure:XREDIS_SENTINEL_UNREACHABLE;
    /* An all-unknown round has no usable topology; do not hide it behind retry. */
    if(Why==XREDIS_SENTINEL_UNKNOWN_SERVICE)xredisSentinelStop(D,Why);else xredisSentinelBackoff(D,Why);
}
static void xredisSentinelSetEndpoint(xredissentineldiscovery* D,xredissentinelendpoint Endpoint)
{
    unsigned char* Host=xredisSentinelData(D)+D->Config.Service.Size;
    memcpy(Host,Endpoint.Host.Data,Endpoint.Host.Size);
    D->Result.Endpoint=(xredissentinelendpoint){{Host,Endpoint.Host.Size},Endpoint.Port};
}
static double xredisSentinelTargetDue(const xredissentineldiscovery* D)
{return D->ReplicaLimit?D->ReplicaDue:D->Deadline;}
static void xredisSentinelFactory(xredissentineldiscovery* D,bool Seed)
{
    double Now=xrtTimer();if(Seed){
        if(D->PeerReply)xredisSentinelDropPeers(D);
        D->SeedDue=fmin(D->Deadline,xredisSentinelDue(Now,D->Config.SeedTimeout));++D->Result.SeedAttempts;
        D->Seed=(D->Result.PreferredSeed+D->SeedOffset)%D->Config.SeedCount;
    }
    else {
        ++D->Result.MasterAttempts;
        if(D->ReplicaLimit){
            D->ReplicaDue=fmin(D->Deadline,xredisSentinelDue(Now,D->Config.SeedTimeout));
            xredisSentinelSetEndpoint(D,xredisSentinelReplicas(D)[D->ReplicaIndex]);
        }
    }
    xredisclient* C=NULL;xredissentinelstatus Status;
    if(Seed){
        int64 Timeout=xredisSentinelRemaining(D->SeedDue);
        Status=D->Peers.Connect?D->Peers.Connect(D->Engine,&D->Peers.Seeds[D->Seed],Timeout,D->Config.Data,&C):
            D->Config.ConnectSeed(D->Engine,D->Seed,Timeout,D->Config.Data,&C);
    }else Status=D->Config.ConnectMaster(D->Engine,&D->Result.Endpoint,xredisSentinelRemaining(xredisSentinelTargetDue(D)),D->Config.Data,&C);
    /* Factory failure is its explicit contract, never classified from ambient error. */
    if(Status!=XREDIS_SENTINEL_OK){
        xredisSentinelCandidateFailure(D,Seed,
            Status>=XREDIS_SENTINEL_UNREACHABLE && Status<=XREDIS_SENTINEL_FACTORY?Status:XREDIS_SENTINEL_FACTORY);
        return;
    }
    if(!C){xredisSentinelStop(D,XREDIS_SENTINEL_FACTORY);return;}
    D->Client=C;
    if(!__xredisSentinelClientProfile(C,D->Engine)){xredisSentinelStop(D,XREDIS_SENTINEL_FACTORY);return;}
    D->Pending=xredisClientReady(C);
    if(!D->Pending){xredisSentinelStop(D,XREDIS_SENTINEL_MEMORY);return;}
    D->State=Seed?XS_SEED_READY:XS_MASTER_READY;
}
static void xredisSentinelReady(xredissentineldiscovery* D,bool Seed)
{
    if(!xrtFutureDone(D->Pending))return;
    const xredisreadyresult* R=xrtFutureValue(D->Pending);
    if(!R){xredisSentinelCandidateFailure(D,Seed,XREDIS_SENTINEL_PROTOCOL);return;}
    D->Result.ReadyObserved=true;D->Result.ReadyStatus=R->Status;
    if(R->Status!=XREDIS_READY_OK){xredisSentinelCandidateFailure(D,Seed,xredisSentinelReadyClass(R->Status));return;}
    if(R->Protocol!=D->Config.Protocol){xredisSentinelCandidateFailure(D,Seed,XREDIS_SENTINEL_PROTOCOL);return;}
    xredisSentinelDropPending(D);
    xbytesview Command=D->ReplicaLimit?(xbytesview)XRT_BYTES_LITERAL("REPLICAS"):(xbytesview)XRT_BYTES_LITERAL("GET-MASTER-ADDR-BY-NAME");
    xbytesview Args[]={XRT_BYTES_LITERAL("SENTINEL"),Command,D->Config.Service};
    xbytesview Role=XRT_BYTES_LITERAL("ROLE");
    xredisadmission A=xredisClientCallAsync(D->Client,Seed?Args:&Role,Seed?3:1,xredisSentinelRemaining(Seed?D->SeedDue:xredisSentinelTargetDue(D)),&D->Pending);
    if(A!=XREDIS_ADMISSION_ACCEPTED){xredissentinelstatus Why=A==XREDIS_ADMISSION_OVERLOADED?XREDIS_SENTINEL_LIMIT:A==XREDIS_ADMISSION_CLOSED?XREDIS_SENTINEL_UNREACHABLE:XREDIS_SENTINEL_MEMORY;
        xredisSentinelCandidateFailure(D,Seed,Why);return;}
    D->State=Seed?XS_QUERY:XS_ROLE;
}
static bool xredisSentinelString(const xredisreplyvalue* V)
{return (V->Type==XREDIS_REPLY_BULK_STRING || V->Type==XREDIS_REPLY_SIMPLE_STRING) && !(V->Flags&XREDIS_REPLY_NULL_VALUE);}
static bool xredisSentinelNull(const xredisreplyvalue* Root)
{return Root->Type==XREDIS_REPLY_NULL || ((Root->Type==XREDIS_REPLY_ARRAY || Root->Type==XREDIS_REPLY_BULK_STRING) && (Root->Flags&XREDIS_REPLY_NULL_VALUE));}
static bool xredisSentinelHostValid(xbytesview Host)
{
    for(size_t I=0;I<Host.Size;++I)if(Host.Data[I]<=32 || Host.Data[I]==127)return false;
    return true;
}
bool __xredisSentinelEndpointValid(xredissentinelendpoint E,size_t MaxHostBytes)
{
    return E.Port && E.Host.Size && E.Host.Size<=MaxHostBytes &&
        xrtMemRangeValid(E.Host.Data,E.Host.Size) && xredisSentinelHostValid(E.Host);
}
static xredissentinelstatus xredisSentinelAddress(xredissentineldiscovery* D,const xredisreplyvalue* H,
    const xredisreplyvalue* P,xredissentinelendpoint* Out)
{
    if(!xredisSentinelString(H) || !H->Data.Size || !xredisSentinelString(P) || !P->Data.Size || P->Data.Size>5)return XREDIS_SENTINEL_PROTOCOL;
    if(H->Data.Size>D->Config.MaxHostBytes)return XREDIS_SENTINEL_LIMIT;
    if(!xredisSentinelHostValid(H->Data))return XREDIS_SENTINEL_PROTOCOL;
    uint32 Port=0;
    for(size_t I=0;I<P->Data.Size;++I){
        unsigned C=P->Data.Data[I];
        if(C<'0' || C>'9' || Port>(65535u-(C-'0'))/10u)return XREDIS_SENTINEL_PROTOCOL;
        Port=Port*10u+C-'0';
    }
    if(!Port)return XREDIS_SENTINEL_PROTOCOL;
    *Out=(xredissentinelendpoint){H->Data,(uint16)Port};
    return XREDIS_SENTINEL_OK;
}
static xredissentinelstatus xredisSentinelEndpoint(xredissentineldiscovery* D,const xredisreply* Reply)
{
    xredisreplyinfo Info={.Size=sizeof(Info)};xredisReplyInfo(Reply,&Info);
    xredisreplyvalue Root,H,P;xredisReplyGet(Reply,Info.Root,&Root);
    if(xredisSentinelNull(&Root))return XREDIS_SENTINEL_UNKNOWN_SERVICE;
    if(Root.Type!=XREDIS_REPLY_ARRAY || Root.ChildCount!=2)return XREDIS_SENTINEL_PROTOCOL;
    xredisReplyGet(Reply,Root.FirstChild,&H);xredisReplyGet(Reply,H.Next,&P);
    xredissentinelendpoint Endpoint;
    xredissentinelstatus S=xredisSentinelAddress(D,&H,&P,&Endpoint);
    if(S==XREDIS_SENTINEL_OK)xredisSentinelSetEndpoint(D,Endpoint);
    return S;
}
static bool xredisSentinelEquals(xbytesview V,xbytesview Expected)
{return V.Size==Expected.Size && !memcmp(V.Data,Expected.Data,V.Size);}
static bool xredisSentinelSameEndpoint(xredissentinelendpoint A,xredissentinelendpoint B)
{return A.Port==B.Port && xredisSentinelEquals(A.Host,B.Host);}
static xredissentinelstatus xredisSentinelFlags(const xredisreplyvalue* Flags,xbytesview Kind,bool* Promoted)
{
    if(!xredisSentinelString(Flags)||!Flags->Data.Size)return XREDIS_SENTINEL_PROTOCOL;
    bool Matching=false,Down=false,Promotion=false;
    size_t At=0;
    for(size_t I=0;I<=Flags->Data.Size;++I){
        if(I<Flags->Data.Size && Flags->Data.Data[I]!=','){
            if(Flags->Data.Data[I]<=32||Flags->Data.Data[I]==127)return XREDIS_SENTINEL_PROTOCOL;
            continue;
        }
        if(I==At)return XREDIS_SENTINEL_PROTOCOL;
        xbytesview Flag={Flags->Data.Data+At,I-At};
        if(xredisSentinelEquals(Flag,Kind))Matching=true;
        if(Promoted && xredisSentinelEquals(Flag,(xbytesview)XRT_BYTES_LITERAL("promoted")))Promotion=true;
        if(xredisSentinelEquals(Flag,(xbytesview)XRT_BYTES_LITERAL("s_down"))||
           xredisSentinelEquals(Flag,(xbytesview)XRT_BYTES_LITERAL("o_down"))||
           xredisSentinelEquals(Flag,(xbytesview)XRT_BYTES_LITERAL("disconnected")))Down=true;
        At=I+1;
    }
    if(Promoted)*Promoted=Matching&&Promotion;
    return Matching&&!Down?XREDIS_SENTINEL_OK:XREDIS_SENTINEL_NO_REPLICA;
}
static xredissentinelstatus xredisSentinelNodeRow(xredissentineldiscovery* D,const xredisreply* Reply,
    const xredisreplyvalue* Row,xbytesview Kind,xredissentinelendpoint* Endpoint,bool* Promoted)
{
    if((Row->Type!=XREDIS_REPLY_MAP&&Row->Type!=XREDIS_REPLY_ARRAY)||
       Row->Flags&XREDIS_REPLY_NULL_VALUE||Row->ChildCount%2)return XREDIS_SENTINEL_PROTOCOL;
    if(Row->ChildCount>256)return XREDIS_SENTINEL_LIMIT;
    xredisreplyvalue Fields[3]={0};
    const xbytesview Keys[]={XRT_BYTES_LITERAL("ip"),XRT_BYTES_LITERAL("port"),XRT_BYTES_LITERAL("flags")};
    unsigned Seen=0;
    for(size_t I=Row->FirstChild;I!=XREDIS_REPLY_NONE;){
        xredisreplyvalue Key,V;
        xredisReplyGet(Reply,I,&Key);xredisReplyGet(Reply,Key.Next,&V);
        if(!xredisSentinelString(&Key))return XREDIS_SENTINEL_PROTOCOL;
        for(unsigned F=0;F<3;++F)if(xredisSentinelEquals(Key.Data,Keys[F])){
            if(Seen&(1u<<F))return XREDIS_SENTINEL_PROTOCOL;
            Seen|=1u<<F;Fields[F]=V;break;
        }
        I=V.Next;
    }
    if(Seen!=7)return XREDIS_SENTINEL_PROTOCOL;
    xredissentinelstatus S=xredisSentinelAddress(D,Fields,Fields+1,Endpoint);
    return S==XREDIS_SENTINEL_OK?xredisSentinelFlags(Fields+2,Kind,Promoted):S;
}
static xredissentinelstatus xredisSentinelReadReplicas(xredissentineldiscovery* D,xredisreply* Reply)
{
    xredisreplyinfo Info={.Size=sizeof(Info)};xredisReplyInfo(Reply,&Info);
    xredisreplyvalue Root;xredisReplyGet(Reply,Info.Root,&Root);
    if(xredisSentinelNull(&Root))return XREDIS_SENTINEL_UNKNOWN_SERVICE;
    if(Root.Type!=XREDIS_REPLY_ARRAY)return XREDIS_SENTINEL_PROTOCOL;
    if(Root.ChildCount>D->ReplicaLimit||Info.PayloadBytes>D->Config.Role.MaxPayloadBytes)return XREDIS_SENTINEL_LIMIT;
    size_t Count=0;
    bool ConflictingPromotion=false;
    xredissentinelendpoint Promoted={0};
    xredissentinelendpoint* Entries=xredisSentinelReplicas(D);
    for(size_t I=Root.FirstChild;I!=XREDIS_REPLY_NONE;){
        xredisreplyvalue Row;xredisReplyGet(Reply,I,&Row);
        xredissentinelendpoint Entry;
        bool Promotion=false;
        xredissentinelstatus S=xredisSentinelNodeRow(D,Reply,&Row,(xbytesview)XRT_BYTES_LITERAL("slave"),&Entry,&Promotion);
        if(S!=XREDIS_SENTINEL_OK && S!=XREDIS_SENTINEL_NO_REPLICA)return S;
        if(Promotion){
            /* A promoted row identifies the new upstream even while Sentinel
             * considers its data link down. It is never a read candidate. */
            if(Promoted.Port && !xredisSentinelSameEndpoint(Promoted,Entry))ConflictingPromotion=true;
            Promoted=Entry;
        }else if(S==XREDIS_SENTINEL_OK)Entries[Count++]=Entry;
        I=Row.Next;
    }
    if(!Count || ConflictingPromotion)return XREDIS_SENTINEL_NO_REPLICA;
    if(D->PreferredReplica.Port){
        for(size_t I=0;I<Count;++I){
            xredissentinelendpoint Entry=Entries[I];
            if(!xredisSentinelSameEndpoint(Entry,D->PreferredReplica))continue;
            if(I){
                memmove(Entries+1,Entries,I*sizeof(*Entries));
                Entries[0]=Entry;
            }
            break;
        }
    }
    if(!xredisReplyRetain(Reply))return XREDIS_SENTINEL_LIMIT;
    D->ReplicaReply=Reply;D->ReplicaCount=Count;D->Promoted=Promoted;
    return XREDIS_SENTINEL_OK;
}
static void xredisSentinelReadPeers(xredissentineldiscovery* D,xredisreply* Reply)
{
    xredisreplyinfo Info={.Size=sizeof(Info)};
    xredisreplyvalue Root;
    if(!Reply || !xredisReplyInfo(Reply,&Info) || !xredisReplyGet(Reply,Info.Root,&Root) ||
        Root.Type!=XREDIS_REPLY_ARRAY || xredisSentinelNull(&Root) ||
        Root.ChildCount>D->Peers.MaxRows || Info.PayloadBytes>D->Config.Role.MaxPayloadBytes)return;
    size_t Count=0;
    xredissentinelendpoint* Entries=xredisSentinelPeerEntries(D);
    for(size_t I=Root.FirstChild;I!=XREDIS_REPLY_NONE;){
        xredisreplyvalue Row;
        xredisReplyGet(Reply,I,&Row);
        xredissentinelstatus S=xredisSentinelNodeRow(D,Reply,&Row,(xbytesview)XRT_BYTES_LITERAL("sentinel"),Entries+Count,NULL);
        if(S==XREDIS_SENTINEL_OK)++Count;
        else if(S!=XREDIS_SENTINEL_NO_REPLICA)return;
        I=Row.Next;
    }
    if(Count && xredisReplyRetain(Reply)){D->PeerReply=Reply;D->PeerCount=Count;}
}
static void xredisSentinelPeerQuery(xredissentineldiscovery* D)
{
    if(D->Peers.MaxRows && !D->PeerAttempted){
        double Now=xrtTimer();
        /* Leave the target one seed-sized setup and ROLE budget. */
        double Latest=fmin(D->SeedDue,D->Deadline-(double)D->Config.SeedTimeout*.001);
        D->PeerDue=fmin(Latest,xredisSentinelDue(Now,D->Peers.Timeout));
        if(D->PeerDue>Now){
            D->PeerAttempted=true;
            xredisSentinelDropPending(D);
            xbytesview Args[]={XRT_BYTES_LITERAL("SENTINEL"),XRT_BYTES_LITERAL("SENTINELS"),D->Config.Service};
            if(xredisClientCallAsync(D->Client,Args,3,xredisSentinelRemaining(D->PeerDue),&D->Pending)==XREDIS_ADMISSION_ACCEPTED){
                D->State=XS_PEERS;return;
            }
        }
    }
    xredisSentinelDrainStart(D,true,XREDIS_SENTINEL_OK);
}
static bool xredisSentinelMissingService(const xredisreply* Reply)
{
    xredisreplyinfo Info={.Size=sizeof(Info)};
    xredisreplyvalue V;
    return Reply && xredisReplyInfo(Reply,&Info) && xredisReplyGet(Reply,Info.Root,&V) &&
        V.Type==XREDIS_REPLY_SIMPLE_ERROR &&
        xredisSentinelEquals(V.Data,(xbytesview)XRT_BYTES_LITERAL("ERR No such master with that name"));
}
static void xredisSentinelUnknown(xredissentineldiscovery* D)
{
    ++D->Result.UnknownReplies;++D->RoundNull;
    xredisSentinelDrainStart(D,true,XREDIS_SENTINEL_UNKNOWN_SERVICE);
}
static void xredisSentinelReply(xredissentineldiscovery* D,bool Seed)
{
    if(!xrtFutureDone(D->Pending))return;
    const xredisrequestresult* R=xrtFutureValue(D->Pending);
    if(!R){xredisSentinelCandidateFailure(D,Seed,XREDIS_SENTINEL_PROTOCOL);return;}
    D->Result.RequestObserved=true;D->Result.RequestStatus=R->Status;
    if(R->Status!=XREDIS_REQUEST_OK){
        if(Seed&&D->ReplicaLimit&&R->Status==XREDIS_REQUEST_SERVER_ERROR&&xredisSentinelMissingService(R->Reply)){
            xredisSentinelUnknown(D);return;
        }
        xredisSentinelCandidateFailure(D,Seed,xredisSentinelRequestClass(R->Status));return;}
    if(!R->Reply){xredisSentinelCandidateFailure(D,Seed,XREDIS_SENTINEL_PROTOCOL);return;}
    if(Seed){
        xredissentinelstatus S=D->ReplicaLimit?xredisSentinelReadReplicas(D,R->Reply):xredisSentinelEndpoint(D,R->Reply);
        if(S==XREDIS_SENTINEL_OK){
            if(!D->ReplicaLimit)D->Result.PreferredSeed=D->Seed;
            xredisSentinelPeerQuery(D);
        }else if(S==XREDIS_SENTINEL_UNKNOWN_SERVICE)xredisSentinelUnknown(D);
        else if(S==XREDIS_SENTINEL_NO_REPLICA){
            if(!D->ReplicaFailure)D->ReplicaFailure=S;
            xredisSentinelDrainStart(D,true,S);
        }else xredisSentinelCandidateFailure(D,true,S);
    }else{
        xredisroleinfo I;xredisrolestatus S=xredisRoleRead(R->Reply,&D->Config.Role,&I);
        if(S!=XREDIS_ROLE_VALUE){xredisSentinelStop(D,S==XREDIS_ROLE_LIMIT?XREDIS_SENTINEL_LIMIT:XREDIS_SENTINEL_PROTOCOL);return;}
        bool Verified=I.Kind==XREDIS_ROLE_MASTER;
        if(D->ReplicaLimit){
            xredissentinelendpoint Upstream={I.MasterHost,I.MasterPort};
            Verified=I.Kind==XREDIS_ROLE_REPLICA && I.Link==XREDIS_ROLE_LINK_CONNECTED &&
                (!D->Promoted.Port || (xredisSentinelSameEndpoint(Upstream,D->Promoted) &&
                                      !xredisSentinelSameEndpoint(D->Result.Endpoint,D->Promoted)));
            if(I.Kind==XREDIS_ROLE_MASTER && !D->Promoted.Port && D->PreferredReplica.Port &&
               xredisSentinelSameEndpoint(D->Result.Endpoint,D->PreferredReplica)){
                /* The previously selected replica is now primary. Refresh the
                 * contradicted seed snapshot before trying its other rows. */
                D->ReplicaIndex=D->ReplicaCount-1;
            }
        }
        if(!Verified){++D->Result.StaleReplies;xredisSentinelDrainStart(D,false,XREDIS_SENTINEL_STALE_ROLE);return;}
        xredisSentinelDropPending(D);D->State=XS_SUCCESS;
    }
}
XREDIS_API bool xredisSentinelConfigInit(xredissentinelconfig* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K)))return xredisSentinelArgument("invalid Sentinel config output");
    *K=(xredissentinelconfig){0};K->Size=sizeof(*K);K->SeedCount=1;K->MaxRounds=3;K->MaxServiceBytes=65536;K->MaxHostBytes=1024;
    K->Timeout=5000;K->SeedTimeout=300;K->RetryDelay=100;K->Protocol=XREDIS_RESP3;return xredisRoleLimitsInit(&K->Role);
}
bool __xredisSentinelConfigValid(const xredissentinelconfig* Config,bool EndpointSeeds,ptr Output,size_t Bytes)
{
    if(!Config || !xrtMemRangeValid(Config,sizeof(*Config)))return xredisSentinelArgument("invalid Sentinel config input");
    xredissentinelconfig K=*Config;
    if(K.Size!=sizeof(K) || K.Role.Size!=sizeof(K.Role) || (!EndpointSeeds && !K.ConnectSeed) || !K.ConnectMaster || !K.SeedCount || K.SeedCount>4096 || K.PreferredSeed>=K.SeedCount || !K.MaxRounds || K.MaxRounds>1024 ||
       K.Timeout<=0 || K.Timeout>86400000 || K.SeedTimeout<=0 || K.SeedTimeout>60000 || K.RetryDelay<=0 || K.RetryDelay>60000 ||
       !K.MaxServiceBytes || K.MaxServiceBytes>65536 || !K.MaxHostBytes || K.MaxHostBytes>65536 || !K.Service.Size || K.Service.Size>K.MaxServiceBytes || !xrtMemRangeValid(K.Service.Data,K.Service.Size) ||
       xrtMemRangesOverlap(K.Service.Data,K.Service.Size,Output,Bytes) || (K.Protocol!=XREDIS_RESP2 && K.Protocol!=XREDIS_RESP3)){
        xredisSentinelArgument("invalid Sentinel bounds/service/factories");return false;}
    return true;
}
static xredisadmission xredisSentinelCreate(xnetengine* Engine,const xredissentinelconfig* Config,double Deadline,
    xredissentinelreplicaconnect ConnectReplica,size_t MaxReplicas,const xredissentinelendpoint* Prefer,
    const xredissentinelpeerdiscovery* Peers,xredissentineldiscovery** Output)
{
    if(!isfinite(Deadline) || !Engine || !Config || !Output || !xrtMemRangeValid(Config,sizeof(*Config)) || !xrtMemRangeValid(Output,sizeof(*Output)) || xrtMemRangesOverlap(Config,sizeof(*Config),Output,sizeof(*Output))){xredisSentinelArgument("invalid Sentinel discovery arguments");return XREDIS_ADMISSION_ERROR;}
    xredissentinelconfig K=*Config;
    if(ConnectReplica)K.ConnectMaster=ConnectReplica;
    if(!__xredisSentinelConfigValid(&K,Peers!=NULL,Output,sizeof(*Output)))return XREDIS_ADMISSION_ERROR;
    size_t PreferredBytes=Prefer?Prefer->Host.Size:0;
    if(Prefer&&(!MaxReplicas||!Prefer->Port||!PreferredBytes||PreferredBytes>K.MaxHostBytes||
       !xrtMemRangeValid(Prefer->Host.Data,PreferredBytes))){
        xredisSentinelArgument("invalid Sentinel preferred replica");return XREDIS_ADMISSION_ERROR;
    }
    if(!xrtNetEnginePin(Engine))return XREDIS_ADMISSION_ERROR;
    xredissentineldiscovery* D=xrtCalloc(1,xredisSentinelBytes(&K,MaxReplicas+(Peers?Peers->MaxRows:0),PreferredBytes));
    if(!D){(void)xrtNetEngineUnpin(Engine);return XREDIS_ADMISSION_ERROR;}
    D->Engine=Engine;D->Config=K;D->ReplicaLimit=MaxReplicas;
    if(Peers)D->Peers=*Peers;
    unsigned char* Service=xredisSentinelData(D);
    memcpy(Service,K.Service.Data,K.Service.Size);D->Config.Service=(xbytesview){Service,K.Service.Size};
    if(Prefer){
        unsigned char* Host=Service+K.Service.Size+K.MaxHostBytes;
        memcpy(Host,Prefer->Host.Data,PreferredBytes);
        D->PreferredReplica=(xredissentinelendpoint){{Host,PreferredBytes},Prefer->Port};
    }
    D->Deadline=Deadline;D->NextWake=xrtTimer();D->Result.Size=sizeof(D->Result);D->Result.PreferredSeed=K.PreferredSeed;D->Result.Rounds=1;*Output=D;return XREDIS_ADMISSION_ACCEPTED;
}
xredisadmission __xredisSentinelDiscoveryCreateUntil(xnetengine* Engine,const xredissentinelconfig* Config,double Deadline,xredissentineldiscovery** Output)
{return xredisSentinelCreate(Engine,Config,Deadline,NULL,0,NULL,NULL,Output);}
xredisadmission __xredisSentinelReplicaCreateUntil(xnetengine* Engine,const xredissentinelconfig* Config,double Deadline,
    size_t MaxReplicas,const xredissentinelendpoint* Prefer,xredissentineldiscovery** Output)
{
    if(!MaxReplicas||MaxReplicas>256){
        xredisSentinelArgument("invalid Sentinel replica limit");return XREDIS_ADMISSION_ERROR;
    }
    return xredisSentinelCreate(Engine,Config,Deadline,NULL,MaxReplicas,Prefer,NULL,Output);
}
xredisadmission __xredisSentinelDiscoveryCreateWithPeers(xnetengine* Engine,const xredissentinelconfig* Config,double Deadline,
    size_t MaxReplicas,const xredissentinelendpoint* Prefer,const xredissentinelpeerdiscovery* Peers,xredissentineldiscovery** Output)
{
    return xredisSentinelCreate(Engine,Config,Deadline,NULL,MaxReplicas,Prefer,Peers,Output);
}
const xredissentinelendpoint* __xredisSentinelDiscoveredPeers(xredissentineldiscovery* D,size_t* Count)
{
    *Count=D->Result.Status==XREDIS_SENTINEL_OK?D->PeerCount:0;
    return xredisSentinelPeerEntries(D);
}
XREDIS_API xredisadmission xredisSentinelDiscoveryCreate(xnetengine* Engine,const xredissentinelconfig* Config,xredissentineldiscovery** Output)
{
    double Entered=xrtTimer();
    if(!Engine || !Config || !Output || !xrtMemRangeValid(Config,sizeof(*Config)) || !xrtMemRangeValid(Output,sizeof(*Output)) || xrtMemRangesOverlap(Config,sizeof(*Config),Output,sizeof(*Output))){xredisSentinelArgument("invalid Sentinel discovery arguments");return XREDIS_ADMISSION_ERROR;}
    return __xredisSentinelDiscoveryCreateUntil(Engine,Config,xredisSentinelDue(Entered,Config->Timeout),Output);
}
XREDIS_API xredisadmission xredisSentinelReplicaDiscoveryCreate(xnetengine* Engine,const xredissentinelconfig* Config,
    xredissentinelreplicaconnect ConnectReplica,size_t MaxReplicas,xredissentineldiscovery** Output)
{
    double Entered=xrtTimer();
    if(!ConnectReplica||!MaxReplicas||MaxReplicas>256||!Config||!xrtMemRangeValid(Config,sizeof(*Config))){
        xredisSentinelArgument("invalid Sentinel replica limits or factory");return XREDIS_ADMISSION_ERROR;
    }
    return xredisSentinelCreate(Engine,Config,xredisSentinelDue(Entered,Config->Timeout),ConnectReplica,MaxReplicas,NULL,NULL,Output);
}
static void xredisSentinelRetryTarget(xredissentineldiscovery* D)
{
    if(!D->ReplicaLimit){xredisSentinelBackoff(D,D->AfterDrain);return;}
    if(D->AfterDrain==XREDIS_SENTINEL_STALE_ROLE||D->ReplicaFailure!=XREDIS_SENTINEL_STALE_ROLE)
        D->ReplicaFailure=D->AfterDrain==XREDIS_SENTINEL_TIMEOUT?XREDIS_SENTINEL_UNREACHABLE:D->AfterDrain;
    if(++D->ReplicaIndex<D->ReplicaCount)D->State=XS_MASTER;
    else xredisSentinelNextSeed(D);
}
XREDIS_API xredissentinelstep xredisSentinelDiscoveryStep(xredissentineldiscovery* D)
{
    if(!D || D->Stepping){xredisSentinelArgument("invalid/reentrant Sentinel Step");return XREDIS_SENTINEL_STEP_ERROR;}
    if(D->State==XS_DONE)return XREDIS_SENTINEL_DONE;
    size_t Before=D->State;D->Stepping=true;double Now=xrtTimer();D->NextWake=D->Deadline;
    if(Now>=D->Deadline && D->State!=XS_STOP)xredisSentinelStop(D,XREDIS_SENTINEL_TIMEOUT);
    bool Seed=D->State==XS_SEED_READY || D->State==XS_QUERY;
    if(Seed && Now>=D->SeedDue)xredisSentinelDrainStart(D,true,XREDIS_SENTINEL_TIMEOUT);
    if(D->ReplicaLimit&&(D->State==XS_MASTER_READY||D->State==XS_ROLE)&&Now>=D->ReplicaDue)
        xredisSentinelDrainStart(D,false,XREDIS_SENTINEL_TIMEOUT);
    switch(D->State){
    case XS_SEED:xredisSentinelFactory(D,true);break;
    case XS_SEED_READY:xredisSentinelReady(D,true);break;
    case XS_QUERY:xredisSentinelReply(D,true);break;
    case XS_PEERS:
        if(Now<D->PeerDue && !xrtFutureDone(D->Pending))break;
        if(Now<D->PeerDue){
            const xredisrequestresult* R=xrtFutureValue(D->Pending);
            if(R && R->Status==XREDIS_REQUEST_OK)xredisSentinelReadPeers(D,R->Reply);
        }
        xredisSentinelDrainStart(D,true,XREDIS_SENTINEL_OK);break;
    case XS_SEED_DRAIN:
        if(xredisSentinelDrain(D)){if(D->AfterDrain==XREDIS_SENTINEL_OK)D->State=XS_MASTER;else xredisSentinelNextSeed(D);}break;
    case XS_MASTER:xredisSentinelFactory(D,false);break;
    case XS_MASTER_READY:xredisSentinelReady(D,false);break;
    case XS_ROLE:xredisSentinelReply(D,false);break;
    case XS_MASTER_DRAIN:if(xredisSentinelDrain(D))xredisSentinelRetryTarget(D);break;
    case XS_BACKOFF:if(Now>=D->RetryDue){D->SeedOffset=0;D->RoundNull=0;D->ReplicaFailure=0;++D->Result.Rounds;D->State=XS_SEED;}break;
    case XS_SUCCESS:{xredisclientinfo I={0};I.Size=sizeof(I);(void)xredisClientInfo(D->Client,&I);
        if(I.State!=XREDIS_CLIENT_OPEN)xredisSentinelDrainStart(D,false,XREDIS_SENTINEL_UNREACHABLE);
        else if(I.Requests || I.InitBytes)D->NextWake=xredisSentinelDue(Now,1);
        else {
            if(D->ReplicaLimit)D->Result.PreferredSeed=D->Seed;
            D->Result.Status=XREDIS_SENTINEL_OK;xredisSentinelFinish(D);
        }break;}
    case XS_STOP:if(xredisSentinelDrain(D))xredisSentinelFinish(D);break;
    default:break;}
    D->Stepping=false;
    if(D->State==XS_DONE)return XREDIS_SENTINEL_DONE;
    Now=xrtTimer();
    if(D->State==XS_SEED_READY || D->State==XS_QUERY)D->NextWake=fmin(D->NextWake,D->SeedDue);
    if(D->State==XS_PEERS)D->NextWake=fmin(D->NextWake,D->PeerDue);
    if(D->ReplicaLimit&&(D->State==XS_MASTER_READY||D->State==XS_ROLE))D->NextWake=fmin(D->NextWake,D->ReplicaDue);
    if(D->State==XS_BACKOFF)D->NextWake=fmin(D->NextWake,D->RetryDue);
    if(D->State==XS_STOP || D->State==XS_MASTER_DRAIN || D->State==XS_SEED_DRAIN){
        /* Close may retire before request/control release tails; no expired-deadline spin. */
        D->NextWake=xredisSentinelDue(Now,1);
        return Before!=D->State?XREDIS_SENTINEL_MORE:XREDIS_SENTINEL_WAIT;
    }
    if(D->State==XS_SUCCESS){D->NextWake=fmin(D->Deadline,xredisSentinelDue(Now,1));return Before!=D->State?XREDIS_SENTINEL_MORE:XREDIS_SENTINEL_WAIT;}
    if((D->Pending && !xrtFutureDone(D->Pending)) || (D->State==XS_BACKOFF && Now<D->RetryDue))return XREDIS_SENTINEL_WAIT;
    return XREDIS_SENTINEL_MORE;
}
XREDIS_API xfuture* xredisSentinelDiscoveryPending(xredissentineldiscovery* D)
{if(!D){xredisSentinelArgument("invalid Sentinel discovery");return NULL;}xfuture* F=D->Closed?D->Closed:D->Pending;return F && !xrtFutureDone(F)?xrtFutureRef(F):NULL;}
XREDIS_API double xredisSentinelDiscoveryNextWake(xredissentineldiscovery* D)
{
    if(!D){xredisSentinelArgument("invalid Sentinel discovery");return 0;}
    /* Pending can disappear between Step and registration after completion. */
    if((D->State==XS_SEED_READY || D->State==XS_QUERY || D->State==XS_PEERS || D->State==XS_MASTER_READY || D->State==XS_ROLE) &&
       D->Pending && xrtFutureDone(D->Pending))return xrtTimer();
    return D->NextWake;
}
XREDIS_API bool xredisSentinelDiscoveryResult(xredissentineldiscovery* D,xredissentinelresult* R)
{if(!D || !xredisSentinelDisjoint(D,R,sizeof(*R)) || R->Size!=sizeof(*R))return xredisSentinelArgument("invalid Sentinel result");*R=D->Result;return true;}
XREDIS_API bool xredisSentinelDiscoveryTakeClient(xredissentineldiscovery* D,xredisclient** C)
{
    if(!D || !xredisSentinelDisjoint(D,C,sizeof(*C)) || D->State!=XS_DONE || D->Result.Status!=XREDIS_SENTINEL_OK || D->Taken)return xredisSentinelArgument("Sentinel client is not available");
    *C=D->Client;D->Client=NULL;D->Taken=true;return true;
}
XREDIS_API bool xredisSentinelDiscoveryCancel(xredissentineldiscovery* D)
{if(!D || D->Stepping)return xredisSentinelArgument("invalid/reentrant Sentinel cancel");if(D->State!=XS_DONE)xredisSentinelStop(D,XREDIS_SENTINEL_CANCELLED);return true;}
bool __xredisSentinelDiscoveryRetire(xredissentineldiscovery* D)
{
    if(D->State!=XS_DONE || !D->Client)return false;
    /* Preserve the verified result while the private probe physically closes. */
    D->State=XS_STOP;
    D->NextWake=xrtTimer();
    (void)xredisClientClose(D->Client);
    return true;
}
XREDIS_API void xredisSentinelDiscoveryDestroy(xredissentineldiscovery* D)
{
    if(!D)return;
    if(D->Stepping){(void)xredisSentinelArgument("reentrant Sentinel destroy");return;}
    if(D->Client){(void)xredisClientClose(D->Client);xredisClientRelease(D->Client);}
    if(D->Pending)(void)xrtFutureCancel(D->Pending);
    xredisSentinelDropPending(D);xredisSentinelDropReplicas(D);if(D->PeerReply)xredisSentinelDropPeers(D);xrtFutureDestroy(D->Closed);if(D->Engine)(void)xrtNetEngineUnpin(D->Engine);xrtFree(D);
}
#endif
