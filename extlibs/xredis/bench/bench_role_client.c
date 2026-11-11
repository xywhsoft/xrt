#include "../tests/support/role_client_peer.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
static int roleSetupCompare(const void* A,const void* B){double X=*(const double*)A,Y=*(const double*)B;return X<Y?-1:X>Y;}
int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):100;bool Manual=Argc>2 && strtoul(Argv[2],NULL,10)!=0;bool Instrument=Argc>3 && strtoul(Argv[3],NULL,10)!=0;if(!Count || Count>10000 || Argc>4)return 2;
    double* Samples=malloc(Count*sizeof(*Samples));redisPeerRequire(Samples!=NULL,"caller benchmark samples");uint64 Blocks=0,Bytes=0;double Total=0;
    for(size_t N=0;N<Count;++N){const redisinitpeerstep Step={{XRT_BYTES_LITERAL("ROLE")},1,XRT_BYTES_LITERAL(ROLE_MASTER_REPLY),false,false};roleclientpeer P;roleClientPeerStart(&P,false,false,&Step,1);xredisclientinitconfig Init;xredisClientInitConfigInit(&Init);Init.Mode=XREDIS_INIT_RESP2;Init.Flags=Manual?0:XREDIS_INIT_REQUIRE_MASTER;
#if defined(XRT_FEATURE_MEMORY_STATS)
        xmemstats A,Z;xrtMemStatsEnable(Instrument);xrtMemStatsGet(&A);
#else
        if(Instrument)return 2;
#endif
        double Start=xrtTimer();xredisclient* C=NULL;redisPeerRequire(roleClientPeerConnect(&P,0,NULL,&Init,&C),"role benchmark admission");xfuture* Ready=xredisClientReady(C);const xredisreadyresult* R=roleClientWait(Ready);redisPeerRequire(R->Status==XREDIS_READY_OK,"role benchmark ready");xfuture* Call=NULL;
        if(Manual){double Remaining=5000-(xrtTimer()-Start)*1000;redisPeerRequire(Remaining>0,"manual outer deadline");xbytesview Role=XRT_BYTES_LITERAL("ROLE");redisPeerRequire(xredisClientCallAsync(C,&Role,1,(int64)Remaining,&Call)==XREDIS_ADMISSION_ACCEPTED,"manual ROLE admission");const xredisrequestresult* Result=roleClientWait(Call);xredisroleinfo Info;redisPeerRequire(Result->Status==XREDIS_REQUEST_OK && xredisRoleRead(Result->Reply,NULL,&Info)==XREDIS_ROLE_VALUE && Info.Kind==XREDIS_ROLE_MASTER,"manual full role verification");}
        Samples[N]=(xrtTimer()-Start)*1000;Total+=Samples[N];
#if defined(XRT_FEATURE_MEMORY_STATS)
        xrtMemStatsGet(&Z);xrtMemStatsEnable(false);Blocks+=Z.BlockAllocCalls-A.BlockAllocCalls;Bytes+=Z.BlockAllocBytes-A.BlockAllocBytes;
#endif
        xrtFutureDestroy(Call);xrtFutureDestroy(Ready);roleClientClose(&P,C);
    }
    qsort(Samples,Count,sizeof(*Samples),roleSetupCompare);size_t P99=(Count*99+99)/100-1;
    printf("setups_per_sec: %.3f\nms_per_setup: %.6f\np99_setup_ms: %.6f\nheap_blocks_per_setup: %.6f\nheap_bytes_per_setup: %.6f\n",Count*1000/Total,Total/Count,Samples[P99],(double)Blocks/Count,(double)Bytes/Count);free(Samples);return 0;
}
