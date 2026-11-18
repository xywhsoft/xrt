#include <xrpc/stdio.h>
#include <xrt/thread.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#endif

typedef struct stdioExamplePipe {
#if defined(_WIN32)
    HANDLE In,Out;
#else
    int In,Out;
#endif
} stdioExamplePipe;

#if defined(_WIN32)
static bool stdioExampleOpen(stdioExamplePipe* P)
{
    P->In=GetStdHandle(STD_INPUT_HANDLE);
    P->Out=GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD Mode=PIPE_READMODE_BYTE|PIPE_NOWAIT;
    return GetFileType(P->In)==FILE_TYPE_PIPE && GetFileType(P->Out)==FILE_TYPE_PIPE &&
           SetNamedPipeHandleState(P->In,&Mode,NULL,NULL) &&
           SetNamedPipeHandleState(P->Out,&Mode,NULL,NULL);
}
static xrpcstdioio stdioExampleRead(ptr Raw,bytes Data,size_t Size,size_t* Count)
{
    stdioExamplePipe* P=Raw;
    DWORD N=0;
    if(ReadFile(P->In,Data,(DWORD)Size,&N,NULL)){
        if(!N)return XRPC_STDIO_IO_AGAIN;
        *Count=N;return XRPC_STDIO_IO_PROGRESS;
    }
    DWORD Error=GetLastError();
    if(Error==ERROR_NO_DATA)return XRPC_STDIO_IO_AGAIN;
    if(Error==ERROR_BROKEN_PIPE)return XRPC_STDIO_IO_EOF;
    return XRPC_STDIO_IO_ERROR;
}
static xrpcstdioio stdioExampleWrite(ptr Raw,cbytes Data,size_t Size,size_t* Count)
{
    stdioExamplePipe* P=Raw;
    DWORD N=0;
    if(!WriteFile(P->Out,Data,(DWORD)Size,&N,NULL))return XRPC_STDIO_IO_ERROR;
    if(!N)return XRPC_STDIO_IO_AGAIN;
    *Count=N;return XRPC_STDIO_IO_PROGRESS;
}
#else
static bool stdioExampleOpen(stdioExamplePipe* P)
{
    P->In=STDIN_FILENO;
    P->Out=STDOUT_FILENO;
    int InFlags=fcntl(P->In,F_GETFL,0),OutFlags=fcntl(P->Out,F_GETFL,0);
    return InFlags>=0 && OutFlags>=0 &&
           signal(SIGPIPE,SIG_IGN)!=SIG_ERR &&
           fcntl(P->In,F_SETFL,InFlags|O_NONBLOCK)==0 &&
           fcntl(P->Out,F_SETFL,OutFlags|O_NONBLOCK)==0;
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

int main(int Argc,char** Argv)
{
    if(Argc==1){
        puts("Run with redirected binary stdio: examples_stdio_main serve [be]");
        return 0;
    }
    if(Argc>3 || strcmp(Argv[1],"serve") || (Argc==3 && strcmp(Argv[2],"be")))return 1;
    stdioExamplePipe Pipe;
    if(!stdioExampleOpen(&Pipe)){
        fputs("stdio example requires supported redirected pipes\n",stderr);
        return 2;
    }
    xrpcserverconfig ServerConfig;
    xrpcServerConfigInit(&ServerConfig);
    ServerConfig.Call=stdioExampleEcho;
    xrpcserver* Server=NULL;
    if(!xrpcServerCreate(&ServerConfig,&Server))return 3;
    xrpcstdioconfig C;
    xrpcStdioConfigInit(&C);
    C.Duplex.Server=Server;
    C.Read=stdioExampleRead;
    C.Write=stdioExampleWrite;
    C.User=&Pipe;
    /* A small offer also fits Windows anonymous pipe buffer credits. */
    C.WriteChunkBytes=512u;
    if(Argc==3)C.Frame.Format=XRPC_FRAME_UINT32_BE;
    xrpcstdio* S=NULL;
    if(!xrpcStdioCreate(&C,&S)){xrpcServerDestroy(Server);return 4;}

    xrpcstdioinfo Info={.Size=sizeof(Info)};
    for(;;){
        if(!xrpcStdioStep(S))xrpcStdioClose(S,XRPC_CALL_TRANSPORT);
        if(!xrpcStdioInfo(S,&Info))return 5;
        if(Info.Drained)break;
        if(!Info.More)xrtSleep(1);
    }
    if(!xrpcStdioDestroy(S) || !xrpcServerDestroy(Server))return 6;
    return Info.Reason==XRPC_STDIO_EOF?0:7;
}
