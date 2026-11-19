#include "../examples/common/tcp_service.h"
#include "fixed_arrival.h"
#include <math.h>

static const xstrview rpcTcpArrivalParams=XRT_STR_LITERAL("[1,2,3]");

static int rpcTcpArrivalSubmit(ptr Context,xfuture** Future)
{
    return (int)xrpcTcpCall(Context,XRT_STR_LITERAL("echo"),rpcTcpArrivalParams,5000,Future);
}

static bool rpcTcpArrivalEcho(ptr Context,const xrpccallresult* Result)
{
    (void)Context;
    xrpcresponseinfo Info={.Size=sizeof(Info)};
    if(Result->Status!=XRPC_CALL_OK || Result->Delivery!=XRPC_REPLIED ||
       !xrpcResponsesInfo(Result->Responses,&Info) || Info.Count!=1 || Result->Index!=0)
        return false;
    const xrpcmessage* Message=Info.Messages;
    return Message->Kind==XRPC_MESSAGE_RESULT &&
        Message->Result.Raw.Size==rpcTcpArrivalParams.Size &&
        !memcmp(Message->Result.Raw.Data,rpcTcpArrivalParams.Data,rpcTcpArrivalParams.Size);
}

static void rpcTcpArrivalNegotiate(rpcTcpService* Service,xrpctcp* Peer)
{
    xrpcduplexextension Extension=XRPC_DUPLEX_EXTENSION_DEADLINE;
    xfuture* Future=NULL;
    rpcTcpServiceRequire(xrpcTcpEnableExtensions(Service->Accepted,Extension)==XRPC_DUPLEX_ACCEPTED &&
        xrpcTcpEnableExtensions(Peer,Extension)==XRPC_DUPLEX_ACCEPTED &&
        xrpcTcpNegotiateExtension(Peer,Extension,5000,&Future)==XRPC_DUPLEX_ACCEPTED,
        "benchmark deadline offer");
    const xrpccallresult* Result=rpcTcpServiceResult(Future);
    xrpcresponseinfo Info={.Size=sizeof(Info)};
    rpcTcpServiceRequire(Result->Status==XRPC_CALL_OK && Result->Delivery==XRPC_REPLIED &&
        xrpcResponsesInfo(Result->Responses,&Info) && Info.Count==1 && Result->Index==0 &&
        Info.Messages[0].Kind==XRPC_MESSAGE_RESULT && Info.Messages[0].Result.Raw.Size==4 &&
        !memcmp(Info.Messages[0].Result.Raw.Data,"true",4),"benchmark deadline agreement");
    xrtFutureDestroy(Future);
}

static size_t rpcTcpArrivalCount(cstr Text,size_t Limit)
{
    if(*Text<'0' || *Text>'9')return 0;
    char* End;
    unsigned long long Value=strtoull(Text,&End,10);
    return *End || Value>Limit?0:(size_t)Value;
}

int main(int Argc,char** Argv)
{
    if(Argc>7)return 1;
    size_t Count=Argc>1?rpcTcpArrivalCount(Argv[1],1000000u):16000;
    char* RateEnd=NULL;
    double Rate=Argc>2?strtod(Argv[2],&RateEnd):8000.;
    cstr Mode=Argc>3?Argv[3]:"plain";
    size_t Limit=Argc>4?rpcTcpArrivalCount(Argv[4],64):32;
    cstr Wait=Argc>5?Argv[5]:"yield";
    bool Deadline=!strcmp(Mode,"deadline"),Sleep=!strcmp(Wait,"sleep");
    if(!Count || !Limit || !(Rate>0.) || Rate>1000000. || !isfinite(Rate) ||
       (Argc>2 && (!*Argv[2] || *RateEnd)) || (!Deadline && strcmp(Mode,"plain")) ||
       (!Sleep && strcmp(Wait,"yield")))return 1;

    rpcTcpService Service;
    rpcTcpServiceStart(&Service);
    xrpctcpconfig Client=Service.Config;
    Client.Duplex.Server=NULL;
    Client.Duplex.MaxOperations=Client.Duplex.MaxPendingCalls=Limit;
    Client.Duplex.MaxIncoming=Limit;
    xrpctcp* Peer=NULL;
    rpcTcpServiceRequire(xrpcTcpConnect(Service.Engine,&Service.Address,1,&Client,&Peer),"benchmark client connect");
    double End=xrtTimer()+10.;
    while(!xrtAtomic32Load(&Service.Ready,XMEMORY_ACQUIRE)){
        rpcTcpServiceRequire(xrtTimer()<End,"benchmark peer accept");
        xrtSleep(1);
    }
    if(Deadline)rpcTcpArrivalNegotiate(&Service,Peer);
    for(unsigned I=0;I<100;++I){
        xfuture* Future=NULL;
        double WarmDeadline=xrtTimer()+5.;
        int Admission;
        while((Admission=rpcTcpArrivalSubmit(Peer,&Future))==XRPC_DUPLEX_OVERLOADED){
            rpcTcpServiceRequire(xrtTimer()<WarmDeadline,"benchmark warm-up admission credit");
            xrtThreadYield();
        }
        rpcTcpServiceRequire(Admission==XRPC_DUPLEX_ACCEPTED &&
            rpcTcpArrivalEcho(Peer,rpcTcpServiceResult(Future)),"benchmark warm-up");
        xrtFutureDestroy(Future);
    }

    rpcBenchArrivalConfig Config={Count,Rate,Sleep,Argc>6?Argv[6]:NULL,
        XRPC_DUPLEX_ACCEPTED,XRPC_DUPLEX_OVERLOADED,rpcTcpArrivalSubmit,rpcTcpArrivalEcho,Peer};
    int Status=rpcBenchRunArrivals(&Config);
    printf("rate: %.9f\nmax_operations: %zu\nmode: call\nitems_per_request: 1\n",Rate,Limit);
    printf("extensions: %s\narrival_wait: %s\n",Mode,Sleep?"sleep1":"yield");
    puts("workload: warmed native loopback Content-Length TCP, one connection, fixed arrivals, client admission and bounded server executor; terminal Future Watch observation and process CPU include benchmark work");
    rpcTcpServiceCycleClose(&Service,Peer);
    rpcTcpServiceStop(&Service);
    return Status;
}
