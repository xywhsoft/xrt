#include <xredis/role.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
static void roleBenchRequire(bool OK,const char* Text){if(!OK){fprintf(stderr,"[FAIL] ROLE benchmark: %s\n",Text);exit(1);}}
static xredisreply* roleBenchParse(xredisreplyreader* Reader,const char* Wire,size_t Bytes)
{xredisreply* R=NULL;size_t Offset=0;while(Offset<Bytes){size_t Used=0;xredisreplystatus S=xredisReplyReaderFeed(Reader,(xbytesview){(cbytes)Wire+Offset,Bytes-Offset},&Used,&R);roleBenchRequire(S!=XREDIS_REPLY_ERROR && (Used || S==XREDIS_REPLY_YIELD),"parse progress");Offset+=Used;}roleBenchRequire(R!=NULL,"owned frame");return R;}
int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):100000,Width=Argc>2?(size_t)strtoull(Argv[2],NULL,10):1;unsigned Mode=Argc>3?(unsigned)strtoul(Argv[3],NULL,10):0;bool Instrument=Argc>4 && strtoul(Argv[4],NULL,10)!=0;
    if(!Count || Count>100000000 || Width>4096 || Mode>2 || Argc>5)return 2;
    size_t Capacity=64+Width*64;char* Wire=malloc(Capacity);roleBenchRequire(Wire!=NULL,"caller fixture");size_t Bytes=(size_t)snprintf(Wire,Capacity,"*3\r\n+master\r\n:123\r\n*%zu\r\n",Width);
    for(size_t N=0;N<Width;++N)Bytes+=(size_t)snprintf(Wire+Bytes,Capacity-Bytes,"*3\r\n+h\r\n+6380\r\n+99\r\n");
    xredisreplyreader* Reader=NULL;roleBenchRequire(xredisReplyReaderCreate(NULL,&Reader),"reader");xredisreply* R=roleBenchParse(Reader,Wire,Bytes);xredisroleinfo Initial;roleBenchRequire(xredisRoleRead(R,NULL,&Initial)==XREDIS_ROLE_VALUE,"initial role");
    if(Mode==1){xredisReplyRelease(R);R=NULL;}
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Instrument);xrtMemStatsGet(&Before);
#else
    if(Instrument)return 2;
#endif
    uint64 Sum=0;double Start=xrtTimer();
    for(size_t N=0;N<Count;++N){if(Mode==1)R=roleBenchParse(Reader,Wire,Bytes);xredisroleinfo I;if(Mode==2)I=Initial;else roleBenchRequire(xredisRoleRead(R,NULL,&I)==XREDIS_ROLE_VALUE,"full role validation");
        Sum+=(uint64)I.Offset;size_t Seen=0;xredisrolereplica Peer;while(xredisRoleReplicaNext(&I.Replicas,&Peer)){Sum+=Peer.Port+(uint64)Peer.Offset+Peer.Host.Size;++Seen;}roleBenchRequire(Seen==Width,"complete traversal");if(Mode==1){xredisReplyRelease(R);R=NULL;}}
    double Seconds=xrtTimer()-Start;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    roleBenchRequire(Sum==(uint64)Count*(123+Width*6480),"semantic checksum");xredisReplyRelease(R);xredisReplyReaderDestroy(Reader);free(Wire);
    printf("roles_per_sec: %.3f\nns_per_role: %.3f\nchecksum: %" PRIu64 "\n",Count/Seconds,Seconds*1e9/Count,Sum);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Instrument)printf("heap_blocks_per_role: %.6f\nheap_bytes_per_role: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count);
#endif
    return 0;
}
