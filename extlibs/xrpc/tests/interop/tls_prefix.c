/* Test-only production-TU/native-API interposition. No production hooks. */
#include <xrpc/tls.h>
#include <xrt/atomic.h>
static xtlsresult tlsProbeSend(xtlsstream*,const void*,size_t,size_t*);
static xtlsresult tlsProbeVec(xtlsstream*,const xnetspan*,size_t,size_t*);
#define xrtTlsStreamSend tlsProbeSend
#define xrtTlsStreamSendVec tlsProbeVec
#ifndef XRPC_TLS_PROBE_SOURCE
#define XRPC_TLS_PROBE_SOURCE "../../src/tls/tls.c"
#endif
#include XRPC_TLS_PROBE_SOURCE
#undef xrtTlsStreamSend
#undef xrtTlsStreamSendVec
#include "../../../../src/internal/xrt_tls_stream.h"
#define main tlsProbeBaseMain
#include "../test_tls.c"
#undef main
static xatomicptr TlsProbeStream;static xrpctls* TlsProbePeer;
static size_t TlsProbeMax,TlsProbeCalls,TlsProbeAccepted;static unsigned TlsProbeMode;static bool TlsProbeFired;
static xnetpost TlsProbeResume;static xatomic32 TlsProbePrepared,TlsProbeResumed;
static void tlsProbeResume(xnetworker* W,ptr Data)
{
    (void)W;(void)Data;testRequire(TlsProbePeer->HeaderOffset==0 && TlsProbePeer->BodyOffset==0 && TlsProbePeer->Offered,"TLS refused offer advanced offsets or lost uncertainty");
    xrtAtomic32Store(&TlsProbeResumed,1,XMEMORY_RELEASE);xrpcTlsPeerWake(TlsProbePeer);
}
static xtlsresult tlsProbeVec(xtlsstream* S,const xnetspan* V,size_t Count,size_t* Written)
{
    if(S!=xrtAtomicPtrLoad(&TlsProbeStream,XMEMORY_ACQUIRE))return xrtTlsStreamSendVec(S,V,Count,Written);
    ++TlsProbeCalls;
    if(TlsProbeMode==2 && !TlsProbeFired){TlsProbeFired=true;*Written=0;testRequire(xrtNetPost(TlsProbePeer->Worker,&TlsProbeResume,tlsProbeResume,NULL),"TLS refusal progress signal");return XTLS_AGAIN;}
    if(TlsProbeMode==4 && !TlsProbeFired){TlsProbeFired=true;*Written=0;return XTLS_ERROR;}
    size_t Left=TlsProbeMax; xnetspan Pieces[2];size_t Used=0;
    for(size_t I=0;I<Count && Left;++I){size_t N=V[I].Size;if(N>Left)N=Left;if(N){Pieces[Used++]=(xnetspan){V[I].Data,N};Left-=N;}}
    xtlsresult R=xrtTlsStreamSendVec(S,Pieces,Used,Written);TlsProbeAccepted+=*Written;
    if(TlsProbeMode==3 && *Written){xrtSetErrorInfo(XERR_MEMORY,"probe.tls",1,"synthetic native prefix allocation refusal");return XTLS_ERROR;}
    if(TlsProbeMode==1 && R==XTLS_OK){xrpcTlsPeerWake(TlsProbePeer);return XTLS_AGAIN;}
    return R;
}
static xtlsresult tlsProbeSend(xtlsstream* S,const void* Data,size_t Size,size_t* Written)
{xnetspan V={Data,Size};return tlsProbeVec(S,&V,1,Written);}
static void tlsProbePrepare(xnetworker* W,ptr Data)
{
    (void)W;xrpctls* P=Data;TlsProbePeer=P;testRequire(P->Stream && P->Open,"TLS probe requires real OPEN native stream");
    /* Suppress incidental application write notifications so OK short-prefix
     * progress must come from the adapter. Native TLS/TCP IO still runs. */
    P->Stream->Events.Writable=NULL;P->Stream->Events.Drain=NULL;
    if(TlsProbeMode==4)xrtSetErrorInfo(XERR_MEMORY,"unrelated.task",99,"must not become this send failure");
    xrtAtomicPtrStore(&TlsProbeStream,P->Stream,XMEMORY_RELEASE);xrtAtomic32Store(&TlsProbePrepared,1,XMEMORY_RELEASE);
}
static void tlsProbeCase(unsigned Format,size_t Chunk,unsigned Mode)
{
    tlsTest T;tlsTestStart(&T,Format,false);TlsProbeMax=Chunk;TlsProbeCalls=TlsProbeAccepted=0;TlsProbeMode=Mode;TlsProbeFired=false;
    xrtAtomicPtrStore(&TlsProbeStream,NULL,XMEMORY_RELEASE);xrtAtomic32Store(&TlsProbePrepared,0,XMEMORY_RELEASE);xrtAtomic32Store(&TlsProbeResumed,0,XMEMORY_RELEASE);testRequire(xrtNetPostInit(&TlsProbeResume),"TLS probe resume post init");
    xrpctlsinfo Info={0};Info.Size=sizeof(Info);double End=xrtTimer()+5;do{testRequire(xrpcTlsInfo(T.Left,&Info) && xrtTimer()<End,"TLS probe client OPEN");xrtSleep(1);}while(Info.State!=XRPC_TLS_OPEN);
    xnetpost Prepare;testRequire(xrtNetPostInit(&Prepare) && xrtNetPost(xrtNetEngineWorker(T.Engine,1),&Prepare,tlsProbePrepare,T.Left),"TLS probe prepare post");
    while(!xrtAtomic32Load(&TlsProbePrepared,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"TLS probe prepare timeout");xrtSleep(1);}
    xfuture* F=NULL;testRequire(xrpcTlsCall(T.Left,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[1,2,3,4,5,6,7,8]"),1000,&F)==XRPC_DUPLEX_ACCEPTED,"TLS probe call");
    if(Mode<3){tlsTestResult(F,XRPC_CALL_OK,XRPC_REPLIED);testRequire(TlsProbeCalls>1 && TlsProbeAccepted>Chunk,"TLS probe did not exercise native prefixes");if(Mode==2)testRequire(xrtAtomic32Load(&TlsProbeResumed,XMEMORY_ACQUIRE)==1,"TLS refusal was not observed");}
    else tlsTestResult(F,Mode==3?XRPC_CALL_MEMORY:XRPC_CALL_TRANSPORT,XRPC_MAY_HAVE_EXECUTED);
    xrtAtomicPtrStore(&TlsProbeStream,NULL,XMEMORY_RELEASE);tlsTestFinish(&T);xrtFutureDestroy(F);
}
int main(int Argc,char** Argv)
{
    (void)Argv;xrtAtomicPtrInit(&TlsProbeStream,NULL);xrtAtomic32Init(&TlsProbePrepared,0);xrtAtomic32Init(&TlsProbeResumed,0);
    for(unsigned Format=1;Format<=2;++Format){tlsProbeCase(Format,1,0);if(Argc>1)continue;for(unsigned Mode=0;Mode<5;++Mode)tlsProbeCase(Format,7,Mode);}
    puts("[PASS] TLS native prefix probe: OK short prefix self-progress/positive AGAIN/zero refusal offsets/partial memory/stale ambient/actual encrypted framing");return 0;
}
