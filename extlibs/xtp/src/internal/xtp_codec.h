#ifndef XTP_INTERNAL_CODEC_H
#define XTP_INTERNAL_CODEC_H
#include <xtp/codec.h>
#include <xrt/memory.h>
#include <limits.h>

static inline uint16_t xtp__U16(const unsigned char* p)
{ return (uint16_t)((uint16_t)p[0]|((uint16_t)p[1]<<8)); }
static inline uint32_t xtp__U32(const unsigned char* p)
{ return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
static inline uint64_t xtp__U64(const unsigned char* p)
{ return (uint64_t)xtp__U32(p)|((uint64_t)xtp__U32(p+4)<<32); }
static inline void xtp__P16(unsigned char* p,uint16_t n)
{ p[0]=(unsigned char)n;p[1]=(unsigned char)(n>>8); }
static inline void xtp__P32(unsigned char* p,uint32_t n)
{ p[0]=(unsigned char)n;p[1]=(unsigned char)(n>>8);p[2]=(unsigned char)(n>>16);p[3]=(unsigned char)(n>>24); }
static inline void xtp__P64(unsigned char* p,uint64_t n)
{ xtp__P32(p,(uint32_t)n);xtp__P32(p+4,(uint32_t)(n>>32)); }
static inline bool xtp__Overlap(const void* a,size_t an,const void* b,size_t bn)
{ return xrtMemRangesOverlap(a,an,b,bn); }
static inline bool xtp__Bytes(xbytesview v) { return xrtMemRangeValid(v.Data,v.Size); }
static inline bool xtp__String(xstrview v) { return xrtMemRangeValid(v.Data,v.Size); }
static inline bool xtp__Status(int32_t s) { return s>=XTP_STATUS_DEADLINE_EXCEEDED; }
/* General envelope only; malformed inner fields can then be discarded at its
 * proven bounded frame boundary. Input has at least32 bytes. */
static inline xtpresult xtp__Envelope(const unsigned char* p,const xtpcodecconfig* c,size_t* size)
{
    uint32_t n=xtp__U32(p+4),meta=xtp__U32(p+20);uint16_t cmd=xtp__U16(p+18);
    if(memcmp(p,"xtp",3))return XTP_E_MAGIC;
    if(p[3]!=XTP_WIRE_VERSION)return XTP_E_VERSION;
    if(n<32 || (uint64_t)32+cmd+meta>n)return XTP_E_LENGTH;
    if(n>c->MaxFrameBytes || meta>c->MaxMetadataBytes || cmd>c->MaxCommandBytes)return XTP_E_LIMIT;
    *size=n;return XTP_OK;
}
xtpresult xtp__CodecConfig(const xtpcodecconfig* Input,xtpcodecconfig* Config);
/* Header/envelope already checked; command+metadata bytes must be readable.
 * Body is not touched. This lets readers discard malformed frames before
 * allocating/copying their bodies. Returned views are not delivered yet. */
xtpresult xtp__DecodePrefix(const unsigned char* Data,size_t Size,
    const xtpcodecconfig* Config,xtpframeview* Frame);
struct xtpreaderconfig;
struct xtpreader;
xtpresult xtp__ReaderConfig(const struct xtpreaderconfig* Config);
xtpresult xtp__ReaderDriveBudget(struct xtpreader* Reader,uint64_t Now,
    size_t Frames,size_t* WorkBytes,uint64_t* NextDue);
struct xtpsession;
struct xtpoutput;
bool xtp__SessionAddress(const struct xtpsession* Session,uintptr_t Address,size_t Size);
uint64_t xtp__SessionTime(const struct xtpsession* Session);
xtpresult xtp__SessionObserve(struct xtpsession* Session,uint64_t Now);
struct xtpoutputticket;
xtpresult xtp__DropNotify(struct xtpsession* Session,const struct xtpoutputticket* Ticket,xtpresult Reason,uint64_t Now);
bool xtp__RecordLane(struct xtpsession* Session,uint64_t Binding,uint32_t Lane);
xtpresult xtp__TransportWrite(struct xtpsession* Session,const struct xtpoutput* Output,
    xtpresult (*Write)(void*,const struct xtpoutput*,size_t*),void* User,size_t* Accepted);
#endif
