#include "test.h"
#include <xredis/cluster_topology.h>
#include <xrt/memory.h>
#if defined(XRT_FEATURE_THREAD)
#include <xrt/thread.h>
#endif
#include "support/cluster_topology_fixture.h"
static xredisreply* ctParse(const void* Data,size_t Bytes,size_t Fragment)
{
    xredisreplyreader* Reader=NULL;xredisreply* R=NULL;testRequire(xredisReplyReaderCreate(NULL,&Reader),"cluster reader");size_t Offset=0;
    while(Offset<Bytes){size_t N=Bytes-Offset;if(N>Fragment)N=Fragment;size_t Used=0;
        xredisreplystatus S=xredisReplyReaderFeed(Reader,(xbytesview){(cbytes)Data+Offset,N},&Used,&R);
        testRequire(S!=XREDIS_REPLY_ERROR && (Used || S==XREDIS_REPLY_YIELD),"fragmented cluster frame");Offset+=Used;
        if(S==XREDIS_REPLY_READY){testRequire(Offset==Bytes,"one cluster frame");break;}}
    testRequire(R && xredisReplyReaderFinish(Reader),"complete cluster frame");xredisReplyReaderDestroy(Reader);return R;
}
static uint16 ctSlotReference(xbytesview Key)
{
    size_t A=0,Z=Key.Size;for(size_t I=0;I<Key.Size;++I)if(Key.Data[I]=='{'){
        for(size_t J=I+1;J<Key.Size;++J){if(Key.Data[J]=='}'){if(J>I+1){A=I+1;Z=J;}goto found;}}
        break;}
found:;
    uint32 C=0;for(size_t I=A;I<Z;++I){C^=(uint32)Key.Data[I]<<8;for(unsigned Bit=0;Bit<8;++Bit)C=((C<<1)^((C&32768)?0x1021u:0u))&65535u;}return (uint16)(C%16384u);
}
static void ctKeys(void)
{
    const cstr Names[]={"","123456789","foo","bar","{user1000}.following","foo{}{bar}","foo{{bar}}zap","foo{bar}{zap}","foo{bar","{a}","{}"};
    for(size_t I=0;I<sizeof(Names)/sizeof(*Names);++I){xbytesview K={(cbytes)Names[I],strlen(Names[I])};uint16 Slot=65535;testRequire(xredisClusterKeySlot(K,&Slot) && Slot==ctSlotReference(K),"CRC/hash tag vectors");if(I==1)testRequire(Slot==12739,"XMODEM check vector");if(I==2)testRequire(Slot==12182,"Redis foo vector");}
    unsigned char Bytes[257];uint32 State=0x12345678u;
    for(size_t I=0;I<12000;++I){size_t N=I%sizeof(Bytes);for(size_t J=0;J<N;++J){State=State*1664525u+1013904223u;Bytes[J]=(unsigned char)(State>>24);}if(N>8 && I%3==0){Bytes[1]='{';Bytes[N-2]='}';}
        uint16 S;testRequire(xredisClusterKeySlot((xbytesview){Bytes,N},&S) && S==ctSlotReference((xbytesview){Bytes,N}),"independent binary CRC oracle");}
    uint16 S=123;testRequire(!xredisClusterKeySlot((xbytesview){(cbytes)(uintptr_t)(UINTPTR_MAX-1),4},&S) && S==123,"wrapped key span");xrtClearError();
    testRequire(!xredisClusterKeySlot((xbytesview){(cbytes)&S,sizeof(S)},&S) && S==123,"key/output overlap");xrtClearError();
}
static void ctRedirects(void)
{
    struct {cstr Wire;xredisclusterstatus Status;uint16 Slot,Port;size_t Host;xredisclusterredirectkind Kind;xredisclusterendpoint Endpoint;} Cases[]={
        {"-MOVED 3999 127.0.0.1:6381\r\n",XREDIS_CLUSTER_VALUE,3999,6381,9,XREDIS_CLUSTER_MOVED,XREDIS_CLUSTER_ENDPOINT_HOST},
        {"-ASK 0 [::1]:65535\r\n",XREDIS_CLUSTER_VALUE,0,65535,3,XREDIS_CLUSTER_ASK,XREDIS_CLUSTER_ENDPOINT_HOST},
        {"-MOVED 16383 :1\r\n",XREDIS_CLUSTER_VALUE,16383,1,0,XREDIS_CLUSTER_MOVED,XREDIS_CLUSTER_ENDPOINT_SOURCE},
        {"!18\r\nASK 9 host.name:80\r\n",XREDIS_CLUSTER_VALUE,9,80,9,XREDIS_CLUSTER_ASK,XREDIS_CLUSTER_ENDPOINT_HOST},
        {"-MOVED 1 2001:db8::1:1\r\n",XREDIS_CLUSTER_VALUE,1,1,11,XREDIS_CLUSTER_MOVED,XREDIS_CLUSTER_ENDPOINT_HOST},
        {"-MOVED 1 ?:1\r\n",XREDIS_CLUSTER_VALUE,1,1,1,XREDIS_CLUSTER_MOVED,XREDIS_CLUSTER_ENDPOINT_UNKNOWN},
        {"-ERR no\r\n",XREDIS_CLUSTER_ABSENT,0,0,0,0,0},{"+MOVED 1 h:2\r\n",XREDIS_CLUSTER_ABSENT,0,0,0,0,0},
        {"-MOVEDx 1 h:2\r\n",XREDIS_CLUSTER_ABSENT,0,0,0,0,0},
        {"-MOVED\r\n",XREDIS_CLUSTER_MALFORMED,0,0,0,0,0},{"-ASK 1 h:0\r\n",XREDIS_CLUSTER_MALFORMED,0,0,0,0,0},
        {"-ASK 16384 h:1\r\n",XREDIS_CLUSTER_MALFORMED,0,0,0,0,0},{"-ASK -1 h:1\r\n",XREDIS_CLUSTER_MALFORMED,0,0,0,0,0},
        {"-ASK 1 h:65536\r\n",XREDIS_CLUSTER_MALFORMED,0,0,0,0,0},{"-ASK 1 [::1:1\r\n",XREDIS_CLUSTER_MALFORMED,0,0,0,0,0},
        {"-ASK 1 h:1 extra\r\n",XREDIS_CLUSTER_MALFORMED,0,0,0,0,0},{"-ASK 1 h@evil:2\r\n",XREDIS_CLUSTER_MALFORMED,0,0,0,0,0},
        {"-ASK  1 h:1\r\n",XREDIS_CLUSTER_MALFORMED,0,0,0,0,0},{"-ASK 000001 h:1\r\n",XREDIS_CLUSTER_MALFORMED,0,0,0,0,0}
    };
    for(size_t I=0;I<sizeof(Cases)/sizeof(*Cases);++I)for(size_t F=1;F<=strlen(Cases[I].Wire);++F){
        xredisreply* R=ctParse(Cases[I].Wire,strlen(Cases[I].Wire),F);xredisclusterredirect E;memset(&E,0x5a,sizeof(E));unsigned char Old[sizeof(E)];memcpy(Old,&E,sizeof(E));
        xrtSetErrorKind(XERR_MEMORY);const xerror* Error=xrtGetError();xredisclusterstatus S=xredisClusterRedirectRead(R,1024,&E);
        testRequire(S==Cases[I].Status && xrtGetError()==Error,"redirection classification/ambient error");
        if(S==XREDIS_CLUSTER_VALUE){testRequire(E.Kind==Cases[I].Kind && E.Slot==Cases[I].Slot && E.Port==Cases[I].Port && E.Host.Size==Cases[I].Host && E.Endpoint==Cases[I].Endpoint,"complete redirection view");
            testRequire(xredisClusterRedirectRead(R,0,&E)==(Cases[I].Host?XREDIS_CLUSTER_LIMIT:XREDIS_CLUSTER_VALUE),"endpoint zero cap");}
        else testRequire(!memcmp(Old,&E,sizeof(E)),"redirection failures preserve output");
        xrtClearError();testRequire(xredisClusterRedirectRead(R,1024,(xredisclusterredirect*)R)==XREDIS_CLUSTER_ERROR,"redirect cannot corrupt Reply");xrtClearError();xredisReplyRelease(R);
    }
}
static void ctShapes(void)
{
    const cstr Good[]={CT_SHARDS2,CT_SHARDS3,CT_SLOTS};
    for(unsigned K=0;K<3;++K)for(size_t F=1;F<=strlen(Good[K]);++F){
        xredisreply* R=ctParse(Good[K],strlen(Good[K]),F);xredisclustertopology* T=NULL;
        testRequire(xredisClusterTopologyCreate(R,K==2?XREDIS_CLUSTER_LEGACY_SLOTS:XREDIS_CLUSTER_SHARDS,NULL,&T)==XREDIS_CLUSTER_VALUE,"SHARDS RESP2/3 and legacy SLOTS");
        xredisReplyRelease(R);xredisclustertopologyinfo I={0};I.Size=sizeof(I);testRequire(xredisClusterTopologyInfo(T,&I) && I.Nodes==(K==2?3u:2u) && I.Ranges==2 && I.AssignedSlots==16294,"topology cardinality");
        xredisclusternode N;for(uint16 Slot=0;Slot<16384;++Slot){xredisclusterstatus S=xredisClusterTopologyLookup(T,Slot,&N);
            testRequire(S==((Slot>=10 && Slot<100)?XREDIS_CLUSTER_ABSENT:XREDIS_CLUSTER_VALUE),"complete slot index/partial holes");
            if(S==XREDIS_CLUSTER_VALUE){testRequire(N.Role==XREDIS_CLUSTER_MASTER && N.Port==(K==2 && Slot<10?6378:6379),"master route");if(K!=2)testRequire(N.TlsPort==6380 && N.ReplicationOffset==INT64_MAX && N.Health==XREDIS_CLUSTER_ONLINE,"lossless native offset and TLS port");}}
        xredisclusterrange Range;testRequire(xredisClusterTopologyRange(T,0,&Range) && Range.First==(K==2?100:0),"wire range order retained");
        testRequire(xredisClusterTopologyNode(T,1,&N) && N.Role==XREDIS_CLUSTER_REPLICA,"replica metadata retained");
        if(K!=2)testRequire(N.Endpoint==XREDIS_CLUSTER_ENDPOINT_SOURCE && N.Health==XREDIS_CLUSTER_HEALTH_OTHER && N.HealthText.Size==13 && N.ReplicationOffset==-1,"unknown health/source host/offline offset");
        xredisClusterTopologyRelease(T);
    }
    struct {cstr Wire;xredisclusterformat Format;xredisclusterstatus Status;} Bad[]={
        {"*1\r\n*3\r\n:0\r\n:16384\r\n*2\r\n+h\r\n:1\r\n",XREDIS_CLUSTER_LEGACY_SLOTS,XREDIS_CLUSTER_MALFORMED},
        {"*1\r\n*3\r\n:2\r\n:1\r\n*2\r\n+h\r\n:1\r\n",XREDIS_CLUSTER_LEGACY_SLOTS,XREDIS_CLUSTER_MALFORMED},
        {"*2\r\n*3\r\n:0\r\n:9\r\n*2\r\n+h\r\n:1\r\n*3\r\n:9\r\n:10\r\n*2\r\n+h\r\n:1\r\n",XREDIS_CLUSTER_LEGACY_SLOTS,XREDIS_CLUSTER_MALFORMED},
        {"*1\r\n*3\r\n:0\r\n:1\r\n*2\r\n+h\r\n:65536\r\n",XREDIS_CLUSTER_LEGACY_SLOTS,XREDIS_CLUSTER_MALFORMED},
        {"*1\r\n*4\r\n+slots\r\n*0\r\n+nodes\r\n*0\r\n",XREDIS_CLUSTER_SHARDS,XREDIS_CLUSTER_MALFORMED},
        {"*1\r\n*4\r\n+slots\r\n*0\r\n+slots\r\n*0\r\n",XREDIS_CLUSTER_SHARDS,XREDIS_CLUSTER_MALFORMED},
        {"*1\r\n*3\r\n+slots\r\n*0\r\n+nodes\r\n",XREDIS_CLUSTER_SHARDS,XREDIS_CLUSTER_MALFORMED},
        {"-ERR denied\r\n",XREDIS_CLUSTER_SHARDS,XREDIS_CLUSTER_SERVER_ERROR},
        {"!3\r\nbad\r\n",XREDIS_CLUSTER_LEGACY_SLOTS,XREDIS_CLUSTER_SERVER_ERROR},
        {"*-1\r\n",XREDIS_CLUSTER_SHARDS,XREDIS_CLUSTER_MALFORMED}
    };
    for(size_t K=0;K<sizeof(Bad)/sizeof(*Bad);++K){xredisreply* R=ctParse(Bad[K].Wire,strlen(Bad[K].Wire),1);xredisclustertopology* T=(xredisclustertopology*)(uintptr_t)1;
        xrtSetErrorKind(XERR_MEMORY);const xerror* Error=xrtGetError();testRequire(xredisClusterTopologyCreate(R,Bad[K].Format,NULL,&T)==Bad[K].Status && T==(void*)(uintptr_t)1 && xrtGetError()==Error,"invalid shape atomic/non-diagnostic");xrtClearError();xredisReplyRelease(R);}
}
static void ctLimitsOwnership(void)
{
    xredisreply* R=ctParse(CT_SHARDS3,strlen(CT_SHARDS3),1);xredisclustertopology* T=NULL;xredisclusterlimits K;testRequire(xredisClusterLimitsInit(&K),"cluster defaults");
    testRequire(xredisClusterTopologyCreate(R,XREDIS_CLUSTER_SHARDS,NULL,&T)==XREDIS_CLUSTER_VALUE,"snapshot owner");xredisclustertopologyinfo I={0};I.Size=sizeof(I);testRequire(xredisClusterTopologyInfo(T,&I),"snapshot info");
    xredisreplyinfo RI={0};RI.Size=sizeof(RI);xredisReplyInfo(R,&RI);
    K.MaxShards=I.Shards;K.MaxNodes=I.Nodes;K.MaxRanges=I.Ranges;K.MaxReplyNodes=RI.Nodes;K.MaxPayloadBytes=RI.PayloadBytes;K.MaxEndpointBytes=5;K.MaxOwnedBytes=I.StorageBytes+I.RetainedReplyBytes;
    xredisclustertopology* Exact=NULL;testRequire(xredisClusterTopologyCreate(R,XREDIS_CLUSTER_SHARDS,&K,&Exact)==XREDIS_CLUSTER_VALUE,"exact snapshot caps");xredisClusterTopologyRelease(Exact);
    size_t* Caps[]={&K.MaxShards,&K.MaxNodes,&K.MaxRanges,&K.MaxReplyNodes,&K.MaxPayloadBytes,&K.MaxEndpointBytes,&K.MaxOwnedBytes};
    for(size_t J=0;J<sizeof(Caps)/sizeof(*Caps);++J){--*Caps[J];Exact=(void*)(uintptr_t)1;testRequire(xredisClusterTopologyCreate(R,XREDIS_CLUSTER_SHARDS,&K,&Exact)==XREDIS_CLUSTER_LIMIT && Exact==(void*)(uintptr_t)1,"each independent topology cap");++*Caps[J];}
    K.RequireFullCoverage=true;testRequire(xredisClusterTopologyCreate(R,XREDIS_CLUSTER_SHARDS,&K,&Exact)==XREDIS_CLUSTER_MALFORMED,"explicit full-coverage requirement");K.RequireFullCoverage=false;
    xredisclusternode N;testRequire(xredisClusterTopologyNode(T,0,&N),"borrowed node");
    testRequire(xredisClusterTopologyLookup(T,0,(xredisclusternode*)N.Host.Data)==XREDIS_CLUSTER_ERROR,"retained Reply payload cannot be output");xrtClearError();
    testRequire(xredisClusterTopologyCreate(R,XREDIS_CLUSTER_SHARDS,NULL,(xredisclustertopology**)R)==XREDIS_CLUSTER_ERROR,"Reply header cannot be creation output");xrtClearError();
    testRequire(xredisClusterTopologyLookup(T,0,(xredisclusternode*)T)==XREDIS_CLUSTER_ERROR,"snapshot storage cannot be output");xrtClearError();
    union {xredisclusterlimits K;xredisclustertopology* T;} Alias;Alias.K=K;testRequire(xredisClusterTopologyCreate(R,XREDIS_CLUSTER_SHARDS,&Alias.K,&Alias.T)==XREDIS_CLUSTER_ERROR,"config/output overlap");xrtClearError();
    K.Size=0;testRequire(xredisClusterTopologyCreate(R,XREDIS_CLUSTER_SHARDS,&K,&Exact)==XREDIS_CLUSTER_ERROR,"versioned limits");xrtClearError();K.Size=sizeof(K);
    testRequire(xredisClusterTopologyCreate(R,(xredisclusterformat)0,NULL,&Exact)==XREDIS_CLUSTER_ERROR,"explicit topology format");xrtClearError();
    testRequire(xredisClusterTopologyLookup(T,65535,&N)==XREDIS_CLUSTER_ERROR,"bad slot");xrtClearError();
    testRequire(!xredisClusterTopologyNode(T,I.Nodes,&N),"bad node index");xrtClearError();
    xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B) && xrtAllocationBudgetEnter(B,&Scope),"one-byte snapshot budget");
    Exact=(void*)(uintptr_t)1;testRequire(xredisClusterTopologyCreate(R,XREDIS_CLUSTER_SHARDS,NULL,&Exact)==XREDIS_CLUSTER_MEMORY && Exact==(void*)(uintptr_t)1,"real snapshot allocation OOM is atomic");xrtClearError();
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats A,Z;xrtMemStatsEnable(true);xrtMemStatsGet(&A);
#endif
    for(size_t J=0;J<10000;++J){testRequire(xredisClusterTopologyLookup(T,0,&N)==XREDIS_CLUSTER_VALUE,"allocation-free routing");uint16 S;testRequire(xredisClusterKeySlot(XRT_BYTES_LITERAL("a{tag}\0b"),&S),"allocation-free binary key slot");}
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&Z);xrtMemStatsEnable(false);testRequire(A.MallocCalls==Z.MallocCalls && A.CallocCalls==Z.CallocCalls && A.ReallocCalls==Z.ReallocCalls && A.BlockAllocCalls==Z.BlockAllocCalls,"no heap calls in hot routing/key slot");
#endif
    testRequire(xrtAllocationBudgetLeave(&Scope),"leave budget");xallocationbudgetinfo BI={0};BI.Size=sizeof(BI);testRequire(xrtAllocationBudgetInfo(B,&BI) && !BI.LiveBytes && !BI.LiveCount,"OOM leaves no allocation");xrtAllocationBudgetFree(B);
    xredisReplyRelease(R);testRequire(xredisClusterTopologyRetain(T),"escape topology lifetime");xredisClusterTopologyRelease(T);testRequire(xredisClusterTopologyLookup(T,100,&N)==XREDIS_CLUSTER_VALUE && N.Host.Size==3 && !memcmp(N.Host.Data,"::1",3),"snapshot pins Reply after reader/source release");xredisClusterTopologyRelease(T);
    R=ctParse("*0\r\n",4,1);xredisClusterLimitsInit(&K);K.MaxShards=K.MaxNodes=K.MaxRanges=0;testRequire(xredisClusterTopologyCreate(R,XREDIS_CLUSTER_SHARDS,&K,&T)==XREDIS_CLUSTER_VALUE,"zero real caps accept empty map");memset(&N,0x5a,sizeof(N));unsigned char Old[sizeof(N)];memcpy(Old,&N,sizeof(N));xrtSetErrorKind(XERR_MEMORY);const xerror* Error=xrtGetError();testRequire(xredisClusterTopologyLookup(T,0,&N)==XREDIS_CLUSTER_ABSENT && !memcmp(Old,&N,sizeof(N)) && xrtGetError()==Error,"unassigned slot preserves output/error");xrtClearError();xredisClusterTopologyRelease(T);xredisReplyRelease(R);
}
#if defined(XRT_FEATURE_THREAD)
typedef struct ctthread {xredisclustertopology* Topology;uint64 Sum;} ctthread;
static int32 ctThread(ptr User)
{ctthread* T=User;for(unsigned I=0;I<10000;++I){xredisclusternode N;if(xredisClusterTopologyLookup(T->Topology,(uint16)(100+I),&N)!=XREDIS_CLUSTER_VALUE)return 1;T->Sum+=N.Port;}xredisClusterTopologyRelease(T->Topology);return 0;}
static void ctThreads(void)
{xredisreply* R=ctParse(CT_SHARDS3,strlen(CT_SHARDS3),1);xredisclustertopology* T=NULL;testRequire(xredisClusterTopologyCreate(R,XREDIS_CLUSTER_SHARDS,NULL,&T)==XREDIS_CLUSTER_VALUE,"thread topology");xredisReplyRelease(R);ctthread State[4]={{0}};xthread* Threads[4];for(unsigned I=0;I<4;++I){testRequire(xredisClusterTopologyRetain(T),"thread retain");State[I].Topology=T;Threads[I]=xrtThreadCreate(ctThread,State+I,0);testRequire(Threads[I]!=NULL,"routing reader thread");}xredisClusterTopologyRelease(T);for(unsigned I=0;I<4;++I){testRequire(xrtThreadWait(Threads[I])==XWAIT_OK && xrtThreadExitCode(Threads[I])==0 && State[I].Sum==63790000,"immutable concurrent routing");xrtThreadDestroy(Threads[I]);}}
#endif
int main(void)
{ctKeys();ctRedirects();ctShapes();ctLimitsOwnership();
#if defined(XRT_FEATURE_THREAD)
ctThreads();
#endif
puts("[PASS] Cluster slots/redirects/SHARDS/SLOTS: binary oracle, complete slot table, fragments, limits, ownership, aliases, OOM and concurrent snapshots");return 0;}
