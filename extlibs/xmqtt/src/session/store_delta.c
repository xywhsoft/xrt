#include <xmqtt/store_delta.h>
#include <xrt/memory.h>
#include "../internal/xmqtt_store_packet.h"
#include <string.h>
#if defined(XMQTT_FEATURE_XMQTT_STORE_DELTA)
enum {XMQTT_D_HEADER=32,XMQTT_D_ENTRY=48};
static xmqttstoredeltastatus xmqttDError(xmqttstoredeltastatus S,cstr Message)
{xrtSetErrorInfo(S==XMQTT_STORE_DELTA_ERROR?XERR_ARGUMENT:XERR_PROTOCOL,"xrt.mqtt.delta",6300-(int32)S,Message);return S;}
static bool xmqttDSpan(const void* P,size_t N){return (P||!N)&&xrtMemRangeValid(P,N);}
static bool xmqttDSeparate(const void* A,size_t N,const void* B,size_t M)
{return !N||!M||!xrtMemRangesOverlap(A,N,B,M);}
static bool xmqttDZero(const unsigned char* P,size_t N)
{for(size_t I=0;I<N;++I)if(P[I])return false;return true;}
static uint64 xmqttDGet(const unsigned char* P,unsigned N)
{uint64 V=0;for(unsigned I=0;I<N;++I)V=(V<<8)|P[I];return V;}
static void xmqttDPut(unsigned char* P,uint64 V,unsigned N)
{for(unsigned I=N;I;--I){P[I-1]=(unsigned char)V;V>>=8;}}
static bool xmqttDConfig(const xmqttstoredeltaconfig* C,xmqttstoredeltaconfig* K)
{
    if(!xmqttDSpan(C,sizeof(*C))||C->Size!=sizeof(*C)||(C->Version!=XMQTT_VERSION_5&&C->Version!=XMQTT_VERSION_311)||C->MaxOperations>UINT32_MAX)return false;
    *K=*C;return xmqttInternalCodecSettings(XMQTT_CLIENT_TO_SERVER,&C->Codec,&K->Codec);
}
XMQTT_API bool xmqttStoreDeltaConfigInit(xmqttstoredeltaconfig* C)
{
    if(!xmqttDSpan(C,sizeof(*C))){xmqttDError(XMQTT_STORE_DELTA_ERROR,"invalid delta config output");return false;}
    *C=(xmqttstoredeltaconfig){0};C->Size=sizeof(*C);C->Version=XMQTT_VERSION_5;C->MaxOperations=128;C->MaxBytes=1024u*1024u;return xmqttCodecConfigInit(&C->Codec);
}
static bool xmqttDProperties(xmqttproperty* P,size_t N)
{return N<=SIZE_MAX/sizeof(*P)&&xmqttDSpan(P,N*sizeof(*P));}
static bool xmqttDRestore(xerror* Saved)
{
    xerror* Discard=NULL;
    if(!xrtTrySwapErrorTake(Saved,&Discard)){xrtErrorFree(Saved);return false;}
    xrtErrorFree(Discard);return true;
}
XMQTT_INTERNAL xmqttstoredeltastatus xmqttInternalStorePacket(const xmqttstoredeltaconfig* K,const xmqttstoredelta* D,xmqttproperty* Properties,size_t Capacity)
{
    if(D->Data.Size&&(D->Data.Data[0]>>4)!=XMQTT_PACKET_PUBLISH)return xmqttDError(XMQTT_STORE_DELTA_PROTOCOL,"only PUBLISH wire is permitted in durable payload records");
    xmqttpacket P;xmqttcodecissue Issue;xmqttcodecbuffers B={Properties,Capacity,NULL,0};xmqttdirection Direction=D->Type==XMQTT_STORE_OUT_PUT?XMQTT_CLIENT_TO_SERVER:XMQTT_SERVER_TO_CLIENT;
    /* A local capacity refusal is normal and cannot replace the ambient error. */
    xerror* Saved=NULL;if(!xrtTrySwapErrorTake(NULL,&Saved))return XMQTT_STORE_DELTA_ERROR;
    xmqttcodecstatus S=K->Version==XMQTT_VERSION_5?xmqttPacketRead5(D->Data,Direction,&K->Codec,&B,&P,&Issue):xmqttPacketRead311(D->Data,Direction,&K->Codec,&B,&P,&Issue);
    if(S==XMQTT_CODEC_LOCAL_ERROR&&(Issue.Error==XMQTT_CODEC_CAPACITY||Issue.Error==XMQTT_CODEC_LIMIT))return xmqttDRestore(Saved)?XMQTT_STORE_DELTA_LIMIT:XMQTT_STORE_DELTA_ERROR;
    if(S!=XMQTT_CODEC_DONE){xrtErrorFree(Saved);if(S==XMQTT_CODEC_MORE)return xmqttDError(XMQTT_STORE_DELTA_MALFORMED,"truncated stored PUBLISH packet");return S==XMQTT_CODEC_MALFORMED?XMQTT_STORE_DELTA_MALFORMED:XMQTT_STORE_DELTA_PROTOCOL;}
    if(!xmqttDRestore(Saved))return XMQTT_STORE_DELTA_ERROR;
    unsigned Qos=(P.Flags>>1)&3u;
    if(P.Type!=XMQTT_PACKET_PUBLISH||P.Wire.Size!=D->Data.Size||!P.Body.Publish.Topic.Size||P.Identifier!=D->Identifier||
        (D->Type==XMQTT_STORE_OUT_PUT&&(!Qos||(P.Flags&8u))))return xmqttDError(XMQTT_STORE_DELTA_PROTOCOL,"stored PUBLISH requires full topic, matching id and outgoing canonical QoS");
    for(size_t I=0;I<P.Properties.Count;++I)if(P.Properties.Data[I].Id==XMQTT_PROPERTY_TOPIC_ALIAS)return xmqttDError(XMQTT_STORE_DELTA_PROTOCOL,"connection-only Topic Alias cannot enter durable state");
    return XMQTT_STORE_DELTA_DONE;
}
static xmqttstoredeltastatus xmqttDValidate(const xmqttstoredeltaconfig* K,const xmqttstoredelta* D,xmqttproperty* P,size_t N)
{
    if(!D->Epoch||D->WallMilliseconds>INT64_MAX)return xmqttDError(XMQTT_STORE_DELTA_PROTOCOL,"invalid delta epoch or wall clock");
    bool Flow=D->Type>=XMQTT_STORE_OUT_PUT&&D->Type<=XMQTT_STORE_INBOX_ACK;
    if((Flow&&!D->Key)||(!Flow&&D->Key))return xmqttDError(XMQTT_STORE_DELTA_PROTOCOL,"invalid delta flow/delivery key");
    if(Flow&&(D->Expiry||D->WallMilliseconds))return xmqttDError(XMQTT_STORE_DELTA_PROTOCOL,"unexpected flow clock/expiry metadata");
    if(D->Type!=XMQTT_STORE_BIND&&D->Type!=XMQTT_STORE_OUT_PUT&&D->Type!=XMQTT_STORE_IN_ACCEPT&&D->Data.Size)return xmqttDError(XMQTT_STORE_DELTA_PROTOCOL,"unexpected delta body");
    switch(D->Type){
    case XMQTT_STORE_BIND:
        if(D->Identifier||D->Flags||D->Reason||D->Expiry||!D->Data.Size||D->Data.Size>65535||
            !xmqttUtf8StringCheck((xstrview){(const char*)D->Data.Data,D->Data.Size},K->Codec.Utf8Flags,NULL))break;
        return XMQTT_STORE_DELTA_DONE;
    case XMQTT_STORE_OPEN:
        if(D->Identifier||D->Reason||(D->Flags&~3u)||D->Flags==3u||
            (K->Version==XMQTT_VERSION_311&&D->Expiry!=((D->Flags&1u)?0:UINT32_MAX)))break;
        return XMQTT_STORE_DELTA_DONE;
    case XMQTT_STORE_END:
        if(D->Identifier||(D->Flags&~1u)||(!D->Flags&&D->Reason)||
            (D->Flags&&!xmqttInternalReason(K->Version,XMQTT_PACKET_DISCONNECT,D->Reason,XMQTT_CLIENT_TO_SERVER))||
            (K->Version==XMQTT_VERSION_311&&D->Expiry&&D->Expiry!=UINT32_MAX))break;
        return XMQTT_STORE_DELTA_DONE;
    case XMQTT_STORE_CLEAR:
        if(D->Identifier||D->Flags||D->Reason>2||D->Expiry)break;
        return XMQTT_STORE_DELTA_DONE;
    case XMQTT_STORE_OUT_PUT:
        if(!D->Identifier||D->Reason||(D->Flags&~3u)||D->Flags==2u)break;
        return xmqttInternalStorePacket(K,D,P,N);
    case XMQTT_STORE_IN_ACCEPT:
        if(D->Flags||D->Reason)break;
        return xmqttInternalStorePacket(K,D,P,N);
    case XMQTT_STORE_OUT_ATTEMPT:case XMQTT_STORE_IN_PUBREL:case XMQTT_STORE_IN_COMPLETE:
        if(!D->Identifier||D->Flags||D->Reason)break;
        return XMQTT_STORE_DELTA_DONE;
    case XMQTT_STORE_OUT_PUBREL:
        if(!D->Identifier||D->Flags||(D->Reason&&!(K->Version==XMQTT_VERSION_5&&D->Reason==0x10)))break;
        return XMQTT_STORE_DELTA_DONE;
    case XMQTT_STORE_OUT_COMPLETE:
        if(!D->Identifier||(D->Flags!=XMQTT_PACKET_PUBACK&&D->Flags!=XMQTT_PACKET_PUBREC&&D->Flags!=XMQTT_PACKET_PUBCOMP)||
            (D->Flags==XMQTT_PACKET_PUBREC&&D->Reason<128)||!xmqttInternalReason(K->Version,(xmqttpackettype)D->Flags,D->Reason,XMQTT_SERVER_TO_CLIENT))break;
        return XMQTT_STORE_DELTA_DONE;
    case XMQTT_STORE_INBOX_ACK:
        if(D->Identifier||D->Flags||D->Reason)break;
        return XMQTT_STORE_DELTA_DONE;
    default:return xmqttDError(XMQTT_STORE_DELTA_MALFORMED,"unknown delta operation");
    }
    return xmqttDError(XMQTT_STORE_DELTA_PROTOCOL,"invalid delta fields or version-specific semantics");
}
static bool xmqttDWriteArgs(const xmqttstoredeltaconfig* C,const xmqttstoredeltaconfig* K,const xmqttstoredelta* D,size_t Count,
    xmqttproperty* P,size_t PC,void* Output,size_t Capacity,size_t* Written,bool Write)
{
    if(Count>SIZE_MAX/sizeof(*D)||!xmqttDSpan(D,Count*sizeof(*D))||!xmqttDProperties(P,PC)||!xmqttDSpan(Written,sizeof(*Written))||(Write&&!xmqttDSpan(Output,Capacity)))return false;
    const void* Outs[]={P,Written,Write?Output:NULL};size_t Sizes[]={PC*sizeof(*P),sizeof(*Written),Write?Capacity:0};
    for(size_t I=0;I<3;++I){if(!xmqttDSeparate(Outs[I],Sizes[I],C,sizeof(*C))||!xmqttDSeparate(Outs[I],Sizes[I],D,Count*sizeof(*D)))return false;
        for(size_t J=0;J<I;++J)if(!xmqttDSeparate(Outs[I],Sizes[I],Outs[J],Sizes[J]))return false;}
    if(Count>K->MaxOperations)return true; /* Refuse local count quota before deep input traversal. */
    for(size_t I=0;I<Count;++I){if(!xmqttDSpan(D[I].Data.Data,D[I].Data.Size))return false;
        for(size_t J=0;J<3;++J)if(!xmqttDSeparate(Outs[J],Sizes[J],D[I].Data.Data,D[I].Data.Size))return false;}
    return true;
}
static xmqttstoredeltastatus xmqttDWrite(const xmqttstoredeltaconfig* C,const xmqttstoredelta* D,size_t Count,
    xmqttproperty* P,size_t PC,void* Output,size_t Capacity,size_t* Written,bool Write)
{
    xmqttstoredeltaconfig K;if(!xmqttDConfig(C,&K)||!xmqttDWriteArgs(C,&K,D,Count,P,PC,Output,Capacity,Written,Write))return xmqttDError(XMQTT_STORE_DELTA_ERROR,"invalid delta input/output/scratch overlap");
    if(Count>K.MaxOperations||K.MaxBytes<XMQTT_D_HEADER||Count>(K.MaxBytes-XMQTT_D_HEADER)/XMQTT_D_ENTRY)return XMQTT_STORE_DELTA_LIMIT;
    size_t Bytes=XMQTT_D_HEADER;
    for(size_t I=0;I<Count;++I){if(D[I].Data.Size>K.MaxBytes-Bytes-XMQTT_D_ENTRY)return XMQTT_STORE_DELTA_LIMIT;Bytes+=XMQTT_D_ENTRY+D[I].Data.Size;
        /* Remaining records have a fixed minimum; avoid subtracting below0. */
        if(Count-I-1>(K.MaxBytes-Bytes)/XMQTT_D_ENTRY)return XMQTT_STORE_DELTA_LIMIT;
        xmqttstoredeltastatus S=xmqttDValidate(&K,D+I,P,PC);if(S!=XMQTT_STORE_DELTA_DONE)return S;}
    if(!Write){*Written=Bytes;return XMQTT_STORE_DELTA_DONE;}if(Capacity<Bytes)return XMQTT_STORE_DELTA_LIMIT;
    unsigned char* B=Output;memset(B,0,XMQTT_D_HEADER);memcpy(B,"XMQDSET",7);xmqttDPut(B+8,1,2);B[10]=(unsigned char)K.Version;xmqttDPut(B+12,Count,4);xmqttDPut(B+16,Bytes,8);
    size_t At=XMQTT_D_HEADER;for(size_t I=0;I<Count;++I){const xmqttstoredelta* O=D+I;unsigned char* H=B+At;memset(H,0,XMQTT_D_ENTRY);H[0]=(unsigned char)O->Type;H[1]=O->Flags;H[2]=O->Reason;xmqttDPut(H+4,O->Identifier,2);xmqttDPut(H+6,XMQTT_D_ENTRY,2);xmqttDPut(H+8,O->Expiry,4);xmqttDPut(H+16,O->Epoch,8);xmqttDPut(H+24,O->Key,8);xmqttDPut(H+32,O->WallMilliseconds,8);xmqttDPut(H+40,O->Data.Size,8);At+=XMQTT_D_ENTRY;if(O->Data.Size)memcpy(B+At,O->Data.Data,O->Data.Size);At+=O->Data.Size;}
    *Written=Bytes;return XMQTT_STORE_DELTA_DONE;
}
XMQTT_API xmqttstoredeltastatus xmqttStoreDeltaSize(const xmqttstoredeltaconfig* C,const xmqttstoredelta* D,size_t N,xmqttproperty* P,size_t PC,size_t* Required)
{return xmqttDWrite(C,D,N,P,PC,NULL,0,Required,false);}
XMQTT_API xmqttstoredeltastatus xmqttStoreDeltaWrite(const xmqttstoredeltaconfig* C,const xmqttstoredelta* D,size_t N,xmqttproperty* P,size_t PC,ptr Output,size_t Capacity,size_t* Written)
{return xmqttDWrite(C,D,N,P,PC,Output,Capacity,Written,true);}
XMQTT_API xmqttstoredeltastatus xmqttStoreDeltaRead(const xmqttstoredeltaconfig* C,xbytesview Input,xmqttstoredelta* D,size_t Capacity,
    xmqttproperty* P,size_t PC,xmqttstoredeltadocument* Doc)
{
    xmqttstoredeltaconfig K;if(!xmqttDConfig(C,&K)||!xmqttDSpan(Input.Data,Input.Size)||Capacity>SIZE_MAX/sizeof(*D)||!xmqttDSpan(D,Capacity*sizeof(*D))||!xmqttDProperties(P,PC)||!xmqttDSpan(Doc,sizeof(*Doc)))return xmqttDError(XMQTT_STORE_DELTA_ERROR,"invalid delta read spans/config");
    const void* Outs[]={D,P,Doc};size_t Sizes[]={Capacity*sizeof(*D),PC*sizeof(*P),sizeof(*Doc)};
    for(size_t I=0;I<3;++I){if(!xmqttDSeparate(Outs[I],Sizes[I],Input.Data,Input.Size)||!xmqttDSeparate(Outs[I],Sizes[I],C,sizeof(*C)))return xmqttDError(XMQTT_STORE_DELTA_ERROR,"delta read output overlaps input/config");
        for(size_t J=0;J<I;++J)if(!xmqttDSeparate(Outs[I],Sizes[I],Outs[J],Sizes[J]))return xmqttDError(XMQTT_STORE_DELTA_ERROR,"delta read scratch/output overlap");}
    if(Input.Size>K.MaxBytes)return XMQTT_STORE_DELTA_LIMIT;
    const unsigned char* B=Input.Data;
    if(Input.Size<XMQTT_D_HEADER||memcmp(B,"XMQDSET\0",8)||xmqttDGet(B+8,2)!=1||B[10]!=K.Version||B[11]||!xmqttDZero(B+24,8)||xmqttDGet(B+16,8)!=Input.Size)return xmqttDError(XMQTT_STORE_DELTA_MALFORMED,"invalid delta version/header/length");
    size_t Count=(size_t)xmqttDGet(B+12,4);if(Count>(Input.Size-XMQTT_D_HEADER)/XMQTT_D_ENTRY)return xmqttDError(XMQTT_STORE_DELTA_MALFORMED,"impossible delta record count");
    if(Count>K.MaxOperations||Count>Capacity)return XMQTT_STORE_DELTA_LIMIT;
    size_t At=XMQTT_D_HEADER;
    for(size_t I=0;I<Count;++I){if(Input.Size-At<XMQTT_D_ENTRY)return xmqttDError(XMQTT_STORE_DELTA_MALFORMED,"truncated delta record");const unsigned char* H=B+At;
        uint64 N=xmqttDGet(H+40,8);if(H[3]||xmqttDGet(H+6,2)!=XMQTT_D_ENTRY||!xmqttDZero(H+12,4)||N>Input.Size-At-XMQTT_D_ENTRY)return xmqttDError(XMQTT_STORE_DELTA_MALFORMED,"invalid delta record header/body bounds");
        xmqttstoredelta O={0};O.Type=(xmqttstoredeltatype)H[0];O.Flags=H[1];O.Reason=H[2];O.Identifier=(uint16)xmqttDGet(H+4,2);O.Expiry=(uint32)xmqttDGet(H+8,4);O.Epoch=xmqttDGet(H+16,8);O.Key=xmqttDGet(H+24,8);O.WallMilliseconds=xmqttDGet(H+32,8);O.Data=(xbytesview){H+XMQTT_D_ENTRY,(size_t)N};
        xmqttstoredeltastatus S=xmqttDValidate(&K,&O,P,PC);if(S!=XMQTT_STORE_DELTA_DONE)return S;D[I]=O;At+=XMQTT_D_ENTRY+(size_t)N;
    }
    if(At!=Input.Size)return xmqttDError(XMQTT_STORE_DELTA_MALFORMED,"trailing bytes beyond delta record count");
    *Doc=(xmqttstoredeltadocument){D,Count,Input};return XMQTT_STORE_DELTA_DONE;
}
#endif
