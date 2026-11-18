#include <xcron/checkpoint_group.h>
#include "../checkpoint/checkpoint_internal.h"
#include <xrt/crc32.h>
#include <xrt/memory.h>
#include <string.h>

#if defined(XCRON_FEATURE_XCRON_CHECKPOINT_GROUP)
enum { XCG_PAGE=4096,XCG_HEAD=40,XCG_BODY_CRC=32,XCG_HEAD_CRC=36 };
#define XCG_MAGIC UINT32_C(0x58434731)
_Static_assert(sizeof(xcroncheckpoint)==XCRON_CHECKPOINT_BYTES && _Alignof(xcroncheckpoint)==1,"checkpoint array wire layout");

struct xcroncheckpointgroup {
    xfile File;
    uint64 Group,SlotBytes;
    size_t FrameBytes,Bytes;
    uint8 *Active,*Scratch;
    xcroncheckpointstoreinfo Info;
    uint32 Capacity,Count,Magic;
    bool Uncertain;
};

static xcroncheckpointstorestatus xcronGroupFail(xcroncheckpointstorestatus Code,xerrkind Kind,cstr Text)
{
    xrtSetErrorInfo(Kind,"xrt.cron.checkpoint_group",Code,Text);
    return Code;
}

static bool xcronGroupValid(const xcroncheckpointgroup* S)
{
    return xrtMemRangeValid(S,sizeof(*S)) && S->Magic==XCG_MAGIC && xrtMemRangeValid(S,S->Bytes);
}

/* Use the ordered per-record CRC bytes. Folding complete CRC-protected
 * records together would give a fixed residue for each valid record. */
static bool xcronGroupRecords(const uint8* Bytes,uint32 Count,uint32* Crc)
{
    uint64 Previous=0;
    uint32 Value=0;
    for(uint32 I=0;I<Count;++I,Bytes+=XCRON_CHECKPOINT_BYTES){
        xcroncheckpointkey Key;
        if(!xcronCheckpointIdentify((xbytesview){Bytes,XCRON_CHECKPOINT_BYTES},&Key) || Key.Job<=Previous || !Key.Revision)
            return false;
        Previous=Key.Job;
        Value=xrtCrc32Update(Value,Bytes+XCRON_CHECKPOINT_CRC_OFFSET,4);
    }
    *Crc=Value;
    return true;
}

static void xcronGroupPublish(xcroncheckpointgroup* S,uint64 Generation,uint32 Count)
{
    uint8* Previous=S->Active;
    S->Active=S->Scratch;
    S->Scratch=Previous;
    S->Info.Generation=Generation;
    S->Count=Count;
}

static bool xcronGroupWrite(xcroncheckpointgroup* S,uint64 Generation,const xcroncheckpoint* Records,
    uint32 Count,uint32 Crc)
{
    uint8* B=S->Scratch;
    memcpy(B,"XCGS",4);
    xcronCheckpointPut(B+4,1,4);
    xcronCheckpointPut(B+8,Generation,8);
    xcronCheckpointPut(B+16,S->Group,8);
    xcronCheckpointPut(B+24,S->Capacity,4);
    xcronCheckpointPut(B+28,Count,4);
    xcronCheckpointPut(B+XCG_BODY_CRC,Crc,4);
    xcronCheckpointPut(B+XCG_HEAD_CRC,xrtCrc32(B,XCG_HEAD_CRC),4);
    size_t Bytes=(size_t)Count*sizeof(*Records);
    if(Bytes)memcpy(B+XCG_HEAD,Records,Bytes);
    return xrtWriteAtFull(S->File,(Generation&1)*S->SlotBytes,B,XCG_HEAD+Bytes,NULL) && xrtFlush(S->File);
}

static xcroncheckpointstorestatus xcronGroupLoadSlot(xcroncheckpointgroup* S,unsigned Slot,
    uint64* Generation,uint32* Count)
{
    uint8* B=S->Scratch;
    uint64 Offset=Slot*S->SlotBytes;
    if(!xrtReadAtFull(S->File,Offset,B,XCG_HEAD,NULL))return XCRON_CHECKPOINT_STORE_IO;
    if(memcmp(B,"XCGS",4) || xcronCheckpointGet(B+XCG_HEAD_CRC,4)!=xrtCrc32(B,XCG_HEAD_CRC))
        return XCRON_CHECKPOINT_STORE_EMPTY;
    if(xcronCheckpointGet(B+4,4)!=1)return XCRON_CHECKPOINT_STORE_CORRUPT;
    if(xcronCheckpointGet(B+16,8)!=S->Group || xcronCheckpointGet(B+24,4)!=S->Capacity)
        return XCRON_CHECKPOINT_STORE_MISMATCH;
    uint64 G=xcronCheckpointGet(B+8,8);
    uint32 N=(uint32)xcronCheckpointGet(B+28,4);
    if((G&1)!=Slot || N>S->Capacity || (!G && N))return XCRON_CHECKPOINT_STORE_CORRUPT;
    size_t Bytes=(size_t)N*XCRON_CHECKPOINT_BYTES;
    if(Bytes && !xrtReadAtFull(S->File,Offset+XCG_HEAD,B+XCG_HEAD,Bytes,NULL))
        return XCRON_CHECKPOINT_STORE_IO;
    uint32 Crc;
    if(!xcronGroupRecords(B+XCG_HEAD,N,&Crc) || Crc!=xcronCheckpointGet(B+XCG_BODY_CRC,4))
        return XCRON_CHECKPOINT_STORE_EMPTY;
    *Generation=G;
    *Count=N;
    return XCRON_CHECKPOINT_STORE_OK;
}

static xcroncheckpointstorestatus xcronGroupRecover(xcroncheckpointgroup* S)
{
    uint64 Size;
    if(!xrtFileSize(S->File,&Size))return XCRON_CHECKPOINT_STORE_IO;
    if(Size!=2*S->SlotBytes)return XCRON_CHECKPOINT_STORE_CORRUPT;
    for(unsigned Slot=0;Slot<2;++Slot){
        uint64 Generation;
        uint32 Count;
        xcroncheckpointstorestatus Code=xcronGroupLoadSlot(S,Slot,&Generation,&Count);
        if(Code==XCRON_CHECKPOINT_STORE_EMPTY)continue;
        if(Code!=XCRON_CHECKPOINT_STORE_OK)return Code;
        uint64 Previous=S->Info.Generation;
        if(S->Info.ValidSlots && (Generation>Previous?Generation-Previous:Previous-Generation)!=1)
            return XCRON_CHECKPOINT_STORE_CORRUPT;
        if(!S->Info.ValidSlots || Generation>Previous)xcronGroupPublish(S,Generation,Count);
        ++S->Info.ValidSlots;
    }
    if(!S->Info.ValidSlots)return XCRON_CHECKPOINT_STORE_CORRUPT;
    return xrtFlush(S->File)?XCRON_CHECKPOINT_STORE_OK:XCRON_CHECKPOINT_STORE_IO;
}

XCRON_API xcroncheckpointstorestatus xcronCheckpointGroupOpen(xroot Root,cstr Path,uint64 Group,
    uint32 Capacity,bool Create,xcroncheckpointgroup** Out)
{
    if(!Root || !Path || !*Path || !Group || !Capacity || !xrtMemRangeValid(Out,sizeof(*Out)))
        return xcronGroupFail(XCRON_CHECKPOINT_STORE_ARGUMENT,XERR_ARGUMENT,"invalid checkpoint group open");
    uint64 Wire=XCG_HEAD+(uint64)Capacity*XCRON_CHECKPOINT_BYTES;
    if(Wire>(SIZE_MAX-sizeof(xcroncheckpointgroup))/2)
        return xcronGroupFail(XCRON_CHECKPOINT_STORE_LIMIT,XERR_RANGE,"checkpoint group exceeds address space");
    size_t FrameBytes=(size_t)Wire,Bytes=sizeof(xcroncheckpointgroup)+2*FrameBytes;
    xcroncheckpointgroup* S=xrtCalloc(1,Bytes);
    if(!S)return XCRON_CHECKPOINT_STORE_MEMORY;
    S->Group=Group;
    S->Capacity=Capacity;
    S->SlotBytes=((Wire+XCG_PAGE-1)/XCG_PAGE)*XCG_PAGE;
    S->Bytes=Bytes;
    S->FrameBytes=FrameBytes;
    S->Active=(uint8*)S+sizeof(*S);
    S->Scratch=S->Active+FrameBytes;

    xfileoptions Options={XFILE_READ|XFILE_WRITE,0600,XFILE_SHARE_READ|XFILE_SHARE_WRITE};
    if(Create)Options.Flags|=XFILE_CREATE|XFILE_EXCLUSIVE;
    S->File=xrtRootFileOpen(Root,Path,&Options);
    xcroncheckpointstorestatus Code=XCRON_CHECKPOINT_STORE_IO;
    if(S->File){
        if(!xrtFileTryLockExclusiveUntilClose(S->File))Code=XCRON_CHECKPOINT_STORE_LOCK;
        else if(Create){
            if(xrtFileResize(S->File,2*S->SlotBytes) && xcronGroupWrite(S,0,NULL,0,0)){
                xcronGroupPublish(S,0,0);
                S->Info.ValidSlots=1;
                Code=XCRON_CHECKPOINT_STORE_OK;
            }
        }else Code=xcronGroupRecover(S);
    }
    if(Code==XCRON_CHECKPOINT_STORE_OK){
        S->Magic=XCG_MAGIC;
        *Out=S;
        return Code;
    }
    if(S->File)(void)xrtClose(S->File);
    xrtFree(S);
    if(Code==XCRON_CHECKPOINT_STORE_CORRUPT)return xcronGroupFail(Code,XERR_VALUE,"invalid checkpoint group image");
    if(Code==XCRON_CHECKPOINT_STORE_MISMATCH)return xcronGroupFail(Code,XERR_VALUE,"checkpoint group identity or capacity mismatch");
    return Code;
}

XCRON_API xcroncheckpointstorestatus xcronCheckpointGroupRead(const xcroncheckpointgroup* S,xcroncheckpointgroupview* View)
{
    if(!xcronGroupValid(S) || !xrtMemRangeValid(View,sizeof(*View)) ||
       xrtMemRangesOverlap(View,sizeof(*View),S,S->Bytes))
        return xcronGroupFail(XCRON_CHECKPOINT_STORE_ARGUMENT,XERR_ARGUMENT,"invalid checkpoint group view");
    if(S->Uncertain)return xcronGroupFail(XCRON_CHECKPOINT_STORE_UNCERTAIN,XERR_STATE,"reopen uncertain checkpoint group");
    *View=(xcroncheckpointgroupview){
        S->Count?(const xcroncheckpoint*)(S->Active+XCG_HEAD):NULL,S->Count,S->Info
    };
    return S->Info.Generation?XCRON_CHECKPOINT_STORE_OK:XCRON_CHECKPOINT_STORE_EMPTY;
}

XCRON_API xcroncheckpointstorestatus xcronCheckpointGroupCommit(xcroncheckpointgroup* S,uint64 ExpectedGeneration,
    const xcroncheckpoint* Records,uint32 Count)
{
    if(!xcronGroupValid(S))return xcronGroupFail(XCRON_CHECKPOINT_STORE_ARGUMENT,XERR_ARGUMENT,"invalid checkpoint group");
    if(S->Uncertain)return xcronGroupFail(XCRON_CHECKPOINT_STORE_UNCERTAIN,XERR_STATE,"reopen uncertain checkpoint group");
    if(ExpectedGeneration!=S->Info.Generation)
        return xcronGroupFail(XCRON_CHECKPOINT_STORE_STALE,XERR_STATE,"stale checkpoint group generation");
    if(Count>S->Capacity || ExpectedGeneration==UINT64_MAX)
        return xcronGroupFail(XCRON_CHECKPOINT_STORE_LIMIT,XERR_RANGE,"checkpoint group capacity or generation exhausted");
    size_t Bytes=(size_t)Count*sizeof(*Records);
    if(!xrtMemRangeValid(Records,Bytes) || xrtMemRangesOverlap(Records,Bytes,S->Scratch,S->FrameBytes) ||
       xrtMemRangesOverlap(Records,Bytes,S,sizeof(*S)))
        return xcronGroupFail(XCRON_CHECKPOINT_STORE_ARGUMENT,XERR_ARGUMENT,"invalid checkpoint group input");
    uint32 Crc;
    if(!xcronGroupRecords((const uint8*)Records,Count,&Crc))
        return xcronGroupFail(XCRON_CHECKPOINT_STORE_CORRUPT,XERR_VALUE,"invalid or unordered checkpoint group records");
    uint64 Next=ExpectedGeneration+1;
    if(!xcronGroupWrite(S,Next,Records,Count,Crc)){
        S->Uncertain=true;
        return XCRON_CHECKPOINT_STORE_UNCERTAIN;
    }
    xcronGroupPublish(S,Next,Count);
    S->Info.ValidSlots=2;
    return XCRON_CHECKPOINT_STORE_OK;
}

XCRON_API xcroncheckpointstorestatus xcronCheckpointGroupClose(xcroncheckpointgroup* S)
{
    if(!xcronGroupValid(S))return xcronGroupFail(XCRON_CHECKPOINT_STORE_ARGUMENT,XERR_ARGUMENT,"invalid checkpoint group");
    S->Magic=0;
    bool Closed=xrtClose(S->File);
    xrtFree(S);
    return Closed?XCRON_CHECKPOINT_STORE_OK:XCRON_CHECKPOINT_STORE_IO;
}
#endif
