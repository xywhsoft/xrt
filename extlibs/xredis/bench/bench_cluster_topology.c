#include <xredis/cluster_topology.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
static void ctBenchRequire(bool OK){if(!OK){fputs("[FAIL] Cluster benchmark\n",stderr);exit(1);}}
int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):1000000;unsigned Mode=Argc>2?(unsigned)strtoul(Argv[2],NULL,10):0;bool Instrument=Argc>3 && strtoul(Argv[3],NULL,10)!=0;
    if(!Count || Count>100000000 || Mode>3 || Argc>4)return 2;
    static const char Wire[]="*1\r\n*3\r\n:0\r\n:16383\r\n*2\r\n+127.0.0.1\r\n:6379\r\n";
    static const char Redirect[]="-MOVED 1234 127.0.0.1:6379\r\n";
    xredisreplyreader* Reader=NULL;xredisreply *R=NULL,*E=NULL;size_t Used=0;xredisclustertopology* T=NULL;
    ctBenchRequire(xredisReplyReaderCreate(NULL,&Reader));ctBenchRequire(xredisReplyReaderFeed(Reader,(xbytesview){(cbytes)Wire,sizeof(Wire)-1},&Used,&R)==XREDIS_REPLY_READY);
    ctBenchRequire(xredisReplyReaderFeed(Reader,(xbytesview){(cbytes)Redirect,sizeof(Redirect)-1},&Used,&E)==XREDIS_REPLY_READY);
    ctBenchRequire(xredisClusterTopologyCreate(R,XREDIS_CLUSTER_LEGACY_SLOTS,NULL,&T)==XREDIS_CLUSTER_VALUE);
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats A,Z;xrtMemStatsEnable(Instrument);xrtMemStatsGet(&A);
#else
    if(Instrument)return 2;
#endif
    uint64 Sum=0;double Start=xrtTimer();
    for(size_t I=0;I<Count;++I){
        if(Mode==0){xredisclusternode N;ctBenchRequire(xredisClusterTopologyLookup(T,(uint16)(I&16383),&N)==XREDIS_CLUSTER_VALUE);Sum+=N.Port;}
        else if(Mode==1){uint16 S;ctBenchRequire(xredisClusterKeySlot(XRT_BYTES_LITERAL("customer:{123456789}:order"),&S));Sum+=S;}
        else if(Mode==2){xredisclustertopology* Map=NULL;ctBenchRequire(xredisClusterTopologyCreate(R,XREDIS_CLUSTER_LEGACY_SLOTS,NULL,&Map)==XREDIS_CLUSTER_VALUE);xredisClusterTopologyRelease(Map);Sum+=1;}
        else{xredisclusterredirect D;ctBenchRequire(xredisClusterRedirectRead(E,1024,&D)==XREDIS_CLUSTER_VALUE);Sum+=D.Slot;}
    }
    double Seconds=xrtTimer()-Start;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&Z);xrtMemStatsEnable(false);
#endif
    ctBenchRequire(Sum==(uint64)Count*(Mode==0?6379u:Mode==1?12739u:Mode==2?1u:1234u));
    printf("operations_per_sec: %.3f\nns_per_operation: %.3f\nchecksum: %" PRIu64 "\n",Count/Seconds,Seconds*1e9/Count,Sum);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Instrument)printf("heap_blocks_per_operation: %.6f\nheap_bytes_per_operation: %.6f\n",(double)(Z.BlockAllocCalls-A.BlockAllocCalls)/Count,(double)(Z.BlockAllocBytes-A.BlockAllocBytes)/Count);
#endif
    xredisClusterTopologyRelease(T);xredisReplyRelease(R);xredisReplyRelease(E);xredisReplyReaderDestroy(Reader);return 0;
}
