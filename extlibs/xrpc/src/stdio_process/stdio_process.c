#include <xrpc/stdio_process.h>
#include <xrt/process_pipe.h>
#include <math.h>

#if defined(XRPC_FEATURE_XRPC_STDIO_PROCESS)

struct xrpcstdioprocess {
    xrpcstdio* Stdio;
    xprocesspipe* Pipe;
    double Deadline;
    bool Read,Write,More,Busy,InputClosed,Drained;
};

static bool xrpcStdioProcessError(xerrkind Kind,cstr Message)
{
    xrtSetErrorInfo(Kind,"xrt.rpc.stdio_process",1,Message);
    return false;
}

static xrpcstdioio xrpcStdioProcessIO(xprocesspipeio Result)
{
    switch(Result){
    case XPROCESS_PIPE_PROGRESS:return XRPC_STDIO_IO_PROGRESS;
    case XPROCESS_PIPE_AGAIN:return XRPC_STDIO_IO_AGAIN;
    case XPROCESS_PIPE_EOF:return XRPC_STDIO_IO_EOF;
    default:return XRPC_STDIO_IO_ERROR;
    }
}

static xrpcstdioio xrpcStdioProcessRead(ptr Raw,bytes Data,size_t Size,size_t* Count)
{
    xrpcstdioprocess* P=Raw;
    return xrpcStdioProcessIO(xrtProcessPipeRead(P->Pipe,Data,Size,Count));
}

static xrpcstdioio xrpcStdioProcessWrite(ptr Raw,cbytes Data,size_t Size,size_t* Count)
{
    xrpcstdioprocess* P=Raw;
    return xrpcStdioProcessIO(xrtProcessPipeWrite(P->Pipe,Data,Size,Count));
}

static void xrpcStdioProcessWake(ptr Raw)
{
    xrpcstdioprocess* P=Raw;
    if(P->Pipe)xrtProcessPipeWake(P->Pipe);
}

XRPC_API bool xrpcStdioProcessSpawn(const xprocessconfig* Config,const xrpcstdioconfig* Stdio,
    xprocess** Process,xrpcstdioprocess** Adapter)
{
    if(!Config || !Process || !Adapter || !xrtMemRangeValid(Config,sizeof(*Config)) ||
       !xrtMemRangeValid(Process,sizeof(*Process)) || !xrtMemRangeValid(Adapter,sizeof(*Adapter)) ||
       xrtMemRangesOverlap(Process,sizeof(*Process),Adapter,sizeof(*Adapter)) ||
       xrtMemRangesOverlap(Config,sizeof(*Config),Process,sizeof(*Process)) ||
       xrtMemRangesOverlap(Config,sizeof(*Config),Adapter,sizeof(*Adapter)) ||
       (Stdio && (!xrtMemRangeValid(Stdio,sizeof(*Stdio)) ||
        xrtMemRangesOverlap(Stdio,sizeof(*Stdio),Process,sizeof(*Process)) ||
        xrtMemRangesOverlap(Stdio,sizeof(*Stdio),Adapter,sizeof(*Adapter)))))
        return xrpcStdioProcessError(XERR_ARGUMENT,"invalid stdio process constructor spans");
    if(Config->Terminal || Config->Stderr.Mode==XPROCESS_IO_MERGE)
        return xrpcStdioProcessError(XERR_ARGUMENT,"stdio process needs unmerged standard streams");

    xrpcstdioconfig C;
    if(Stdio)C=*Stdio;
    else xrpcStdioConfigInit(&C);
    if(C.Read || C.Write || C.User || C.Duplex.Wake || C.Duplex.User)
        return xrpcStdioProcessError(XERR_ARGUMENT,"stdio process supplies the I/O and wake callbacks");

    xrpcstdioprocess* P=xrtCalloc(1,sizeof(*P));
    if(!P)return false;
    P->More=true;
    C.Read=xrpcStdioProcessRead;
    C.Write=xrpcStdioProcessWrite;
    C.User=P;
    C.Duplex.Wake=xrpcStdioProcessWake;
    C.Duplex.User=P;
    if(!xrpcStdioCreate(&C,&P->Stdio)){xrtFree(P);return false;}

    size_t Chunk=C.ReadBytes>C.WriteChunkBytes?C.ReadBytes:C.WriteChunkBytes;
    xprocess* Child=NULL;
    if(!xrtProcessPipeSpawn(Config,Chunk,&Child,&P->Pipe)){
        /* Nothing could have entered the unpublished endpoint. Its input and
         * output stay sealed during this synchronous rollback. */
        xerror* Error=xrtTakeError();
        xrpcStdioClose(P->Stdio,XRPC_CALL_CLOSED);
        xrpcStdioStep(P->Stdio);
        xrpcStdioDestroy(P->Stdio);
        xrtFree(P);
        xrtSetErrorTake(Error);
        return false;
    }
    *Process=Child;
    *Adapter=P;
    return true;
}

XRPC_API xrpcstdio* xrpcStdioProcessEndpoint(xrpcstdioprocess* Adapter)
{
    return Adapter?Adapter->Stdio:NULL;
}

XRPC_API bool xrpcStdioProcessStep(xrpcstdioprocess* P,xrpcstdioinfo* Info)
{
    if(!P || !Info || !xrtMemRangeValid(Info,sizeof(*Info)) ||
       xrtMemRangesOverlap(P,sizeof(*P),Info,sizeof(*Info)) || Info->Size!=sizeof(*Info))
        return xrpcStdioProcessError(XERR_ARGUMENT,"invalid stdio process step output");
    if(P->Busy)return xrpcStdioProcessError(XERR_STATE,"stdio process owner reentry");

    P->Busy=true;
    bool OK=xrpcStdioStep(P->Stdio);
    xerror* Error=OK?NULL:xrtTakeError();
    xrpcstdioinfo Fallback={.Size=sizeof(Fallback)};
    xrpcstdioinfo* State=OK?Info:&Fallback;
    if(xrpcStdioInfo(P->Stdio,State)){
        P->Deadline=State->NextDeadline;
        P->Read=State->WantRead;
        P->Write=State->WantWrite;
        P->More=State->More;
        P->Drained=State->Drained;
        if(State->Reason!=XRPC_STDIO_OPEN && !P->InputClosed){
            P->InputClosed=true;
            if(!xrtProcessPipeCloseInput(P->Pipe))OK=false;
        }
    }else OK=false;
    P->Busy=false;
    if(Error)xrtSetErrorTake(Error);
    return OK;
}

XRPC_API xwaitresult xrpcStdioProcessWait(xrpcstdioprocess* P,int64 Timeout)
{
    if(!P || Timeout<XRT_WAIT_FOREVER){
        xrpcStdioProcessError(XERR_ARGUMENT,"invalid stdio process wait");
        return XWAIT_ERROR;
    }
    if(P->Busy){
        xrpcStdioProcessError(XERR_STATE,"stdio process owner reentry");
        return XWAIT_ERROR;
    }
    if(P->Drained)return XWAIT_CLOSED;
    if(P->More)return XWAIT_OK;
    bool Due=false;
    if(P->Deadline<INFINITY){
        double Delay=ceil((P->Deadline-xrtTimer())*1000.0);
        if(Delay<=0)return XWAIT_OK;
        if(Delay<0x1p63 && (Timeout==XRT_WAIT_FOREVER || (int64)Delay<=Timeout)){
            Timeout=(int64)Delay;
            Due=true;
        }
    }
    P->Busy=true;
    xwaitresult Result=xrtProcessPipeWait(P->Pipe,P->Read,P->Write,Timeout);
    if(Result==XWAIT_ERROR){
        xerror* Error=xrtTakeError();
        xrpcStdioClose(P->Stdio,XRPC_CALL_TRANSPORT);
        xrtSetErrorTake(Error);
    }
    P->Busy=false;
    return Due && Result==XWAIT_TIMEOUT?XWAIT_OK:Result;
}

XRPC_API bool xrpcStdioProcessDestroy(xrpcstdioprocess* P)
{
    if(!P)return true;
    if(P->Busy)return xrpcStdioProcessError(XERR_STATE,"stdio process owner reentry");
    if(!xrpcStdioDestroy(P->Stdio))return false;
    xrtProcessPipeDestroy(P->Pipe);
    xrtFree(P);
    return true;
}
#endif
