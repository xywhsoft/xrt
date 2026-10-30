#include <xrpc/server.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bool serverBenchEcho(const xrpcmessage* R,xrpcreply* Reply,const xrpcservercontext* Context,ptr User)
{(void)Context;(void)User;return xrpcReplyResult(Reply,R->Params.Raw);}
static int serverBenchCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return(X>Y)-(X<Y);}
static bool serverBenchResult(xfuture* F,size_t Items)
{
    xfutureresult V;if(xrtFutureWaitFor(F,10000)!=XWAIT_OK || !xrtFutureResult(F,&V) || V.State!=XFUTURE_RESOLVED || !V.Value)return false;
    const xrpcserverresult* R=V.Value;
    if(R->Status!=XRPC_SERVER_DONE || !R->MayHaveExecuted || R->Dispatch.Replies!=Items || R->Dispatch.Invoked!=Items || !R->Wire.Size)return false;
    size_t Values=0;for(size_t K=0;K+2<R->Wire.Size;++K)if(!memcmp(R->Wire.Data+K,"[7]",3))++Values;
    return Values==Items;
}
int main(int argc,char** argv)
{
    size_t Count=argc>1?(size_t)strtoull(argv[1],NULL,10):20000,Window=argc>2?(size_t)strtoull(argv[2],NULL,10):64,Items=argc>3?(size_t)strtoull(argv[3],NULL,10):1;
    if(!Count || Count>1000000u || !Window || Window>128 || !Items || Items>64 || argc>4)return 1;
    char Wire[8192];size_t Used=0;if(Items>1)Wire[Used++]='[';
    for(size_t K=0;K<Items;++K){if(K)Wire[Used++]=',';int N=snprintf(Wire+Used,sizeof(Wire)-Used,"{\"jsonrpc\":\"2.0\",\"id\":%zu,\"method\":\"echo\",\"params\":[7]}",K);if(N<=0 || (size_t)N>=sizeof(Wire)-Used)return 2;Used+=(size_t)N;}
    if(Items>1)Wire[Used++]=']';
    xstrview Input={Wire,Used};xrpcserverconfig K;xrpcServerConfigInit(&K);K.Call=serverBenchEcho;K.MaxOperations=256;K.MaxResults=512;
    xrpcserver* S=NULL;if(!xrpcServerCreate(&K,&S))return 3;xfuture* Futures[128];double Start=xrtTimer();size_t Completed=0;
    while(Completed<Count){size_t N=Count-Completed<Window?Count-Completed:Window;
        for(size_t I=0;I<N;++I)if(xrpcServerSubmit(S,Input,5000,&Futures[I])!=XRPC_SERVER_ACCEPTED)return 4;
        for(size_t I=0;I<N;++I){if(!serverBenchResult(Futures[I],Items))return 5;xrtFutureDestroy(Futures[I]);}Completed+=N;
    }
    double Seconds=xrtTimer()-Start;size_t Samples=Count<10000?Count:10000;double* Latency=malloc(Samples*sizeof(double));if(!Latency || !(Seconds>0))return 6;
    for(size_t I=0;I<Samples;++I){Start=xrtTimer();xfuture* F=NULL;if(xrpcServerSubmit(S,Input,5000,&F)!=XRPC_SERVER_ACCEPTED || !serverBenchResult(F,Items))return 7;xrtFutureDestroy(F);Latency[I]=(xrtTimer()-Start)*1000000.;}
    qsort(Latency,Samples,sizeof(double),serverBenchCompare);
    printf("operations_per_sec: %.9f\ncompleted: %zu\nlatency_samples: %zu\np50_us: %.9f\np99_us: %.9f\nwindow: %zu\nitems: %zu\n",(double)Completed/Seconds,Completed,Samples,Latency[(Samples*50+99)/100-1],Latency[(Samples*99+99)/100-1],Window,Items);
    puts("workload: bounded server execution, owned request, full decode/dispatch/encode, worker queue/Future/result and destruction; windowed throughput, separate one-in-flight latency phase; in-process execution, no transport or independent comparator; operation is one document");
    free(Latency);return xrpcServerDestroy(S)?0:8;
}
