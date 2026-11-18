#include <xrpc/duplex.h>
#include <stdio.h>
#include <stdlib.h>

static int benchDeadlineCompare(const void* A,const void* B)
{
    double X=*(const double*)A,Y=*(const double*)B;
    return (X>Y)-(X<Y);
}

static bool benchDeadlineFill(xrpcduplex* D,size_t Window)
{
    for(size_t I=0;I<Window;++I){
        xfuture* F=NULL;
        if(xrpcDuplexCall(D,XRT_STR_LITERAL("hold"),(xstrview){0},120000,&F)!=XRPC_DUPLEX_ACCEPTED)return false;
        xrtFutureDestroy(F);
    }
    xrpcduplexinfo Info={.Size=sizeof(Info)};
    do {
        if(!xrpcDuplexStep(D) || !xrpcDuplexInfo(D,&Info))return false;
    } while(Info.More);
    for(size_t I=0;I<Window;++I){
        xrpcduplexoutput* O=NULL;uint64 Token;
        if(xrpcDuplexNext(D,&O,&Token)!=1)return false;
        bool OK=xrpcDuplexStarted(D,Token) && xrpcDuplexSent(D,Token);
        xrpcDuplexOutputRelease(O);
        if(!OK)return false;
    }
    return true;
}

static bool benchDeadlineClose(xrpcduplex* D)
{
    xrpcduplexinfo Info={.Size=sizeof(Info)};
    if(!xrpcDuplexClose(D,XRPC_CALL_CLOSED))return false;
    do {
        if(!xrpcDuplexStep(D) || !xrpcDuplexInfo(D,&Info))return false;
    } while(!Info.Drained);
    return xrpcDuplexDestroy(D);
}

int main(int Argc,char** Argv)
{
    enum {Block=64};
    size_t Samples=Argc>1?(size_t)strtoull(Argv[1],NULL,10):256;
    size_t Window=Argc>2?(size_t)strtoull(Argv[2],NULL,10):8192;
    if(!Samples || Samples>65536 || !Window || Window>8192 || Argc>3)return 1;
    double* Durations=malloc(Samples*sizeof(*Durations));
    if(!Durations)return 2;
    xrpcduplexconfig C;xrpcDuplexConfigInit(&C);
    C.MaxOperations=C.MaxPendingCalls=C.MaxOutputs=C.MaxResults=Window;
    C.MaxIncoming=1;C.MaxControlBytes=8u*1024u*1024u;
    xrpcduplex* D=NULL;
    if(!xrpcDuplexCreate(&C,&D) || !benchDeadlineFill(D,Window))return 3;
    xrpcduplexinfo Info={.Size=sizeof(Info)};
    for(size_t I=0;I<256;++I)if(!xrpcDuplexInfo(D,&Info))return 4;
    double Next=Info.NextDeadline,Total=0;
    if(!(Next>xrtTimer()) || Info.More || Info.Operations!=Window || Info.PendingCalls!=Window || Info.Outputs)return 5;
    for(size_t I=0;I<Samples;++I){
        double Start=xrtTimer();
        for(size_t J=0;J<Block;++J)
            if(!xrpcDuplexInfo(D,&Info) || Info.NextDeadline!=Next)return 6;
        Durations[I]=(xrtTimer()-Start)*1e9/Block;
        Total+=Durations[I];
    }
    if(Next<=xrtTimer() || Info.Operations!=Window || Info.Outputs)return 7;
    qsort(Durations,Samples,sizeof(*Durations),benchDeadlineCompare);
    printf("info_ns_per_call: %.6f\ninfo_block_p99_ns: %.6f\n",Total/Samples,Durations[(Samples*99u+99u)/100u-1u]);
    printf("window: %zu\nqueries: %zu\nendpoint_fixed_bytes: %zu\n",Window,Samples*Block,Info.OwnedBytes);
    free(Durations);
    return benchDeadlineClose(D)?0:8;
}
