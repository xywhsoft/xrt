#include <xrpc/stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct benchStdioPipe {
    unsigned char In[512],Out[512];
    size_t ReadAt,ReadEnd,Written;
} benchStdioPipe;

static xrpcstdioio benchStdioRead(ptr Data,bytes Output,size_t Size,size_t* Count)
{
    benchStdioPipe* P=Data;
    size_t Left=P->ReadEnd-P->ReadAt;
    if(!Left)return XRPC_STDIO_IO_AGAIN;
    if(Size>Left)Size=Left;
    memcpy(Output,P->In+P->ReadAt,Size);
    P->ReadAt+=Size;
    *Count=Size;
    return XRPC_STDIO_IO_PROGRESS;
}

static xrpcstdioio benchStdioWrite(ptr Data,cbytes Input,size_t Size,size_t* Count)
{
    benchStdioPipe* P=Data;
    if(Size>sizeof(P->Out)-P->Written)return XRPC_STDIO_IO_ERROR;
    memcpy(P->Out+P->Written,Input,Size);
    P->Written+=Size;
    *Count=Size;
    return XRPC_STDIO_IO_PROGRESS;
}

static bool benchStdioCycle(xrpcstdio* S,xrpcframereader* Reader,const xrpcframeconfig* Frame,benchStdioPipe* P)
{
    P->Written=0;
    xfuture* F=NULL;
    if(xrpcStdioCall(S,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[1,2,3]"),XRT_WAIT_FOREVER,&F)!=XRPC_DUPLEX_ACCEPTED)
        return false;
    bool OK=false;
    if(!xrpcStdioStep(S) || !P->Written)goto Done;

    xrpcframeview V;
    size_t Used=0;
    if(xrpcFrameReaderFeed(Reader,(xbytesview){P->Out,P->Written},&Used,&V)!=XRPC_FRAME_READY ||
       Used!=P->Written)goto Done;
    xrpcmessage M;
    xrpcdocument Document;
    if(xrpcDecode((xstrview){(cstr)V.Body.Data,V.Body.Size},NULL,&M,1,&Document)!=XRPC_DECODE_DONE ||
       M.Kind!=XRPC_MESSAGE_CALL)goto Done;

    xrpcoutgoing Reply={.Kind=XRPC_MESSAGE_RESULT,.Id=M.Id.Raw,.Result=XRT_STR_LITERAL("[1,2,3]")};
    char Body[256];
    size_t BodyBytes=0;
    if(!xrpcEncodeWrite(Body,sizeof(Body),&Reply,1,false,NULL,&BodyBytes))goto Done;
    P->ReadAt=0;
    if(!xrpcFrameWrite((xbytesview){(cbytes)Body,BodyBytes},Frame,P->In,sizeof(P->In),&P->ReadEnd) ||
       !xrpcStdioStep(S))goto Done;
    xfutureresult Result;
    if(xrtFutureResult(F,&Result) && Result.State==XFUTURE_RESOLVED && Result.Value){
        const xrpccallresult* Call=Result.Value;
        OK=Call->Status==XRPC_CALL_OK && Call->Delivery==XRPC_REPLIED;
    }

Done:
    xrtFutureDestroy(F);
    return OK;
}

static int benchStdioCompare(const void* A,const void* B)
{
    double X=*(const double*)A,Y=*(const double*)B;
    return (X>Y)-(X<Y);
}

int main(int Argc,char** Argv)
{
    size_t Samples=Argc>1?(size_t)strtoull(Argv[1],NULL,10):20000;
    unsigned Format=Argc>2?(unsigned)strtoul(Argv[2],NULL,10):1;
    if(!Samples || Samples>1000000 || Format<1 || Format>2 || Argc>3)return 1;
    double* Times=malloc(Samples*sizeof(*Times));
    if(!Times)return 2;
    benchStdioPipe P={0};
    xrpcstdioconfig C;
    xrpcStdioConfigInit(&C);
    C.Frame.Format=(xrpcframeformat)Format;
    C.Read=benchStdioRead;C.Write=benchStdioWrite;C.User=&P;
    xrpcstdio* S=NULL;
    xrpcframereader* Reader=NULL;
    if(!xrpcStdioCreate(&C,&S) || !xrpcFrameReaderCreate(&C.Frame,&Reader))return 3;
    for(unsigned I=0;I<256;++I)if(!benchStdioCycle(S,Reader,&C.Frame,&P))return 4;

    double Begin=xrtTimer();
    for(size_t I=0;I<Samples;++I){
        double Start=xrtTimer();
        if(!benchStdioCycle(S,Reader,&C.Frame,&P))return 5;
        Times[I]=(xrtTimer()-Start)*1e9;
    }
    double Elapsed=xrtTimer()-Begin;
    qsort(Times,Samples,sizeof(*Times),benchStdioCompare);
    printf("calls_per_sec: %.6f\np50_call_ns: %.6f\np99_call_ns: %.6f\ncompleted: %zu\n",
           Samples/Elapsed,Times[(Samples+1u)/2u-1u],Times[(Samples*99u+99u)/100u-1u],Samples);
    free(Times);

    xrpcstdioinfo Info={.Size=sizeof(Info)};
    if(!xrpcStdioClose(S,XRPC_CALL_CLOSED))return 6;
    for(unsigned I=0;I<8;++I){
        if(!xrpcStdioStep(S) || !xrpcStdioInfo(S,&Info))return 7;
        if(Info.Drained){
            xrpcFrameReaderDestroy(Reader);
            return xrpcStdioDestroy(S)?0:8;
        }
    }
    return 8;
}
