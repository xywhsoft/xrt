#include <xredis/cluster_topology.h>
#include "../internal/cluster_slot.h"
#include "../internal/cluster_reply.h"
#include "../internal/cluster_topology_internal.h"
#include <xrt/memory.h>
#include <xrt/atomic.h>
#include <string.h>
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_TOPOLOGY)
struct xredisclustertopology {
    xatomic32 References;xredisreply* Reply;xredisclustertopologyinfo Info;
    xredisclusternode* Nodes;xredisclusterrange* Ranges;
    uint16 Slots[XREDIS_CLUSTER_SLOTS];
};
typedef struct xredisclusterbuild {
    xredisreply* Reply;const xredisclusterlimits* Limits;xredisclustertopology* Output;
    size_t Shards,Nodes,Ranges,Assigned;unsigned char Seen[XREDIS_CLUSTER_SLOTS/8];
} xredisclusterbuild;
static bool xredisClusterArgument(cstr Text)
{xrtSetErrorInfo(XERR_ARGUMENT,"xrt.redis",XREDIS_REPLY_ARGUMENT,Text);return false;}
static bool xredisClusterString(xredisreplyvalue V)
{return (V.Type==XREDIS_REPLY_SIMPLE_STRING || V.Type==XREDIS_REPLY_BULK_STRING) && !(V.Flags&XREDIS_REPLY_NULL_VALUE);}
static bool xredisClusterNull(xredisreplyvalue V)
{return V.Type==XREDIS_REPLY_NULL || (V.Type==XREDIS_REPLY_BULK_STRING && (V.Flags&XREDIS_REPLY_NULL_VALUE));}
static bool xredisClusterArray(xredisreplyvalue V)
{return V.Type==XREDIS_REPLY_ARRAY && !(V.Flags&XREDIS_REPLY_NULL_VALUE);}
static bool xredisClusterWord(xbytesview V,cstr Word)
{size_t N=strlen(Word);return V.Size==N && (!N || !memcmp(V.Data,Word,N));}
static xredisreplyvalue xredisClusterGet(const xredisreply* R,size_t Index)
{xredisreplyvalue V={0};xredisReplyGet(R,Index,&V);return V;}
static bool xredisClusterHostText(xbytesview Host)
{
    for(size_t N=0;N<Host.Size;++N){unsigned C=Host.Data[N];if(C<=32 || C==127 || C=='/' || C=='\\' || C=='@' || C=='#')return false;}
    return true;
}
static xredisclusterendpoint xredisClusterEndpoint(xbytesview Host)
{return !Host.Size?XREDIS_CLUSTER_ENDPOINT_SOURCE:xredisClusterWord(Host,"?")?XREDIS_CLUSTER_ENDPOINT_UNKNOWN:XREDIS_CLUSTER_ENDPOINT_HOST;}
XREDIS_API bool xredisClusterKeySlot(xbytesview Key,uint16* Slot)
{
    if(!Slot || !xrtMemRangeValid(Slot,sizeof(*Slot)) || !xrtMemRangeValid(Key.Data,Key.Size) ||
        xrtMemRangesOverlap(Key.Data,Key.Size,Slot,sizeof(*Slot)))return xredisClusterArgument("invalid Cluster key/slot output");
    *Slot=xredisClusterSlotRaw(Key);return true;
}
static bool xredisClusterDecimal(xbytesview V,uint32 Max,uint16* Output)
{
    if(!V.Size || V.Size>5)return false;
    uint32 N=0;for(size_t I=0;I<V.Size;++I){unsigned C=V.Data[I];if(C<'0' || C>'9')return false;N=N*10+C-'0';}
    if(N>Max)return false;
    *Output=(uint16)N;return true;
}
XREDIS_API xredisclusterstatus xredisClusterRedirectRead(const xredisreply* R,size_t MaxHost,xredisclusterredirect* Output)
{
    if(!__xredisClusterReplyOutput(R,Output,sizeof(*Output))){xredisClusterArgument("invalid Cluster redirection output");return XREDIS_CLUSTER_ERROR;}
    xredisreplyinfo I={0};I.Size=sizeof(I);xredisReplyInfo(R,&I);xredisreplyvalue V=xredisClusterGet(R,I.Root);
    if(V.Type!=XREDIS_REPLY_SIMPLE_ERROR && V.Type!=XREDIS_REPLY_BULK_ERROR)return XREDIS_CLUSTER_ABSENT;
    xbytesview T=V.Data;size_t Prefix=0;xredisclusterredirect E={0};
    if(T.Size>=5 && !memcmp(T.Data,"MOVED",5) && (T.Size==5 || T.Data[5]==' ')){Prefix=5;E.Kind=XREDIS_CLUSTER_MOVED;}
    else if(T.Size>=3 && !memcmp(T.Data,"ASK",3) && (T.Size==3 || T.Data[3]==' ')){Prefix=3;E.Kind=XREDIS_CLUSTER_ASK;}
    else return XREDIS_CLUSTER_ABSENT;
    if(T.Size<=Prefix+1)return XREDIS_CLUSTER_MALFORMED;
    size_t Start=Prefix+1,End=Start;while(End<T.Size && End-Start<6 && T.Data[End]!=' ')++End;
    if(End==T.Size || T.Data[End]!=' ' || !xredisClusterDecimal((xbytesview){T.Data+Start,End-Start},16383,&E.Slot))return XREDIS_CLUSTER_MALFORMED;
    Start=End+1;if(Start==T.Size)return XREDIS_CLUSTER_MALFORMED;
    End=T.Size;size_t Digits=0;while(End>Start && T.Data[End-1]!=':' && Digits<6){--End;++Digits;}
    if(End==Start || T.Data[End-1]!=':' || !xredisClusterDecimal((xbytesview){T.Data+End,Digits},65535,&E.Port) || !E.Port)return XREDIS_CLUSTER_MALFORMED;
    E.Host=(xbytesview){T.Data+Start,End-1-Start};
    if(E.Host.Size>MaxHost)return XREDIS_CLUSTER_LIMIT;
    if(E.Host.Size && E.Host.Data[0]=='['){if(E.Host.Size<3 || E.Host.Data[E.Host.Size-1]!=']')return XREDIS_CLUSTER_MALFORMED;++E.Host.Data;E.Host.Size-=2;}
    for(size_t N=0;N<E.Host.Size;++N)if(E.Host.Data[N]=='[' || E.Host.Data[N]==']')return XREDIS_CLUSTER_MALFORMED;
    if(!xredisClusterHostText(E.Host))return XREDIS_CLUSTER_MALFORMED;
    E.Endpoint=xredisClusterEndpoint(E.Host);*Output=E;return XREDIS_CLUSTER_VALUE;
}
XREDIS_API bool xredisClusterLimitsInit(xredisclusterlimits* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K)))return xredisClusterArgument("invalid Cluster limits output");
    *K=(xredisclusterlimits){sizeof(*K),4096,4096,16384,262144,4u*1024u*1024u,1024,32u*1024u*1024u,false};return true;
}
/* Fixed known-field lists: each map is scanned once, future fields are skipped.
 * The immutable reader already guarantees complete child/attribute chains. */
static bool xredisClusterFields(const xredisreply* R,xredisreplyvalue V,const cstr* Names,size_t Count,size_t* Fields)
{
    if((V.Type!=XREDIS_REPLY_MAP && !xredisClusterArray(V)) || V.ChildCount%2)return false;
    for(size_t N=0;N<Count;++N)Fields[N]=XREDIS_REPLY_NONE;
    for(size_t I=V.FirstChild;I!=XREDIS_REPLY_NONE;){xredisreplyvalue Key=xredisClusterGet(R,I);if(!xredisClusterString(Key))return false;
        size_t Value=Key.Next;xredisreplyvalue Data=xredisClusterGet(R,Value);
        for(size_t N=0;N<Count;++N)if(xredisClusterWord(Key.Data,Names[N])){if(Fields[N]!=XREDIS_REPLY_NONE)return false;Fields[N]=Value;break;}
        I=Data.Next;
    }
    return true;
}
static xredisclusterstatus xredisClusterHost(xredisreplyvalue V,size_t Max,xbytesview* Host)
{
    if(xredisClusterNull(V)){*Host=(xbytesview){0};return XREDIS_CLUSTER_VALUE;}
    if(!xredisClusterString(V))return XREDIS_CLUSTER_MALFORMED;
    if(V.Data.Size>Max)return XREDIS_CLUSTER_LIMIT;
    if(!xredisClusterHostText(V.Data))return XREDIS_CLUSTER_MALFORMED;
    *Host=V.Data;return XREDIS_CLUSTER_VALUE;
}
static bool xredisClusterPort(xredisreplyvalue V,uint16* Port)
{if(V.Type!=XREDIS_REPLY_INTEGER || V.Integer<0 || V.Integer>65535)return false;*Port=(uint16)V.Integer;return true;}
static xredisclusterstatus xredisClusterReadNode(xredisclusterbuild* B,size_t Index,bool Legacy,bool Master,xredisclusternode* N)
{
    const xredisreply* R=B->Reply;xredisreplyvalue V=xredisClusterGet(R,Index);*N=(xredisclusternode){0};N->Shard=B->Shards;N->ReplicationOffset=-1;
    xredisclusterstatus Status;
    if(Legacy){
        if(!xredisClusterArray(V) || V.ChildCount<2)return XREDIS_CLUSTER_MALFORMED;
        xredisreplyvalue Host=xredisClusterGet(R,V.FirstChild),Port=xredisClusterGet(R,Host.Next);
        Status=xredisClusterHost(Host,B->Limits->MaxEndpointBytes,&N->Host);if(Status!=XREDIS_CLUSTER_VALUE)return Status;
        if(!xredisClusterPort(Port,&N->Port))return XREDIS_CLUSTER_MALFORMED;
        if(V.ChildCount>=3){xredisreplyvalue Id=xredisClusterGet(R,Port.Next);if(!xredisClusterString(Id) && !xredisClusterNull(Id))return XREDIS_CLUSTER_MALFORMED;N->Id=Id.Data;
            if(V.ChildCount>=4){static const cstr Names[]={"ip","hostname"};size_t F[2];if(!xredisClusterFields(R,xredisClusterGet(R,Id.Next),Names,2,F))return XREDIS_CLUSTER_MALFORMED;
                for(unsigned K=0;K<2;++K)if(F[K]!=XREDIS_REPLY_NONE){xbytesview Text;Status=xredisClusterHost(xredisClusterGet(R,F[K]),B->Limits->MaxEndpointBytes,&Text);if(Status!=XREDIS_CLUSTER_VALUE)return Status;if(K)N->Hostname=Text;else N->Ip=Text;}}}
        N->Role=Master?XREDIS_CLUSTER_MASTER:XREDIS_CLUSTER_REPLICA;N->Health=XREDIS_CLUSTER_UNREPORTED;
    }else{
        static const cstr Names[]={"id","endpoint","ip","hostname","port","tls-port","role","replication-offset","health"};size_t F[9];
        if(!xredisClusterFields(R,V,Names,9,F))return XREDIS_CLUSTER_MALFORMED;
        const unsigned Required[]={0,1,2,6,7,8};for(size_t K=0;K<sizeof(Required)/sizeof(*Required);++K)if(F[Required[K]]==XREDIS_REPLY_NONE)return XREDIS_CLUSTER_MALFORMED;
        xredisreplyvalue Id=xredisClusterGet(R,F[0]),Ip=xredisClusterGet(R,F[2]),Role=xredisClusterGet(R,F[6]),Offset=xredisClusterGet(R,F[7]),Health=xredisClusterGet(R,F[8]);
        if(!xredisClusterString(Id) || !Id.Data.Size || !xredisClusterString(Ip) || !xredisClusterString(Role) || !xredisClusterString(Health) || Offset.Type!=XREDIS_REPLY_INTEGER || Offset.Integer< -1)return XREDIS_CLUSTER_MALFORMED;
        N->Id=Id.Data;N->RoleText=Role.Data;N->HealthText=Health.Data;N->ReplicationOffset=Offset.Integer;
        Status=xredisClusterHost(xredisClusterGet(R,F[1]),B->Limits->MaxEndpointBytes,&N->Host);if(Status!=XREDIS_CLUSTER_VALUE)return Status;
        Status=xredisClusterHost(Ip,B->Limits->MaxEndpointBytes,&N->Ip);if(Status!=XREDIS_CLUSTER_VALUE)return Status;
        if(F[3]!=XREDIS_REPLY_NONE){Status=xredisClusterHost(xredisClusterGet(R,F[3]),B->Limits->MaxEndpointBytes,&N->Hostname);if(Status!=XREDIS_CLUSTER_VALUE)return Status;}
        if(F[4]==XREDIS_REPLY_NONE && F[5]==XREDIS_REPLY_NONE)return XREDIS_CLUSTER_MALFORMED;
        if(F[4]!=XREDIS_REPLY_NONE && !xredisClusterPort(xredisClusterGet(R,F[4]),&N->Port))return XREDIS_CLUSTER_MALFORMED;
        if(F[5]!=XREDIS_REPLY_NONE && !xredisClusterPort(xredisClusterGet(R,F[5]),&N->TlsPort))return XREDIS_CLUSTER_MALFORMED;
        N->Role=xredisClusterWord(N->RoleText,"master")?XREDIS_CLUSTER_MASTER:xredisClusterWord(N->RoleText,"replica")?XREDIS_CLUSTER_REPLICA:XREDIS_CLUSTER_ROLE_OTHER;
        N->Health=xredisClusterWord(N->HealthText,"online")?XREDIS_CLUSTER_ONLINE:xredisClusterWord(N->HealthText,"failed")?XREDIS_CLUSTER_FAILED:xredisClusterWord(N->HealthText,"loading")?XREDIS_CLUSTER_LOADING:XREDIS_CLUSTER_HEALTH_OTHER;
    }
    N->Endpoint=xredisClusterEndpoint(N->Host);return XREDIS_CLUSTER_VALUE;
}
static xredisclusterstatus xredisClusterBuildRange(xredisclusterbuild* B,xredisreplyvalue First,xredisreplyvalue Last,size_t Master)
{
    if(First.Type!=XREDIS_REPLY_INTEGER || Last.Type!=XREDIS_REPLY_INTEGER || First.Integer<0 || Last.Integer<First.Integer || Last.Integer>=XREDIS_CLUSTER_SLOTS)return XREDIS_CLUSTER_MALFORMED;
    if(B->Ranges==B->Limits->MaxRanges)return XREDIS_CLUSTER_LIMIT;
    if(B->Output)B->Output->Ranges[B->Ranges]=(xredisclusterrange){(uint16)First.Integer,(uint16)Last.Integer,Master,B->Shards};
    ++B->Ranges;
    for(size_t Slot=(size_t)First.Integer;Slot<=(size_t)Last.Integer;++Slot){unsigned char Bit=(unsigned char)(1u<<(Slot%8));
        if(B->Seen[Slot/8]&Bit)return XREDIS_CLUSTER_MALFORMED;
        B->Seen[Slot/8]|=Bit;++B->Assigned;if(B->Output)B->Output->Slots[Slot]=(uint16)(Master+1);}
    return XREDIS_CLUSTER_VALUE;
}
static xredisclusterstatus xredisClusterBuild(xredisclusterbuild* B,xredisreplyvalue Root,bool Legacy)
{
    if(!xredisClusterArray(Root))return XREDIS_CLUSTER_MALFORMED;
    for(size_t Row=Root.FirstChild;Row!=XREDIS_REPLY_NONE;){
        if(B->Shards==B->Limits->MaxShards)return XREDIS_CLUSTER_LIMIT;
        xredisreplyvalue Item=xredisClusterGet(B->Reply,Row),Ranges={0},Nodes={0};size_t Node,Count;size_t Master=SIZE_MAX;
        if(Legacy){if(!xredisClusterArray(Item) || Item.ChildCount<3)return XREDIS_CLUSTER_MALFORMED;
            xredisreplyvalue First=xredisClusterGet(B->Reply,Item.FirstChild),Last=xredisClusterGet(B->Reply,First.Next);Node=Last.Next;Count=Item.ChildCount-2;
        }else{static const cstr Names[]={"slots","nodes"};size_t F[2];
            if(!xredisClusterFields(B->Reply,Item,Names,2,F) || F[0]==XREDIS_REPLY_NONE || F[1]==XREDIS_REPLY_NONE)return XREDIS_CLUSTER_MALFORMED;
            Ranges=xredisClusterGet(B->Reply,F[0]);Nodes=xredisClusterGet(B->Reply,F[1]);
            if(!xredisClusterArray(Ranges) || Ranges.ChildCount%2 || !xredisClusterArray(Nodes) || !Nodes.ChildCount)return XREDIS_CLUSTER_MALFORMED;
            Node=Nodes.FirstChild;Count=Nodes.ChildCount;
        }
        if(Count>B->Limits->MaxNodes-B->Nodes)return XREDIS_CLUSTER_LIMIT;
        for(size_t K=0;K<Count;++K){xredisclusternode N;xredisclusterstatus S=xredisClusterReadNode(B,Node,Legacy,K==0,&N);if(S!=XREDIS_CLUSTER_VALUE)return S;
            if(N.Role==XREDIS_CLUSTER_MASTER){if(Master!=SIZE_MAX)return XREDIS_CLUSTER_MALFORMED;Master=B->Nodes;}
            if(B->Output)B->Output->Nodes[B->Nodes]=N;
            ++B->Nodes;Node=xredisClusterGet(B->Reply,Node).Next;
        }
        if(Master==SIZE_MAX)return XREDIS_CLUSTER_MALFORMED;
        if(Legacy){xredisreplyvalue First=xredisClusterGet(B->Reply,Item.FirstChild);xredisclusterstatus S=xredisClusterBuildRange(B,First,xredisClusterGet(B->Reply,First.Next),Master);if(S!=XREDIS_CLUSTER_VALUE)return S;}
        else for(size_t Pair=Ranges.FirstChild;Pair!=XREDIS_REPLY_NONE;){xredisreplyvalue First=xredisClusterGet(B->Reply,Pair),Last=xredisClusterGet(B->Reply,First.Next);xredisclusterstatus S=xredisClusterBuildRange(B,First,Last,Master);if(S!=XREDIS_CLUSTER_VALUE)return S;Pair=Last.Next;}
        ++B->Shards;Row=Item.Next;
    }
    if(B->Limits->RequireFullCoverage && B->Assigned!=XREDIS_CLUSTER_SLOTS)return XREDIS_CLUSTER_MALFORMED;
    return XREDIS_CLUSTER_VALUE;
}
XREDIS_API xredisclusterstatus xredisClusterTopologyCreate(xredisreply* R,xredisclusterformat Format,const xredisclusterlimits* Config,xredisclustertopology** Output)
{
    if(!__xredisClusterReplyOutput(R,Output,sizeof(*Output)) || (Format!=XREDIS_CLUSTER_SHARDS && Format!=XREDIS_CLUSTER_LEGACY_SLOTS) ||
        (Config && (!__xredisClusterReplyOutput(R,Config,sizeof(*Config)) || xrtMemRangesOverlap(Config,sizeof(*Config),Output,sizeof(*Output)) || Config->Size!=sizeof(*Config)))){
        xredisClusterArgument("invalid Cluster topology arguments");return XREDIS_CLUSTER_ERROR;}
    xredisclusterlimits K;if(Config)K=*Config;else xredisClusterLimitsInit(&K);
    if(K.MaxNodes>65535 || K.MaxShards>XREDIS_CLUSTER_SLOTS || K.MaxRanges>XREDIS_CLUSTER_SLOTS){xredisClusterArgument("invalid Cluster topology bounds");return XREDIS_CLUSTER_ERROR;}
    xredisreplyinfo I={0};I.Size=sizeof(I);xredisReplyInfo(R,&I);
    if(I.Nodes>K.MaxReplyNodes || I.PayloadBytes>K.MaxPayloadBytes)return XREDIS_CLUSTER_LIMIT;
    xredisreplyvalue Root=xredisClusterGet(R,I.Root);
    if(Root.Type==XREDIS_REPLY_SIMPLE_ERROR || Root.Type==XREDIS_REPLY_BULK_ERROR)return XREDIS_CLUSTER_SERVER_ERROR;
    xredisclusterbuild B={0};B.Reply=R;B.Limits=&K;
    xredisclusterstatus S=xredisClusterBuild(&B,Root,Format==XREDIS_CLUSTER_LEGACY_SLOTS);if(S!=XREDIS_CLUSTER_VALUE)return S;
    size_t Bytes=sizeof(xredisclustertopology)+B.Nodes*sizeof(xredisclusternode)+B.Ranges*sizeof(xredisclusterrange);
    if(Bytes>K.MaxOwnedBytes || I.ReservedBytes>K.MaxOwnedBytes-Bytes)return XREDIS_CLUSTER_LIMIT;
    xredisclustertopology* T=xrtCalloc(1,Bytes);if(!T)return XREDIS_CLUSTER_MEMORY;
    T->Nodes=(xredisclusternode*)(T+1);T->Ranges=(xredisclusterrange*)(T->Nodes+B.Nodes);
    T->Info=(xredisclustertopologyinfo){sizeof(T->Info),B.Shards,B.Nodes,B.Ranges,B.Assigned,Bytes,I.ReservedBytes,Format};
    xredisclusterbuild Fill={0};Fill.Reply=R;Fill.Limits=&K;Fill.Output=T;
    S=xredisClusterBuild(&Fill,Root,Format==XREDIS_CLUSTER_LEGACY_SLOTS);
    if(S!=XREDIS_CLUSTER_VALUE){xrtFree(T);return S;}
    if(!xredisReplyRetain(R)){xrtFree(T);return XREDIS_CLUSTER_ERROR;}
    T->Reply=R;xrtAtomic32Init(&T->References,1);*Output=T;return XREDIS_CLUSTER_VALUE;
}
XREDIS_API bool xredisClusterTopologyRetain(xredisclustertopology* T)
{
    if(!T)return xredisClusterArgument("Cluster topology is NULL");
    uint32 N=xrtAtomic32Load(&T->References,XMEMORY_RELAXED);
    for(;;){if(N==UINT32_MAX){xrtSetErrorInfo(XERR_RANGE,"xrt.redis",XREDIS_REPLY_LIMIT,"Cluster topology reference overflow");return false;}
        if(xrtAtomic32CompareExchange(&T->References,&N,N+1,XMEMORY_RELAXED,XMEMORY_RELAXED))return true;}
}
XREDIS_API void xredisClusterTopologyRelease(xredisclustertopology* T)
{if(T && xrtAtomic32FetchSub(&T->References,1,XMEMORY_ACQ_REL)==1){xredisReplyRelease(T->Reply);xrtFree(T);}}
static bool xredisClusterOutput(const xredisclustertopology* T,const void* Output,size_t Bytes)
{
    if(!T || !__xredisClusterReplyOutput(T->Reply,Output,Bytes) || xrtMemRangesOverlap(T,T->Info.StorageBytes,Output,Bytes))return xredisClusterArgument("invalid Cluster topology output");
    return true;
}
bool __xredisClusterTopologyOutput(const xredisclustertopology* T,const void* Output,size_t Bytes)
{return xredisClusterOutput(T,Output,Bytes);}
XREDIS_API bool xredisClusterTopologyInfo(const xredisclustertopology* T,xredisclustertopologyinfo* I)
{if(!xredisClusterOutput(T,I,sizeof(*I)) || I->Size!=sizeof(*I))return xredisClusterArgument("invalid Cluster topology info size");*I=T->Info;return true;}
XREDIS_API xredisclusterstatus xredisClusterTopologyLookup(const xredisclustertopology* T,uint16 Slot,xredisclusternode* N)
{
    if(!xredisClusterOutput(T,N,sizeof(*N)) || Slot>=XREDIS_CLUSTER_SLOTS){xredisClusterArgument("invalid Cluster slot lookup");return XREDIS_CLUSTER_ERROR;}
    uint16 Index=T->Slots[Slot];if(!Index)return XREDIS_CLUSTER_ABSENT;
    *N=T->Nodes[Index-1];return XREDIS_CLUSTER_VALUE;
}
XREDIS_API bool xredisClusterTopologyNode(const xredisclustertopology* T,size_t Index,xredisclusternode* N)
{if(!xredisClusterOutput(T,N,sizeof(*N)) || Index>=T->Info.Nodes)return xredisClusterArgument("invalid Cluster node index");*N=T->Nodes[Index];return true;}
XREDIS_API bool xredisClusterTopologyRange(const xredisclustertopology* T,size_t Index,xredisclusterrange* N)
{if(!xredisClusterOutput(T,N,sizeof(*N)) || Index>=T->Info.Ranges)return xredisClusterArgument("invalid Cluster range index");*N=T->Ranges[Index];return true;}
#endif
