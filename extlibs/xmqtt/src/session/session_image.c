#include <xmqtt/session_image.h>
#include <xrt/crc32.h>
#include <xrt/memory.h>
#include "../internal/xmqtt_session_state.h"
#include "../internal/xmqtt_session_restore.h"
#include "../internal/xmqtt_internal.h"
#include <math.h>
#include <string.h>
#if defined(XMQTT_FEATURE_XMQTT_SESSION_IMAGE)
enum {XMQTT_IMAGE_HEADER=64,XMQTT_IMAGE_TRAILER=4,XMQTT_IMAGE_PACKET=12,
      XMQTT_IMAGE_STATE=1,XMQTT_IMAGE_LIVE=2,XMQTT_IMAGE_INFINITE=4};
static xmqttsessionstatus xmqttImageError(xmqttsessionstatus Status,cstr Message)
{
    xerrkind Kind=Status==XMQTT_SESSION_STATE_ERROR?XERR_STATE:Status==XMQTT_SESSION_PROTOCOL_ERROR?XERR_PROTOCOL:XERR_ARGUMENT;
    xrtSetErrorInfo(Kind,"xrt.mqtt.image",6101-(int32)Status,Message);return Status;
}
static bool xmqttImageSpan(const void* P,size_t N)
{return (P||!N)&&xrtMemRangeValid(P,N);}
static bool xmqttImageSeparate(const void* A,size_t N,const void* B,size_t M)
{return !N||!M||!xrtMemRangesOverlap(A,N,B,M);}
static uint64 xmqttImageGet(const unsigned char* P,unsigned N)
{uint64 V=0;for(unsigned I=0;I<N;++I)V=(V<<8)|P[I];return V;}
static void xmqttImagePut(unsigned char* P,uint64 V,unsigned N)
{for(unsigned I=N;I;--I){P[I-1]=(unsigned char)V;V>>=8;}}
static bool xmqttImageConfig(const xmqttsessionimageconfig* Config,xmqttsessionimageconfig* C)
{
    if(!xmqttImageSpan(Config,sizeof(*Config))){return false;}*C=*Config;
    return C->Size==sizeof(*C)&&C->Binding.Size&&C->Binding.Size<=XMQTT_SESSION_IMAGE_BINDING_MAX&&
        xmqttImageSpan(C->Binding.Data,C->Binding.Size)&&isfinite(C->MonotonicTime)&&C->MonotonicTime>=0&&C->WallMilliseconds<=INT64_MAX;
}
static bool xmqttImageOutput(const void* Out,size_t N,const xmqttsessionimageconfig* Config,const xmqttsessionimageconfig* C)
{return xmqttImageSpan(Out,N)&&xmqttImageSeparate(Out,N,Config,sizeof(*Config))&&xmqttImageSeparate(Out,N,C->Binding.Data,C->Binding.Size);}
XMQTT_API bool xmqttSessionImageConfigInit(xmqttsessionimageconfig* C)
{
    if(!xmqttImageSpan(C,sizeof(*C))){xmqttImageError(XMQTT_SESSION_ERROR,"invalid image config output");return false;}
    *C=(xmqttsessionimageconfig){0};C->Size=sizeof(*C);C->MaxBytes=16u*1024u*1024u;return true;
}
typedef struct xmqttimagecut {size_t Bytes;uint64 Remaining;uint32 Outgoing,Incoming;unsigned Flags;} xmqttimagecut;
/* Gate is held. No snapshot is taken through a live Client's exclusive claim. */
static xmqttsessionstatus xmqttImageMeasure(xmqttsession* S,const xmqttsessionimageconfig* C,
    const void* Buffer,size_t Capacity,const void* Output,xmqttimagecut* Cut)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    if(S->Driver)return xmqttImageError(XMQTT_SESSION_STATE_ERROR,"image requires the sole passive protocol owner");
#endif
    if(C->MonotonicTime<S->LastNow||!S->Config.ClientId.Size)return xmqttImageError(XMQTT_SESSION_STATE_ERROR,"image requires bound identity and non-reversed time");
    if(!xmqttImageSeparate(Output,sizeof(size_t),S,S->BaseBytes)||!xmqttImageSeparate(Buffer,Capacity,S,S->BaseBytes))return xmqttImageError(XMQTT_SESSION_ERROR,"image output overlaps Session owner");
    *Cut=(xmqttimagecut){0};Cut->Bytes=XMQTT_IMAGE_HEADER+XMQTT_IMAGE_TRAILER+C->Binding.Size+S->Config.ClientId.Size;
    bool Keep=S->HasState;
    if(S->Connected){Keep=Keep&&(S->Config.Version==XMQTT_VERSION_5?S->Peer.SessionExpiry!=0:S->Persistent);
        if(Keep){Cut->Flags=XMQTT_IMAGE_STATE|XMQTT_IMAGE_LIVE;
            bool Infinite=S->Config.Version==XMQTT_VERSION_311||S->Peer.SessionExpiry==UINT32_MAX;
            Cut->Remaining=Infinite?UINT64_MAX:(uint64)S->Peer.SessionExpiry*1000u;if(Infinite)Cut->Flags|=XMQTT_IMAGE_INFINITE;}}
    else if(Keep){if(S->InfiniteExpiry){Cut->Flags=XMQTT_IMAGE_STATE|XMQTT_IMAGE_INFINITE;Cut->Remaining=UINT64_MAX;}
        else {double Remaining=S->Expiry-(C->MonotonicTime-S->ClosedAt);Keep=Remaining>=0.001;
            if(Keep){Cut->Remaining=(uint64)floor(Remaining*1000.0);if((double)Cut->Remaining/1000.0>Remaining)--Cut->Remaining;Keep=Cut->Remaining!=0;if(Keep)Cut->Flags=XMQTT_IMAGE_STATE;}}}
    if(Keep){Cut->Outgoing=(uint32)S->Active;Cut->Incoming=(uint32)(S->RxCount-S->Prepared);}
    for(xmqttsessionpacket* P=S->Head;P;P=P->Next){
        if(!xmqttImageSeparate(Output,sizeof(size_t),P,P->Allocation)||!xmqttImageSeparate(Buffer,Capacity,P,P->Allocation))return xmqttImageError(XMQTT_SESSION_ERROR,"image output overlaps active packet");
        if(Keep){if(Cut->Bytes>SIZE_MAX-XMQTT_IMAGE_PACKET||P->Bytes>SIZE_MAX-Cut->Bytes-XMQTT_IMAGE_PACKET)return XMQTT_SESSION_LIMIT;Cut->Bytes+=XMQTT_IMAGE_PACKET+P->Bytes;}}
    if(Cut->Incoming>(SIZE_MAX-Cut->Bytes)/3u){return XMQTT_SESSION_LIMIT;}Cut->Bytes+=(size_t)Cut->Incoming*3u;
    return Cut->Bytes>C->MaxBytes?XMQTT_SESSION_LIMIT:XMQTT_SESSION_DONE;
}
static xmqttsessionstatus xmqttImageWriteImpl(xmqttsession* S,const xmqttsessionimageconfig* Config,ptr Buffer,size_t Capacity,size_t* Bytes,bool Write)
{
    xmqttsessionimageconfig C;
    if(!S||!xmqttImageConfig(Config,&C)||!Bytes||!xmqttImageOutput(Bytes,sizeof(*Bytes),Config,&C)||
       (Write&&(!Buffer||!xmqttImageOutput(Buffer,Capacity,Config,&C)||!xmqttImageSeparate(Buffer,Capacity,Bytes,sizeof(*Bytes)))))return xmqttImageError(XMQTT_SESSION_ERROR,"invalid image inputs/outputs or overlap");
    xrtMutexLock(&S->Gate);xmqttimagecut Cut;xmqttsessionstatus Status=xmqttImageMeasure(S,&C,Write?Buffer:NULL,Write?Capacity:0,Bytes,&Cut);
    if(Status!=XMQTT_SESSION_DONE){xrtMutexUnlock(&S->Gate);return Status;}
    if(!Write){*Bytes=Cut.Bytes;xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_DONE;}
    if(Capacity<Cut.Bytes){xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_LIMIT;}
    unsigned char* B=Buffer;memset(B,0,XMQTT_IMAGE_HEADER);memcpy(B,"XMQSIMG",7);xmqttImagePut(B+8,1,2);xmqttImagePut(B+10,XMQTT_IMAGE_HEADER,2);
    B[12]=(unsigned char)S->Config.Version;B[13]=(unsigned char)Cut.Flags;xmqttImagePut(B+16,Cut.Bytes,8);xmqttImagePut(B+24,C.WallMilliseconds,8);xmqttImagePut(B+32,Cut.Remaining,8);xmqttImagePut(B+40,S->Generation,8);
    xmqttImagePut(B+48,Cut.Outgoing,4);xmqttImagePut(B+52,Cut.Incoming,4);xmqttImagePut(B+56,S->Config.ClientId.Size,2);xmqttImagePut(B+58,C.Binding.Size,2);xmqttImagePut(B+60,S->Next,2);
    size_t At=XMQTT_IMAGE_HEADER;memcpy(B+At,C.Binding.Data,C.Binding.Size);At+=C.Binding.Size;memcpy(B+At,S->Config.ClientId.Data,S->Config.ClientId.Size);At+=S->Config.ClientId.Size;
    if(Cut.Flags&XMQTT_IMAGE_STATE){
        for(xmqttsessionpacket* P=S->Head;P;P=P->Next){B[At]=(unsigned char)P->Stage;B[At+1]=(unsigned char)((P->Attempted?1:0)|(P->Sent?2:0));B[At+2]=P->PubrecReason;B[At+3]=0;xmqttImagePut(B+At+4,P->Bytes,4);xmqttImagePut(B+At+8,P->Packet.Properties.Count,4);At+=XMQTT_IMAGE_PACKET;memcpy(B+At,P->Packet.Wire.Data,P->Bytes);At+=P->Bytes;}
        if(Cut.Incoming)for(unsigned I=0;I<XMQTT_SESSION_RX_BYTES;++I){unsigned Bits=S->Receive[I];if(!(Bits&0xaau))continue;
            for(unsigned Shift=0;Shift<8;Shift+=2){unsigned State=(Bits>>Shift)&3u;if(State<2)continue;xmqttImagePut(B+At,I*4u+Shift/2u,2);B[At+2]=(unsigned char)State;At+=3;}}
    }
    xmqttImagePut(B+At,xrtCrc32(B,At),4);*Bytes=Cut.Bytes;xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_DONE;
}
XMQTT_API xmqttsessionstatus xmqttSessionImageSize(xmqttsession* S,const xmqttsessionimageconfig* C,size_t* Bytes)
{return xmqttImageWriteImpl(S,C,NULL,0,Bytes,false);}
XMQTT_API xmqttsessionstatus xmqttSessionImageWrite(xmqttsession* S,const xmqttsessionimageconfig* C,ptr Buffer,size_t Capacity,size_t* Bytes)
{return xmqttImageWriteImpl(S,C,Buffer,Capacity,Bytes,true);}

typedef struct xmqttimageheader {unsigned Flags;uint32 Outgoing,Incoming;uint16 Next;uint64 Remaining,Wall,Generation;size_t Packets;} xmqttimageheader;
/* CRC and structural boundaries are checked before the first state allocation. */
static xmqttsessionstatus xmqttImageReadHeader(xmqttsessionconfig* K,const xmqttsessionimageconfig* C,xbytesview Image,xmqttimageheader* H,bool Assigned)
{
    if(Image.Size>C->MaxBytes)return XMQTT_SESSION_LIMIT;
    const unsigned char* B=Image.Data;
    if(Image.Size<XMQTT_IMAGE_HEADER+XMQTT_IMAGE_TRAILER||memcmp(B,"XMQSIMG\0",8)||xmqttImageGet(B+8,2)!=1||xmqttImageGet(B+10,2)!=XMQTT_IMAGE_HEADER||
       (B[12]!=XMQTT_VERSION_5&&B[12]!=XMQTT_VERSION_311)||(B[13]&~7u)||xmqttImageGet(B+14,2)||xmqttImageGet(B+16,8)!=Image.Size||xmqttImageGet(B+62,2)||
       xmqttImageGet(B+Image.Size-4,4)!=xrtCrc32(B,Image.Size-4))return xmqttImageError(XMQTT_SESSION_PROTOCOL_ERROR,"invalid image format/length/version/CRC");
    H->Flags=B[13];H->Wall=xmqttImageGet(B+24,8);H->Remaining=xmqttImageGet(B+32,8);H->Generation=xmqttImageGet(B+40,8);H->Outgoing=(uint32)xmqttImageGet(B+48,4);H->Incoming=(uint32)xmqttImageGet(B+52,4);H->Next=(uint16)xmqttImageGet(B+60,2);
    size_t Id=(size_t)xmqttImageGet(B+56,2),Binding=(size_t)xmqttImageGet(B+58,2),End=Image.Size-4;
    if(!Id||!Binding||Binding>XMQTT_SESSION_IMAGE_BINDING_MAX||Binding+Id>End-XMQTT_IMAGE_HEADER||!H->Next||H->Wall>INT64_MAX||H->Outgoing>65535||H->Incoming>65535||
       (!(H->Flags&XMQTT_IMAGE_STATE)&&(H->Flags||H->Remaining||H->Outgoing||H->Incoming))||
       ((H->Flags&XMQTT_IMAGE_STATE)&&((H->Flags&XMQTT_IMAGE_INFINITE)?H->Remaining!=UINT64_MAX:!H->Remaining||H->Remaining>(uint64)(UINT32_MAX-1u)*1000u))||
       (B[12]==XMQTT_VERSION_311&&(H->Flags&XMQTT_IMAGE_STATE)&&!(H->Flags&XMQTT_IMAGE_INFINITE)))return xmqttImageError(XMQTT_SESSION_PROTOCOL_ERROR,"invalid image state metadata");
    if(B[12]!=K->Version||Binding!=C->Binding.Size||memcmp(B+XMQTT_IMAGE_HEADER,C->Binding.Data,Binding)||(!Assigned&&(Id!=K->ClientId.Size||memcmp(B+XMQTT_IMAGE_HEADER+Binding,K->ClientId.Data,Id))))return xmqttImageError(XMQTT_SESSION_STATE_ERROR,"image broker binding, version or ClientId differs");
    if(Assigned){xstrview Identity={(const char*)B+XMQTT_IMAGE_HEADER+Binding,Id};if(!xmqttUtf8StringCheck(Identity,K->Codec.Utf8Flags,NULL))return xmqttImageError(XMQTT_SESSION_PROTOCOL_ERROR,"invalid assigned image ClientId");K->ClientId=Identity;}
    if(C->WallMilliseconds<H->Wall)return xmqttImageError(XMQTT_SESSION_STATE_ERROR,"image restore wall clock went backwards");
    if(H->Outgoing>K->MaxOutgoing||H->Incoming>K->MaxIncoming)return XMQTT_SESSION_LIMIT;
    H->Packets=XMQTT_IMAGE_HEADER+Binding+Id;size_t At=H->Packets;
    for(uint32 I=0;I<H->Outgoing;++I){
        if(End-At<XMQTT_IMAGE_PACKET)return xmqttImageError(XMQTT_SESSION_PROTOCOL_ERROR,"truncated image packet metadata");
        size_t N=(size_t)xmqttImageGet(B+At+4,4);unsigned Stage=B[At],Flags=B[At+1],Reason=B[At+2];
        if(Stage>1||(Flags&~3u)||((Flags&2u)&&!(Flags&1u))||B[At+3]||(Stage?(Flags!=3u||(Reason!=0&&Reason!=0x10)||(K->Version==XMQTT_VERSION_311&&Reason)):Reason!=0)||
           !N||N>End-At-XMQTT_IMAGE_PACKET)return xmqttImageError(XMQTT_SESSION_PROTOCOL_ERROR,"invalid image packet boundary/stage");
        At+=XMQTT_IMAGE_PACKET+N;
    }
    if(H->Incoming>(End-At)/3u||At+(size_t)H->Incoming*3u!=End)return xmqttImageError(XMQTT_SESSION_PROTOCOL_ERROR,"invalid image incoming/trailing length");
    unsigned Previous=0;for(uint32 I=0;I<H->Incoming;++I){unsigned IdValue=(unsigned)xmqttImageGet(B+At,2),State=B[At+2];if(IdValue<=Previous||(State!=2&&State!=3))return xmqttImageError(XMQTT_SESSION_PROTOCOL_ERROR,"invalid image incoming order/state");Previous=IdValue;At+=3;}
    return XMQTT_SESSION_DONE;
}
static xmqttsessionstatus xmqttImageRestoreImpl(const xmqttsessionconfig* SessionConfig,const xmqttsessionimageconfig* Config,xbytesview Image,xmqttsession** Out,bool Assigned)
{
    xmqttsessionimageconfig C;
    if(!xmqttImageSpan(SessionConfig,sizeof(*SessionConfig))||!xmqttImageConfig(Config,&C)||!Out||!xmqttImageOutput(Out,sizeof(*Out),Config,&C)||!xmqttImageSpan(Image.Data,Image.Size)||
       !xmqttImageSeparate(Out,sizeof(*Out),SessionConfig,sizeof(*SessionConfig))||!xmqttImageSeparate(Out,sizeof(*Out),Image.Data,Image.Size))return xmqttImageError(XMQTT_SESSION_ERROR,"invalid image restore inputs/output or overlap");
    xmqttsessionconfig K=*SessionConfig;
    if(K.Size!=sizeof(K)||(K.Version!=XMQTT_VERSION_5&&K.Version!=XMQTT_VERSION_311)||(Assigned?(K.Version!=XMQTT_VERSION_5||K.ClientId.Size):!K.ClientId.Size)||K.ClientId.Size>65535||K.MaxOutgoing>65535||K.MaxIncoming>65535||!xmqttImageSpan(K.ClientId.Data,K.ClientId.Size)||!xmqttImageSeparate(Out,sizeof(*Out),K.ClientId.Data,K.ClientId.Size))return xmqttImageError(XMQTT_SESSION_ERROR,"invalid image Session identity/config");
    xmqttcodecconfig Codec;if(!xmqttInternalCodecSettings(XMQTT_CLIENT_TO_SERVER,&K.Codec,&Codec))return XMQTT_SESSION_ERROR;K.Codec=Codec;
    xmqttimageheader H;xmqttsessionstatus Status=xmqttImageReadHeader(&K,&C,Image,&H,Assigned);if(Status!=XMQTT_SESSION_DONE)return Status;
    if(xmqttSessionStorageSize(&K,0,NULL)>K.MaxBytes)return XMQTT_SESSION_LIMIT;
    xmqttsession* S=NULL;if(!xmqttSessionCreate(&K,&S))return XMQTT_SESSION_ERROR;
    S->Generation=H.Generation;S->Next=H.Next;bool Keep=(H.Flags&XMQTT_IMAGE_STATE)!=0;bool Infinite=(H.Flags&(XMQTT_IMAGE_LIVE|XMQTT_IMAGE_INFINITE))!=0;uint64 Remaining=H.Remaining;
    bool Expired=Keep&&!Infinite&&C.WallMilliseconds-H.Wall>=Remaining;
    /* Expired images still undergo full semantic validation. A stale timestamp
     * cannot turn CRC-valid corrupted packets into an accepted empty state. */
    size_t At=H.Packets;
    for(uint32 I=0;I<H.Outgoing;++I){Status=xmqttInternalSessionRestorePacket(S,(xbytesview){Image.Data+At+XMQTT_IMAGE_PACKET,(size_t)xmqttImageGet(Image.Data+At+4,4)},(size_t)xmqttImageGet(Image.Data+At+8,4),Image.Data[At],Image.Data[At+1],Image.Data[At+2],NULL);if(Status==XMQTT_SESSION_PROTOCOL_ERROR)xmqttImageError(Status,"invalid canonical image PUBLISH/properties/identifier");if(Status!=XMQTT_SESSION_DONE){(void)xmqttSessionDestroy(S);return Status;}At+=XMQTT_IMAGE_PACKET+(size_t)xmqttImageGet(Image.Data+At+4,4);}
    for(uint32 I=0;I<H.Incoming;++I){uint16 Id=(uint16)xmqttImageGet(Image.Data+At,2);unsigned State=Image.Data[At+2],Shift=(Id&3u)*2u;S->Receive[Id>>2]|=(unsigned char)(State<<Shift);++S->RxCount;At+=3;}
    if(Keep&&!Infinite&&!Expired)Remaining-=C.WallMilliseconds-H.Wall;
    xmqttInternalSessionRestoreExpiry(S,C.MonotonicTime,Keep,Infinite,Remaining);
    if(Expired){(void)xmqttSessionReset(S);Status=XMQTT_SESSION_RESET;}else Status=XMQTT_SESSION_DONE;
    *Out=S;return Status;
}
XMQTT_API xmqttsessionstatus xmqttSessionImageRestore(const xmqttsessionconfig* K,const xmqttsessionimageconfig* C,xbytesview Image,xmqttsession** Out)
{return xmqttImageRestoreImpl(K,C,Image,Out,false);}
XMQTT_API xmqttsessionstatus xmqttSessionImageRestoreAssigned(const xmqttsessionconfig* K,const xmqttsessionimageconfig* C,xbytesview Image,xmqttsession** Out)
{return xmqttImageRestoreImpl(K,C,Image,Out,true);}
#endif
