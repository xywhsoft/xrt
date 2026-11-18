#include "../internal/xrt_process.h"
#include <xrt/process_pipe.h>

#if defined(XRT_FEATURE_PROCESS_PIPE)

#if defined(_WIN32) || defined(_WIN64)
#include <xrt/atomic.h>
#include <stdio.h>
#ifndef PIPE_REJECT_REMOTE_CLIENTS
#define PIPE_REJECT_REMOTE_CLIENTS 0x00000008u
#endif

/* This API is present on the same Windows versions as CancelIoEx. */
#if defined(__TINYC__)
BOOL WINAPI GetNamedPipeClientProcessId(HANDLE Pipe,PULONG ProcessId);
BOOL WINAPI CancelIoEx(HANDLE File,LPOVERLAPPED Operation);
#endif

typedef struct xprocesspipeoperation {
    HANDLE File;
    OVERLAPPED Native;
    size_t Offered;
    bool Pending;
} xprocesspipeoperation;

struct xprocesspipe {
    size_t Chunk;
    xprocesspipeoperation Read,Write;
    HANDLE Wake;
    unsigned char Data[];
};

static void __xrtProcessPipeCloseWin(HANDLE File)
{
    if(File && File!=INVALID_HANDLE_VALUE)CloseHandle(File);
}

static void __xrtProcessPipeRetire(xprocesspipeoperation* Op)
{
    if(Op->Pending){
        DWORD Count;
        CancelIoEx(Op->File,&Op->Native);
        GetOverlappedResult(Op->File,&Op->Native,&Count,TRUE);
        Op->Pending=false;
    }
    __xrtProcessPipeCloseWin(Op->File);
    Op->File=NULL;
}

static void __xrtProcessPipeUnit(xprocesspipe* P)
{
    __xrtProcessPipeRetire(&P->Read);
    __xrtProcessPipeRetire(&P->Write);
    __xrtProcessPipeCloseWin(P->Read.Native.hEvent);
    __xrtProcessPipeCloseWin(P->Write.Native.hEvent);
    __xrtProcessPipeCloseWin(P->Wake);
}

static bool __xrtProcessPipePair(bool ParentReads,HANDLE* Read,HANDLE* Write)
{
    static xatomic32 Sequence=XRT_ATOMIC32_INIT(0);
    char Name[96];
    DWORD ProcessId=GetCurrentProcessId();
    uint32 Number=xrtAtomic32FetchAdd(&Sequence,1,XMEMORY_RELAXED);
    int Length=snprintf(Name,sizeof(Name),"\\\\.\\pipe\\xrt.process.%08lx.%08lx",
        (unsigned long)ProcessId,(unsigned long)Number);
    if(Length<0 || (size_t)Length>=sizeof(Name)){
        SetLastError(ERROR_INVALID_NAME);
        return false;
    }
    /* The parent owns the overlapped end. The child's CRT uses ordinary
     * blocking handles. Neither end is inheritable until ProcessSpawn. */
    DWORD Access=PIPE_ACCESS_INBOUND|FILE_FLAG_FIRST_PIPE_INSTANCE;
    if(ParentReads)Access|=FILE_FLAG_OVERLAPPED;
    *Read=CreateNamedPipeA(Name,Access,PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|
        PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,0,16384,0,NULL);
    if(*Read==INVALID_HANDLE_VALUE)return false;
    *Write=CreateFileA(Name,GENERIC_WRITE,0,NULL,OPEN_EXISTING,
        ParentReads?0:FILE_FLAG_OVERLAPPED,NULL);
    if(*Write==INVALID_HANDLE_VALUE)return false;

    /* Opening the client endpoint connects the instance. Accept only the
     * connection made by this parent, before any payload enters the pipe. */
    ULONG Client;
    if(!GetNamedPipeClientProcessId(*Read,&Client))return false;
    if(Client==ProcessId)return true;
    SetLastError(ERROR_ACCESS_DENIED);
    return false;
}

static int __xrtProcessPipePrepare(xprocesspipe* P,intptr_t Child[2])
{
    HANDLE In=NULL,Out=NULL;
    P->Wake=CreateEventW(NULL,FALSE,FALSE,NULL);
    if(!P->Wake)return (int)GetLastError();
    P->Read.Native.hEvent=CreateEventW(NULL,TRUE,FALSE,NULL);
    if(!P->Read.Native.hEvent)return (int)GetLastError();
    P->Write.Native.hEvent=CreateEventW(NULL,TRUE,FALSE,NULL);
    if(!P->Write.Native.hEvent)return (int)GetLastError();
    bool OK=__xrtProcessPipePair(false,&In,&P->Write.File) &&
            __xrtProcessPipePair(true,&P->Read.File,&Out);
    Child[0]=(intptr_t)In;
    Child[1]=(intptr_t)Out;
    return OK?0:(int)GetLastError();
}

static void __xrtProcessPipeCloseChild(intptr_t Child[2])
{
    __xrtProcessPipeCloseWin((HANDLE)Child[0]);
    __xrtProcessPipeCloseWin((HANDLE)Child[1]);
}

static size_t __xrtProcessPipeAllocation(size_t Chunk)
{
    return sizeof(xprocesspipe)+2u*Chunk;
}

static int __xrtProcessPipeSignal(xprocesspipe* P)
{
    return SetEvent(P->Wake)?0:(int)GetLastError();
}

static int __xrtProcessPipeEndInput(xprocesspipe* P)
{
    __xrtProcessPipeRetire(&P->Write);
    return 0;
}

static xprocesspipeio __xrtProcessPipeIo(xprocesspipe* P,bytes Input,cbytes Output,
    size_t Size,size_t* Count,int* Error)
{
    xprocesspipeoperation* Op=Input?&P->Read:&P->Write;
    if(!Op->File){
        *Error=ERROR_INVALID_HANDLE;
        return XPROCESS_PIPE_ERROR;
    }
    unsigned char* Stage=Input?P->Data:P->Data+P->Chunk;
    if(Op->Pending && Op->Offered!=Size){
        *Error=ERROR_INVALID_PARAMETER;
        return XPROCESS_PIPE_ERROR;
    }
    if(!Op->Pending){
        Op->Native=(OVERLAPPED){.hEvent=Op->Native.hEvent};
        if(Output)memcpy(Stage,Output,Size);
        BOOL Started=Input?ReadFile(Op->File,Stage,(DWORD)Size,NULL,&Op->Native):
            WriteFile(Op->File,Stage,(DWORD)Size,NULL,&Op->Native);
        if(!Started){
            DWORD Code=GetLastError();
            if(Code==ERROR_IO_PENDING){
                Op->Pending=true;
                Op->Offered=Size;
                return XPROCESS_PIPE_AGAIN;
            }
            if(Input && (Code==ERROR_BROKEN_PIPE || Code==ERROR_PIPE_NOT_CONNECTED))
                return XPROCESS_PIPE_EOF;
            *Error=(int)Code;
            return XPROCESS_PIPE_ERROR;
        }
    }
    DWORD Done;
    if(!GetOverlappedResult(Op->File,&Op->Native,&Done,FALSE)){
        DWORD Code=GetLastError();
        if(Code==ERROR_IO_INCOMPLETE){
            Op->Pending=true;
            Op->Offered=Size;
            return XPROCESS_PIPE_AGAIN;
        }
        Op->Pending=false;
        if(Input && (Code==ERROR_BROKEN_PIPE || Code==ERROR_PIPE_NOT_CONNECTED))
            return XPROCESS_PIPE_EOF;
        *Error=(int)Code;
        return XPROCESS_PIPE_ERROR;
    }
    Op->Pending=false;
    if(!Done){
        if(Input)return XPROCESS_PIPE_AGAIN; /* Peer wrote zero bytes. */
        *Error=ERROR_WRITE_FAULT;
        return XPROCESS_PIPE_ERROR;
    }
    if(Input)memcpy(Input,Stage,Done);
    *Count=Done;
    return XPROCESS_PIPE_PROGRESS;
}

static xwaitresult __xrtProcessPipeWaitNative(xprocesspipe* P,bool Read,bool Write,
    int Milliseconds,int* Error)
{
    HANDLE Events[3]={P->Wake};
    DWORD Count=1;
    if(Read){
        if(!P->Read.Pending)return XWAIT_OK;
        Events[Count++]=P->Read.Native.hEvent;
    }
    if(Write){
        if(!P->Write.Pending)return XWAIT_OK;
        Events[Count++]=P->Write.Native.hEvent;
    }
    DWORD Result=WaitForMultipleObjects(Count,Events,FALSE,
        Milliseconds<0?INFINITE:(DWORD)Milliseconds);
    if(Result==WAIT_TIMEOUT)return XWAIT_TIMEOUT;
    if(Result<WAIT_OBJECT_0+Count)return XWAIT_OK;
    *Error=(int)GetLastError();
    return XWAIT_ERROR;
}

#else
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

struct xprocesspipe {
    size_t Chunk;
    int Read,Write,Wake[2];
};

static int __xrtProcessPipeCloseFd(int* Fd)
{
    int Old=*Fd;
    *Fd=-1;
    return Old<0 || close(Old)==0?0:errno;
}

static void __xrtProcessPipeUnit(xprocesspipe* P)
{
    __xrtProcessPipeCloseFd(&P->Write);
    __xrtProcessPipeCloseFd(&P->Read);
    __xrtProcessPipeCloseFd(&P->Wake[0]);
    __xrtProcessPipeCloseFd(&P->Wake[1]);
}

static bool __xrtProcessPipeOwnFd(int* Fd)
{
    if(*Fd>STDERR_FILENO)return fcntl(*Fd,F_SETFD,FD_CLOEXEC)==0;
    int Copy=fcntl(*Fd,F_DUPFD_CLOEXEC,STDERR_FILENO+1);
    if(Copy<0)return false;
    close(*Fd);
    *Fd=Copy;
    return true;
}

static bool __xrtProcessPipeNonblocking(int Fd)
{
    int Flags=fcntl(Fd,F_GETFL,0);
    return Flags>=0 && fcntl(Fd,F_SETFL,Flags|O_NONBLOCK)==0;
}

static bool __xrtProcessPipePair(bool Socket,int Fds[2])
{
    if(Socket){
        int Type=SOCK_STREAM;
        #if defined(SOCK_CLOEXEC)
        Type|=SOCK_CLOEXEC;
        #endif
        if(socketpair(AF_UNIX,Type,0,Fds)!=0)return false;
    } else {
        #if defined(__linux__) || defined(__ANDROID__)
        if(pipe2(Fds,O_CLOEXEC)!=0)return false;
        #else
        if(pipe(Fds)!=0)return false;
        #endif
    }
    return __xrtProcessPipeOwnFd(&Fds[0]) && __xrtProcessPipeOwnFd(&Fds[1]);
}

static int __xrtProcessPipePrepare(xprocesspipe* P,intptr_t Child[2])
{
    int In[2]={-1,-1},Out[2]={-1,-1};
    P->Read=P->Write=P->Wake[0]=P->Wake[1]=-1;
    bool OK=__xrtProcessPipePair(true,In) && __xrtProcessPipePair(false,Out) &&
        __xrtProcessPipePair(false,P->Wake);
    Child[0]=In[0];
    Child[1]=Out[1];
    P->Read=Out[0];
    P->Write=In[1];
    if(!OK)return errno;
    if(!__xrtProcessPipeNonblocking(P->Read) || !__xrtProcessPipeNonblocking(P->Write) ||
       !__xrtProcessPipeNonblocking(P->Wake[0]) || !__xrtProcessPipeNonblocking(P->Wake[1]))
        return errno;
#if !defined(MSG_NOSIGNAL) && defined(SO_NOSIGPIPE)
    int On=1;
    if(setsockopt(P->Write,SOL_SOCKET,SO_NOSIGPIPE,&On,sizeof(On))!=0)return errno;
#elif !defined(MSG_NOSIGNAL)
    return ENOTSUP;
#endif
    return 0;
}

static void __xrtProcessPipeCloseChild(intptr_t Child[2])
{
    if(Child[0]>=0)close((int)Child[0]);
    if(Child[1]>=0)close((int)Child[1]);
}

static size_t __xrtProcessPipeAllocation(size_t Chunk)
{
    (void)Chunk;
    return sizeof(xprocesspipe);
}

static int __xrtProcessPipeSignal(xprocesspipe* P)
{
    unsigned char Byte=0;
    ssize_t Done;
    do{Done=write(P->Wake[1],&Byte,1);}while(Done<0 && errno==EINTR);
    if(Done>=0 || errno==EAGAIN || errno==EWOULDBLOCK)return 0;
    return errno;
}

static int __xrtProcessPipeEndInput(xprocesspipe* P)
{
    return __xrtProcessPipeCloseFd(&P->Write);
}

static xprocesspipeio __xrtProcessPipeIo(xprocesspipe* P,bytes Input,cbytes Output,
    size_t Size,size_t* Count,int* Error)
{
    ssize_t Done;
    int Flags=0;
#if defined(MSG_NOSIGNAL)
    Flags=MSG_NOSIGNAL;
#endif
    do{
        Done=Input?read(P->Read,Input,Size):send(P->Write,Output,Size,Flags);
    }while(Done<0 && errno==EINTR);
    if(Done>0){*Count=(size_t)Done;return XPROCESS_PIPE_PROGRESS;}
    if(!Done && Input)return XPROCESS_PIPE_EOF;
    if(Done<0 && (errno==EAGAIN || errno==EWOULDBLOCK))return XPROCESS_PIPE_AGAIN;
    *Error=Done<0?errno:EIO;
    return XPROCESS_PIPE_ERROR;
}

static xwaitresult __xrtProcessPipeWaitNative(xprocesspipe* P,bool Read,bool Write,
    int Milliseconds,int* Error)
{
    if(Write && P->Write<0)return XWAIT_OK;
    struct pollfd Fds[3]={
        {P->Wake[0],POLLIN,0},
        {Read?P->Read:-1,POLLIN,0},
        {Write?P->Write:-1,POLLOUT,0}
    };
    int Ready=poll(Fds,3,Milliseconds);
    if(!Ready)return XWAIT_TIMEOUT;
    if(Ready<0){
        if(errno==EINTR)return XWAIT_CLOSED; /* Internal retry, original deadline. */
        *Error=errno;
        return XWAIT_ERROR;
    }
    if((Fds[0].revents|Fds[1].revents|Fds[2].revents)&POLLNVAL){
        *Error=EBADF;
        return XWAIT_ERROR;
    }
    if(Fds[0].revents&POLLIN){
        unsigned char Bytes[256];
        ssize_t Done;
        do{Done=read(P->Wake[0],Bytes,sizeof(Bytes));}while(Done<0 && errno==EINTR);
        if(Done<0 && errno!=EAGAIN && errno!=EWOULDBLOCK){
            *Error=errno;
            return XWAIT_ERROR;
        }
    }
    return XWAIT_OK;
}
#endif

static bool __xrtProcessPipeReport(xprocesserror Code,cstr Operation,int Error)
{
    __xrtProcessErrorSet(Error?__xrtSystemErrorKind(Error):XERR_ARGUMENT,Code,
        Operation,Error?"native pipe operation failed":"invalid process pipe argument",Error);
    return false;
}

XRT_API bool xrtProcessPipeSpawn(const xprocessconfig* Config,size_t ChunkBytes,
    xprocess** Process,xprocesspipe** Pipe)
{
    if(!Config || !Process || !Pipe || !xrtMemRangeValid(Config,sizeof(*Config)) ||
       !xrtMemRangeValid(Process,sizeof(*Process)) || !xrtMemRangeValid(Pipe,sizeof(*Pipe)) ||
       xrtMemRangesOverlap(Process,sizeof(*Process),Pipe,sizeof(*Pipe)) ||
       xrtMemRangesOverlap(Config,sizeof(*Config),Process,sizeof(*Process)) ||
       xrtMemRangesOverlap(Config,sizeof(*Config),Pipe,sizeof(*Pipe)) ||
       Config->Terminal || !ChunkBytes || ChunkBytes>XPROCESS_PIPE_MAX_CHUNK)
        return __xrtProcessPipeReport(XPROCESS_ERROR_ARGUMENT,"pipe.spawn",0);

    xprocesspipe* P=xrtCalloc(1,__xrtProcessPipeAllocation(ChunkBytes));
    if(!P)return false;
    P->Chunk=ChunkBytes;
    intptr_t Child[2]={-1,-1};
    int Error=__xrtProcessPipePrepare(P,Child);
    xprocess* Started=NULL;
    if(!Error){
        xprocessconfig C=*Config;
        C.Stdin=(xprocessio){XPROCESS_IO_HANDLE,Child[0]};
        C.Stdout=(xprocessio){XPROCESS_IO_HANDLE,Child[1]};
        Started=xrtProcessSpawn(&C);
    }
    __xrtProcessPipeCloseChild(Child);
    if(!Started){
        xrtProcessPipeDestroy(P);
        if(Error)__xrtProcessPipeReport(XPROCESS_ERROR_PIPE,"pipe.spawn",Error);
        return false;
    }
    *Process=Started;
    *Pipe=P;
    return true;
}

static xprocesspipeio __xrtProcessPipeTransfer(xprocesspipe* P,bytes Input,cbytes Output,
    size_t Size,size_t* Count)
{
    cbytes Data=Input?Input:Output;
    const xprocesserror Code=Input?XPROCESS_ERROR_READ:XPROCESS_ERROR_WRITE;
    cstr Operation=Input?"pipe.read":"pipe.write";
    if(!P || !Data || !Count || !Size || Size>P->Chunk ||
       !xrtMemRangeValid(Data,Size) || !xrtMemRangeValid(Count,sizeof(*Count)) ||
       xrtMemRangesOverlap(Data,Size,Count,sizeof(*Count))){
        __xrtProcessPipeReport(XPROCESS_ERROR_ARGUMENT,Operation,0);
        return XPROCESS_PIPE_ERROR;
    }
    size_t Done=0;
    int Error=0;
    xprocesspipeio Result=__xrtProcessPipeIo(P,Input,Output,Size,&Done,&Error);
    if(Result==XPROCESS_PIPE_ERROR)__xrtProcessPipeReport(Code,Operation,Error);
    else *Count=Done;
    return Result;
}

XRT_API xprocesspipeio xrtProcessPipeRead(xprocesspipe* Pipe,bytes Data,size_t Size,size_t* Count)
{
    return __xrtProcessPipeTransfer(Pipe,Data,NULL,Size,Count);
}

XRT_API xprocesspipeio xrtProcessPipeWrite(xprocesspipe* Pipe,cbytes Data,size_t Size,size_t* Count)
{
    return __xrtProcessPipeTransfer(Pipe,NULL,Data,Size,Count);
}

XRT_API xwaitresult xrtProcessPipeWait(xprocesspipe* Pipe,bool Read,bool Write,int64 Timeout)
{
    if(!Pipe || Timeout<XRT_WAIT_FOREVER){
        __xrtProcessPipeReport(XPROCESS_ERROR_ARGUMENT,"pipe.wait",0);
        return XWAIT_ERROR;
    }
    double Deadline=__xrtWaitAfter(Timeout);
    for(;;){
        int64 Left=__xrtWaitRemaining(Deadline);
        if(Left<XRT_WAIT_FOREVER)return XWAIT_ERROR;
        int Duration=Left>INT_MAX?INT_MAX:(int)Left;
        int Error=0;
        xwaitresult Result=__xrtProcessPipeWaitNative(Pipe,Read,Write,Duration,&Error);
        if(Result==XWAIT_ERROR){
            __xrtProcessPipeReport(XPROCESS_ERROR_WAIT,"pipe.wait",Error);
            return Result;
        }
        if(Result==XWAIT_OK)return Result;
        if(__xrtWaitExpired(Deadline))return XWAIT_TIMEOUT;
    }
}

XRT_API bool xrtProcessPipeWake(xprocesspipe* Pipe)
{
    if(!Pipe)return __xrtProcessPipeReport(XPROCESS_ERROR_ARGUMENT,"pipe.wake",0);
    int Error=__xrtProcessPipeSignal(Pipe);
    return !Error || __xrtProcessPipeReport(XPROCESS_ERROR_PIPE,"pipe.wake",Error);
}

XRT_API bool xrtProcessPipeCloseInput(xprocesspipe* Pipe)
{
    if(!Pipe)return __xrtProcessPipeReport(XPROCESS_ERROR_ARGUMENT,"pipe.close_input",0);
    int Error=__xrtProcessPipeEndInput(Pipe);
    return !Error || __xrtProcessPipeReport(XPROCESS_ERROR_CLOSE,"pipe.close_input",Error);
}

XRT_API void xrtProcessPipeDestroy(xprocesspipe* Pipe)
{
    if(!Pipe)return;
    __xrtProcessPipeUnit(Pipe);
    xrtFree(Pipe);
}
#endif
