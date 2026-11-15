#include <xrpc/ws_uri.h>
#include <xrt/memory_stats.h>
#include <xrt/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):1000000;unsigned Mode=Argc>2?(unsigned)strtoul(Argv[2],NULL,10):0;bool Heap=Argc>3&&strtoul(Argv[3],NULL,10);if(!Count||Count>100000000||Mode>1||Argc>4)return 1;
    const char* Text[]={"ws://host/rpc","wss://route.test:443/a%2Fb?x=%23","ws://127.0.0.1:8080/rpc?","wss://[::1]:443/?x=1","ws://r%6fute.test?x=%23","wss://host:000443//a/b?x=a?b","ws://host","wss://host:"};xstrview Inputs[8];for(size_t I=0;I<8;++I)Inputs[I]=(xstrview){Text[I],strlen(Text[I])};
    size_t Checksum=0;xmemstats Before,After;xrtMemStatsEnable(Heap);xrtMemStatsGet(&Before);double Start=xrtTimer();
    for(size_t I=0;I<Count;++I){xstrview Input=Inputs[I&7];if(!Mode){xrpcwsuri U={.Size=sizeof(U)};if(!xrpcWsUriParse(Input,&U))return 2;Checksum+=U.Host.Size+U.Path.Size+U.Query.Size;}
        else{xhttptarget T;if(!xrtHttpTargetParse(XRT_STR_LITERAL("GET"),Input,&T))return 3;Checksum+=T.Host.Host.Size+T.Path.Size+T.Query.Size;}}
    double Seconds=xrtTimer()-Start;xrtMemStatsGet(&After);xrtMemStatsEnable(false);printf("uris_per_sec: %.6f\nchecksum: %zu\nmode: %s\nscope: single-thread bounded URI input, Core target baseline lacks WS scheme/port/size semantics, no connection/tail acceptance\n",Count/Seconds,Checksum,Mode?"core-target":"ws-uri");
    if(Heap)printf("heap_blocks_per_uri: %.9f\nheap_bytes_per_uri: %.9f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count);
    return 0;
}
