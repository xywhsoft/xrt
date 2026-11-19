#include "../tests/support/http_tls_service.h"
#include "fixed_arrival.h"
#include <math.h>
static xrpchttpauthstatus rpcBenchAuthorize(const xrpchttpauthrequest* R,bytes Out,size_t Capacity,size_t* Used,ptr User)
{
    (void)Out;(void)Capacity;(void)Used;(void)User;const xhttpfield* F=NULL;
    return xrtHttpFieldGetUnique(R->Head->Fields,R->Head->FieldCount,XRT_STR_LITERAL("Authorization"),&F)==XHTTP_NEXT_ITEM && F->Value.Size==31 && !memcmp(F->Value.Data,"Bearer benchmark-public-fixture",31)?XRPC_HTTP_AUTH_ALLOW:XRPC_HTTP_AUTH_UNAUTHORIZED;
}

typedef struct rpcHttpBenchmark {
    xrpchttpclient* Client;
    const xrpchttpitem* Items;
    size_t Mode;
} rpcHttpBenchmark;

static int rpcHttpBenchSubmit(ptr Data,xfuture** Future)
{
    rpcHttpBenchmark* B=Data;
    if(B->Mode==1)
        return (int)xrpcHttpNotifyAsync(B->Client,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[]"),5000,Future);
    if(B->Mode)
        return (int)xrpcHttpBatchAsync(B->Client,B->Items,B->Mode,5000,Future);
    return (int)xrpcHttpCallAsync(B->Client,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[]"),5000,Future);
}

static bool rpcHttpBenchValidate(ptr Data,const xrpccallresult* Result)
{
    const rpcHttpBenchmark* B=Data;
    if(B->Mode==1)
        return Result->Status==XRPC_CALL_NOTIFY_DONE &&
            Result->Delivery==XRPC_MAY_HAVE_EXECUTED && !Result->Responses;
    xrpcresponseinfo Info={.Size=sizeof(Info)};
    if(Result->Status!=(B->Mode?XRPC_CALL_BATCH_DONE:XRPC_CALL_OK) || Result->Delivery!=XRPC_REPLIED ||
       !xrpcResponsesInfo(Result->Responses,&Info) || Info.Count!=(B->Mode?B->Mode:1))return false;
    for(size_t K=0;K<Info.Count;++K){
        size_t Index=B->Mode?((const xrpcbatchcallresult*)Result)->Indices[K]:0;
        if(Index>=Info.Count)return false;
        const xrpcmessage* Message=&Info.Messages[Index];
        if(Message->Kind!=XRPC_MESSAGE_RESULT || Message->Result.Raw.Size!=2 ||
           memcmp(Message->Result.Raw.Data,"[]",2))return false;
    }
    return true;
}

int main(int argc,char** argv)
{
    size_t Count=argc>1?(size_t)strtoull(argv[1],NULL,10):1000;double Rate=argc>2?strtod(argv[2],NULL):1000.;
    size_t Mode=argc>3?(size_t)strtoull(argv[3],NULL,10):0;
    if(!Count || Count>1000000u || !(Rate>0) || !isfinite(Rate) || Rate>1000000. || Mode>256 || argc>9)return 1;
    bool Secure=argc>4?strtoul(argv[4],NULL,10)!=0:true;bool Sleep=argc>5 && !strcmp(argv[5],"sleep");
    if(argc>5 && !Sleep && strcmp(argv[5],"yield"))return 1;
    char* EndArg=NULL;unsigned long WithHeaders=argc>7?strtoul(argv[7],&EndArg,10):0;
    if(WithHeaders>1 || (argc>7 && (!*argv[7] || *EndArg)))return 1;
    unsigned long WithAuthorization=argc>8?strtoul(argv[8],&EndArg,10):0;
    if(WithAuthorization>1 || (argc>8 && (!*argv[8] || *EndArg)) || (WithAuthorization && !WithHeaders))return 1;
    xrpchttpserverconfig Server;xrpcHttpServerConfigInit(&Server);if(WithAuthorization){Server.Authorize=rpcBenchAuthorize;Server.MaxAuthorizationContextBytes=0;}
    rpcTlsService P;rpcTlsServiceStart(&P,&Server,NULL,Secure);xrpchttpconfig K;xrpcHttpConfigInit(&K);K.MaxRequests=128;
    const xhttpfield Headers[]={{XRT_STR_LITERAL("Authorization"),XRT_STR_LITERAL("Bearer benchmark-public-fixture")},{XRT_STR_LITERAL("X-Trace"),XRT_STR_LITERAL("benchmark-trace")}};
    size_t HeaderBytes=0;if(WithHeaders){K.RequestHeaders=Headers;K.RequestHeaderCount=2;rpcServiceRequire(xrtHttpFieldBlockWrite(Headers,2,NULL,0,&HeaderBytes),"benchmark header block size");}
    xrpchttpitem Items[256];for(size_t N=0;N<Mode;++N)Items[N]=(xrpchttpitem){XRPC_MESSAGE_CALL,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[]")};
    xrpchttpclient* C=Secure?rpcTlsServiceClient(&P,&K):rpcServiceClient(&P.Base,&K);
    xfuture* Ready=rpcServiceSubmit(C,"echo","[]",5000);rpcServiceRequire(rpcServiceResult(Ready)->Status==XRPC_CALL_OK,"matched benchmark readiness");xrtFutureDestroy(Ready);
    rpcHttpBenchmark Bench={C,Items,Mode};
    rpcBenchArrivalConfig Arrival={Count,Rate,Sleep,argc>6?argv[6]:NULL,
        XRPC_ADMISSION_ACCEPTED,XRPC_ADMISSION_OVERLOADED,rpcHttpBenchSubmit,rpcHttpBenchValidate,&Bench};
    int Status=rpcBenchRunArrivals(&Arrival);
    printf("custom_headers: %zu\ncustom_header_bytes: %zu\n",K.RequestHeaderCount,HeaderBytes);
    printf("authorization: %lu\n",WithAuthorization);
    puts("workload: absolute planned arrivals; every offer includes queue/arrival delay; terminal WATCH notification timestamp (READY registration is a separately counted upper bound, not historical Core publication); submit-to-notification and collector lag reported separately; refusals included at admission return; matched TCP/TLS 1.3 AES-128-GCM, same closure/workers/payload/rate and warmed persistent production server with bounded executor; setup/handshake/readiness/shutdown excluded; process CPU includes all threads, watch/timer/atomic/collection/validation/destruction and yielding load generator; array allocation/sort/report/optional trace excluded; no comparator, saturation or isolated native/TLS latency claim");
    printf("arrival_wait: %s\ntransport: %s\nmode: %s\nitems_per_request: %zu\n",Sleep?"sleep1":"yield",Secure?"tls":"tcp",Mode==1?"notification":Mode?"batch":"call",Mode?Mode:1);
    rpcServiceClientClose(C);
    rpcTlsServiceStop(&P,Secure);
    return Status;
}
