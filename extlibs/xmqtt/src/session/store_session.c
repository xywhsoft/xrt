#include <xmqtt/store_session.h>
#include <xrt/memory.h>
#include "../internal/xmqtt_internal.h"
#include "../internal/xmqtt_session_restore.h"
#include "../internal/xmqtt_store_session_source.h"
#include <math.h>
#include <string.h>
#if defined(XMQTT_FEATURE_XMQTT_STORE_SESSION)
static xmqttsessionstatus xmqttSIError(xmqttsessionstatus Status,cstr Message)
{
    xrtSetErrorInfo(Status==XMQTT_SESSION_STATE_ERROR?XERR_STATE:Status==XMQTT_SESSION_PROTOCOL_ERROR?XERR_PROTOCOL:XERR_ARGUMENT,
        "xrt.mqtt.store.session",6500-(int32)Status,Message);return Status;
}
static bool xmqttSISpan(const void* P,size_t N){return (P||!N)&&xrtMemRangeValid(P,N);}
static bool xmqttSISeparate(const void* A,size_t N,const void* B,size_t M)
{return !N||!M||!xrtMemRangesOverlap(A,N,B,M);}
XMQTT_API bool xmqttStoreSessionConfigInit(xmqttstoresessionconfig* C)
{
    if(!xmqttSISpan(C,sizeof(*C))){xmqttSIError(XMQTT_SESSION_ERROR,"invalid store Session config output");return false;}
    *C=(xmqttstoresessionconfig){0};C->Size=sizeof(*C);C->MaxScratchBytes=16384;return true;
}
static xmqttsessionstatus xmqttSIRead(const xmqttsessionconfig* K,xbytesview Wire,xmqttproperty* Properties,size_t Capacity,xmqttpacket* Packet)
{
    xmqttcodecbuffers B={Properties,Capacity,NULL,0};xmqttcodecissue Issue={0};
    xmqttcodecstatus S=K->Version==XMQTT_VERSION_5?xmqttPacketRead5(Wire,XMQTT_CLIENT_TO_SERVER,&K->Codec,&B,Packet,&Issue):xmqttPacketRead311(Wire,XMQTT_CLIENT_TO_SERVER,&K->Codec,&B,Packet,&Issue);
    if(S==XMQTT_CODEC_DONE)return XMQTT_SESSION_DONE;
    if(S==XMQTT_CODEC_LOCAL_ERROR&&(Issue.Error==XMQTT_CODEC_LIMIT||Issue.Error==XMQTT_CODEC_CAPACITY))return XMQTT_SESSION_LIMIT;
    return xmqttSIError(XMQTT_SESSION_PROTOCOL_ERROR,"invalid canonical recovered outgoing packet");
}
static xmqttsessionstatus xmqttSIRestore(const xmqttstorestate* State,const xmqttsessionconfig* SessionConfig,
    const xmqttstoresessionconfig* Config,xmqttsession** Out,bool Assigned)
{
    if(!xmqttSISpan(Config,sizeof(*Config))||!xmqttSISpan(SessionConfig,sizeof(*SessionConfig))||!xmqttSISpan(Out,sizeof(*Out)))return xmqttSIError(XMQTT_SESSION_ERROR,"invalid store Session input/output spans");
    xmqttstoresessionconfig C=*Config;xmqttsessionconfig K=*SessionConfig;
    if(C.Size!=sizeof(C)||!C.Binding.Size||C.Binding.Size>1024||!xmqttSISpan(C.Binding.Data,C.Binding.Size)||!C.Generation||
       !isfinite(C.MonotonicTime)||C.MonotonicTime<0||C.WallMilliseconds>INT64_MAX||
       K.Size!=sizeof(K)||(K.Version!=XMQTT_VERSION_5&&K.Version!=XMQTT_VERSION_311)||K.ClientId.Size>65535||!xmqttSISpan(K.ClientId.Data,K.ClientId.Size)||
       (Assigned?(K.Version!=XMQTT_VERSION_5||K.ClientId.Size):!K.ClientId.Size)||K.MaxOutgoing>65535||K.MaxIncoming>65535||
       !xmqttSISeparate(Out,sizeof(*Out),Config,sizeof(*Config))||!xmqttSISeparate(Out,sizeof(*Out),SessionConfig,sizeof(*SessionConfig))||
       !xmqttSISeparate(Out,sizeof(*Out),C.Binding.Data,C.Binding.Size)||!xmqttSISeparate(Out,sizeof(*Out),K.ClientId.Data,K.ClientId.Size))return xmqttSIError(XMQTT_SESSION_ERROR,"invalid store Session identity/config or overlapping output");
    xmqttcodecconfig Codec;if(!xmqttInternalCodecSettings(XMQTT_CLIENT_TO_SERVER,&K.Codec,&Codec))return XMQTT_SESSION_ERROR;K.Codec=Codec;
    uint64 RecoveryWall=0;xmqttstorestatus Checked=xmqttInternalStoreSessionSource(State,C.Binding,C.Generation,Out,sizeof(*Out),&RecoveryWall);
    if(Checked!=XMQTT_STORE_DONE)return Checked==XMQTT_STORE_STATE_ERROR?XMQTT_SESSION_STATE_ERROR:XMQTT_SESSION_ERROR;
    xmqttstorestateinfo I={0};if(!xmqttStoreStateInfo(State,&I))return XMQTT_SESSION_ERROR;
    if(I.Version!=K.Version||!I.ClientId.Size||C.WallMilliseconds<RecoveryWall||(!Assigned&&(I.ClientId.Size!=K.ClientId.Size||memcmp(I.ClientId.Data,K.ClientId.Data,I.ClientId.Size))))
        return xmqttSIError(XMQTT_SESSION_STATE_ERROR,"recovered version/identity or trusted wall clock differs");
    if(Assigned)K.ClientId=I.ClientId;
    if(I.Outgoing>K.MaxOutgoing||I.Incoming>K.MaxIncoming||xmqttSessionStorageSize(&K,0,NULL)>K.MaxBytes)return XMQTT_SESSION_LIMIT;
    size_t Capacity=K.Version==XMQTT_VERSION_5&&I.Outgoing?K.Codec.MaxProperties:0;
    if(Capacity>C.MaxScratchBytes/sizeof(xmqttproperty))Capacity=C.MaxScratchBytes/sizeof(xmqttproperty);
    size_t Largest=0;for(size_t N=0;N<I.Outgoing;++N){xmqttstoreview V={0};if(xmqttStoreStateView(State,XMQTT_STORE_OUTGOING,N,&V)!=XMQTT_STORE_DONE)return XMQTT_SESSION_ERROR;if(V.Wire.Size>Largest)Largest=V.Wire.Size;}
    if(Capacity>Largest/2u)Capacity=Largest/2u;
    xerror* Saved=NULL;if(!xrtTrySwapErrorTake(NULL,&Saved))return XMQTT_SESSION_ERROR;
    xmqttsessionstatus Result=XMQTT_SESSION_ERROR;xmqttsession* S=NULL;xmqttproperty* Properties=NULL;
    if(Capacity){Properties=xrtMalloc(Capacity*sizeof(*Properties));if(!Properties)goto Finish;}
    if(!xmqttSessionCreate(&K,&S))goto Finish;
    for(size_t N=0;N<I.Outgoing;++N){
        xmqttstoreview V={0};if(xmqttStoreStateView(State,XMQTT_STORE_OUTGOING,N,&V)!=XMQTT_STORE_DONE){Result=XMQTT_SESSION_ERROR;goto Finish;}
        xmqttpacket P={0};xmqttsessionstatus Step=xmqttSIRead(&K,V.Wire,Properties,Capacity,&P);if(Step!=XMQTT_SESSION_DONE){Result=Step;goto Finish;}
        if(P.Identifier!=V.Identifier||((P.Flags>>1)&3u)!=V.Qos){Result=xmqttSIError(XMQTT_SESSION_PROTOCOL_ERROR,"recovered outgoing metadata differs from packet");goto Finish;}
        uint8 Flags=(uint8)((V.Attempted?1u:0u)|(V.Sent?2u:0u));Step=xmqttInternalSessionRestorePacket(S,V.Wire,P.Properties.Count,V.Pubrel?1:0,Flags,V.PubrecReason,&P);
        if(Step==XMQTT_SESSION_PROTOCOL_ERROR)xmqttSIError(Step,"invalid recovered outgoing stage/properties");
        if(Step!=XMQTT_SESSION_DONE){Result=Step;goto Finish;}
    }
    for(size_t N=0;N<I.Incoming;++N){xmqttstoreview V={0};if(xmqttStoreStateView(State,XMQTT_STORE_INCOMING,N,&V)!=XMQTT_STORE_DONE){Result=XMQTT_SESSION_ERROR;goto Finish;}
        unsigned Shift=(V.Identifier&3u)*2u;S->Receive[V.Identifier>>2]|=(unsigned char)((V.Pubrel?3u:2u)<<Shift);++S->RxCount;}
    bool Keep=I.HasSession,Infinite=I.RemainingMilliseconds==UINT64_MAX;
    uint64 Remaining=I.RemainingMilliseconds,Elapsed=C.WallMilliseconds-RecoveryWall;bool Expired=Keep&&!Infinite&&Elapsed>=Remaining;
    if(Keep&&!Infinite)Remaining=Expired?0:Remaining-Elapsed;
    xmqttInternalSessionRestoreExpiry(S,C.MonotonicTime,Keep,Infinite,Remaining);
    if(Expired){(void)xmqttSessionReset(S);Result=XMQTT_SESSION_RESET;}else Result=XMQTT_SESSION_DONE;
Finish:
    xrtFree(Properties);
    if(Result==XMQTT_SESSION_DONE||Result==XMQTT_SESSION_RESET||Result==XMQTT_SESSION_LIMIT){xerror* Previous=NULL;
        if(!xrtTrySwapErrorTake(Saved,&Previous)){xrtErrorFree(Saved);Result=XMQTT_SESSION_ERROR;}xrtErrorFree(Previous);
    }else xrtErrorFree(Saved);
    if(Result==XMQTT_SESSION_DONE||Result==XMQTT_SESSION_RESET)*Out=S;else if(S)(void)xmqttSessionDestroy(S);
    return Result;
}
XMQTT_API xmqttsessionstatus xmqttStoreSessionRestore(const xmqttstorestate* State,const xmqttsessionconfig* K,const xmqttstoresessionconfig* C,xmqttsession** S)
{return xmqttSIRestore(State,K,C,S,false);}
XMQTT_API xmqttsessionstatus xmqttStoreSessionRestoreAssigned(const xmqttstorestate* State,const xmqttsessionconfig* K,const xmqttstoresessionconfig* C,xmqttsession** S)
{return xmqttSIRestore(State,K,C,S,true);}
#endif
