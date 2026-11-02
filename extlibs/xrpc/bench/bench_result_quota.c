#include "../src/internal/xrpc_result_quota.h"
#include <xrt/memory.h>
#include <xrt/time.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char** argv)
{
    size_t Count=argc>1?(size_t)strtoull(argv[1],NULL,10):2000000u;
    if(argc>2 || !Count || Count>100000000u || Count%2u)return 1;
    xrpcresultquota* Q=NULL;if(!xrpcResultQuotaCreate(64,65536,&Q))return 2;
    ptr Slots[64];bool Limited=false;for(size_t N=0;N<64;++N){Slots[N]=xrpcResultQuotaAlloc(Q,256,&Limited);if(!Slots[N])return 3;}
    size_t PeakBlocks,PeakBytes;uint64 Rejected;xrpcResultQuotaInfo(Q,&PeakBlocks,&PeakBytes,&Rejected);size_t Accepted=0,Refused=0;double Began=xrtTimer();
    for(size_t N=0;N<Count;++N){
        if(!(N%2u)){size_t Index=(N/2u)%64u;xrpcResultQuotaFree(Slots[Index]);Slots[Index]=xrpcResultQuotaAlloc(Q,256,&Limited);if(!Slots[Index] || Limited)return 4;++Accepted;}
        else{ptr P=xrpcResultQuotaAlloc(Q,256,&Limited);if(P || !Limited)return 5;++Refused;}
    }
    double Seconds=xrtTimer()-Began;for(size_t N=0;N<64;++N)xrpcResultQuotaFree(Slots[N]);size_t Blocks,Bytes;xrpcResultQuotaInfo(Q,&Blocks,&Bytes,&Rejected);
    if(Blocks || Bytes || Rejected!=Refused || Accepted+Refused!=Count || !(Seconds>0))return 6;
    printf("offered: %zu\naccepted: %zu\nrejected: %zu\npeak_blocks: %zu\npeak_bytes: %zu\nfinal_blocks: %zu\nfinal_bytes: %zu\nallocations_per_sec: %.9f\n",Count,Accepted,Refused,PeakBlocks,PeakBytes,Blocks,Bytes,(double)Accepted/Seconds);
    puts("workload: private result ledger microbenchmark; 64 retained 256-byte payload blocks, alternate exact-limit refusal and physical free/reallocation; count includes all attempts; no Future/JSON/network or end-to-end capacity claim");xrpcResultQuotaRelease(Q);return 0;
}
