#include <xrpc/stdio_process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32) || defined(_WIN64)
#include <fcntl.h>
#include <io.h>
#endif

static bool processSampleEcho(const xrpcmessage* M,xrpcreply* R,ptr User)
{
    (void)User;
    return xrpcReplyResult(R,M->Params.Raw.Size?M->Params.Raw:XRT_STR_LITERAL("null"));
}

static bool processSampleEmit(const xrpcoutgoing* Values,bool Batch,size_t EncodedBytes,ptr Raw)
{
    const xrpcframeconfig* Frame=Raw;
    unsigned char Header[64];
    char Body[2048];
    size_t Head,Written;
    return EncodedBytes<=sizeof(Body) && xrpcEncodeWrite(Body,sizeof(Body),Values,1,Batch,NULL,&Written) &&
        xrpcFrameHeaderWrite(Written,Frame,Header,sizeof(Header),&Head) &&
        fwrite(Header,1,Head,stdout)==Head && fwrite(Body,1,Written,stdout)==Written && fflush(stdout)==0;
}

static int processSamplePeer(xrpcframeformat Format)
{
#if defined(_WIN32) || defined(_WIN64)
    if(_setmode(_fileno(stdin),_O_BINARY)<0 || _setmode(_fileno(stdout),_O_BINARY)<0)return 2;
#endif
    xrpcframeconfig C;
    xrpcFrameConfigInit(&C);
    C.Format=Format;
    xrpcframereader* R=NULL;
    if(!xrpcFrameReaderCreate(&C,&R))return 3;
    bool OK=true;
    int Byte;
    while((Byte=fgetc(stdin))!=EOF){
        unsigned char Input=(unsigned char)Byte;
        size_t Used;
        xrpcframeview Frame;
        xrpcframestatus Status=xrpcFrameReaderFeed(R,(xbytesview){&Input,1},&Used,&Frame);
        if(Status==XRPC_FRAME_MORE)continue;
        if(Status!=XRPC_FRAME_READY){OK=false;break;}
        /* This demonstration peer handles one JSON-RPC call per frame. */
        xrpcmessage M;
        xrpcdispatchinfo Info={.Size=sizeof(Info)};
        if(!xrpcDispatch((xstrview){(cstr)Frame.Body.Data,Frame.Body.Size},NULL,&M,1,
            processSampleEcho,NULL,processSampleEmit,&C,&Info)){OK=false;break;}
    }
    OK=OK && !ferror(stdin) && xrpcFrameReaderEnd(R)==XRPC_FRAME_END;
    xrpcFrameReaderDestroy(R);
    return OK?0:4;
}

static bool processSampleResult(xfuture* F)
{
    const xrpccallresult* V=xrtFutureValue(F);
    xrpcresponseinfo Info={.Size=sizeof(Info)};
    return V && V->Status==XRPC_CALL_OK && V->Delivery==XRPC_REPLIED &&
        xrpcResponsesInfo(V->Responses,&Info) && V->Index<Info.Count &&
        Info.Messages[V->Index].Result.Raw.Size==4 &&
        !memcmp(Info.Messages[V->Index].Result.Raw.Data,"[42]",4);
}

static bool processSampleGroup(xrpcstdioprocess* Owner,size_t Count,double* Latency)
{
    xrpcstdio* RPC=xrpcStdioProcessEndpoint(Owner);
    xfuture* Futures[64]={0};
    double Started[64];
    size_t Done=0;
    bool OK=true;
    for(size_t I=0;I<Count;++I){
        Started[I]=xrtTimer();
        if(xrpcStdioCall(RPC,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[42]"),5000,&Futures[I])
           !=XRPC_DUPLEX_ACCEPTED){OK=false;break;}
    }
    xrpcstdioinfo Info={.Size=sizeof(Info)};
    while(OK && Done<Count){
        if(!xrpcStdioProcessStep(Owner,&Info)){OK=false;break;}
        double Now=xrtTimer();
        for(size_t I=0;I<Count;++I){
            if(!Futures[I] || !xrtFutureDone(Futures[I]))continue;
            OK=processSampleResult(Futures[I]) && OK;
            Latency[I]=Now-Started[I];
            xrtFutureDestroy(Futures[I]);
            Futures[I]=NULL;
            ++Done;
        }
        if(Done<Count && (!OK || xrpcStdioProcessWait(Owner,5000)!=XWAIT_OK)){OK=false;break;}
    }
    for(size_t I=0;I<Count;++I)xrtFutureDestroy(Futures[I]);
    return OK;
}

static bool processSampleClose(xrpcstdioprocess* Owner)
{
    if(!xrpcStdioClose(xrpcStdioProcessEndpoint(Owner),XRPC_CALL_CLOSED))return false;
    xrpcstdioinfo Info={.Size=sizeof(Info)};
    double Until=xrtTimer()+5;
    do{
        if(!xrpcStdioProcessStep(Owner,&Info))return false;
        if(Info.Drained)return xrpcStdioProcessDestroy(Owner);
        if(xrpcStdioProcessWait(Owner,250)==XWAIT_ERROR)return false;
    }while(xrtTimer()<Until);
    return false;
}

static int processSampleCompare(const void* Left,const void* Right)
{
    double A=*(const double*)Left,B=*(const double*)Right;
    return (A>B)-(A<B);
}

static size_t processSampleNumber(cstr Text,size_t Limit)
{
    if(Text[0]<'1' || Text[0]>'9')return 0;
    char* End;
    unsigned long N=strtoul(Text,&End,10);
    return *End || N>Limit?0:(size_t)N;
}

int main(int Argc,char** Argv)
{
    if(Argc==3 && !strcmp(Argv[1],"--peer"))
        return processSamplePeer(!strcmp(Argv[2],"be")?XRPC_FRAME_UINT32_BE:XRPC_FRAME_CONTENT_LENGTH);
    if(Argc!=1 && (Argc<4 || (strcmp(Argv[1],"text") && strcmp(Argv[1],"be")))){
        fputs("Usage: examples_stdio_process_main [text|be calls window [program [args...]]]\n",stderr);
        return 1;
    }

    const cstr Mode=Argc>1?Argv[1]:"text";
    size_t Count=Argc>1?processSampleNumber(Argv[2],1000000u):1u;
    size_t Width=Argc>1?processSampleNumber(Argv[3],64u):1u;
    if(!Count || !Width)return 1;
    const cstr PeerArgs[]={"--peer",Mode};
    xprocessconfig Native;
    xrtProcessConfigInit(&Native);
    Native.Program=Argc>4?Argv[4]:Argv[0];
    Native.Args=Argc>4?(const cstr*)(Argv+5):PeerArgs;
    Native.ArgCount=Argc>4?(size_t)(Argc-5):2u;
    xrpcstdioconfig Config;
    xrpcStdioConfigInit(&Config);
    if(!strcmp(Mode,"be"))Config.Frame.Format=XRPC_FRAME_UINT32_BE;
    xrpcstdioprocess* Owner=NULL;
    xprocess* Child=NULL;
    double* Latency=xrtMalloc(Count*sizeof(*Latency));
    if(!Latency)return 5;
    if(!xrpcStdioProcessSpawn(&Native,&Config,&Child,&Owner)){xrtFree(Latency);return 6;}

    /*
     * Prime the child and native pipes before timed multi-call samples.
     * Admission and owner-side collection remain inside the measured window.
     */
    bool OK=Count==1 || processSampleGroup(Owner,Width<Count?Width:Count,Latency);
    double Start=xrtTimer();
    for(size_t I=0;OK && I<Count;){
        size_t N=Count-I<Width?Count-I:Width;
        OK=processSampleGroup(Owner,N,Latency+I);
        I+=N;
    }
    double Elapsed=xrtTimer()-Start;
    OK=processSampleClose(Owner) && OK;
    if(xrtProcessWaitFor(Child,5000)!=XWAIT_OK){
        if(xrtProcessKill(Child))xrtProcessWaitFor(Child,5000);
        OK=false;
    }
    xprocessstatus Exit;
    OK=OK && xrtProcessStatus(Child,&Exit) && Exit.Kind==XPROCESS_EXIT_CODE && !Exit.Code;
    xrtProcessDestroy(Child);
    if(OK){
        qsort(Latency,Count,sizeof(*Latency),processSampleCompare);
        printf("calls=%zu window=%zu calls_per_sec=%.3f p50_us=%.3f p95_us=%.3f p99_us=%.3f p999_us=%.3f\n",
               Count,Width,Elapsed>0?Count/Elapsed:0.0,
               Latency[(Count*50u+99u)/100u-1u]*1e6,
               Latency[(Count*95u+99u)/100u-1u]*1e6,
               Latency[(Count*99u+99u)/100u-1u]*1e6,
               Latency[(Count*999u+999u)/1000u-1u]*1e6);
    }
    xrtFree(Latency);
    return OK?0:7;
}
