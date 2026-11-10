#ifndef XMQTT_CLIENT_SESSION_PROBE_H
#define XMQTT_CLIENT_SESSION_PROBE_H
#include "test.h"
/* Installed only after Core implementation and before MQTT implementation.
 * Production headers/sources contain no hooks. Enforce a real zero/one-byte
 * native PUBLISH prefix, then return ordinary AGAIN until the test releases it. */
static xatomic32 csProbeMode,csProbeSeen,csProbeAccepted;
static xatomicptr csProbeStream;
static xatomicptr csProbeFreeTarget;
static xatomic32 csProbeFreeEntered,csProbeFreeRelease;
static xatomic64 csProbeAllocBytes;
static xatomic32 csProbeAllocCountdown,csProbeAllocFailures;
static void csProbeFailAllocation(size_t Bytes,unsigned Successful)
{xrtAtomic32Store(&csProbeAllocCountdown,Successful,XMEMORY_RELAXED);xrtAtomic32Store(&csProbeAllocFailures,0,XMEMORY_RELAXED);xrtAtomic64Store(&csProbeAllocBytes,(uint64)Bytes,XMEMORY_RELEASE);}
static ptr csProbeCalloc(size_t Count,size_t Bytes)
{
    uint64 Target=xrtAtomic64Load(&csProbeAllocBytes,XMEMORY_ACQUIRE);
    if(Target && Count==1 && Bytes==Target){uint32 N=xrtAtomic32Load(&csProbeAllocCountdown,XMEMORY_RELAXED);
        if(!N){xrtAtomic64Store(&csProbeAllocBytes,0,XMEMORY_RELEASE);xrtAtomic32FetchAdd(&csProbeAllocFailures,1,XMEMORY_RELEASE);xrtSetErrorInfo(XERR_MEMORY,"xmqtt.test",1,"forced replay header allocation failure");return NULL;}
        xrtAtomic32FetchSub(&csProbeAllocCountdown,1,XMEMORY_RELAXED);}
    return xrtCalloc(Count,Bytes);
}
static void csProbeDelayFree(void* Target)
{xrtAtomic32Store(&csProbeFreeEntered,0,XMEMORY_RELAXED);xrtAtomic32Store(&csProbeFreeRelease,0,XMEMORY_RELAXED);xrtAtomicPtrStore(&csProbeFreeTarget,Target,XMEMORY_RELEASE);}
static void csProbeFree(ptr Data)
{
    if(Data && Data==xrtAtomicPtrLoad(&csProbeFreeTarget,XMEMORY_ACQUIRE)){xrtAtomic32Store(&csProbeFreeEntered,1,XMEMORY_RELEASE);double End=xrtTimer()+8;
        while(!xrtAtomic32Load(&csProbeFreeRelease,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"controlled free release");xrtThreadYield();}}
    xrtFree(Data);
}
static void csProbeReset(unsigned Mode)
{xrtAtomic32Init(&csProbeMode,Mode);xrtAtomic32Init(&csProbeSeen,0);xrtAtomic32Init(&csProbeAccepted,0);xrtAtomicPtrInit(&csProbeStream,NULL);}
static bool csProbeBlock(xnetstream* S,const void* Data,size_t Bytes)
{
    uint32 Mode=xrtAtomic32Load(&csProbeMode,XMEMORY_ACQUIRE);if(!Mode)return false;
    void* Target=xrtAtomicPtrLoad(&csProbeStream,XMEMORY_ACQUIRE);
    if(!Target && Bytes && (((const unsigned char*)Data)[0]>>4)==3){xrtAtomicPtrStore(&csProbeStream,S,XMEMORY_RELEASE);Target=S;}
    if(Target!=S)return false;
    xrtAtomic32Store(&csProbeSeen,1,XMEMORY_RELEASE);return Mode==1 || xrtAtomic32Load(&csProbeAccepted,XMEMORY_ACQUIRE)!=0;
}
static xnetresult csProbeSendRef(xnetstream* S,cbytes Data,size_t Bytes,xnetreleaseproc Release,ptr User)
{
    if(csProbeBlock(S,Data,Bytes))return XNET_RESULT_AGAIN;
    xnetresult R=xrtNetStreamSendRef(S,Data,Bytes,Release,User);
    if(R==XNET_RESULT_OK && S==xrtAtomicPtrLoad(&csProbeStream,XMEMORY_ACQUIRE)){testRequire(Bytes==1,"probe requires one-byte write work");xrtAtomic32FetchAdd(&csProbeAccepted,(uint32)Bytes,XMEMORY_RELEASE);}return R;
}
static xnetresult csProbeSendRefs(xnetstream* S,const xnetref* Refs,size_t Count)
{
    if(Count && csProbeBlock(S,Refs[0].Data,Refs[0].Size))return XNET_RESULT_AGAIN;
    xnetresult R=xrtNetStreamSendRefs(S,Refs,Count);
    if(R==XNET_RESULT_OK && S==xrtAtomicPtrLoad(&csProbeStream,XMEMORY_ACQUIRE)){size_t Bytes=0;for(size_t I=0;I<Count;++I)Bytes+=Refs[I].Size;testRequire(Bytes==1,"probe vector requires one-byte work");xrtAtomic32FetchAdd(&csProbeAccepted,(uint32)Bytes,XMEMORY_RELEASE);}return R;
}
#define xrtNetStreamSendRef csProbeSendRef
#define xrtNetStreamSendRefs csProbeSendRefs
#define xrtFree csProbeFree
#define xrtCalloc csProbeCalloc
#endif
