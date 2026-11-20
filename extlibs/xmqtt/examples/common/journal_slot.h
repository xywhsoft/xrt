#ifndef XMQTT_EXAMPLE_JOURNAL_SLOT_H
#define XMQTT_EXAMPLE_JOURNAL_SLOT_H

#include <xmqtt/journal.h>
#include <xrt/crc32.h>
#include <xrt/memory.h>
#include <string.h>

enum {MQTT_EXAMPLE_SLOT_PAGE=4096,MQTT_EXAMPLE_SLOT_START=12288,MQTT_EXAMPLE_SLOT_FRAME=64};
typedef struct mqttExampleJournalSlotFiles { xfile Control,Data[2]; } mqttExampleJournalSlotFiles;
typedef struct mqttExampleJournalSlotInfo {
    uint64 Generation,PublishedSequence;
    xmqttjournaleffect Effect;
    bool Poisoned,JournalOpen,Prepared;
} mqttExampleJournalSlotInfo;
typedef struct mqttExampleSlotMark {uint64 Generation;xmqttjournalprefix Prefix;} mqttExampleSlotMark;
typedef struct mqttExampleJournalSlot {
    xfile Control,Data[2];
    xmqttjournalconfig Config;
    mqttExampleSlotMark Current;
    uint64 RepairOffset;
    xmqttjournal* Child;
    xmqttjournaleffect Effect;
    bool Busy,Validated,Prepared,Poisoned;
    unsigned char Binding[];
} mqttExampleJournalSlot;

static inline xmqttjournalstatus mqttExampleSlotError(xmqttjournalstatus S,cstr Message)
{
    xerrkind K=S==XMQTT_JOURNAL_CORRUPT?XERR_PROTOCOL:S==XMQTT_JOURNAL_STATE?XERR_STATE:
        (S==XMQTT_JOURNAL_IO||S==XMQTT_JOURNAL_UNCERTAIN)?XERR_IO:XERR_ARGUMENT;
    xrtSetErrorInfo(K,"xrt.mqtt.example.selector",6200-(int32)S,Message);return S;
}
static inline uint64 mqttExampleSlotGet(const unsigned char* P,unsigned N)
{uint64 V=0;for(unsigned I=0;I<N;++I)V=(V<<8)|P[I];return V;}
static inline void mqttExampleSlotPut(unsigned char* P,uint64 V,unsigned N)
{for(unsigned I=N;I;--I){P[I-1]=(unsigned char)V;V>>=8;}}
static inline bool mqttExampleJSpan(const void* P,size_t N)
{return (P||!N)&&xrtMemRangeValid(P,N);}
static inline bool mqttExampleJSeparate(const void* A,size_t N,const void* B,size_t M)
{return !N||!M||!xrtMemRangesOverlap(A,N,B,M);}
static inline bool mqttExampleSlotZero(const unsigned char* P,size_t N)
{for(size_t I=0;I<N;++I)if(P[I])return false;return true;}
static inline void mqttExampleJSeal(unsigned char* P)
{mqttExampleSlotPut(P+4092,xrtCrc32(P,4092),4);}
static inline bool mqttExampleSlotCheck(const unsigned char* P)
{return mqttExampleSlotGet(P+4092,4)==xrtCrc32(P,4092);}
static inline xmqttjournalstatus mqttExampleJournalIOResult(xmqttjournalstatus Status,xerror* Saved)
{
    if(Status<0){
        xrtErrorFree(Saved);
        if(!xrtGetError())xrtSetErrorKind(XERR_IO);
        return Status;
    }
    xerror* Discard=NULL;
    if(!xrtTrySwapErrorTake(Saved,&Discard)){
        xrtErrorFree(Saved);
        return XMQTT_JOURNAL_ERROR;
    }
    xrtErrorFree(Discard);
    return Status;
}

static inline bool mqttExampleSlotConfig(const xmqttjournalconfig* C)
{
    return mqttExampleJSpan(C,sizeof(*C))&&C->Size==sizeof(*C)&&
        (C->Version==XMQTT_VERSION_5||C->Version==XMQTT_VERSION_311)&&
        C->Binding.Size&&C->Binding.Size<=1024&&mqttExampleJSpan(C->Binding.Data,C->Binding.Size)&&
        C->Generation&&C->PayloadSchema&&
        C->MaxFileBytes>=MQTT_EXAMPLE_SLOT_START&&C->MaxFileBytes<=INT64_MAX;
}
static inline bool mqttExampleSlotFileUsable(xfile File,uint64 Required)
{
    return (xrtFileCapabilities(File)&Required)==Required&&
        !(xrtFileFlags(File)&(XFILE_APPEND|XFILE_ASYNC));
}
static inline bool mqttExampleJSRead(xfile File,uint64 Offset,unsigned char* Page)
{return xrtReadAtFull(File,Offset,Page,MQTT_EXAMPLE_SLOT_PAGE,NULL);}

static inline bool mqttExampleJSWrite(xfile File,uint64 Offset,const unsigned char* Page)
{return xrtWriteAtFull(File,Offset,Page,MQTT_EXAMPLE_SLOT_PAGE,NULL);}

static inline void mqttExampleJSDescriptor(unsigned char* P,const xmqttjournalconfig* C)
{
    memset(P,0,MQTT_EXAMPLE_SLOT_PAGE);
    memcpy(P,"XMQSLT1",7);
    P[8]=(unsigned char)C->Version;
    mqttExampleSlotPut(P+12,C->Binding.Size,4);
    mqttExampleSlotPut(P+16,C->Generation,8);
    mqttExampleSlotPut(P+24,C->PayloadSchema,4);
    memcpy(P+64,C->Binding.Data,C->Binding.Size);
    mqttExampleJSeal(P);
}

static inline void mqttExampleJSWitness(unsigned char* P,const mqttExampleSlotMark* Mark)
{
    memset(P,0,MQTT_EXAMPLE_SLOT_PAGE);
    memcpy(P,"XMQSEL1",7);
    mqttExampleSlotPut(P+8,Mark->Generation,8);
    mqttExampleSlotPut(P+16,Mark->Prefix.Sequence,8);
    mqttExampleSlotPut(P+24,Mark->Prefix.End,8);
    mqttExampleSlotPut(P+32,Mark->Prefix.Chain,4);
    mqttExampleJSeal(P);
}

static inline bool mqttExampleJSParse(const unsigned char* P,uint64 Initial,mqttExampleSlotMark* Mark)
{
    if(memcmp(P,"XMQSEL1\0",8)||!mqttExampleSlotCheck(P)||!mqttExampleSlotZero(P+36,4092-36))return false;
    Mark->Generation=mqttExampleSlotGet(P+8,8);
    xmqttjournalprefix* W=&Mark->Prefix;
    *W=(xmqttjournalprefix){mqttExampleSlotGet(P+16,8),mqttExampleSlotGet(P+24,8),(uint32)mqttExampleSlotGet(P+32,4)};
    return Mark->Generation>=Initial&&W->End>=MQTT_EXAMPLE_SLOT_START&&W->End<=INT64_MAX&&
        W->Sequence<=(W->End-MQTT_EXAMPLE_SLOT_START)/MQTT_EXAMPLE_SLOT_FRAME&&
        (W->Sequence||(W->End==MQTT_EXAMPLE_SLOT_START&&!W->Chain));
}

static inline bool mqttExampleJSPrefix(xmqttjournalprefix A,xmqttjournalprefix B)
{return A.Sequence==B.Sequence&&A.End==B.End&&A.Chain==B.Chain;}

static inline xmqttjournalstatus mqttExampleJSLoad(mqttExampleJournalSlot* S,uint64 ControlBytes)
{
    const xmqttjournalconfig* C=&S->Config;
    unsigned char P[MQTT_EXAMPLE_SLOT_PAGE];
    if(ControlBytes!=MQTT_EXAMPLE_SLOT_START)
        return mqttExampleSlotError(XMQTT_JOURNAL_CORRUPT,"invalid slot control length");
    if(!mqttExampleJSRead(S->Control,0,P))return XMQTT_JOURNAL_IO;
    if(memcmp(P,"XMQSLT1\0",8)||!mqttExampleSlotCheck(P)||P[8]!=C->Version||
       !mqttExampleSlotZero(P+9,3)||mqttExampleSlotGet(P+12,4)!=C->Binding.Size||mqttExampleSlotGet(P+16,8)!=C->Generation||
       mqttExampleSlotGet(P+24,4)!=C->PayloadSchema||!mqttExampleSlotZero(P+28,36)||
       memcmp(P+64,C->Binding.Data,C->Binding.Size)||!mqttExampleSlotZero(P+64+C->Binding.Size,4028-C->Binding.Size))
        return mqttExampleSlotError(XMQTT_JOURNAL_CORRUPT,"slot descriptor, generation or binding mismatch");
    mqttExampleSlotMark A={0},B={0};
    if(!mqttExampleJSRead(S->Control,MQTT_EXAMPLE_SLOT_PAGE,P))return XMQTT_JOURNAL_IO;
    bool Left=mqttExampleJSParse(P,C->Generation,&A);
    if(!mqttExampleJSRead(S->Control,2*MQTT_EXAMPLE_SLOT_PAGE,P))return XMQTT_JOURNAL_IO;
    bool Right=mqttExampleJSParse(P,C->Generation,&B);
    if(!Left&&!Right)return mqttExampleSlotError(XMQTT_JOURNAL_CORRUPT,"both selector witnesses invalid");
    if(Left&&Right){
        uint64 Distance=A.Generation>B.Generation?A.Generation-B.Generation:B.Generation-A.Generation;
        if(Distance>1||(!Distance&&!mqttExampleJSPrefix(A.Prefix,B.Prefix)))
            return mqttExampleSlotError(XMQTT_JOURNAL_CORRUPT,"conflicting selector witnesses");
    }
    bool SelectLeft=Left&&(!Right||A.Generation>=B.Generation);
    S->Current=SelectLeft?A:B;
    if(!Left||!Right||A.Generation!=B.Generation)
        S->RepairOffset=SelectLeft?2*MQTT_EXAMPLE_SLOT_PAGE:MQTT_EXAMPLE_SLOT_PAGE;
    return XMQTT_JOURNAL_DONE;
}

static inline xmqttjournalstatus mqttExampleJSData(mqttExampleJournalSlot* S,uint64 Generation,bool Create,bool Reset,xmqttjournal** Out)
{
    size_t Index=(size_t)((Generation-S->Config.Generation)&1u);
    xfile File=S->Data[Index];
    if(Reset&&!xrtFileResize(File,0))return XMQTT_JOURNAL_IO;
    xmqttjournalconfig C=S->Config;
    C.Generation=Generation;
    return Create?xmqttJournalCreate(File,&C,Out):
        xmqttJournalOpenAtPrefix(File,&C,&S->Current.Prefix,Out);
}

static inline xmqttjournalstatus mqttExampleJSInitialize(mqttExampleJournalSlot* S)
{
    xmqttjournal* First=NULL;
    xmqttjournalstatus Status=mqttExampleJSData(S,S->Config.Generation,true,false,&First);
    if(Status!=XMQTT_JOURNAL_DONE)return Status;
    if(!xmqttJournalDestroy(First))return XMQTT_JOURNAL_STATE;
    unsigned char P[MQTT_EXAMPLE_SLOT_PAGE];
    mqttExampleJSDescriptor(P,&S->Config);
    if(!mqttExampleJSWrite(S->Control,0,P))return XMQTT_JOURNAL_IO;
    S->Current=(mqttExampleSlotMark){S->Config.Generation,{0,MQTT_EXAMPLE_SLOT_START,0}};
    mqttExampleJSWitness(P,&S->Current);
    if(!mqttExampleJSWrite(S->Control,MQTT_EXAMPLE_SLOT_PAGE,P)||!mqttExampleJSWrite(S->Control,2*MQTT_EXAMPLE_SLOT_PAGE,P)||!xrtFlush(S->Control))
        return XMQTT_JOURNAL_UNCERTAIN;
    S->Validated=true;
    return XMQTT_JOURNAL_DONE;
}

static inline xmqttjournalstatus mqttExampleJSConstruct(const mqttExampleJournalSlotFiles* Files,const xmqttjournalconfig* C,
    mqttExampleJournalSlot** Out,bool Create)
{
    if(!mqttExampleJSpan(Files,sizeof(*Files))||!mqttExampleSlotConfig(C)||!mqttExampleJSpan(Out,sizeof(*Out))||
       !mqttExampleJSeparate(Out,sizeof(*Out),Files,sizeof(*Files))||!mqttExampleJSeparate(Out,sizeof(*Out),C,sizeof(*C))||
       !mqttExampleJSeparate(Out,sizeof(*Out),C->Binding.Data,C->Binding.Size))
        return mqttExampleSlotError(XMQTT_JOURNAL_ERROR,"invalid slot files/config/output");
    xfile Control=Files->Control,A=Files->Data[0],B=Files->Data[1];
    if(!Control||!A||!B||Control==A||Control==B||A==B)
        return mqttExampleSlotError(XMQTT_JOURNAL_STATE,"slot requires three distinct file handles");
    xerror* Saved=NULL;
    if(!xrtTrySwapErrorTake(NULL,&Saved))return XMQTT_JOURNAL_ERROR;
    uint64 Required=XFILE_CAP_READ_AT|XFILE_CAP_WRITE_AT|XFILE_CAP_STAT|XFILE_CAP_FLUSH;
    if(!mqttExampleSlotFileUsable(Control,Required)||!mqttExampleSlotFileUsable(A,Required|XFILE_CAP_RESIZE)||
       !mqttExampleSlotFileUsable(B,Required|XFILE_CAP_RESIZE))
        return mqttExampleJournalIOResult(mqttExampleSlotError(XMQTT_JOURNAL_ERROR,"slot files lack synchronous absolute I/O capabilities"),Saved);
    uint64 ControlBytes=0;
    if(!xrtFileSize(Control,&ControlBytes))
        return mqttExampleJournalIOResult(XMQTT_JOURNAL_IO,Saved);
    if(Create){
        uint64 ABytes=0,BBytes=0;
        if(!xrtFileSize(A,&ABytes)||!xrtFileSize(B,&BBytes))
            return mqttExampleJournalIOResult(XMQTT_JOURNAL_IO,Saved);
        if(ControlBytes||ABytes||BBytes)
            return mqttExampleJournalIOResult(mqttExampleSlotError(XMQTT_JOURNAL_STATE,"slot initialization requires three empty files"),Saved);
    }
    mqttExampleJournalSlot* S=xrtCalloc(1,sizeof(*S)+C->Binding.Size);
    if(!S)return mqttExampleJournalIOResult(XMQTT_JOURNAL_ERROR,Saved);
    S->Control=Control;
    S->Data[0]=A;
    S->Data[1]=B;
    S->Config=*C;
    memcpy(S->Binding,C->Binding.Data,C->Binding.Size);
    S->Config.Binding.Data=S->Binding;
    xmqttjournalstatus Status=Create?mqttExampleJSInitialize(S):mqttExampleJSLoad(S,ControlBytes);
    Status=mqttExampleJournalIOResult(Status,Saved);
    if(Status==XMQTT_JOURNAL_DONE)*Out=S;
    else xrtFree(S);
    return Status;
}

static inline xmqttjournalstatus mqttExampleJournalSlotCreate(const mqttExampleJournalSlotFiles* Files,const xmqttjournalconfig* C,mqttExampleJournalSlot** Out)
{return mqttExampleJSConstruct(Files,C,Out,true);}

static inline xmqttjournalstatus mqttExampleJournalSlotOpen(const mqttExampleJournalSlotFiles* Files,const xmqttjournalconfig* C,mqttExampleJournalSlot** Out)
{return mqttExampleJSConstruct(Files,C,Out,false);}

static inline bool mqttExampleJSSpan(const mqttExampleJournalSlot* S,void* Out,size_t Size)
{
    if(!S||!Out)return false;
    uintptr_t Address=(uintptr_t)Out,Base=(uintptr_t)S;
    size_t Allocation=sizeof(*S)+S->Config.Binding.Size;
    return Size<=UINTPTR_MAX-Address&&(Address+Size<=Base||Address>=Base+Allocation);
}

static inline xmqttjournalstatus mqttExampleJSRepair(mqttExampleJournalSlot* S)
{
    if(!S->RepairOffset)return XMQTT_JOURNAL_DONE;
    /* Leave the selected witness intact before reusing the inactive file. */
    unsigned char P[MQTT_EXAMPLE_SLOT_PAGE];
    mqttExampleJSWitness(P,&S->Current);
    if(mqttExampleJSWrite(S->Control,S->RepairOffset,P)&&xrtFlush(S->Control)){
        S->RepairOffset=0;
        return XMQTT_JOURNAL_DONE;
    }
    S->Poisoned=true;
    return XMQTT_JOURNAL_IO;
}

static inline xmqttjournalstatus mqttExampleJSOpenData(mqttExampleJournalSlot* S,xmqttjournal** Out,bool Prepare)
{
    if(!mqttExampleJSSpan(S,Out,sizeof(*Out)))
        return mqttExampleSlotError(XMQTT_JOURNAL_ERROR,"invalid slot or Journal output");
    if(S->Busy||S->Poisoned||S->Child||(Prepare&&!S->Validated))
        return mqttExampleSlotError(XMQTT_JOURNAL_STATE,"slot requires an idle owner and verified current history for replacement");
    if(Prepare&&S->Current.Generation==UINT64_MAX)return XMQTT_JOURNAL_LIMIT;
    xerror* Saved=NULL;
    if(!xrtTrySwapErrorTake(NULL,&Saved))return XMQTT_JOURNAL_ERROR;
    S->Busy=true;
    if(!Prepare)S->Validated=false;
    xmqttjournal* J=NULL;
    uint64 Generation=S->Current.Generation+(Prepare?1u:0u);
    xmqttjournalstatus Status=XMQTT_JOURNAL_DONE;
    if(Prepare)Status=mqttExampleJSRepair(S);
    if(Status==XMQTT_JOURNAL_DONE)Status=mqttExampleJSData(S,Generation,Prepare,Prepare,&J);
    Status=mqttExampleJournalIOResult(Status,Saved);
    if(Status==XMQTT_JOURNAL_DONE){
        S->Child=J;
        S->Prepared=Prepare;
        *Out=J;
    }else if(J){
        (void)xmqttJournalDestroy(J);
    }
    S->Busy=false;
    return Status;
}

static inline xmqttjournalstatus mqttExampleJournalSlotOpenCurrent(mqttExampleJournalSlot* S,xmqttjournal** Out)
{return mqttExampleJSOpenData(S,Out,false);}

static inline xmqttjournalstatus mqttExampleJournalSlotPrepare(mqttExampleJournalSlot* S,xmqttjournal** Out)
{return mqttExampleJSOpenData(S,Out,true);}

static inline xmqttjournalstatus mqttExampleJSSelect(mqttExampleJournalSlot* S,uint64 Generation,const xmqttjournalinfo* Info)
{
    mqttExampleSlotMark Mark={Generation,{Info->Sequence,Info->CommittedEnd,Info->Chain}};
    unsigned char P[MQTT_EXAMPLE_SLOT_PAGE];
    mqttExampleJSWitness(P,&Mark);
    S->Effect=XMQTT_JOURNAL_MAY_BE_COMMITTED;
    if(!mqttExampleJSWrite(S->Control,MQTT_EXAMPLE_SLOT_PAGE,P)||!mqttExampleJSWrite(S->Control,2*MQTT_EXAMPLE_SLOT_PAGE,P)||
       !xrtFlush(S->Control)){
        S->Poisoned=true;
        return XMQTT_JOURNAL_UNCERTAIN;
    }
    S->Current=Mark;
    S->Effect=XMQTT_JOURNAL_COMMITTED;
    S->Prepared=false;
    return XMQTT_JOURNAL_DONE;
}

static inline xmqttjournalstatus mqttExampleJournalSlotPublish(mqttExampleJournalSlot* S,xmqttjournal* J)
{
    xmqttjournalinfo Info={0};
    if(!S||S->Busy||S->Poisoned||!S->Prepared||S->Child!=J||!J||!xmqttJournalInfo(J,&Info)||
       Info.State!=XMQTT_JOURNAL_READY)
        return mqttExampleSlotError(XMQTT_JOURNAL_STATE,"publication requires the selected slot's ready candidate");
    xerror* Saved=NULL;
    if(!xrtTrySwapErrorTake(NULL,&Saved))return XMQTT_JOURNAL_ERROR;
    S->Busy=true;
    xmqttjournalstatus Status=mqttExampleJSSelect(S,S->Current.Generation+1,&Info);
    Status=mqttExampleJournalIOResult(Status,Saved);
    if(Status!=XMQTT_JOURNAL_DONE)S->Poisoned=true;
    S->Busy=false;
    return Status;
}

static inline bool mqttExampleJournalSlotCanTransfer(const mqttExampleJournalSlot* S,const xmqttjournal* J)
{return S&&!S->Busy&&!S->Poisoned&&!S->Prepared&&J&&S->Child==J;}

/* Call after a successful Worker claim. Keep the slot and file handles
 * alive, and wait for that Worker to retire before offline access. */
static inline bool mqttExampleJournalSlotDetach(mqttExampleJournalSlot* S,const xmqttjournal* J)
{
    if(!mqttExampleJournalSlotCanTransfer(S,J))return false;
    S->Child=NULL;
    S->Validated=true;
    return true;
}

static inline bool mqttExampleJournalSlotClose(mqttExampleJournalSlot* S,xmqttjournal* J)
{
    if(!J)return true;
    if(!S||S->Busy||S->Child!=J)return false;
    xmqttjournalinfo Info={0};
    if(!xmqttJournalInfo(J,&Info)||!xmqttJournalDestroy(J))return false;
    if(!S->Prepared)S->Validated=Info.State==XMQTT_JOURNAL_READY;
    S->Prepared=false;
    S->Child=NULL;
    return true;
}

static inline xmqttjournalstatus mqttExampleJournalSlotAppend(mqttExampleJournalSlot* S,xmqttjournal* J,
    xbytesview Bytes,uint64* Sequence)
{
    if(!S||S->Busy||S->Poisoned||!J||S->Child!=J)
        return mqttExampleSlotError(XMQTT_JOURNAL_STATE,"append requires a usable slot child");
    return xmqttJournalAppend(J,Bytes,Sequence);
}

static inline bool mqttExampleJournalSlotGetInfo(mqttExampleJournalSlot* S,mqttExampleJournalSlotInfo* Out)
{
    if(!mqttExampleJSSpan(S,Out,sizeof(*Out))){
        mqttExampleSlotError(XMQTT_JOURNAL_ERROR,"invalid slot info output");
        return false;
    }
    if(S->Busy){
        mqttExampleSlotError(XMQTT_JOURNAL_STATE,"reentrant slot info");
        return false;
    }
    *Out=(mqttExampleJournalSlotInfo){S->Current.Generation,S->Current.Prefix.Sequence,S->Effect,
        S->Poisoned,S->Child!=NULL,S->Prepared};
    return true;
}

static inline bool mqttExampleJournalSlotDestroy(mqttExampleJournalSlot* S)
{
    if(!S)return true;
    if(S->Busy||S->Child){
        mqttExampleSlotError(XMQTT_JOURNAL_STATE,"slot still owns a live Journal or operation");
        return false;
    }
    xrtFree(S);
    return true;
}

#endif
