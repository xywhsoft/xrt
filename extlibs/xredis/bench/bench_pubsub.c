#include <xredis/pubsub.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
static void pubsubBenchRequire(bool OK,const char* Text)
{if(!OK){fprintf(stderr,"[FAIL] Pub/Sub benchmark: %s\n",Text);exit(1);}}
int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):100000,Width=Argc>2?(size_t)strtoull(Argv[2],NULL,10):32;
    unsigned Mode=Argc>3?(unsigned)strtoul(Argv[3],NULL,10):0;bool Instrument=Argc>4 && strtoul(Argv[4],NULL,10)!=0;
    if(!Count || Count>10000000 || Mode>3 || Width>1048576 || (Mode>=2 && (!Width || Width>XREDIS_PUBSUB_NAMES_MAX)) || Argc>5)return 2;
    unsigned char* Wire=NULL;size_t Bytes=0;xredisreply* R=NULL;xredisreplyreader* Reader=NULL;
    xbytesview Names[XREDIS_PUBSUB_NAMES_MAX],Args[XREDIS_PUBSUB_NAMES_MAX+1];unsigned char* Output=NULL;size_t Required=0;
    if(Mode<2){
        Wire=malloc(Width+128);pubsubBenchRequire(Wire!=NULL,"caller input");int H=snprintf((char*)Wire,128,"*3\r\n$7\r\nmessage\r\n$3\r\nkey\r\n$%zu\r\n",Width);
        memset(Wire+H,'v',Width);if(Width)Wire[H]=0;memcpy(Wire+H+Width,"\r\n",2);Bytes=(size_t)H+Width+2;
        pubsubBenchRequire(xredisReplyReaderCreate(NULL,&Reader),"reader");
        if(!Mode){size_t Offset=0;while(Offset<Bytes){size_t Used=0;xredisreplystatus S=xredisReplyReaderFeed(Reader,(xbytesview){Wire+Offset,Bytes-Offset},&Used,&R);pubsubBenchRequire(S!=XREDIS_REPLY_ERROR,"setup frame");Offset+=Used;}pubsubBenchRequire(R!=NULL,"owned setup frame");}
    }else{
        for(size_t N=0;N<Width;++N){Names[N]=(xbytesview)XRT_BYTES_LITERAL("binary\0name");}
        Args[0]=(xbytesview)XRT_BYTES_LITERAL("SUBSCRIBE");memcpy(Args+1,Names,Width*sizeof(*Names));
        pubsubBenchRequire(xredisPubSubCommandSize(XREDIS_PUBSUB_CMD_SUBSCRIBE,Names,Width,&Required),"command measurement");Output=malloc(Required);pubsubBenchRequire(Output!=NULL,"caller output");
    }
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Instrument);xrtMemStatsGet(&Before);
#else
    if(Instrument)return 2;
#endif
    uint64 Sum=0;double Start=xrtTimer();
    for(size_t N=0;N<Count;++N){
        if(Mode<2){
            if(Mode){size_t Offset=0;while(Offset<Bytes){size_t Used=0;xredisreplystatus S=xredisReplyReaderFeed(Reader,(xbytesview){Wire+Offset,Bytes-Offset},&Used,&R);pubsubBenchRequire(S!=XREDIS_REPLY_ERROR,"parse workload");Offset+=Used;}pubsubBenchRequire(R!=NULL,"parsed complete workload frame");}
            xredispubsubevent E;pubsubBenchRequire(xredisPubSubEventRead(R,&E)==XREDIS_PUBSUB_EVENT && E.Kind==XREDIS_PUBSUB_MESSAGE && E.Channel.Size==3 && E.Payload.Size==Width,"view workload semantic check");Sum+=E.Channel.Size+E.Payload.Size;
            if(Mode){xredisReplyRelease(R);R=NULL;}
        }else{
            size_t Written=0;bool OK=Mode==2?xredisPubSubCommandWrite(Output,Required,XREDIS_PUBSUB_CMD_SUBSCRIBE,Names,Width,&Written):xredisCommandWrite(Output,Required,Args,Width+1,&Written);
            pubsubBenchRequire(OK && Written==Required && Output[0]=='*' && Output[Required-1]=='\n',"full command workload");Sum+=Written;
        }
    }
    double Seconds=xrtTimer()-Start;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    pubsubBenchRequire(Sum==(uint64)Count*(Mode<2?Width+3:Required),"benchmark checksum");
    xredisReplyRelease(R);xredisReplyReaderDestroy(Reader);free(Wire);free(Output);
    printf("operations_per_sec: %.3f\nns_per_operation: %.3f\nchecksum: %" PRIu64 "\n",Count/Seconds,Seconds*1000000000/Count,Sum);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Instrument)printf("heap_blocks_per_operation: %.6f\nheap_bytes_per_operation: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count);
#endif
    return 0;
}
