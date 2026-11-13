#include <xrpc/tls.h>
#include <xrt/memory_stats.h>
#include <xrt/thread.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include "process_cpu.h"
#include "../../../tests/fixtures/tls_server.h"
static xtlscontext* TLSContext;static xtlsidentity* Identity;static xtlsverifier* Verifier;static xtlsserverconfig NativeServer;
static xtlsverifydecision benchTlsPin(const xtlspeer* Peer,ptr User)
{(void)User;xbytesview Pin={0};if(!Peer || !Peer->CertificateCount || !xrtTlsIdentityCertificate(Identity,0,&Pin))return XTLS_VERIFY_REJECT;return Peer->Certificates[0].Raw.Size==Pin.Size && !memcmp(Peer->Certificates[0].Raw.Data,Pin.Data,Pin.Size)?XTLS_VERIFY_ACCEPT:XTLS_VERIFY_REJECT;}
static xstrview BenchTlsParams={"[1,2,3]",7};
static xatomic32 Ready;static xrpctls* Accepted;static xrpctlsconfig Config;
static bool benchTlsEcho(const xrpcmessage* M,xrpcreply* R,const xrpcservercontext* C,ptr U)
{(void)C;(void)U;return xrpcReplyResult(R,M->Params.Raw);}
static bool benchTlsAccept(xnetlistener* L,xnetstream* S,ptr U)
{(void)L;(void)U;if(!xrpcTlsAccept(S,&NativeServer,&Config,&Accepted))return false;xrtAtomic32Store(&Ready,1,XMEMORY_RELEASE);return true;}
static bool benchTlsCycle(xrpctls* Peer,size_t N,double* Latency)
{
    xfuture* F[32];double Start=xrtTimer();
    for(size_t I=0;I<N;++I)if(xrpcTlsCall(Peer,XRT_STR_LITERAL("echo"),BenchTlsParams,10000,F+I)!=XRPC_DUPLEX_ACCEPTED)return false;
    for(size_t I=0;I<N;++I){if(xrtFutureWaitFor(F[I],15000)!=XWAIT_OK)return false;const xrpccallresult* R=xrtFutureValue(F[I]);if(!R || R->Status!=XRPC_CALL_OK || R->Delivery!=XRPC_REPLIED)return false;xrpcresponseinfo Info={0};Info.Size=sizeof(Info);if(!xrpcResponsesInfo(R->Responses,&Info) || R->Index>=Info.Count || Info.Messages[R->Index].Result.Raw.Size!=BenchTlsParams.Size || memcmp(Info.Messages[R->Index].Result.Raw.Data,BenchTlsParams.Data,BenchTlsParams.Size))return false;xrtFutureDestroy(F[I]);}
    if(Latency)*Latency=(xrtTimer()-Start)*1e6;
    return true;
}
static int benchTlsCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return (X>Y)-(X<Y);}
int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):2000,Width=Argc>2?(size_t)strtoull(Argv[2],NULL,10):1;bool Heap=Argc>3 && strtoul(Argv[3],NULL,10);
    size_t PayloadBytes=Argc>4?(size_t)strtoull(Argv[4],NULL,10):0;
    if(!Count || Count>1000000 || !Width || Width>32 || Argc>5 || (PayloadBytes && (PayloadBytes<4 || PayloadBytes>524288)))return 1;
    char* Payload=PayloadBytes?xrtMalloc(PayloadBytes):NULL;if(PayloadBytes && !Payload)return 16;
    if(Payload){Payload[0]='[';Payload[1]='"';memset(Payload+2,'x',PayloadBytes-4);Payload[PayloadBytes-2]='"';Payload[PayloadBytes-1]=']';BenchTlsParams=(xstrview){Payload,PayloadBytes};}
    TLSContext=testTlsServerContext();Identity=testTlsServerIdentity();if(!TLSContext || !Identity)return 17;
    xtlsverifierconfig V;xrtTlsVerifierConfigInit(&V);V.Verify=benchTlsPin;Verifier=xrtTlsVerifierCreate(&V);if(!Verifier)return 18;
    xrtTlsServerConfigInit(&NativeServer);NativeServer.Context=TLSContext;NativeServer.Identity=Identity;
    xrtAtomic32Init(&Ready,0);xnetengineconfig C;xrtNetEngineConfigInit(&C);C.Workers=2;xnetengine* E=xrtNetEngineCreate(&C);if(!E || !xrtNetEngineStart(E))return 2;
    xrpcserverconfig S;xrpcServerConfigInit(&S);S.Call=benchTlsEcho;xrpcserver* Server=NULL;if(!xrpcServerCreate(&S,&Server))return 3;xrpcTlsConfigInit(&Config);Config.Duplex.Server=Server;
    xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrParse(&L.Address,"127.0.0.1",0);xnetlistenerevents Events={benchTlsAccept,NULL,NULL};xnetlistener* Listener=xrtNetListen(E,&L,&Events,NULL,NULL);xnetaddr Remote;
    if(!Listener || !xrtNetListenerLocal(Listener,&Remote))return 4;
    xtlsclientconfig Client;xrtTlsClientConfigInit(&Client);Client.Context=TLSContext;Client.Verifier=Verifier;Client.ServerName=XRT_STR_LITERAL("rpc.fixture");
    xrpctls* Peer=NULL;if(!xrpcTlsConnect(E,&Remote,1,&Client,&Config,&Peer))return 5;double End=xrtTimer()+10;
    while(!xrtAtomic32Load(&Ready,XMEMORY_ACQUIRE)){if(xrtTimer()>=End)return 6;xrtSleep(1);}
    double* Latency=Width==1?xrtMalloc(Count*sizeof(double)):NULL;if(Width==1 && !Latency)return 7;
    for(unsigned I=0;I<100;++I)if(!benchTlsCycle(Peer,Width,NULL))return 8;
    xmemstats Before,After;xrtMemStatsEnable(Heap);xrtMemStatsGet(&Before);double CpuStart,CpuEnd;if(!rpcBenchProcessCpu(&CpuStart))return 9;
    double Start=xrtTimer();for(size_t I=0;I<Count;){size_t N=Count-I;if(N>Width)N=Width;if(!benchTlsCycle(Peer,N,Latency?Latency+I:NULL))return 10;I+=N;}double Seconds=xrtTimer()-Start;
    if(!rpcBenchProcessCpu(&CpuEnd))return 11;
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
    printf("calls_per_sec: %.6f\nns_per_call: %.6f\nprocess_cpu_ns_per_call: %.6f\ncompleted: %zu\n",Count/Seconds,Seconds*1e9/Count,(CpuEnd-CpuStart)*1000./Count,Count);
    if(Latency){qsort(Latency,Count,sizeof(double),benchTlsCompare);printf("closed_loop_p50_us: %.6f\nclosed_loop_p99_us: %.6f\n",Latency[(Count-1)/2],Latency[(Count*99-1)/100]);}
    if(Heap)printf("heap_blocks_per_call: %.6f\nheap_bytes_per_call: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count);
    printf("payload_bytes: %zu\n",BenchTlsParams.Size);
    puts("scope: native loopback TLS13 fixture-pin/Content-Length, record/ciphertext copies included, handshake excluded from steady samples; one connection, shared executor, closed-loop pipeline1 or barrier-window32; P99 only pipeline1 includes caller wait/collection and has coordinated omission; all process threads in CPU; unpinned, no fixed-arrival/connection-scale/native-Linux production acceptance");
    xrtFree(Latency);xrtFree(Payload);if(!xrpcTlsClose(Peer) || !xrpcTlsClose(Accepted) || xrpcTlsWaitFor(Peer,10000)!=XWAIT_OK || xrpcTlsWaitFor(Accepted,10000)!=XWAIT_OK || !xrpcTlsDestroy(Peer) || !xrpcTlsDestroy(Accepted))return 12;
    xrtNetListenerClose(Listener);xrtNetListenerDestroy(Listener);if(!xrpcServerDestroy(Server))return 13;End=xrtTimer()+10;xnetretireresult Q;while((Q=xrtNetEngineTryDestroy(E))==XNET_RETIRE_BUSY){if(xrtTimer()>=End)return 14;xrtSleep(1);}xrtTlsVerifierRelease(Verifier);xrtTlsIdentityRelease(Identity);xrtTlsContextRelease(TLSContext);return Q==XNET_RETIRE_READY?0:15;
}
