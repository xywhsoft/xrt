#include <xrpc/stdio.h>
#include <xrt/thread.h>
#include <xrt/time.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>
#endif

enum {STDIO_READ_BYTES=16384, STDIO_WRITE_BYTES=512};

static int stdioExampleTimeout(double Deadline)
{
    if(Deadline==INFINITY)return -1;
    double Milliseconds=(Deadline-xrtTimer())*1000.0;
    if(!(Milliseconds>0))return 0;
    if(Milliseconds>=INT_MAX)return INT_MAX;
    return (int)ceil(Milliseconds);
}

#if defined(_WIN32)
typedef struct stdioExampleOp {
    OVERLAPPED Overlapped;
    bool Pending;
} stdioExampleOp;

typedef struct stdioExamplePipe {
    HANDLE In,Out,Wake;
    stdioExampleOp Read,Write;
    unsigned char Input[STDIO_READ_BYTES],Output[STDIO_WRITE_BYTES];
} stdioExamplePipe;

static bool stdioExampleOpen(stdioExamplePipe* P,bool EventDriven)
{
    P->In=GetStdHandle(STD_INPUT_HANDLE);
    P->Out=GetStdHandle(STD_OUTPUT_HANDLE);
    if(GetFileType(P->In)!=FILE_TYPE_PIPE || GetFileType(P->Out)!=FILE_TYPE_PIPE)return false;
    if(EventDriven){
        /* The launcher must open both child handles with FILE_FLAG_OVERLAPPED. */
        P->Wake=CreateEventW(NULL,TRUE,FALSE,NULL);
        P->Read.Overlapped.hEvent=CreateEventW(NULL,TRUE,FALSE,NULL);
        P->Write.Overlapped.hEvent=CreateEventW(NULL,TRUE,FALSE,NULL);
        return P->Wake && P->Read.Overlapped.hEvent && P->Write.Overlapped.hEvent;
    }
    DWORD Mode=PIPE_READMODE_BYTE|PIPE_NOWAIT;
    return SetNamedPipeHandleState(P->In,&Mode,NULL,NULL) &&
           SetNamedPipeHandleState(P->Out,&Mode,NULL,NULL);
}

static void stdioExampleRetire(HANDLE File,stdioExampleOp* Op)
{
    if(Op->Pending){
        DWORD Bytes;
        /* A cancellation request does not release the buffers. Join the I/O. */
        CancelIoEx(File,&Op->Overlapped);
        GetOverlappedResult(File,&Op->Overlapped,&Bytes,TRUE);
    }
    if(Op->Overlapped.hEvent)CloseHandle(Op->Overlapped.hEvent);
}

static void stdioExampleClose(stdioExamplePipe* P)
{
    stdioExampleRetire(P->In,&P->Read);
    stdioExampleRetire(P->Out,&P->Write);
    if(P->Wake)CloseHandle(P->Wake);
}

static void stdioExampleWake(ptr Raw)
{
    stdioExamplePipe* P=Raw;
    SetEvent(P->Wake);
}

static void stdioExampleClearWake(stdioExamplePipe* P)
{
    ResetEvent(P->Wake);
}

static bool stdioExampleWait(stdioExamplePipe* P,const xrpcstdioinfo* Info)
{
    HANDLE Events[3]={P->Wake};
    DWORD Count=1;
    if(Info->WantRead){
        /* A completed zero-length pipe read needs another submission. */
        if(!P->Read.Pending)return true;
        Events[Count++]=P->Read.Overlapped.hEvent;
    }
    if(Info->WantWrite){
        if(!P->Write.Pending)return true;
        Events[Count++]=P->Write.Overlapped.hEvent;
    }
    int Milliseconds=stdioExampleTimeout(Info->NextDeadline);
    return WaitForMultipleObjects(Count,Events,FALSE,Milliseconds<0?INFINITE:(DWORD)Milliseconds)!=WAIT_FAILED;
}

static DWORD stdioExampleFinish(HANDLE File,stdioExampleOp* Op,DWORD* Bytes)
{
    DWORD Error=GetOverlappedResult(File,&Op->Overlapped,Bytes,FALSE)?ERROR_SUCCESS:GetLastError();
    if(Error!=ERROR_IO_INCOMPLETE)Op->Pending=false;
    return Error;
}

static OVERLAPPED* stdioExampleBegin(stdioExampleOp* Op)
{
    Op->Overlapped=(OVERLAPPED){.hEvent=Op->Overlapped.hEvent};
    return &Op->Overlapped;
}

static xrpcstdioio stdioExampleRead(ptr Raw,bytes Data,size_t Size,size_t* Count)
{
    stdioExamplePipe* P=Raw;
    DWORD N=0,Error;
    if(!P->Wake)
        Error=ReadFile(P->In,Data,(DWORD)Size,&N,NULL)?ERROR_SUCCESS:GetLastError();
    else{
        if(Size!=sizeof(P->Input))return XRPC_STDIO_IO_ERROR;
        if(P->Read.Pending)Error=stdioExampleFinish(P->In,&P->Read,&N);
        else{
            OVERLAPPED* Op=stdioExampleBegin(&P->Read);
            if(ReadFile(P->In,P->Input,(DWORD)Size,NULL,Op))
                Error=stdioExampleFinish(P->In,&P->Read,&N);
            else Error=GetLastError();
            P->Read.Pending=Error==ERROR_IO_PENDING;
        }
        if(Error==ERROR_SUCCESS && N)memcpy(Data,P->Input,N);
    }
    if(Error==ERROR_SUCCESS && N){*Count=N;return XRPC_STDIO_IO_PROGRESS;}
    if(Error==ERROR_BROKEN_PIPE || Error==ERROR_PIPE_NOT_CONNECTED)return XRPC_STDIO_IO_EOF;
    if(Error==ERROR_SUCCESS || Error==ERROR_IO_PENDING || Error==ERROR_IO_INCOMPLETE ||
       (!P->Wake && Error==ERROR_NO_DATA))return XRPC_STDIO_IO_AGAIN;
    return XRPC_STDIO_IO_ERROR;
}

static xrpcstdioio stdioExampleWrite(ptr Raw,cbytes Data,size_t Size,size_t* Count)
{
    stdioExamplePipe* P=Raw;
    DWORD N=0,Error;
    if(!P->Wake)
        Error=WriteFile(P->Out,Data,(DWORD)Size,&N,NULL)?ERROR_SUCCESS:GetLastError();
    else{
        if(Size>sizeof(P->Output))return XRPC_STDIO_IO_ERROR;
        if(P->Write.Pending)Error=stdioExampleFinish(P->Out,&P->Write,&N);
        else{
            /* xrpc retries the same prefix after AGAIN. The kernel owns this
             * staging buffer until completion, including during shutdown. */
            memcpy(P->Output,Data,Size);
            OVERLAPPED* Op=stdioExampleBegin(&P->Write);
            if(WriteFile(P->Out,P->Output,(DWORD)Size,NULL,Op))
                Error=stdioExampleFinish(P->Out,&P->Write,&N);
            else Error=GetLastError();
            P->Write.Pending=Error==ERROR_IO_PENDING;
        }
    }
    if(Error==ERROR_SUCCESS && N){*Count=N;return XRPC_STDIO_IO_PROGRESS;}
    if(Error==ERROR_IO_PENDING || Error==ERROR_IO_INCOMPLETE ||
       (!P->Wake && Error==ERROR_SUCCESS))return XRPC_STDIO_IO_AGAIN;
    return XRPC_STDIO_IO_ERROR;
}
#else
typedef struct stdioExamplePipe {
    int In,Out;
    int Wake[2];
} stdioExamplePipe;

static bool stdioExampleNonblocking(int Fd)
{
    int Flags=fcntl(Fd,F_GETFL,0);
    return Flags>=0 && fcntl(Fd,F_SETFL,Flags|O_NONBLOCK)==0;
}

static bool stdioExampleOpen(stdioExamplePipe* P,bool EventDriven)
{
    P->In=STDIN_FILENO;
    P->Out=STDOUT_FILENO;
    P->Wake[0]=P->Wake[1]=-1;
    if(signal(SIGPIPE,SIG_IGN)==SIG_ERR ||
       !stdioExampleNonblocking(P->In) || !stdioExampleNonblocking(P->Out))return false;
    if(!EventDriven)return true;
    return pipe(P->Wake)==0 &&
           stdioExampleNonblocking(P->Wake[0]) && stdioExampleNonblocking(P->Wake[1]) &&
           fcntl(P->Wake[0],F_SETFD,FD_CLOEXEC)==0 && fcntl(P->Wake[1],F_SETFD,FD_CLOEXEC)==0;
}

static void stdioExampleClose(stdioExamplePipe* P)
{
    if(P->Wake[0]>=0)close(P->Wake[0]);
    if(P->Wake[1]>=0)close(P->Wake[1]);
}

static void stdioExampleWake(ptr Raw)
{
    stdioExamplePipe* P=Raw;
    char Byte=0;
    ssize_t N;
    do{N=write(P->Wake[1],&Byte,1);}while(N<0 && errno==EINTR);
    /* A full nonblocking wake pipe already has a readable notification. */
}

static void stdioExampleClearWake(stdioExamplePipe* P)
{
    char Bytes[256];
    ssize_t N;
    do{N=read(P->Wake[0],Bytes,sizeof(Bytes));}while(N<0 && errno==EINTR);
}

static bool stdioExampleWait(stdioExamplePipe* P,const xrpcstdioinfo* Info)
{
    struct pollfd Fds[3]={
        {P->Wake[0],POLLIN,0},
        {Info->WantRead?P->In:-1,POLLIN,0},
        {Info->WantWrite?P->Out:-1,POLLOUT,0}
    };
    int Ready;
    do{Ready=poll(Fds,3,stdioExampleTimeout(Info->NextDeadline));}while(Ready<0 && errno==EINTR);
    return Ready>=0 && !((Fds[0].revents|Fds[1].revents|Fds[2].revents)&POLLNVAL);
}

static xrpcstdioio stdioExampleRead(ptr Raw,bytes Data,size_t Size,size_t* Count)
{
    stdioExamplePipe* P=Raw;
    ssize_t N=read(P->In,Data,Size);
    if(N>0){*Count=(size_t)N;return XRPC_STDIO_IO_PROGRESS;}
    if(!N)return XRPC_STDIO_IO_EOF;
    return errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR ? XRPC_STDIO_IO_AGAIN : XRPC_STDIO_IO_ERROR;
}

static xrpcstdioio stdioExampleWrite(ptr Raw,cbytes Data,size_t Size,size_t* Count)
{
    stdioExamplePipe* P=Raw;
    ssize_t N=write(P->Out,Data,Size);
    if(N>0){*Count=(size_t)N;return XRPC_STDIO_IO_PROGRESS;}
    if(!N)return XRPC_STDIO_IO_AGAIN;
    return errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR ? XRPC_STDIO_IO_AGAIN : XRPC_STDIO_IO_ERROR;
}
#endif

static bool stdioExampleEcho(const xrpcmessage* M,xrpcreply* R,const xrpcservercontext* Context,ptr User)
{
    (void)Context;(void)User;
    return xrpcReplyResult(R,M->Params.Raw.Size?M->Params.Raw:XRT_STR_LITERAL("null"));
}

static int stdioExampleRun(stdioExamplePipe* Pipe,bool EventDriven,bool Binary)
{
    xrpcserverconfig ServerConfig;
    xrpcServerConfigInit(&ServerConfig);
    ServerConfig.Call=stdioExampleEcho;
    xrpcserver* Server=NULL;
    if(!xrpcServerCreate(&ServerConfig,&Server))return 3;
    xrpcstdioconfig C;
    xrpcStdioConfigInit(&C);
    C.Duplex.Server=Server;
    if(EventDriven){
        C.Duplex.Wake=stdioExampleWake;
        C.Duplex.User=Pipe;
    }
    C.Read=stdioExampleRead;
    C.Write=stdioExampleWrite;
    C.User=Pipe;
    C.ReadBytes=STDIO_READ_BYTES;
    /* Keep the same write limit in both modes for a comparable pipe workload. */
    C.WriteChunkBytes=STDIO_WRITE_BYTES;
    if(Binary)C.Frame.Format=XRPC_FRAME_UINT32_BE;
    xrpcstdio* S=NULL;
    if(!xrpcStdioCreate(&C,&S)){xrpcServerDestroy(Server);return 4;}

    xrpcstdioinfo Info={.Size=sizeof(Info)};
    int Result=0;
    for(;;){
        /* Clear before Step so a concurrent publication during Step is visible. */
        if(EventDriven)stdioExampleClearWake(Pipe);
        if(!xrpcStdioStep(S))xrpcStdioClose(S,XRPC_CALL_TRANSPORT);
        if(!xrpcStdioInfo(S,&Info)){Result=5;break;}
        if(Info.Drained)break;
        if(Info.More)continue;
        if(!EventDriven)xrtSleep(1);
        else if(!stdioExampleWait(Pipe,&Info)){
            Result=5;
            xrpcStdioClose(S,XRPC_CALL_TRANSPORT);
        }
    }
    if(!xrpcStdioDestroy(S) || !xrpcServerDestroy(Server))return 6;
    if(Result)return Result;
    return Info.Reason==XRPC_STDIO_EOF?0:7;
}

int main(int Argc,char** Argv)
{
    if(Argc==1){
        puts("Run with redirected binary stdio: examples_stdio_main {serve|serve-event} [be]");
        return 0;
    }
    bool EventDriven=!strcmp(Argv[1],"serve-event");
    if(Argc>3 || (!EventDriven && strcmp(Argv[1],"serve")) ||
       (Argc==3 && strcmp(Argv[2],"be")))return 1;
    stdioExamplePipe Pipe={0};
    bool Open=stdioExampleOpen(&Pipe,EventDriven);
    int Result=Open?stdioExampleRun(&Pipe,EventDriven,Argc==3):2;
    if(!Open)fputs("stdio example requires supported redirected pipes\n",stderr);
    stdioExampleClose(&Pipe);
    return Result;
}
