#include "checkpoint_internal.h"
#include <xrt/crc32.h>
#include <string.h>

#if defined(XCRON_FEATURE_XCRON_CHECKPOINT)
enum { XCC_VERSION=1, XCC_JOB=8, XCC_CONFIG=24, XCC_NEXT=40, XCC_POLICY=122,
       XCC_COUNTS=128, XCC_CRC=176 };
_Static_assert(XCC_CRC+4==XCRON_CHECKPOINT_BYTES && XCRON_FIELD_COUNT==6,"checkpoint v1 field layout");
_Static_assert(XCRON_JOB_SCHEDULED==0 && XCRON_JOB_EXHAUSTED==1 && XCRON_JOB_CANCELLED==2,
               "checkpoint v1 job states");

static void xcronCheckpointPut(uint8* Out,uint64 Value,size_t Bytes)
{
    for(size_t I=0;I<Bytes;++I){Out[I]=(uint8)Value;Value>>=8;}
}
static uint64 xcronCheckpointGet(const uint8* In,size_t Bytes)
{
    uint64 Value=0;
    for(size_t I=Bytes;I;--I)Value=(Value<<8)|In[I-1];
    return Value;
}
static xtime xcronCheckpointTime(const uint8* In)
{
    uint64 Value=xcronCheckpointGet(In,8);
    return Value<=INT64_MAX?(xtime)Value:INT64_MIN+(xtime)(Value-(UINT64_C(1)<<63));
}
static void xcronCheckpointConfig(uint8* Out,const xcronjobconfig* Config,xtime Next,xcrondeliverypolicy Policy)
{
    xcronCheckpointPut(Out,(uint64)Config->Begin,8);
    xcronCheckpointPut(Out+8,(uint64)Config->End,8);
    xcronCheckpointPut(Out+16,(uint64)Next,8);
    xcronCheckpointPut(Out+24,Config->GraceMilliseconds,8);
    for(size_t I=0;I<XCRON_FIELD_COUNT;++I)xcronCheckpointPut(Out+32+8*I,Config->Expression.Fields[I],8);
    xcronCheckpointPut(Out+80,Config->Expression.Flags,4);
    xcronCheckpointPut(Out+84,(uint32)Config->Expression.Dialect,4);
    xcronCheckpointPut(Out+88,Config->SearchFlags,4);
    xcronCheckpointPut(Out+92,Config->RunningLimit,4);
    Out[96]=(uint8)Config->Misfire;Out[97]=(uint8)Config->Overlap;
    Out[98]=(uint8)Policy.Fold;Out[99]=(uint8)Policy.Missing;
    memset(Out+100,0,4);
}
void xcronCheckpointEncode(const xcronjobconfig* Config,const xcronjobinfo* Info,bool Selected,
    xcrondeliverypolicy Policy,xcroncheckpointkey Key,xcroncheckpoint* Out)
{
    uint8* B=Out->Bytes;
    memcpy(B,"XCCP",4);
    B[4]=XCC_VERSION;B[5]=Selected;B[6]=(uint8)Info->State;B[7]=0;
    xcronCheckpointPut(B+XCC_JOB,Key.Job,8);
    xcronCheckpointPut(B+XCC_JOB+8,Key.Revision,8);
    xcronCheckpointConfig(B+XCC_CONFIG,Config,Info->HasNext?Info->Next:0,Policy);
    const uint64 Counts[]={Info->Dispatched,Info->Succeeded,Info->Failed,Info->Cancelled,Info->SkippedRanges,Info->CoalescedRanges};
    for(size_t I=0;I<sizeof(Counts)/sizeof(*Counts);++I)xcronCheckpointPut(B+XCC_COUNTS+8*I,Counts[I],8);
    xcronCheckpointPut(B+XCC_CRC,xrtCrc32(B,XCC_CRC),4);
}
static uint64 xcronCheckpointAdd(uint64 A,uint64 B)
{return B>UINT64_MAX-A?UINT64_MAX:A+B;}

xcroncheckpointstatus xcronCheckpointDecode(xbytesview Wire,const xcronjobconfig* Config,
    bool Selected,xcroncheckpointkey Key,xcronjobinfo* Info,xcrondeliverypolicy* Policy)
{
    const uint8* B=Wire.Data;
    if(Wire.Size!=XCRON_CHECKPOINT_BYTES || memcmp(B,"XCCP",4) || B[4]!=XCC_VERSION ||
       B[5]>1 || B[7] || B[6]>XCRON_JOB_CANCELLED || xcronCheckpointGet(B+XCC_CRC,4)!=xrtCrc32(B,XCC_CRC) ||
       B[XCC_POLICY]>XCRON_FOLD_REJECT || B[XCC_POLICY+1]>XCRON_MISSING_REJECT ||
       (!B[5] && (B[XCC_POLICY] || B[XCC_POLICY+1])))return XCRON_CHECKPOINT_INVALID;
    if(B[5]!=(uint8)Selected || xcronCheckpointGet(B+XCC_JOB,8)!=Key.Job ||
       xcronCheckpointGet(B+XCC_JOB+8,8)!=Key.Revision)return XCRON_CHECKPOINT_INCOMPATIBLE;
    bool Scheduled=B[6]==XCRON_JOB_SCHEDULED;
    xtime Next=xcronCheckpointTime(B+XCC_NEXT);
    xcrondeliverypolicy Saved={(xcronfoldpolicy)B[XCC_POLICY],(xcronmissingpolicy)B[XCC_POLICY+1]};
    uint8 Expected[XCC_COUNTS-XCC_CONFIG];
    xcronCheckpointConfig(Expected,Config,Next,Saved);
    if(memcmp(Expected,B+XCC_CONFIG,sizeof(Expected)))return XCRON_CHECKPOINT_INCOMPATIBLE;
    if(Scheduled?(Next<Config->Begin || Next>Config->End ||
                  (Next==Config->Begin && Config->SearchFlags==XCRON_SEARCH_EXCLUSIVE)):Next!=0)
        return XCRON_CHECKPOINT_INVALID;
    uint64 Counts[6];
    for(size_t I=0;I<sizeof(Counts)/sizeof(*Counts);++I)Counts[I]=xcronCheckpointGet(B+XCC_COUNTS+8*I,8);
    if(Counts[0]!=xcronCheckpointAdd(xcronCheckpointAdd(Counts[1],Counts[2]),Counts[3]) || Counts[5]>Counts[0])
        return XCRON_CHECKPOINT_INVALID;
    *Info=(xcronjobinfo){.State=(xcronjobstate)B[6],.HasNext=Scheduled,.Next=Next,
        .Dispatched=Counts[0],.Succeeded=Counts[1],.Failed=Counts[2],.Cancelled=Counts[3],
        .SkippedRanges=Counts[4],.CoalescedRanges=Counts[5],.LastIssue={.Field=XCRON_FIELD_NONE}};
    *Policy=Saved;
    return XCRON_CHECKPOINT_VALID;
}
#endif
