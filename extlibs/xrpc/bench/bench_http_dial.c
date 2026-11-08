#include "../examples/common/http_service.h"
#include <xrpc/http_dial.h>
static int compare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return(X>Y)-(X<Y);}
int main(int argc,char** argv)
{
    size_t Count=argc>1?(size_t)strtoull(argv[1],NULL,10):500;if(argc>2 || !Count || Count>100000u)return 1;
    double* Latency=malloc(Count*sizeof(double));if(!Latency)return 2;
    rpcService S;rpcServiceStart(&S,NULL);xnetresolverconfig K;xrtNetResolverConfigInit(&K);K.Workers=1;K.CacheEntries=0;K.RequestLimit=64;K.QueryLimit=64;
    xnetresolver* R=xrtNetResolverCreate(&K);rpcServiceRequire(R!=NULL,"dial benchmark resolver");xrpchttpdialconfig D;xrpcHttpDialConfigInit(&D);D.Family=XNET_FAMILY_IPV4;
    double Began=xrtTimer();
    for(size_t N=0;N<Count;++N){double Start=xrtTimer();xrpchttpclient* C=NULL;
        rpcServiceRequire(xrpcHttpDial(S.Engine,R,XRT_STR_LITERAL("localhost"),S.Address.Port,1,XRT_STR_LITERAL("localhost"),XRT_STR_LITERAL("/rpc"),NULL,&D,&C),"cold dial");
        xfuture* F=rpcServiceSubmit(C,"echo","[]",5000);rpcServiceRequire(rpcServiceResult(F)->Status==XRPC_CALL_OK,"cold call result");
        xrtFutureDestroy(F);rpcServiceClientClose(C);Latency[N]=(xrtTimer()-Start)*1000000.;
    }
    double Seconds=xrtTimer()-Began;xnetresolverstats Stats;rpcServiceRequire(xrtNetResolverStats(R,&Stats) && Stats.Submitted==Count && !Stats.CacheHits,"cold resolver count/cache policy");
    rpcServiceRequire(xrtNetResolverDestroy(R),"cold resolver join");rpcServiceStop(&S);qsort(Latency,Count,sizeof(double),compare);
    printf("completed: %zu\nlatency_samples: %zu\nresolver_submitted: %llu\ncycles_per_sec: %.9f\np50_us: %.9f\np99_us: %.9f\n",Count,Count,(unsigned long long)Stats.Submitted,(double)Count/Seconds,Latency[(Count*50+99)/100-1],Latency[(Count*99+99)/100-1]);
    puts("workload: serial closed-loop IPv4 localhost factory/DNS-cache-miss/TCP/call/result-drop/client-close observation, callback return tails at final engine join; system resolver may cache internally, app resolver cache disabled; engine/server/resolver construction and final join outside timed loop; no fixed-arrival, external DNS, TLS or saturation-capacity claim");free(Latency);return 0;
}
