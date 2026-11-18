#include <xrpc/stdio.h>
#include "../internal/xrpc_duplex_internal.h"
#include <math.h>

#if defined(XRPC_FEATURE_XRPC_STDIO)
struct xrpcstdio {
    xrpcstdioconfig Config;
    xrpcduplex* Duplex;
    xrpcframereader* Reader;
    xrpcframeview Input;
    xrpcduplexoutput* Output;
    xstrview Body;
    uint64 Token;
    size_t ReadAt,ReadEnd,HeaderBytes,HeaderAt,BodyAt,OwnedBytes;
    double ReadEntered,FrameEntered,RetryAt,DrainAt;
    xrpcstdioreason Reason;
    bool Ready,FrameActive,InputEnded,Started,WriteTurn,Busy,More;
    unsigned char Header[64];
    unsigned char Buffer[];
};

static bool xrpcStdioReport(xerrkind Kind,cstr Message)
{
    xrtSetErrorInfo(Kind,"xrt.rpc.stdio",1,Message);
    return false;
}

static bool xrpcStdioSpan(xrpcstdio* S,const void* Span,size_t Bytes)
{
    return xrtMemRangeValid(Span,Bytes) && !xrtMemRangesOverlap(S,S->OwnedBytes,Span,Bytes);
}

XRPC_API bool xrpcStdioConfigInit(xrpcstdioconfig* Config)
{
    if(!Config || !xrtMemRangeValid(Config,sizeof(*Config)))
        return xrpcStdioReport(XERR_ARGUMENT,"invalid stdio configuration output");
    *Config=(xrpcstdioconfig){0};
    Config->Size=sizeof(*Config);
    xrpcDuplexConfigInit(&Config->Duplex);
    xrpcFrameConfigInit(&Config->Frame);
    Config->ReadBytes=16u*1024u;
    Config->WriteChunkBytes=16u*1024u;
    Config->MaxIOWork=64;
    Config->DrainTimeout=5000;
    return true;
}

XRPC_API bool xrpcStdioCreate(const xrpcstdioconfig* Config,xrpcstdio** Result)
{
    if(!Config || !Result || !xrtMemRangeValid(Config,sizeof(*Config)) ||
       !xrtMemRangeValid(Result,sizeof(*Result)) ||
       xrtMemRangesOverlap(Config,sizeof(*Config),Result,sizeof(*Result)))
        return xrpcStdioReport(XERR_ARGUMENT,"invalid stdio constructor spans");
    xrpcstdioconfig C=*Config;
    if(C.Size!=sizeof(C) || !C.Read || !C.Write ||
       !C.ReadBytes || C.ReadBytes>1024u*1024u || !C.WriteChunkBytes ||
       C.WriteChunkBytes>1024u*1024u || !C.MaxIOWork || C.MaxIOWork>65536u ||
       C.DrainTimeout<XRT_WAIT_FOREVER || C.Duplex.Size!=sizeof(C.Duplex) ||
       C.Frame.Size!=sizeof(C.Frame))
        return xrpcStdioReport(XERR_ARGUMENT,"invalid stdio configuration");
    size_t Prefix;
    if(!xrpcFrameHeaderSize(C.Frame.MaxBodyBytes,&C.Frame,&Prefix))return false;
    if(C.Duplex.Encode.MaxOutputBytes>C.Frame.MaxBodyBytes)
        C.Duplex.Encode.MaxOutputBytes=C.Frame.MaxBodyBytes;

    size_t Bytes=sizeof(xrpcstdio)+C.ReadBytes;
    xrpcstdio* S=xrtCalloc(1,Bytes);
    if(!S)return xrpcStdioReport(XERR_MEMORY,"stdio adapter allocation failed");
    S->Config=C;
    S->OwnedBytes=Bytes;
    S->DrainAt=INFINITY;
    if(!xrpcFrameReaderCreate(&C.Frame,&S->Reader) ||
       !xrpcDuplexCreate(&C.Duplex,&S->Duplex)){
        xrpcFrameReaderDestroy(S->Reader);
        xrtFree(S);
        return false;
    }
    *Result=S;
    return true;
}

static xrpcduplexadmission xrpcStdioSubmit(xrpcstdio* S,const xrpcduplexitem* Items,size_t Count,
    bool Batch,int64 Timeout,xfuture** Future,double Entered)
{
    if(!S || !Future || !xrpcStdioSpan(S,Future,sizeof(*Future))){
        xrpcStdioReport(XERR_ARGUMENT,"invalid stdio future output");
        return XRPC_DUPLEX_ERROR;
    }
    return xrpcDuplexSubmitEntered(S->Duplex,Items,Count,Batch,Timeout,Future,Entered);
}
XRPC_API xrpcduplexadmission xrpcStdioCall(xrpcstdio* S,xstrview Method,xstrview Params,int64 Timeout,xfuture** Future)
{
    double Entered=xrtTimer();
    xrpcduplexitem Item={false,Method,Params};
    return xrpcStdioSubmit(S,&Item,1,false,Timeout,Future,Entered);
}
XRPC_API xrpcduplexadmission xrpcStdioNotify(xrpcstdio* S,xstrview Method,xstrview Params,int64 Timeout,xfuture** Future)
{
    double Entered=xrtTimer();
    xrpcduplexitem Item={true,Method,Params};
    return xrpcStdioSubmit(S,&Item,1,false,Timeout,Future,Entered);
}
XRPC_API xrpcduplexadmission xrpcStdioBatch(xrpcstdio* S,const xrpcduplexitem* Items,size_t Count,int64 Timeout,xfuture** Future)
{
    return xrpcStdioSubmit(S,Items,Count,true,Timeout,Future,xrtTimer());
}
XRPC_API bool xrpcStdioClose(xrpcstdio* S,xrpccallstatus Reason)
{
    if(!S)return xrpcStdioReport(XERR_ARGUMENT,"null stdio adapter");
    return xrpcDuplexClose(S->Duplex,Reason);
}

static void xrpcStdioDropOutput(xrpcstdio* S)
{
    xrpcDuplexOutputRelease(S->Output);
    S->Output=NULL;
    S->Body=(xstrview){0};
    S->Started=false;
}

static void xrpcStdioStop(xrpcstdio* S,xrpcstdioreason Reason,xrpccallstatus Status)
{
    if(S->Reason==XRPC_STDIO_OPEN)S->Reason=Reason;
    xrpcDuplexClose(S->Duplex,Status);
    xrpcStdioDropOutput(S);
}

static void xrpcStdioFrameStop(xrpcstdio* S,xrpcframestatus Status)
{
    if(Status==XRPC_FRAME_TRUNCATED)xrpcStdioStop(S,XRPC_STDIO_TRUNCATED,XRPC_CALL_PROTOCOL);
    else if(Status==XRPC_FRAME_LIMIT)xrpcStdioStop(S,XRPC_STDIO_LIMIT,XRPC_CALL_LIMIT);
    else if(Status==XRPC_FRAME_MEMORY)xrpcStdioStop(S,XRPC_STDIO_MEMORY,XRPC_CALL_MEMORY);
    else xrpcStdioStop(S,XRPC_STDIO_FRAME,XRPC_CALL_PROTOCOL);
}

static void xrpcStdioReadEnd(xrpcstdio* S)
{
    S->InputEnded=true;
    xrpcframestatus Status=xrpcFrameReaderEnd(S->Reader);
    if(Status!=XRPC_FRAME_END){
        xrpcStdioFrameStop(S,Status);
        return;
    }
    xrpcDuplexEndInput(S->Duplex);
    if(S->Config.DrainTimeout!=XRT_WAIT_FOREVER)
        S->DrainAt=xrtTimer()+(double)S->Config.DrainTimeout*.001;
}

static bool xrpcStdioDispatch(xrpcstdio* S)
{
    if(S->RetryAt && S->RetryAt>xrtTimer())return false;
    xstrview Body={(cstr)S->Input.Body.Data,S->Input.Body.Size};
    xrpcduplexadmission Result=xrpcDuplexReceiveEntered(S->Duplex,Body,(xbytesview){0},S->FrameEntered);
    if(Result==XRPC_DUPLEX_OVERLOADED && S->Config.Duplex.Server){
        /* A shared server can release capacity without a wake for this peer. */
        S->RetryAt=xrtTimer()+.001;
        return false;
    }
    S->Ready=S->FrameActive=false;
    S->RetryAt=0;
    if(Result==XRPC_DUPLEX_OVERLOADED)xrpcStdioStop(S,XRPC_STDIO_LIMIT,XRPC_CALL_LIMIT);
    else if(Result==XRPC_DUPLEX_PROTOCOL)xrpcStdioStop(S,XRPC_STDIO_DUPLEX,XRPC_CALL_PROTOCOL);
    else if(Result==XRPC_DUPLEX_ERROR)xrpcStdioStop(S,XRPC_STDIO_DUPLEX,XRPC_CALL_TRANSPORT);
    else if(Result==XRPC_DUPLEX_CLOSED)xrpcStdioStop(S,XRPC_STDIO_CLOSED,XRPC_CALL_CLOSED);
    return true;
}

static bool xrpcStdioReadWork(xrpcstdio* S)
{
    if(S->InputEnded)return false;
    if(S->Ready)return xrpcStdioDispatch(S);
    if(S->ReadAt!=S->ReadEnd){
        if(!S->FrameActive){
            S->FrameActive=true;
            S->FrameEntered=S->ReadEntered;
        }
        size_t Used=0;
        xbytesview Input={S->Buffer+S->ReadAt,S->ReadEnd-S->ReadAt};
        xrpcframestatus Status=xrpcFrameReaderFeed(S->Reader,Input,&Used,&S->Input);
        S->ReadAt+=Used;
        if(Status==XRPC_FRAME_READY)S->Ready=true;
        else if(Status!=XRPC_FRAME_MORE || !Used)xrpcStdioFrameStop(S,Status);
        return true;
    }

    size_t Count=0;
    double Entered=xrtTimer();
    xrpcstdioio IO=S->Config.Read(S->Config.User,S->Buffer,S->Config.ReadBytes,&Count);
    if(IO==XRPC_STDIO_IO_PROGRESS && Count && Count<=S->Config.ReadBytes){
        S->ReadAt=0;
        S->ReadEnd=Count;
        S->ReadEntered=Entered;
    } else if(IO==XRPC_STDIO_IO_AGAIN && !Count)return false;
    else if(IO==XRPC_STDIO_IO_EOF && !Count)xrpcStdioReadEnd(S);
    else xrpcStdioStop(S,XRPC_STDIO_IO,XRPC_CALL_TRANSPORT);
    return true;
}

static bool xrpcStdioWriteWork(xrpcstdio* S)
{
    if(!S->Output){
        int Selected=xrpcDuplexNext(S->Duplex,&S->Output,&S->Token);
        if(!Selected)return false;
        if(Selected<0){
            xrpcStdioStop(S,XRPC_STDIO_DUPLEX,XRPC_CALL_TRANSPORT);
            return true;
        }
        S->Body=xrpcDuplexOutputWire(S->Output);
        S->HeaderAt=S->BodyAt=0;
        if(!xrpcFrameHeaderWrite(S->Body.Size,&S->Config.Frame,S->Header,sizeof(S->Header),&S->HeaderBytes)){
            xrpcStdioStop(S,XRPC_STDIO_LIMIT,XRPC_CALL_LIMIT);
            return true;
        }
    }
    if(!S->Started){
        if(!xrpcDuplexStarted(S->Duplex,S->Token)){
            S->More=true;
            return false;
        }
        S->Started=true;
    }

    bool IsHeader=S->HeaderAt!=S->HeaderBytes;
    cbytes Data=IsHeader?S->Header+S->HeaderAt:(cbytes)S->Body.Data+S->BodyAt;
    size_t At=IsHeader?S->HeaderAt:S->BodyAt;
    size_t Left=(IsHeader?S->HeaderBytes:S->Body.Size)-At;
    if(Left>S->Config.WriteChunkBytes)Left=S->Config.WriteChunkBytes;
    size_t Count=0;
    xrpcstdioio IO=S->Config.Write(S->Config.User,Data,Left,&Count);
    if(IO==XRPC_STDIO_IO_AGAIN && !Count)return false;
    if(IO!=XRPC_STDIO_IO_PROGRESS || !Count || Count>Left){
        xrpcStdioStop(S,XRPC_STDIO_IO,XRPC_CALL_TRANSPORT);
        return true;
    }
    if(IsHeader)S->HeaderAt+=Count;
    else S->BodyAt+=Count;
    if(S->HeaderAt==S->HeaderBytes && S->BodyAt==S->Body.Size){
        if(!xrpcDuplexSent(S->Duplex,S->Token))
            xrpcStdioStop(S,XRPC_STDIO_DUPLEX,XRPC_CALL_TRANSPORT);
        else xrpcStdioDropOutput(S);
    }
    return true;
}

static void xrpcStdioCheckDrain(xrpcstdio* S,const xrpcduplexinfo* D)
{
    if(D->Closing){
        xrpcStdioDropOutput(S);
        return;
    }
    if(!S->InputEnded)return;
    if(!D->Operations && !D->Outputs)
        xrpcStdioStop(S,XRPC_STDIO_EOF,XRPC_CALL_TRANSPORT);
    else if(xrtTimer()>=S->DrainAt)
        xrpcStdioStop(S,XRPC_STDIO_TIMEOUT,XRPC_CALL_TIMEOUT);
}

XRPC_API bool xrpcStdioStep(xrpcstdio* S)
{
    if(!S || S->Busy)return xrpcStdioReport(XERR_STATE,"invalid stdio owner entry");
    S->Busy=true;
    S->More=false;
    bool OK=xrpcDuplexStep(S->Duplex);
    xrpcduplexinfo D={.Size=sizeof(D)};
    if(!OK || !xrpcDuplexInfo(S->Duplex,&D)){
        xrpcStdioStop(S,XRPC_STDIO_DUPLEX,XRPC_CALL_TRANSPORT);
        S->Busy=false;
        return false;
    }
    if(S->Output && !D.Selected)xrpcStdioDropOutput(S);
    xrpcStdioCheckDrain(S,&D);
    if(!D.Closing && S->Reason==XRPC_STDIO_OPEN){
        bool Blocked[2]={false,false};
        bool WasEnded=S->InputEnded;
        size_t Work=0;
        /* Recheck the drain policy before issuing any post-EOF write. */
        while(Work<S->Config.MaxIOWork && S->Reason==XRPC_STDIO_OPEN && WasEnded==S->InputEnded){
            unsigned Lane=S->WriteTurn?1u:0u;
            S->WriteTurn=!S->WriteTurn;
            if(Blocked[Lane]){
                if(Blocked[1u-Lane])break;
                continue;
            }
            bool Progress=Lane?xrpcStdioWriteWork(S):xrpcStdioReadWork(S);
            if(Progress)++Work;
            else Blocked[Lane]=true;
        }
        S->More|=Work==S->Config.MaxIOWork || WasEnded!=S->InputEnded;
        if(S->InputEnded && xrpcDuplexInfo(S->Duplex,&D))xrpcStdioCheckDrain(S,&D);
    }
    S->Busy=false;
    return true;
}

XRPC_API bool xrpcStdioInfo(xrpcstdio* S,xrpcstdioinfo* Info)
{
    if(!S || !Info || !xrpcStdioSpan(S,Info,sizeof(*Info)) || Info->Size!=sizeof(*Info))
        return xrpcStdioReport(XERR_ARGUMENT,"invalid stdio info output");
    xrpcstdioinfo V={.Size=sizeof(V),.Duplex={.Size=sizeof(V.Duplex)},.Frame={.Size=sizeof(V.Frame)}};
    if(!xrpcDuplexInfo(S->Duplex,&V.Duplex) || !xrpcFrameReaderInfo(S->Reader,&V.Frame))return false;
    V.Reason=S->Reason;
    if(V.Reason==XRPC_STDIO_OPEN && V.Duplex.Closing)
        V.Reason=V.Duplex.Reason==XRPC_CALL_CLOSED?XRPC_STDIO_CLOSED:XRPC_STDIO_DUPLEX;
    V.FixedBytes=S->OwnedBytes;
    V.BufferedInputBytes=S->ReadEnd-S->ReadAt;
    V.InputEnded=S->InputEnded;
    V.Drained=V.Duplex.Drained && !S->Output;
    V.WantRead=V.Reason==XRPC_STDIO_OPEN && !S->InputEnded && !S->Ready && !V.BufferedInputBytes;
    V.WantWrite=V.Reason==XRPC_STDIO_OPEN && S->Output;
    V.More=!V.Drained && (S->More || V.Duplex.More || (V.Duplex.Closing && S->Output));
    V.NextDeadline=V.Duplex.NextDeadline;
    if(V.Reason==XRPC_STDIO_OPEN){
        if(S->Ready && S->RetryAt<V.NextDeadline)V.NextDeadline=S->RetryAt;
        if(S->DrainAt<V.NextDeadline)V.NextDeadline=S->DrainAt;
    }
    if(V.More)V.NextDeadline=xrtTimer();
    *Info=V;
    return true;
}

XRPC_API bool xrpcStdioDestroy(xrpcstdio* S)
{
    if(!S)return true;
    if(S->Busy || S->Output)
        return xrpcStdioReport(XERR_STATE,"stdio owner or output remains active");
    if(!xrpcDuplexDestroy(S->Duplex))return false;
    xrpcFrameReaderDestroy(S->Reader);
    xrtFree(S);
    return true;
}
#endif
