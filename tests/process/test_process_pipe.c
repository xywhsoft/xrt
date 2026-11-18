#include "../test.h"
#include "test_process_oom_allocator.h"
#if defined(_WIN32) || defined(_WIN64)
#include <fcntl.h>
#include <io.h>
#elif defined(__linux__)
#include <dirent.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

enum {Chunk=16384,Transfer=1024*1024};

static int pipeChild(bool Echo)
{
#if defined(_WIN32) || defined(_WIN64)
    if(_setmode(_fileno(stdin),_O_BINARY)<0 || _setmode(_fileno(stdout),_O_BINARY)<0)
        return 30;
#endif
    if(!Echo){
        xrtSleep(30000);
        return 0;
    }
    unsigned char Buffer[Chunk];
    size_t N;
    while((N=fread(Buffer,1,sizeof(Buffer),stdin))!=0){
        if(fwrite(Buffer,1,N,stdout)!=N || fflush(stdout)!=0)return 31;
    }
    return ferror(stdin)?32:0;
}

static void pipeConfig(xprocessconfig* C,cstr Program,const cstr* Args)
{
    testRequire(xrtProcessConfigInit(C),"pipe config");
    C->Program=Program;
    C->Args=Args;
    C->ArgCount=1;
    C->Stderr.Mode=XPROCESS_IO_NULL;
}

static void pipeSpawn(const xprocessconfig* C,xprocess** Child,xprocesspipe** Pipe,cstr Message)
{
    if(xrtProcessPipeSpawn(C,Chunk,Child,Pipe))return;
    const xerror* Error=xrtGetError();
    fprintf(stderr,"[process pipe error] operation=%s system=%d message=%s\n",
        xrtErrorOperation(Error),(int)xrtErrorSystemCode(Error),xrtErrorMessage(Error));
    testRequire(false,Message);
}

static int32 pipeSignal(ptr Raw)
{
    return xrtProcessPipeWake(Raw)?0:1;
}

static void pipeBlocked(cstr Program)
{
    const cstr Args[]={"--pipe-block"};
    xprocessconfig C;
    pipeConfig(&C,Program,Args);
    xprocesspipe* Pipe=NULL;
    xprocess* Child=NULL;
    pipeSpawn(&C,&Child,&Pipe,"spawn blocked child");
    unsigned char Buffer[Chunk]={1};
    size_t N=123;
    testRequire(xrtProcessPipeRead(Pipe,Buffer,sizeof(Buffer),&N)==XPROCESS_PIPE_AGAIN && N==0,
        "idle read must return without blocking");
    testRequire(xrtProcessPipeWait(Pipe,true,false,15)==XWAIT_TIMEOUT,"idle read wait expires");
    xthread* Signal=xrtThreadCreate(pipeSignal,Pipe,0);
    testRequire(Signal && xrtProcessPipeWait(Pipe,true,false,5000)==XWAIT_OK,
        "cross-thread wake interrupts the read wait");
    testRequire(xrtThreadWait(Signal)==XWAIT_OK,"wake producer joins");
    xrtThreadDestroy(Signal);
    bool Pending=false;
    for(size_t i=0;i<256;++i){
        xprocesspipeio R=xrtProcessPipeWrite(Pipe,Buffer,sizeof(Buffer),&N);
        if(R==XPROCESS_PIPE_AGAIN){Pending=true;break;}
        testRequire(R==XPROCESS_PIPE_PROGRESS && N>0,"blocked child write outcome");
    }
    testRequire(Pending,"bounded native write pressure");
#if defined(_WIN32) || defined(_WIN64)
    N=654;
    testRequire(xrtProcessPipeWrite(Pipe,Buffer,sizeof(Buffer)-1,&N)==XPROCESS_PIPE_ERROR && N==654,
        "reject resize while overlapped write owns its staging buffer");
    xrtClearError();
#endif
    testRequire(xrtProcessPipeCloseInput(Pipe) && xrtProcessPipeCloseInput(Pipe),
        "retire blocked write and half-close idempotently");
    N=987;
    testRequire(xrtProcessPipeWrite(Pipe,Buffer,sizeof(Buffer),&N)==XPROCESS_PIPE_ERROR && N==987,
        "write after half-close is an error");
    xrtClearError();
    xrtProcessPipeDestroy(Pipe);
    testRequire(xrtProcessWaitFor(Child,0)==XWAIT_TIMEOUT,"pipe retirement leaves child running");
    testRequire(xrtProcessKill(Child) && xrtProcessWaitFor(Child,5000)==XWAIT_OK,"retire test child");
    xrtProcessDestroy(Child);
}

static unsigned char pipeByte(size_t Offset)
{
    return (unsigned char)(Offset*27u+Offset/257u);
}

static void pipeEcho(cstr Program)
{
    const cstr Args[]={"--pipe-echo"};
    xprocessconfig C;
    pipeConfig(&C,Program,Args);
    xprocesspipe* Pipe=NULL;
    xprocess* Child=NULL;
    pipeSpawn(&C,&Child,&Pipe,"spawn blocking CRT echo child");
    unsigned char In[Chunk],Out[Chunk];
    size_t Sent=0,Received=0;
    bool Closed=false,Eof=false;
    double Deadline=xrtTimer()+15.0;
    while(!Eof){
        testRequire(xrtTimer()<Deadline,"finite echo deadline");
        size_t Written=0,Read=0;
        if(!Closed){
            size_t Offer=Transfer-Sent;
            if(Offer>sizeof(Out))Offer=sizeof(Out);
            for(size_t K=0;K<Offer;++K)Out[K]=pipeByte(Sent+K);
            xprocesspipeio W=xrtProcessPipeWrite(Pipe,Out,Offer,&Written);
            testRequire(W==XPROCESS_PIPE_PROGRESS || W==XPROCESS_PIPE_AGAIN,"echo write");
            Sent+=Written;
            if(Sent==Transfer){
                testRequire(xrtProcessPipeCloseInput(Pipe),"half-close after exact input");
                Closed=true;
            }
        }
        xprocesspipeio R=xrtProcessPipeRead(Pipe,In,sizeof(In),&Read);
        testRequire(R==XPROCESS_PIPE_PROGRESS || R==XPROCESS_PIPE_AGAIN || R==XPROCESS_PIPE_EOF,
            "echo read result");
        testRequire(Received+Read<=Transfer,"output is bounded");
        for(size_t K=0;K<Read;++K)testRequire(In[K]==pipeByte(Received+K),"echo byte order");
        Received+=Read;
        Eof=R==XPROCESS_PIPE_EOF;
        if(!Eof && !Read && !Written)testRequire(xrtProcessPipeWait(Pipe,true,!Closed,5000)==XWAIT_OK,
            "wait for either echo direction");
    }
    testRequire(Closed && Sent==Transfer && Received==Transfer,"echo every byte through EOF");
    xrtProcessPipeDestroy(Pipe);
    xprocessstatus Status;
    testRequire(xrtProcessWaitFor(Child,5000)==XWAIT_OK && xrtProcessStatus(Child,&Status) &&
        Status.Kind==XPROCESS_EXIT_CODE && Status.Code==0,"normal child exit");
    xrtProcessDestroy(Child);
}

static void pipePeerDeath(cstr Program)
{
    const cstr Args[]={"--pipe-block"};
    xprocessconfig C;
    pipeConfig(&C,Program,Args);
    xprocesspipe* Pipe=NULL;
    xprocess* Child=NULL;
    pipeSpawn(&C,&Child,&Pipe,"spawn child for peer death");
    unsigned char Buffer[128]={0};
    size_t N=919;
    testRequire(xrtProcessPipeRead(Pipe,Buffer,sizeof(Buffer),&N)==XPROCESS_PIPE_AGAIN,"prime read");
    testRequire(xrtProcessKill(Child) && xrtProcessWaitFor(Child,5000)==XWAIT_OK,"kill peer");
    testRequire(xrtProcessPipeWait(Pipe,true,false,5000)==XWAIT_OK,"child death wakes read");
    testRequire(xrtProcessPipeRead(Pipe,Buffer,sizeof(Buffer),&N)==XPROCESS_PIPE_EOF && N==0,
        "dead peer produces EOF");
    N=928;
    testRequire(xrtProcessPipeWrite(Pipe,Buffer,sizeof(Buffer),&N)==XPROCESS_PIPE_ERROR && N==928,
        "dead peer write returns an error without a signal");
    xrtClearError();
    xrtProcessPipeDestroy(Pipe);
    xrtProcessDestroy(Child);
}

#if defined(_WIN32) || defined(_WIN64) || defined(__linux__)
static unsigned long pipeResourceCount(void)
{
#if defined(_WIN32) || defined(_WIN64)
    DWORD Count;
    testRequire(GetProcessHandleCount(GetCurrentProcess(),&Count),"read native handle count");
    return Count;
#else
    DIR* Directory=opendir("/proc/self/fd");
    testRequire(Directory!=NULL,"open native fd inventory");
    unsigned long Count=0;
    while(readdir(Directory))++Count;
    closedir(Directory);
    return Count;
#endif
}
#endif

static void pipeConstructorFailures(cstr Program,testprocessoomallocator* Allocator)
{
    const cstr Args[]={"--pipe-echo"};
    xprocessconfig C;
    pipeConfig(&C,Program,Args);
    xprocesspipe* Pipe=(xprocesspipe*)(uintptr_t)1;
    xprocess* Child=(xprocess*)(uintptr_t)2;
    testRequire(!xrtProcessPipeSpawn(&C,0,&Child,&Pipe),"reject zero offer limit");
    xrtClearError();
    testProcessOomFailStore(Allocator,true);
    testRequire(!xrtProcessPipeSpawn(&C,Chunk,&Child,&Pipe),"initial allocation fails cleanly");
    testRequire(testProcessOomDeniedLoad(Allocator)>0 && xrtErrorKind(xrtGetError())==XERR_MEMORY,
        "allocation failure remains typed");
    testProcessOomFailStore(Allocator,false);
    xrtClearError();
#if defined(_WIN32) || defined(_WIN64) || defined(__linux__)
#if defined(_WIN32) || defined(_WIN64)
    /* A failed CreateProcess can initialize a process-wide Windows handle.
     * Establish the inventory after that existing Process path has run. */
    C.Program="xrt-missing-process-pipe-program-0e76836a";
    testRequire(xrtProcessSpawn(&C)==NULL,"prime native child launch failure");
    xrtClearError();
    C.Program=Program;
#endif
    unsigned long Before=pipeResourceCount();
#endif
#if defined(__linux__)
    /* Give the constructor room for stdin, then fail while opening stdout. */
    int Fd=open("/dev/null",O_RDONLY);
    struct rlimit Normal,Small;
    testRequire(Fd>=0 && getrlimit(RLIMIT_NOFILE,&Normal)==0,"read fd budget");
    Small=Normal;
    Small.rlim_cur=(rlim_t)Fd+4u;
    testRequire(Small.rlim_cur<Normal.rlim_cur && setrlimit(RLIMIT_NOFILE,&Small)==0,
        "constrain test process fd budget");
    bool Spawned=xrtProcessPipeSpawn(&C,Chunk,&Child,&Pipe);
    testRequire(setrlimit(RLIMIT_NOFILE,&Normal)==0 && close(Fd)==0,"restore fd budget");
    testRequire(!Spawned &&
        Pipe==(xprocesspipe*)(uintptr_t)1 && Child==(xprocess*)(uintptr_t)2,
        "native pipe creation failure preserves outputs");
    xrtClearError();
    testRequire(pipeResourceCount()==Before,"partial pipe creation closes acquired fd slots");
#endif
    C.Program="xrt-missing-process-pipe-program-0e76836a";
    for(size_t i=0;i<16;++i){
        testRequire(!xrtProcessPipeSpawn(&C,Chunk,&Child,&Pipe),"spawn failure retires prepared ends");
        testRequire(Pipe==(xprocesspipe*)(uintptr_t)1 && Child==(xprocess*)(uintptr_t)2,
            "failed construction preserves both outputs");
        xrtClearError();
    }
#if defined(_WIN32) || defined(_WIN64) || defined(__linux__)
    unsigned long After=pipeResourceCount();
    if(After!=Before)fprintf(stderr,"[native resource count] before=%lu after=%lu\n",Before,After);
    testRequire(After==Before,"failed child launch closes native endpoints");
#endif
}

int main(int Argc,char** Argv)
{
    if(Argc==2 && !strcmp(Argv[1],"--pipe-block"))return pipeChild(false);
    if(Argc==2 && !strcmp(Argv[1],"--pipe-echo"))return pipeChild(true);
    testprocessoomallocator Allocator;
    testRequire(testProcessOomInstall(&Allocator),"install fault allocator");
    pipeConstructorFailures(Argv[0],&Allocator);
    pipeBlocked(Argv[0]);
    pipeEcho(Argv[0]);
    pipePeerDeath(Argv[0]);
    puts("[PASS] process pipe: rollback, live wake, backpressure, half close, duplex echo, peer death");
    return 0;
}
