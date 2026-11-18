#include <xrt/process_pipe.h>
#include <stdio.h>
#include <string.h>

static int pipeChild(void)
{
    char Data[4];
    if(fread(Data,1,sizeof(Data),stdin)!=sizeof(Data) || memcmp(Data,"ping",4) ||
       fgetc(stdin)!=EOF || ferror(stdin))return 1;
    return fwrite("pong",1,4,stdout)==4 && fflush(stdout)==0?0:2;
}

static bool pipeExchange(xprocesspipe* Pipe)
{
    /* The owner can wait for I/O or a signal from any submitting thread.
     * Pre-signal once here to demonstrate the same wake contract. */
    if(!xrtProcessPipeWake(Pipe) || xrtProcessPipeWait(Pipe,false,false,0)!=XWAIT_OK)
        return false;
    const unsigned char Data[]="ping";
    size_t Sent=0;
    while(Sent<4){
        size_t N=0;
        xprocesspipeio R=xrtProcessPipeWrite(Pipe,Data+Sent,4-Sent,&N);
        if(R==XPROCESS_PIPE_PROGRESS)Sent+=N;
        else if(R!=XPROCESS_PIPE_AGAIN || xrtProcessPipeWait(Pipe,false,true,5000)!=XWAIT_OK)
            return false;
    }
    if(!xrtProcessPipeCloseInput(Pipe))return false;
    unsigned char Buffer[16],Reply[4];
    size_t Total=0;
    for(;;){
        size_t N=0;
        xprocesspipeio R=xrtProcessPipeRead(Pipe,Buffer,sizeof(Buffer),&N);
        if(R==XPROCESS_PIPE_EOF)break;
        if(R==XPROCESS_PIPE_PROGRESS){
            if(N>sizeof(Reply)-Total)return false;
            memcpy(Reply+Total,Buffer,N);
            Total+=N;
        } else if(R!=XPROCESS_PIPE_AGAIN || xrtProcessPipeWait(Pipe,true,false,5000)!=XWAIT_OK)
            return false;
    }
    return Total==sizeof(Reply) && memcmp(Reply,"pong",4)==0;
}

int main(int Argc,char** Argv)
{
    if(Argc==2 && !strcmp(Argv[1],"--child"))return pipeChild();
    if(Argc!=1)return 1;

    const cstr Args[]={"--child"};
    xprocessconfig Config;
    xrtProcessConfigInit(&Config);
    Config.Program=Argv[0];
    Config.Args=Args;
    Config.ArgCount=1;
    xprocess* Child=NULL;
    xprocesspipe* Pipe=NULL;
    if(!xrtProcessPipeSpawn(&Config,4096,&Child,&Pipe))return 2;
    bool Exchanged=pipeExchange(Pipe);
    xrtProcessPipeDestroy(Pipe);

    xwaitresult Reaped=xrtProcessWaitFor(Child,5000);
    if(Reaped!=XWAIT_OK && xrtProcessKill(Child))xrtProcessWaitFor(Child,5000);
    xprocessstatus Status;
    bool OK=Exchanged && Reaped==XWAIT_OK && xrtProcessStatus(Child,&Status) &&
        Status.Kind==XPROCESS_EXIT_CODE && Status.Code==0;
    xrtProcessDestroy(Child);
    if(OK)puts("ping -> pong");
    return OK?0:3;
}
