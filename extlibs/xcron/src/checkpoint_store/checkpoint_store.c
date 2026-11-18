#include <xcron/checkpoint_store.h>
#include "../checkpoint/checkpoint_internal.h"
#include <xrt/crc32.h>
#include <xrt/memory.h>
#include <string.h>

#if defined(XCRON_FEATURE_XCRON_CHECKPOINT_STORE)
enum { XCS_PAGE=4096, XCS_RECORD=32, XCS_CRC=XCS_RECORD+XCRON_CHECKPOINT_BYTES, XCS_WIRE=XCS_CRC+4 };
_Static_assert(XCS_WIRE<=XCS_PAGE,"checkpoint store slot");
#define XCS_MAGIC UINT32_C(0x58435331)

struct xcroncheckpointstore {
    xfile File;
    xcroncheckpointkey Key;
    xcroncheckpointstoreinfo Info;
    xcroncheckpoint Record;
    uint32 Magic;
    bool Uncertain;
};

static xcroncheckpointstorestatus xcronStoreFail(xcroncheckpointstorestatus Status,xerrkind Kind,cstr Text)
{
    xrtSetErrorInfo(Kind,"xrt.cron.checkpoint_store",Status,Text);
    return Status;
}

static bool xcronStoreValid(const xcroncheckpointstore* Store)
{
    return xrtMemRangeValid(Store,sizeof(*Store)) && Store->Magic==XCS_MAGIC;
}

static void xcronStorePut(uint8* B,uint64 Value,size_t Count)
{
    for(size_t I=0;I<Count;++I){B[I]=(uint8)Value;Value>>=8;}
}
static uint64 xcronStoreGet(const uint8* B,size_t Count)
{
    uint64 Value=0;
    while(Count)Value=(Value<<8)|B[--Count];
    return Value;
}

static bool xcronStoreWrite(const xcroncheckpointstore* Store,uint64 Generation,const xcroncheckpoint* Record)
{
    uint8 B[XCS_WIRE]={0};
    memcpy(B,"XCST",4);
    B[4]=1;
    xcronStorePut(B+8,Generation,8);
    xcronStorePut(B+16,Store->Key.Job,8);
    xcronStorePut(B+24,Store->Key.Revision,8);
    if(Record)memcpy(B+XCS_RECORD,Record->Bytes,sizeof(Record->Bytes));
    xcronStorePut(B+XCS_CRC,xrtCrc32(B,XCS_CRC),4);
    return xrtWriteAtFull(Store->File,(Generation&1)*XCS_PAGE,B,sizeof(B),NULL) && xrtFlush(Store->File);
}

static xcroncheckpointstorestatus xcronStoreDecode(const uint8* B,unsigned Slot,xcroncheckpointkey Key,uint64* Generation)
{
    if(memcmp(B,"XCST",4) || xcronStoreGet(B+XCS_CRC,4)!=xrtCrc32(B,XCS_CRC))
        return XCRON_CHECKPOINT_STORE_EMPTY;
    if(xcronStoreGet(B+4,4)!=1)return XCRON_CHECKPOINT_STORE_CORRUPT;
    if(xcronStoreGet(B+16,8)!=Key.Job || xcronStoreGet(B+24,8)!=Key.Revision)
        return XCRON_CHECKPOINT_STORE_MISMATCH;
    uint64 G=xcronStoreGet(B+8,8);
    if((G&1)!=Slot)return XCRON_CHECKPOINT_STORE_CORRUPT;
    if(G){
        xcroncheckpointstatus Status=xcronCheckpointValidate((xbytesview){B+XCS_RECORD,XCRON_CHECKPOINT_BYTES},Key);
        if(Status!=XCRON_CHECKPOINT_VALID)
            return Status==XCRON_CHECKPOINT_INCOMPATIBLE?XCRON_CHECKPOINT_STORE_MISMATCH:XCRON_CHECKPOINT_STORE_CORRUPT;
    }else{
        for(size_t I=XCS_RECORD;I<XCS_CRC;++I)if(B[I])return XCRON_CHECKPOINT_STORE_CORRUPT;
    }
    *Generation=G;
    return XCRON_CHECKPOINT_STORE_OK;
}

static xcroncheckpointstorestatus xcronStoreRecover(xcroncheckpointstore* Store)
{
    uint64 Size;
    if(!xrtFileSize(Store->File,&Size))return XCRON_CHECKPOINT_STORE_IO;
    if(Size!=2*XCS_PAGE)return XCRON_CHECKPOINT_STORE_CORRUPT;
    for(unsigned Slot=0;Slot<2;++Slot){
        uint8 B[XCS_WIRE];uint64 Generation;
        if(!xrtReadAtFull(Store->File,(uint64)Slot*XCS_PAGE,B,sizeof(B),NULL))return XCRON_CHECKPOINT_STORE_IO;
        xcroncheckpointstorestatus Status=xcronStoreDecode(B,Slot,Store->Key,&Generation);
        if(Status==XCRON_CHECKPOINT_STORE_EMPTY)continue;
        if(Status!=XCRON_CHECKPOINT_STORE_OK)return Status;
        if(Store->Info.ValidSlots){
            uint64 First=Store->Info.Generation;
            if((Generation>First?Generation-First:First-Generation)!=1)return XCRON_CHECKPOINT_STORE_CORRUPT;
        }
        if(!Store->Info.ValidSlots || Generation>Store->Info.Generation){
            Store->Info.Generation=Generation;
            memcpy(Store->Record.Bytes,B+XCS_RECORD,sizeof(Store->Record.Bytes));
        }
        ++Store->Info.ValidSlots;
    }
    if(!Store->Info.ValidSlots)return XCRON_CHECKPOINT_STORE_CORRUPT;
    return xrtFlush(Store->File)?XCRON_CHECKPOINT_STORE_OK:XCRON_CHECKPOINT_STORE_IO;
}

XCRON_API xcroncheckpointstorestatus xcronCheckpointStoreOpen(xroot Root,cstr Path,
    xcroncheckpointkey Key,bool Create,xcroncheckpointstore** Out)
{
    if(!Root || !Path || !*Path || !Key.Job || !Key.Revision || !xrtMemRangeValid(Out,sizeof(*Out)))
        return xcronStoreFail(XCRON_CHECKPOINT_STORE_ARGUMENT,XERR_ARGUMENT,"invalid checkpoint store open");
    xcroncheckpointstore* Store=xrtCalloc(1,sizeof(*Store));
    if(!Store)return XCRON_CHECKPOINT_STORE_MEMORY;
    xfileoptions Options={XFILE_READ|XFILE_WRITE,0600,XFILE_SHARE_READ|XFILE_SHARE_WRITE};
    if(Create)Options.Flags|=XFILE_CREATE|XFILE_EXCLUSIVE;
    Store->File=xrtRootFileOpen(Root,Path,&Options);
    Store->Key=Key;
    xcroncheckpointstorestatus Status=XCRON_CHECKPOINT_STORE_IO;
    if(Store->File){
        if(!xrtFileTryLockExclusiveUntilClose(Store->File))Status=XCRON_CHECKPOINT_STORE_LOCK;
        else if(Create){
            if(xrtFileResize(Store->File,2*XCS_PAGE) && xcronStoreWrite(Store,0,NULL)){
                Store->Info.ValidSlots=1;
                Status=XCRON_CHECKPOINT_STORE_OK;
            }
        }else Status=xcronStoreRecover(Store);
    }
    if(Status==XCRON_CHECKPOINT_STORE_OK){
        Store->Magic=XCS_MAGIC;
        *Out=Store;
        return Status;
    }
    if(Store->File)(void)xrtClose(Store->File);
    xrtFree(Store);
    if(Status==XCRON_CHECKPOINT_STORE_CORRUPT)return xcronStoreFail(Status,XERR_VALUE,"invalid checkpoint store image");
    if(Status==XCRON_CHECKPOINT_STORE_MISMATCH)return xcronStoreFail(Status,XERR_VALUE,"checkpoint store key mismatch");
    return Status;
}

XCRON_API xcroncheckpointstorestatus xcronCheckpointStoreRead(const xcroncheckpointstore* Store,
    xcroncheckpoint* Record,xcroncheckpointstoreinfo* Info)
{
    size_t Bytes=Record?sizeof(*Record):0;
    if(!xcronStoreValid(Store) || !xrtMemRangeValid(Info,sizeof(*Info)) || !xrtMemRangeValid(Record,Bytes) ||
       xrtMemRangesOverlap(Record,Bytes,Info,sizeof(*Info)) ||
       xrtMemRangesOverlap(Record,Bytes,Store,sizeof(*Store)) ||
       xrtMemRangesOverlap(Info,sizeof(*Info),Store,sizeof(*Store)))
        return xcronStoreFail(XCRON_CHECKPOINT_STORE_ARGUMENT,XERR_ARGUMENT,"invalid checkpoint store outputs");
    if(Store->Uncertain)return xcronStoreFail(XCRON_CHECKPOINT_STORE_UNCERTAIN,XERR_STATE,"reopen uncertain checkpoint store");
    *Info=Store->Info;
    if(!Info->Generation)return XCRON_CHECKPOINT_STORE_EMPTY;
    if(Record)*Record=Store->Record;
    return XCRON_CHECKPOINT_STORE_OK;
}

XCRON_API xcroncheckpointstorestatus xcronCheckpointStoreCommit(xcroncheckpointstore* Store,
    uint64 ExpectedGeneration,const xcroncheckpoint* Record)
{
    if(!xcronStoreValid(Store) || !xrtMemRangeValid(Record,sizeof(*Record)) ||
       xrtMemRangesOverlap(Record,sizeof(*Record),Store,sizeof(*Store)))
        return xcronStoreFail(XCRON_CHECKPOINT_STORE_ARGUMENT,XERR_ARGUMENT,"invalid checkpoint commit");
    if(Store->Uncertain)return xcronStoreFail(XCRON_CHECKPOINT_STORE_UNCERTAIN,XERR_STATE,"reopen uncertain checkpoint store");
    if(ExpectedGeneration!=Store->Info.Generation)
        return xcronStoreFail(XCRON_CHECKPOINT_STORE_STALE,XERR_STATE,"stale checkpoint generation");
    if(ExpectedGeneration==UINT64_MAX)
        return xcronStoreFail(XCRON_CHECKPOINT_STORE_LIMIT,XERR_RANGE,"checkpoint generation exhausted");
    xcroncheckpointstatus Status=xcronCheckpointValidate((xbytesview){Record->Bytes,sizeof(Record->Bytes)},Store->Key);
    if(Status!=XCRON_CHECKPOINT_VALID)return xcronStoreFail(
        Status==XCRON_CHECKPOINT_INCOMPATIBLE?XCRON_CHECKPOINT_STORE_MISMATCH:XCRON_CHECKPOINT_STORE_CORRUPT,
        XERR_VALUE,"invalid checkpoint record or key");
    uint64 Next=ExpectedGeneration+1;
    if(!xcronStoreWrite(Store,Next,Record)){
        Store->Uncertain=true;
        return XCRON_CHECKPOINT_STORE_UNCERTAIN;
    }
    Store->Record=*Record;
    Store->Info.Generation=Next;
    Store->Info.ValidSlots=2;
    return XCRON_CHECKPOINT_STORE_OK;
}

XCRON_API xcroncheckpointstorestatus xcronCheckpointStoreClose(xcroncheckpointstore* Store)
{
    if(!xcronStoreValid(Store))return xcronStoreFail(XCRON_CHECKPOINT_STORE_ARGUMENT,XERR_ARGUMENT,"invalid checkpoint store");
    Store->Magic=0;
    bool Closed=xrtClose(Store->File);
    xrtFree(Store);
    return Closed?XCRON_CHECKPOINT_STORE_OK:XCRON_CHECKPOINT_STORE_IO;
}
#endif
