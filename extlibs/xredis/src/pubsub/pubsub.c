#include <xredis/pubsub.h>
#include <xrt/memory.h>
#include <string.h>
#if defined(XREDIS_FEATURE_XREDIS_PUBSUB)
static bool xredisPubSubFail(xerrkind Kind,int32 Code,cstr Text)
{xrtSetErrorInfo(Kind,"xrt.redis",Code,Text);return false;}
static size_t xredisPubSubDigits(size_t N)
{size_t D=1;while(N>=10){N/=10;++D;}return D;}
static bool xredisPubSubMeasure(xredispubsubcommand Command,const xbytesview* Names,size_t Count,size_t* Bytes,xbytesview* Word)
{
    static const xbytesview Words[]={XRT_BYTES_LITERAL("SUBSCRIBE"),XRT_BYTES_LITERAL("PSUBSCRIBE"),XRT_BYTES_LITERAL("SSUBSCRIBE"),
        XRT_BYTES_LITERAL("UNSUBSCRIBE"),XRT_BYTES_LITERAL("PUNSUBSCRIBE"),XRT_BYTES_LITERAL("SUNSUBSCRIBE"),XRT_BYTES_LITERAL("PING")};
    if((unsigned)Command>=sizeof(Words)/sizeof(*Words) || (Command<=XREDIS_PUBSUB_CMD_SSUBSCRIBE && !Count) ||
        (Command==XREDIS_PUBSUB_CMD_PING && Count>1))return xredisPubSubFail(XERR_ARGUMENT,XREDIS_PUBSUB_ARGUMENT,"invalid Pub/Sub command or cardinality");
    if(Count>XREDIS_PUBSUB_NAMES_MAX)return xredisPubSubFail(XERR_RANGE,XREDIS_PUBSUB_LIMIT,"Pub/Sub name count exceeds limit");
    /* Validate names once; report existing command input diagnostics directly.
     * A nested public size call would add an irrelevant output-alias pass. */
    if(Count && !Names)return xredisPubSubFail(XERR_ARGUMENT,XREDIS_COMMAND_ARGUMENT,"a command requires a nonempty argument vector");
    if(Count && !xrtMemRangeValid(Names,Count*sizeof(*Names)))return xredisPubSubFail(XERR_RANGE,XREDIS_COMMAND_OVERFLOW,"command argument count or descriptor span overflows");
    *Word=Words[Command];size_t Total=3+xredisPubSubDigits(Count+1)+5+xredisPubSubDigits(Word->Size)+Word->Size;
    for(size_t N=0;N<Count;++N){
        size_t Length=Names[N].Size;
        if(!Names[N].Data && Length)return xredisPubSubFail(XERR_ARGUMENT,XREDIS_COMMAND_ARGUMENT,"a nonempty command argument requires data");
        if(Length>(uint64)INT64_MAX || !xrtMemRangeValid(Names[N].Data,Length))return xredisPubSubFail(XERR_RANGE,XREDIS_COMMAND_OVERFLOW,"command argument length or address span overflows");
        size_t Overhead=5+xredisPubSubDigits(Length);
        if(Length>SIZE_MAX-Overhead || Total>SIZE_MAX-Length-Overhead)return xredisPubSubFail(XERR_RANGE,XREDIS_PUBSUB_LIMIT,"Pub/Sub encoded size overflows");
        Total+=Length+Overhead;
    }
    *Bytes=Total;return true;
}
static bool xredisPubSubSafe(const void* Output,size_t Bytes,const xbytesview* Names,size_t Count)
{
    if(!Output || !xrtMemRangeValid(Output,Bytes))return xredisPubSubFail(XERR_ARGUMENT,XREDIS_PUBSUB_ARGUMENT,"invalid Pub/Sub output span");
    if(xrtMemRangesOverlap(Output,Bytes,Names,Count*sizeof(*Names)))return xredisPubSubFail(XERR_ARGUMENT,XREDIS_PUBSUB_OVERLAP,"Pub/Sub output overlaps name descriptors");
    for(size_t N=0;N<Count;++N)if(xrtMemRangesOverlap(Output,Bytes,Names[N].Data,Names[N].Size))
        return xredisPubSubFail(XERR_ARGUMENT,XREDIS_PUBSUB_OVERLAP,"Pub/Sub output overlaps name payload");
    return true;
}
static unsigned char* xredisPubSubNumber(unsigned char* P,size_t N)
{
    unsigned char Digits[3*sizeof(size_t)];size_t Count=0;
    do{Digits[Count++]=(unsigned char)('0'+N%10);N/=10;}while(N);
    while(Count){*P++=Digits[--Count];}
    *P++='\r';*P++='\n';return P;
}
static inline unsigned char* xredisPubSubBulk(unsigned char* P,xbytesview V)
{
    *P++='$';P=xredisPubSubNumber(P,V.Size);if(V.Size){memcpy(P,V.Data,V.Size);P+=V.Size;}*P++='\r';*P++='\n';return P;
}
XREDIS_API bool xredisPubSubCommandSize(xredispubsubcommand Command,const xbytesview* Names,size_t Count,size_t* Required)
{
    size_t Bytes;xbytesview Word;
    if(!Required)return xredisPubSubFail(XERR_ARGUMENT,XREDIS_PUBSUB_ARGUMENT,"missing Pub/Sub size output");
    if(!xredisPubSubMeasure(Command,Names,Count,&Bytes,&Word) || !xredisPubSubSafe(Required,sizeof(*Required),Names,Count))return false;
    *Required=Bytes;return true;
}
XREDIS_API bool xredisPubSubCommandWrite(ptr Output,size_t Capacity,xredispubsubcommand Command,const xbytesview* Names,size_t Count,size_t* Written)
{
    size_t Bytes;xbytesview Word;
    if(!xredisPubSubMeasure(Command,Names,Count,&Bytes,&Word))return false;
    if(Capacity<Bytes)return xredisPubSubFail(XERR_RANGE,XREDIS_PUBSUB_CAPACITY,"Pub/Sub output capacity is insufficient");
    if(!xredisPubSubSafe(Output,Bytes,Names,Count))return false;
    if(Written && (!xredisPubSubSafe(Written,sizeof(*Written),Names,Count) || xrtMemRangesOverlap(Output,Bytes,Written,sizeof(*Written)))){
        if(xrtMemRangesOverlap(Output,Bytes,Written,sizeof(*Written)))xredisPubSubFail(XERR_ARGUMENT,XREDIS_PUBSUB_OVERLAP,"Pub/Sub outputs overlap");
        return false;
    }
    unsigned char* P=Output;*P++='*';P=xredisPubSubNumber(P,Count+1);P=xredisPubSubBulk(P,Word);
    for(size_t N=0;N<Count;++N)P=xredisPubSubBulk(P,Names[N]);
    if(Written)*Written=Bytes;
    return true;
}
#endif
