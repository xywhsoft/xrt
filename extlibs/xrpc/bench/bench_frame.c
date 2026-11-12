#include <xrpc/frame.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):1000000;unsigned Format=Argc>2?(unsigned)strtoul(Argv[2],NULL,10):1,Mode=Argc>3?(unsigned)strtoul(Argv[3],NULL,10):1;bool Heap=Argc>4 && strtoul(Argv[4],NULL,10);size_t Payload=Argc>5?(size_t)strtoull(Argv[5],NULL,10):256;
    if(!Count || Count>100000000 || Format<1 || Format>2 || Mode>3 || !Payload || Payload>4096 || Argc>6)return 2;
    unsigned char Body[4096],Wire[4200],Output[4200];memset(Body,'x',Payload);xrpcframeconfig C;xrpcFrameConfigInit(&C);C.Format=(xrpcframeformat)Format;size_t Head=0,Bytes=0;
    if(!xrpcFrameHeaderSize(Payload,&C,&Head) || !xrpcFrameWrite((xbytesview){Body,Payload},&C,Wire,sizeof(Wire),&Bytes))return 3;
    xrpcframereader* R=NULL;if(!xrpcFrameReaderCreate(&C,&R) || xrpcFrameReaderReserve(R,Format==1?128:0,4096)!=XRPC_FRAME_READY)return 4;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Heap);xrtMemStatsGet(&Before);
#else
    if(Heap)return 5;
#endif
    volatile const unsigned char* Raw=Wire;
    uint64 Sum=0;double Start=xrtTimer();for(size_t I=0;I<Count;++I){size_t Used=0;xrpcframeview V;
        if(Mode==0){Sum+=Raw[Head]+Payload;continue;}
        if(Mode==3){if(!xrpcFrameWrite((xbytesview){Body,Payload},&C,Output,sizeof(Output),&Used) || Used!=Bytes)return 6;Sum+=Output[Head]+Payload;continue;}
        if(Mode==1){if(xrpcFrameReaderFeed(R,(xbytesview){Wire,Bytes},&Used,&V)!=XRPC_FRAME_READY || Used!=Bytes || V.Buffered)return 7;}
        else {size_t Cut=Head+Payload/2;if(xrpcFrameReaderFeed(R,(xbytesview){Wire,Cut},&Used,&V)!=XRPC_FRAME_MORE || Used!=Cut || xrpcFrameReaderFeed(R,(xbytesview){Wire+Cut,Bytes-Cut},&Used,&V)!=XRPC_FRAME_READY || !V.Buffered)return 8;}
        if(V.Body.Size!=Payload)return 9;
        Sum+=V.Body.Data[0]+V.Body.Size;
    }double Seconds=xrtTimer()-Start;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    printf("frames_per_sec: %.6f\nns_per_frame: %.6f\nchecksum: %" PRIu64 "\n",Count/Seconds,Seconds*1e9/Count,Sum);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Heap)printf("heap_blocks_per_frame: %.6f\nheap_bytes_per_frame: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count);
#endif
    xrpcFrameReaderDestroy(R);return 0;
}
