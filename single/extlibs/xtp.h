/*
 * MIT License
 *
 * Copyright (c) 2025 xLeaves [xywhsoft] <xywhsoft@qq.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* 此文件由 tools/amalgamate.py 生成，请勿直接修改。 */
/* Supply XRT and selected extension dependencies before this header. */
#if !defined(XRT_CORE_H)
#error "xtp requires XRT; include xrt.h or xrt_decl.h first"
#endif
#if defined(XTP_IMPLEMENTATION) && defined(_MSC_VER) && \
	!defined(_CRT_SECURE_NO_WARNINGS)
	#define _CRT_SECURE_NO_WARNINGS
#endif
#if defined(XTP_IMPLEMENTATION) && \
	!defined(_WIN32) && !defined(_WIN64)
	#if defined(__linux__) && !defined(_GNU_SOURCE)
		#define _GNU_SOURCE 1
	#endif
	#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
		#define _DARWIN_C_SOURCE 1
	#endif
	#if !defined(_POSIX_C_SOURCE)
		#define _POSIX_C_SOURCE 200809L
	#endif
	#if !defined(_FILE_OFFSET_BITS)
		#define _FILE_OFFSET_BITS 64
	#endif
#endif
#ifndef XTP_SINGLE_HEADER_H
#define XTP_SINGLE_HEADER_H
#define XTP_SINGLE_HEADER 1


/* ========================================================================== */
/* feature selection: extlibs/xtp/include/xtp/features.h */
/* ========================================================================== */

/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XTP_FEATURES_H
#define XTP_FEATURES_H

/* xtp 及其直接依赖。 */
#if defined(XTP_MODULE_ALL) || defined(XTP_MODULE_XTP)
#ifndef XTP_FEATURE_XTP
#define XTP_FEATURE_XTP
#endif
#ifndef XTP_MODULE_XTP_CODEC
#define XTP_MODULE_XTP_CODEC
#endif
#ifndef XTP_MODULE_XTP_READER
#define XTP_MODULE_XTP_READER
#endif
#endif

/* xtp_reader 及其直接依赖。 */
#if defined(XTP_MODULE_ALL) || defined(XTP_MODULE_XTP_READER)
#ifndef XTP_FEATURE_XTP_READER
#define XTP_FEATURE_XTP_READER
#endif
#ifndef XTP_MODULE_XTP_CODEC
#define XTP_MODULE_XTP_CODEC
#endif
#endif

/* xtp_codec 及其直接依赖。 */
#if defined(XTP_MODULE_ALL) || defined(XTP_MODULE_XTP_CODEC)
#ifndef XTP_FEATURE_XTP_CODEC
#define XTP_FEATURE_XTP_CODEC
#endif
#endif

#endif /* XTP_FEATURES_H */


/* ========================================================================== */
/* public: extlibs/xtp/include/xtp/export.h */
/* ========================================================================== */

#ifndef XTP_EXPORT_H
#define XTP_EXPORT_H
#if defined(_WIN32) || defined(_WIN64)
 #if defined(XTP_BUILD_SHARED)
  #define XTP_API __declspec(dllexport)
 #elif defined(XTP_USE_SHARED)
  #define XTP_API __declspec(dllimport)
 #else
  #define XTP_API
 #endif
#elif defined(__GNUC__) || defined(__clang__)
 #if defined(XTP_BUILD_SHARED)
  #define XTP_API __attribute__((visibility("default")))
 #else
  #define XTP_API
 #endif
#else
 #define XTP_API
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xtp/include/xtp/types.h */
/* ========================================================================== */

#ifndef XTP_TYPES_H
#define XTP_TYPES_H
#include <string.h>

#define XTP_HEADER_SIZE 32u
#define XTP_WIRE_VERSION 3u
#define XTP_PARAMS_MAX 256u
#define XTP_BATCH_MAX 64u
#define XTP_FLAG_ROUTE_ID 0x01u
#define XTP_FLAG_PARAM_ARRAY 0x02u
#define XTP_FLAG_RESULT_ACK 0x04u
#define XTP_FLAG_REPLAYABLE 0x08u

typedef enum xtpcommandkind { XTP_COMMAND_NONE=0, XTP_COMMAND_ID, XTP_COMMAND_NAME } xtpcommandkind;
typedef struct xtpcommand {
    xtpcommandkind Kind;
    union { uint32_t ID; xstrview Name; } As;
} xtpcommand;
typedef enum xtpparamkind { XTP_PARAMS_TABLE=0, XTP_PARAMS_ARRAY } xtpparamkind;
typedef struct xtpfield { xstrview Key; xbytesview Value; } xtpfield;
typedef struct xtpparams {
    xtpparamkind Kind;
    size_t Count;
    union { const xtpfield* Table; const xbytesview* Array; } As;
} xtpparams;
/* Library-produced borrowed view only. Do not alter _Wire/Count/Kind. */
typedef struct xtpparamsview { xtpparamkind Kind; size_t Count; xbytesview _Wire; } xtpparamsview;
typedef enum xtpmessagekind { XTP_REQUEST=1, XTP_RESPONSE, XTP_PUSH, XTP_ACK, XTP_NOTIFY } xtpmessagekind;
typedef enum xtpstatus {
    XTP_STATUS_OK=0, XTP_STATUS_BUSY=-1, XTP_STATUS_REJECTED=-2,
    XTP_STATUS_UNSUPPORTED=-3, XTP_STATUS_BAD_ARGUMENT=-4,
    XTP_STATUS_RESULT_TOO_LARGE=-5, XTP_STATUS_INTERNAL=-6,
    XTP_STATUS_NOT_FOUND=-7, XTP_STATUS_STATE_LOST=-8,
    XTP_STATUS_DEADLINE_EXCEEDED=-9
} xtpstatus;
typedef enum xtpresult {
    XTP_OK=0, XTP_AGAIN=1, XTP_CLOSED=2, XTP_MORE=3, XTP_MESSAGE=4,
    XTP_DROPPED=5, XTP_PAUSED=6, XTP_END=7,
    XTP_E_ARGUMENT=-1, XTP_E_LIMIT=-2, XTP_E_MEMORY=-3, XTP_E_STATE=-4,
    XTP_E_MAGIC=-5, XTP_E_VERSION=-6, XTP_E_TYPE=-7, XTP_E_LENGTH=-8,
    XTP_E_VALUE=-9, XTP_E_TRUNCATED=-10, XTP_E_CAPACITY=-11, XTP_E_DEADLINE=-12
} xtpresult;
/* Session submission descriptions; all contents borrow until the call returns. */
typedef struct xtpmessage {
    xtpmessagekind Type; xtpcommand Command; xtpparams Params; xbytesview Body;
} xtpmessage;
typedef struct xtpresponse { int32_t Status; xtpparams Params; xbytesview Body; } xtpresponse;
/* Low-level codec descriptor. Session users do not assign MessageID/flags. */
typedef struct xtppacket {
    uint64_t MessageID; xtpmessagekind Type; xtpcommand Command;
    xtpparams Params; xbytesview Body; int32_t Status;
    bool Replayable, ResultAck;
} xtppacket;
typedef struct xtpframeview {
    uint64_t MessageID; xtpmessagekind Type; int32_t Status;
    xtpcommand Command; xtpparamsview Params; xbytesview Body, Packet;
    bool Replayable, ResultAck;
} xtpframeview;
typedef enum xtpparamresult {
    XTP_PARAM_FOUND=1, XTP_PARAM_MISSING=0, XTP_PARAM_WRONG_MODE=-1, XTP_PARAM_ARGUMENT=-2
} xtpparamresult;
typedef struct xtpparamiter { const xtpparamsview* _Params; size_t _Next; } xtpparamiter;
typedef struct xtpparamentry { size_t Index; bool HasKey; xstrview Key; xbytesview Value; } xtpparamentry;

static inline xtpcommand xtpCmdID(uint32_t ID)
{ xtpcommand c={XTP_COMMAND_ID,{0}}; c.As.ID=ID; return c; }
static inline xtpcommand xtpCmdName(xstrview Name)
{ xtpcommand c={XTP_COMMAND_NAME,{0}}; c.As.Name=Name; return c; }
static inline xtpparams xtpArray(const xbytesview* Items,size_t Count)
{ xtpparams p={XTP_PARAMS_ARRAY,0,{NULL}}; p.Count=Count; p.As.Array=Items; return p; }
static inline xtpparams xtpTable(const xtpfield* Items,size_t Count)
{ xtpparams p={XTP_PARAMS_TABLE,0,{NULL}}; p.Count=Count; p.As.Table=Items; return p; }
static inline bool xtpCommandIsName(const xtpcommand* Command,xstrview Name)
{
    return Command && Command->Kind==XTP_COMMAND_NAME && Command->As.Name.Size==Name.Size &&
        (!Name.Size || (Name.Data && Command->As.Name.Data && !memcmp(Command->As.Name.Data,Name.Data,Name.Size)));
}
#endif


/* ========================================================================== */
/* public: extlibs/xtp/include/xtp/codec.h */
/* ========================================================================== */

#ifndef XTP_CODEC_H
#define XTP_CODEC_H
#if defined(XTP_FEATURE_XTP_CODEC)
typedef struct xtpcodecconfig {
    size_t MaxFrameBytes, MaxMetadataBytes, MaxCommandBytes, MaxParams, MaxKeyBytes, MaxBatch;
} xtpcodecconfig;
XRT_EXTERN_C_BEGIN
/* Defaults: 1MiB frame,16KiB metadata,256 command bytes,64 params,64 key bytes,
 * 64 batch items. Non-NULL config fields are literal limits; zero is not unlimited.
 * Frame>=32 and <=UINT32_MAX; params<=256,batch in1..64,wire field widths apply. */
XTP_API void xtpCodecConfigInit(xtpcodecconfig* Config);
/* Synchronous, zero allocation; NULL Config selects defaults. Inputs must stay
 * stable/readable. Required/Written and written bytes must not overlap sources,
 * descriptors, config, or each other. Every failure preserves all outputs.
 * Encode preflights the ENTIRE batch before touching output; table order stays
 * unchanged. No implicit NUL, value conversion, hashing of Body, or sorting fields. */
XTP_API xtpresult xtpEncodeSize(const xtppacket* Packets,size_t Count,
    const xtpcodecconfig* Config,size_t* Required);
XTP_API xtpresult xtpEncode(const xtppacket* Packets,size_t Count,
    const xtpcodecconfig* Config,void* Output,size_t Capacity,size_t* Written);
/* Exactly one complete frame, borrowed until caller changes/frees Input.
 * No allocation/copy. Short input=TRUNCATED,extra bytes=LENGTH. Failure preserves
 * Frame; Frame/config/input must be disjoint. All fields validated before success. */
XTP_API xtpresult xtpDecode(xbytesview Input,const xtpcodecconfig* Config,xtpframeview* Frame);
/* Accept only live library-produced views. At supports BOTH array and table in
 * transmitted order,O(1),without comparing keys. Find is a linear table lookup;
 * array returns WRONG_MODE. FOUND may contain an empty value. Non-FOUND zeroes
 * a valid disjoint output; invalid/overlapping output preserves bytes. No rescans. */
XTP_API xtpparamresult xtpParamAt(const xtpparamsview* Params,size_t Index,xbytesview* Value);
XTP_API xtpparamresult xtpParamFind(const xtpparamsview* Params,xstrview Key,xbytesview* Value);
XTP_API xtpparamiter xtpParams(const xtpparamsview* Params);
XTP_API xtpparamresult xtpParamNext(xtpparamiter* Iter,xtpparamentry* Entry);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xtp/include/xtp/reader.h */
/* ========================================================================== */

#ifndef XTP_READER_H
#define XTP_READER_H
#if defined(XTP_FEATURE_XTP_READER)
typedef struct xtpreader xtpreader;
typedef enum xtpinputmode { XTP_INPUT_STREAM=0,XTP_INPUT_RECORD=1 } xtpinputmode;
typedef enum xtpflow { XTP_FLOW_ACCEPT=1,XTP_FLOW_PAUSE=2,XTP_FLOW_STOP=3 } xtpflow;
/* Normal realloc alignment/preservation; Size=0 frees and returns NULL.
 * Context/code live until Destroy. NULL selects public XRT memory functions. */
typedef void* (*xtpreallocproc)(void* User,void* Memory,size_t Size);
typedef struct xtpreaderconfig {
    xtpcodecconfig Codec;xtpinputmode Mode;
    size_t RetainBytes,MaxStorageBytes,MaxWorkBytes,MaxFrames;
    uint32_t MaxConsecutiveBad,MaxBad;
    uint64_t ReceiveTimeoutUs;
    xtpreallocproc Realloc;void* AllocatorUser;
} xtpreaderconfig;
typedef struct xtpreaderevents {
    /* Borrow valid only in callback. No recursive Input/End/Reset/Destroy.
     * PAUSE means no application effects; complete borrowed frame remains in
     * the caller's unconsumed suffix. Buffered paused frames remain in reader.
     * ACCEPT consumes exactly that frame; STOP consumes it and closes reader. */
    xtpflow (*Frame)(xtpreader*,const xtpframeview*,void* User);
    void (*Drop)(xtpreader*,xtpresult Reason,void* User);
} xtpreaderevents;
typedef struct xtpreadprogress {size_t Used,Frames,Dropped,WorkBytes;} xtpreadprogress;
typedef struct xtpreaderinfo {
    size_t BufferedBytes,Capacity,StorageBytes,PeakReservedBytes;
    uint64_t Frames,Dropped,NextDeadlineUs;
    uint32_t ConsecutiveBad;
    xtpresult Failure;bool Paused,Ended,DiscardingRecord;
} xtpreaderinfo;
XRT_EXTERN_C_BEGIN
/* Default STREAM;64KiB retention,3MiB storage,256KiB work,64 frames,
 * 8 consecutive/64 total bad frames,5s absolute frame receive deadline. */
XTP_API void xtpReaderConfigInit(xtpreaderconfig* Config);
/* Copies config/events. Only Create and fragmented-buffer growth allocate.
 * No socket/thread/timer/global state. One owner; failure preserves output. */
XTP_API xtpresult xtpReaderCreate(const xtpreaderconfig* Config,
    const xtpreaderevents* Events,void* User,xtpreader** Reader);
XTP_API xtpresult xtpReaderDestroy(xtpreader* Reader);
/* Reports Used on every valid call including a failing prefix. Progress itself
 * must not overlap input/reader storage. Input must not alias internal buffers.
 * Empty input is NOT EOF. RECORD End applies only after this input was consumed;
 * after AGAIN/PAUSED retry its unconsumed suffix with the same End flag.
 * OK=boundary,MORE=partial,AGAIN=work budget,PAUSED=explicit resume required,
 * DROPPED=recoverable record/frame discard. Fatal errors stick until Reset.
 * Budget counts copies,discarded bytes and prefix validation,not opaque borrowed
 * Body bytes. No unchecked magic search or body allocation for invalid prefixes. */
XTP_API xtpresult xtpReaderInput(xtpreader* Reader,xbytesview Input,bool RecordEnd,
    uint64_t NowUs,xtpreadprogress* Progress);
XTP_API xtpresult xtpReaderResume(xtpreader* Reader);
/* End is receive EOF; partial STREAM is sticky TRUNCATED. EndRecord is supplied
 * to Input, not End. Reset is allowed only at a trusted new stream/boundary. */
XTP_API xtpresult xtpReaderEnd(xtpreader* Reader,uint64_t NowUs);
XTP_API xtpresult xtpReaderReset(xtpreader* Reader);
/* Drives deadlines and internally buffered complete frames without new network
 * bytes. AGAIN requests another bounded turn;0 NextDue means no timed deadline.
 * A borrowed paused suffix still has to be supplied by the caller after Resume. */
XTP_API xtpresult xtpReaderDrive(xtpreader* Reader,uint64_t NowUs,uint64_t* NextDue);
XTP_API xtpresult xtpReaderInfo(const xtpreader* Reader,xtpreaderinfo* Info);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xtp/include/xtp.h */
/* ========================================================================== */

#ifndef XTP_H
#define XTP_H
#endif

#endif

#if defined(XTP_IMPLEMENTATION) && !defined(XTP_IMPLEMENTATION_ONCE)
#define XTP_IMPLEMENTATION_ONCE 1


/* ========================================================================== */
/* internal: extlibs/xtp/src/internal/xtp_codec.h */
/* ========================================================================== */

#if defined(XTP_FEATURE_XTP_CODEC)
#ifndef XTP_INTERNAL_CODEC_H
#define XTP_INTERNAL_CODEC_H
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
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xtp/src/codec/codec.c */
/* ========================================================================== */

#if defined(XTP_FEATURE_XTP_CODEC)
#if defined(XTP_FEATURE_XTP_CODEC)

XTP_API void xtpCodecConfigInit(xtpcodecconfig* c)
{
    if(c)*c=(xtpcodecconfig){1048576u,16384u,256u,64u,64u,64u};
}
xtpresult xtp__CodecConfig(const xtpcodecconfig* input,xtpcodecconfig* c)
{
    if(input){if(!xrtMemRangeValid(input,sizeof(*input)))return XTP_E_ARGUMENT;*c=*input;}
    else xtpCodecConfigInit(c);
    if(c->MaxFrameBytes<32 || c->MaxFrameBytes>UINT32_MAX || c->MaxMetadataBytes>UINT32_MAX ||
        c->MaxCommandBytes>UINT16_MAX || c->MaxParams>XTP_PARAMS_MAX ||
        c->MaxKeyBytes>UINT16_MAX || !c->MaxBatch || c->MaxBatch>XTP_BATCH_MAX)return XTP_E_ARGUMENT;
    return XTP_OK;
}
static bool xtpKeyValid(xstrview key)
{
    if(!key.Size)return false;
    for(size_t i=0;i<key.Size;++i){unsigned char x=(unsigned char)key.Data[i];
        if(!((x>='a'&&x<='z')||(x>='0'&&x<='9')||x=='_'||x=='.'||x=='-'))return false;}
    return true;
}
static bool xtpCommandValid(xstrview name)
{
    if(!name.Size)return false;
    for(size_t i=0;i<name.Size;++i)if((unsigned char)name.Data[i]<0x21 || (unsigned char)name.Data[i]>0x7e)return false;
    return true;
}
/* A stack-only key source for duplicate validation. Fields are NEVER reordered.
 * Index heap sorting bounds comparison count even for adversarial input order. */
typedef struct xtpkeys { const xtpfield* Fields;const unsigned char* Table;const unsigned char* Data; } xtpkeys;
static xstrview xtpKey(const xtpkeys* k,size_t i)
{
    if(k->Fields)return k->Fields[i].Key;
    size_t start=i?xtp__U32(k->Table+(i-1)*6+2):0;
    return (xstrview){(const char*)k->Data+start,xtp__U16(k->Table+i*6)};
}
static int xtpKeyCompare(const xtpkeys* k,size_t a,size_t b)
{
    xstrview x=xtpKey(k,a),y=xtpKey(k,b);size_t n=x.Size<y.Size?x.Size:y.Size;
    int r=memcmp(x.Data,y.Data,n);return r?r:(x.Size>y.Size)-(x.Size<y.Size);
}
static void xtpKeySift(const xtpkeys* k,uint16_t* index,size_t root,size_t n)
{
    for(;;){size_t child=root*2+1;if(child>=n)return;
        if(child+1<n && xtpKeyCompare(k,index[child],index[child+1])<0)++child;
        if(xtpKeyCompare(k,index[root],index[child])>=0)return;
        uint16_t t=index[root];index[root]=index[child];index[child]=t;root=child;}
}
static bool xtpUnique(const xtpkeys* k,size_t n)
{
    uint16_t index[XTP_PARAMS_MAX];
    for(size_t i=0;i<n;++i)index[i]=(uint16_t)i;
    for(size_t i=n/2;i>0;--i)xtpKeySift(k,index,i-1,n);
    for(size_t i=n;i>1;--i){uint16_t t=index[0];index[0]=index[i-1];index[i-1]=t;xtpKeySift(k,index,0,i-1);}
    for(size_t i=1;i<n;++i)if(!xtpKeyCompare(k,index[i-1],index[i]))return false;
    return true;
}
static xtpresult xtpFields(xtpmessagekind type,uint64_t id,xtpcommandkind route,
    size_t cmd,uint32_t command,size_t params,xtpparamkind pk,size_t body,int32_t status,bool replay,bool ack)
{
    bool routed=type==XTP_REQUEST||type==XTP_PUSH||type==XTP_NOTIFY;
    if(type<XTP_REQUEST||type>XTP_NOTIFY)return XTP_E_TYPE;
    if(pk!=XTP_PARAMS_ARRAY&&pk!=XTP_PARAMS_TABLE)return XTP_E_VALUE;
    if(!xtp__Status(status))return XTP_E_VALUE;
    if(type==XTP_NOTIFY ? id!=0 : id==0)return XTP_E_VALUE;
    if(routed){
        if(status || (route!=XTP_COMMAND_ID&&route!=XTP_COMMAND_NAME))return XTP_E_VALUE;
        if(route==XTP_COMMAND_ID ? cmd!=0 : (!cmd||command!=0))return XTP_E_VALUE;
    }else if(route!=XTP_COMMAND_NONE||cmd||command)return XTP_E_VALUE;
    if(replay && type!=XTP_REQUEST && type!=XTP_PUSH)return XTP_E_VALUE;
    if(ack && (type!=XTP_ACK||status))return XTP_E_VALUE;
    if(type==XTP_ACK && (params||pk!=XTP_PARAMS_TABLE||body||status==XTP_STATUS_DEADLINE_EXCEEDED))return XTP_E_VALUE;
    return XTP_OK;
}
typedef struct xtpmeasure {uint32_t Size,Meta;uint16_t Command;} xtpmeasure;
static xtpresult xtpMeasure(const xtppacket* p,const xtpcodecconfig* c,xtpmeasure* f,
    const void* out,size_t outn,const void* scalar,size_t scalarn)
{
    size_t cmd=p->Command.Kind==XTP_COMMAND_NAME?p->Command.As.Name.Size:0;
    uint32_t id=p->Command.Kind==XTP_COMMAND_ID?p->Command.As.ID:0;
    xtpresult r=xtpFields(p->Type,p->MessageID,p->Command.Kind,cmd,id,p->Params.Count,p->Params.Kind,
        p->Body.Size,p->Status,p->Replayable,p->ResultAck);
    if(r!=XTP_OK)return r;
    if(cmd>UINT16_MAX||p->Body.Size>UINT32_MAX||p->Params.Count>c->MaxParams||cmd>c->MaxCommandBytes)return XTP_E_LIMIT;
    if(!xtp__Bytes(p->Body))return XTP_E_ARGUMENT;
    const void* desc=p->Params.Kind==XTP_PARAMS_ARRAY?(const void*)p->Params.As.Array:(const void*)p->Params.As.Table;
    size_t dn=p->Params.Count*(p->Params.Kind==XTP_PARAMS_ARRAY?sizeof(xbytesview):sizeof(xtpfield));
    if(!xrtMemRangeValid(desc,dn))return XTP_E_ARGUMENT;
    if(xtp__Overlap(out,outn,desc,dn)||xtp__Overlap(scalar,scalarn,desc,dn)||
       xtp__Overlap(out,outn,p->Body.Data,p->Body.Size)||xtp__Overlap(scalar,scalarn,p->Body.Data,p->Body.Size))return XTP_E_ARGUMENT;
    if(cmd){xstrview s=p->Command.As.Name;if(!xtp__String(s))return XTP_E_ARGUMENT;
        if(xtp__Overlap(out,outn,s.Data,s.Size)||xtp__Overlap(scalar,scalarn,s.Data,s.Size))return XTP_E_ARGUMENT;
        if(!xtpCommandValid(s))return XTP_E_VALUE;}
    uint64_t meta=p->Params.Count?2+p->Params.Count*(p->Params.Kind==XTP_PARAMS_ARRAY?4u:6u):0;
    for(size_t i=0;i<p->Params.Count;++i){
        xbytesview v=p->Params.Kind==XTP_PARAMS_ARRAY?p->Params.As.Array[i]:p->Params.As.Table[i].Value;
        if(!xtp__Bytes(v))return XTP_E_ARGUMENT;
        if(v.Size>UINT32_MAX||xtp__Overlap(out,outn,v.Data,v.Size)||xtp__Overlap(scalar,scalarn,v.Data,v.Size))return v.Size>UINT32_MAX?XTP_E_LIMIT:XTP_E_ARGUMENT;
        meta+=v.Size;
        if(p->Params.Kind==XTP_PARAMS_TABLE){xstrview k=p->Params.As.Table[i].Key;
            if(k.Size>c->MaxKeyBytes)return XTP_E_LIMIT;
            if(!xtp__String(k)||xtp__Overlap(out,outn,k.Data,k.Size)||xtp__Overlap(scalar,scalarn,k.Data,k.Size))return XTP_E_ARGUMENT;
            if(!xtpKeyValid(k))return XTP_E_VALUE;
            meta+=k.Size;}
        if(meta>c->MaxMetadataBytes)return XTP_E_LIMIT;
    }
    if(meta>c->MaxMetadataBytes || (uint64_t)32+cmd+meta+p->Body.Size>c->MaxFrameBytes)return XTP_E_LIMIT;
    if(p->Params.Kind==XTP_PARAMS_TABLE && p->Params.Count>1){xtpkeys k={p->Params.As.Table,NULL,NULL};
        if(!xtpUnique(&k,p->Params.Count))return XTP_E_VALUE;}
    f->Command=(uint16_t)cmd;f->Meta=(uint32_t)meta;f->Size=(uint32_t)(32+cmd+meta+p->Body.Size);return XTP_OK;
}
static xtpresult xtpPreflight(const xtppacket* p,size_t n,const xtpcodecconfig* input,
    xtpmeasure* frames,size_t* total,const void* out,size_t outn,const void* scalar,size_t scalarn)
{
    xtpcodecconfig c;xtpresult r=xtp__CodecConfig(input,&c);if(r!=XTP_OK)return r;
    if(!n||n>c.MaxBatch||!xrtMemRangeValid(p,n*sizeof(*p)))return XTP_E_ARGUMENT;
    if(!xrtMemRangeValid(scalar,scalarn)||!xrtMemRangeValid(out,outn))return XTP_E_ARGUMENT;
    if(xtp__Overlap(scalar,scalarn,p,n*sizeof(*p))||xtp__Overlap(out,outn,p,n*sizeof(*p))||
       xtp__Overlap(out,outn,scalar,scalarn)||xtp__Overlap(scalar,scalarn,input,input?sizeof(*input):0)||
       xtp__Overlap(out,outn,input,input?sizeof(*input):0))return XTP_E_ARGUMENT;
    uint64_t size=0;
    for(size_t i=0;i<n;++i){r=xtpMeasure(p+i,&c,frames+i,out,outn,scalar,scalarn);if(r!=XTP_OK)return r;
        size+=frames[i].Size;if(size>SIZE_MAX)return XTP_E_LIMIT;}
    *total=(size_t)size;return XTP_OK;
}
XTP_API xtpresult xtpEncodeSize(const xtppacket* p,size_t n,const xtpcodecconfig* c,size_t* required)
{
    xtpmeasure frames[XTP_BATCH_MAX];size_t size;
    if(!required)return XTP_E_ARGUMENT;
    xtpresult r=xtpPreflight(p,n,c,frames,&size,NULL,0,required,sizeof(*required));
    if(r==XTP_OK)*required=size;
    return r;
}
static void xtpCopy(unsigned char* out,const void* data,size_t n) {if(n)memcpy(out,data,n);}
static void xtpEmit(const xtppacket* p,const xtpmeasure* f,unsigned char* out)
{
    memcpy(out,"xtp\3",4);xtp__P32(out+4,f->Size);xtp__P64(out+8,p->MessageID);out[16]=(unsigned char)p->Type;
    out[17]=(unsigned char)((p->Command.Kind==XTP_COMMAND_ID?1:0)|(p->Params.Kind==XTP_PARAMS_ARRAY?2:0)|
        (p->ResultAck?4:0)|(p->Replayable?8:0));
    xtp__P16(out+18,f->Command);xtp__P32(out+20,f->Meta);xtp__P32(out+24,(uint32_t)p->Status);
    xtp__P32(out+28,p->Command.Kind==XTP_COMMAND_ID?p->Command.As.ID:0);
    if(f->Command)xtpCopy(out+32,p->Command.As.Name.Data,f->Command);
    unsigned char* m=out+32+f->Command;size_t count=p->Params.Count;
    if(count){size_t stride=p->Params.Kind==XTP_PARAMS_ARRAY?4:6,end=0;unsigned char* data=m+2+stride*count;xtp__P16(m,(uint16_t)count);
        for(size_t i=0;i<count;++i){xbytesview v=p->Params.Kind==XTP_PARAMS_ARRAY?p->Params.As.Array[i]:p->Params.As.Table[i].Value;
            if(p->Params.Kind==XTP_PARAMS_TABLE){xstrview k=p->Params.As.Table[i].Key;xtp__P16(m+2+i*6,(uint16_t)k.Size);
                xtpCopy(data+end,k.Data,k.Size);end+=k.Size;}
            xtpCopy(data+end,v.Data,v.Size);end+=v.Size;xtp__P32(m+2+i*stride+(stride==6?2:0),(uint32_t)end);}}
    xtpCopy(out+32+f->Command+f->Meta,p->Body.Data,p->Body.Size);
}
XTP_API xtpresult xtpEncode(const xtppacket* p,size_t n,const xtpcodecconfig* c,void* output,size_t capacity,size_t* written)
{
    xtpmeasure frames[XTP_BATCH_MAX];size_t size;
    if(!written||!output)return XTP_E_ARGUMENT;
    xtpresult r=xtpPreflight(p,n,c,frames,&size,output,capacity,written,sizeof(*written));if(r!=XTP_OK)return r;
    if(size>capacity)return XTP_E_CAPACITY;
    unsigned char* out=output;for(size_t i=0;i<n;++i){xtpEmit(p+i,frames+i,out);out+=frames[i].Size;}
    *written=size;return XTP_OK;
}
static xtpresult xtpMetadata(xbytesview wire,xtpparamkind kind,const xtpcodecconfig* c,xtpparamsview* out)
{
    *out=(xtpparamsview){kind,0,wire};if(!wire.Size)return XTP_OK;
    if(wire.Size<2)return XTP_E_LENGTH;
    size_t n=xtp__U16(wire.Data),stride=kind==XTP_PARAMS_ARRAY?4:6,head=2+stride*n,prev=0;
    if(!n)return XTP_E_VALUE;
    if(n>c->MaxParams)return XTP_E_LIMIT;
    if(head>wire.Size)return XTP_E_LENGTH;
    const unsigned char* table=wire.Data+2;const unsigned char* data=wire.Data+head;size_t bytes=wire.Size-head;
    for(size_t i=0;i<n;++i){size_t end=xtp__U32(table+i*stride+(stride==6?2:0));
        if(end<prev||end>bytes)return XTP_E_LENGTH;
        if(kind==XTP_PARAMS_TABLE){size_t key=xtp__U16(table+i*6);
            if(key>c->MaxKeyBytes)return XTP_E_LIMIT;
            if(key>end-prev)return XTP_E_LENGTH;
            if(!xtpKeyValid((xstrview){(const char*)data+prev,key}))return XTP_E_VALUE;}
        prev=end;}
    if(prev!=bytes)return XTP_E_LENGTH;
    if(kind==XTP_PARAMS_TABLE&&n>1){xtpkeys k={NULL,table,data};if(!xtpUnique(&k,n))return XTP_E_VALUE;}
    out->Count=n;return XTP_OK;
}
xtpresult xtp__DecodePrefix(const unsigned char* p,size_t size,const xtpcodecconfig* config,xtpframeview* frame)
{
    unsigned flags=p[17];size_t cmd=xtp__U16(p+18),meta=xtp__U32(p+20);xtpresult r;
    if(flags&~15u)return XTP_E_VALUE;
    xtpframeview f;memset(&f,0,sizeof(f));f.Type=(xtpmessagekind)p[16];f.MessageID=xtp__U64(p+8);
    uint32_t status=xtp__U32(p+24);f.Status=status<=INT32_MAX?(int32_t)status:-(int32_t)(UINT32_MAX-status)-1;
    f.Command.Kind=(f.Type==XTP_REQUEST||f.Type==XTP_PUSH||f.Type==XTP_NOTIFY)?
        (flags&1?XTP_COMMAND_ID:XTP_COMMAND_NAME):XTP_COMMAND_NONE;
    f.Replayable=(flags&8)!=0;f.ResultAck=(flags&4)!=0;
    if(f.Command.Kind==XTP_COMMAND_ID)f.Command.As.ID=xtp__U32(p+28);
    else if(f.Command.Kind==XTP_COMMAND_NAME)f.Command.As.Name=(xstrview){(const char*)p+32,cmd};
    f.Body=(xbytesview){p+32+cmd+meta,size-32-cmd-meta};f.Packet=(xbytesview){p,size};
    if(f.Command.Kind==XTP_COMMAND_NONE && (flags&1))return XTP_E_VALUE;
    r=xtpFields(f.Type,f.MessageID,f.Command.Kind,cmd,xtp__U32(p+28),meta,
        flags&2?XTP_PARAMS_ARRAY:XTP_PARAMS_TABLE,f.Body.Size,f.Status,f.Replayable,f.ResultAck);
    if(r!=XTP_OK)return r;
    if(f.Command.Kind==XTP_COMMAND_NAME&&!xtpCommandValid(f.Command.As.Name))return XTP_E_VALUE;
    r=xtpMetadata((xbytesview){p+32+cmd,meta},flags&2?XTP_PARAMS_ARRAY:XTP_PARAMS_TABLE,config,&f.Params);
    if(r!=XTP_OK)return r;
    *frame=f;return XTP_OK;
}
XTP_API xtpresult xtpDecode(xbytesview in,const xtpcodecconfig* input,xtpframeview* frame)
{
    xtpcodecconfig c;size_t size;xtpresult r=xtp__CodecConfig(input,&c);if(r!=XTP_OK)return r;
    if(!xtp__Bytes(in)||!xrtMemRangeValid(frame,sizeof(*frame))||
        xtp__Overlap(in.Data,in.Size,frame,sizeof(*frame))||xtp__Overlap(input,input?sizeof(*input):0,frame,sizeof(*frame)))return XTP_E_ARGUMENT;
    if(in.Size<32)return XTP_E_TRUNCATED;
    r=xtp__Envelope(in.Data,&c,&size);if(r!=XTP_OK)return r;
    if(in.Size<size)return XTP_E_TRUNCATED;
    if(in.Size>size)return XTP_E_LENGTH;
    return xtp__DecodePrefix(in.Data,size,&c,frame);
}
static bool xtpParamOutput(const xtpparamsview* p,const void* out,size_t bytes)
{
    return p&&xrtMemRangeValid(p,sizeof(*p))&&xrtMemRangeValid(out,bytes)&&
        xtp__Bytes(p->_Wire)&&!xtp__Overlap(out,bytes,p,sizeof(*p))&&!xtp__Overlap(out,bytes,p->_Wire.Data,p->_Wire.Size);
}
static xtpparamentry xtpEntry(const xtpparamsview* p,size_t i)
{
    size_t stride=p->Kind==XTP_PARAMS_ARRAY?4:6;const unsigned char* table=p->_Wire.Data+2;
    const unsigned char* data=table+stride*p->Count;size_t start=i?xtp__U32(table+(i-1)*stride+(stride==6?2:0)):0;
    size_t end=xtp__U32(table+i*stride+(stride==6?2:0)),key=stride==6?xtp__U16(table+i*stride):0;
    xtpparamentry e={i,stride==6,{NULL,0},{data+start+key,end-start-key}};
    if(key)e.Key=(xstrview){(const char*)data+start,key};
    return e;
}
XTP_API xtpparamresult xtpParamAt(const xtpparamsview* p,size_t i,xbytesview* value)
{
    if(!xtpParamOutput(p,value,sizeof(*value)))return XTP_PARAM_ARGUMENT;
    *value=(xbytesview){NULL,0};
    if(p->Kind!=XTP_PARAMS_ARRAY&&p->Kind!=XTP_PARAMS_TABLE)return XTP_PARAM_ARGUMENT;
    if(i>=p->Count)return XTP_PARAM_MISSING;
    *value=xtpEntry(p,i).Value;return XTP_PARAM_FOUND;
}
XTP_API xtpparamresult xtpParamFind(const xtpparamsview* p,xstrview key,xbytesview* value)
{
    if(!xtpParamOutput(p,value,sizeof(*value))||!xtp__String(key)||xtp__Overlap(value,sizeof(*value),key.Data,key.Size))return XTP_PARAM_ARGUMENT;
    *value=(xbytesview){NULL,0};
    if(p->Kind==XTP_PARAMS_ARRAY)return XTP_PARAM_WRONG_MODE;
    if(p->Kind!=XTP_PARAMS_TABLE)return XTP_PARAM_ARGUMENT;
    for(size_t i=0;i<p->Count;++i){xtpparamentry e=xtpEntry(p,i);
        if(e.Key.Size==key.Size&&(!key.Size||!memcmp(e.Key.Data,key.Data,key.Size))){*value=e.Value;return XTP_PARAM_FOUND;}}
    return XTP_PARAM_MISSING;
}
XTP_API xtpparamiter xtpParams(const xtpparamsview* p)
{ xtpparamiter i={p,0};return i; }
XTP_API xtpparamresult xtpParamNext(xtpparamiter* i,xtpparamentry* e)
{
    if(!i||!xtpParamOutput(i->_Params,e,sizeof(*e))||xtp__Overlap(i,sizeof(*i),e,sizeof(*e)))return XTP_PARAM_ARGUMENT;
    memset(e,0,sizeof(*e));const xtpparamsview* p=i->_Params;
    if(p->Kind!=XTP_PARAMS_ARRAY&&p->Kind!=XTP_PARAMS_TABLE)return XTP_PARAM_ARGUMENT;
    if(i->_Next>=p->Count)return XTP_PARAM_MISSING;
    *e=xtpEntry(p,i->_Next++);return XTP_PARAM_FOUND;
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xtp/src/reader/reader.c */
/* ========================================================================== */

#if defined(XTP_FEATURE_XTP_READER)
#if defined(XTP_FEATURE_XTP_READER)
struct xtpreader {
    xtpreaderconfig Config;xtpreaderevents Events;void* User;
    xtpframeview Validated;
    unsigned char Header[32];unsigned char* Buffer;
    size_t Used,Capacity,Need,Prefix,Discard,Peak;
    uint64_t LastNow,Deadline,Frames,Dropped;
    uint32_t Consecutive;
    xtpresult Failure,DropReason;
    bool PrefixChecked,Paused,Ended,Busy,DropRecord,PendingEnd;
};
static void* xtpReaderMemory(void* user,void* memory,size_t n)
{(void)user;if(!n){xrtFree(memory);return NULL;}return xrtRealloc(memory,n);}
XTP_API void xtpReaderConfigInit(xtpreaderconfig* c)
{
    if(!c)return;
    memset(c,0,sizeof(*c));xtpCodecConfigInit(&c->Codec);c->RetainBytes=65536;
    c->MaxStorageBytes=3u*1024*1024;c->MaxWorkBytes=262144;c->MaxFrames=64;
    c->MaxConsecutiveBad=8;c->MaxBad=64;c->ReceiveTimeoutUs=5000000;
}
XTP_API xtpresult xtpReaderCreate(const xtpreaderconfig* input,const xtpreaderevents* events,void* user,xtpreader** out)
{
    xtpreaderconfig c;xtpcodecconfig checked;
    if(!out||!events||!events->Frame||!xrtMemRangeValid(out,sizeof(*out))||
        !xrtMemRangeValid(events,sizeof(*events))||xtp__Overlap(out,sizeof(*out),events,sizeof(*events)))return XTP_E_ARGUMENT;
    if(input){if(!xrtMemRangeValid(input,sizeof(*input))||xtp__Overlap(out,sizeof(*out),input,sizeof(*input)))return XTP_E_ARGUMENT;c=*input;}
    else xtpReaderConfigInit(&c);
    if(xtp__CodecConfig(&c.Codec,&checked)!=XTP_OK || c.Mode>XTP_INPUT_RECORD || c.Mode<XTP_INPUT_STREAM ||
        c.MaxStorageBytes<sizeof(xtpreader)+32 || !c.MaxFrames || !c.MaxBad || !c.MaxConsecutiveBad ||
        !c.ReceiveTimeoutUs || c.MaxWorkBytes<(uint64_t)32+c.Codec.MaxCommandBytes+c.Codec.MaxMetadataBytes)return XTP_E_ARGUMENT;
    if(!c.Realloc)c.Realloc=xtpReaderMemory;
    xtpreader* r=c.Realloc(c.AllocatorUser,NULL,sizeof(*r));if(!r)return XTP_E_MEMORY;
    memset(r,0,sizeof(*r));r->Config=c;r->Events=*events;r->User=user;r->Peak=sizeof(*r);*out=r;return XTP_OK;
}
static void xtpReaderRelease(xtpreader* r)
{if(r->Buffer)r->Config.Realloc(r->Config.AllocatorUser,r->Buffer,0);r->Buffer=NULL;r->Capacity=0;}
XTP_API xtpresult xtpReaderDestroy(xtpreader* r)
{
    if(!r)return XTP_OK;
    if(r->Busy)return XTP_E_STATE;
    xtpreallocproc f=r->Config.Realloc;void* u=r->Config.AllocatorUser;xtpReaderRelease(r);f(u,r,0);return XTP_OK;
}
static xtpresult xtpReaderReserve(xtpreader* r,size_t need)
{
    if(need<=r->Capacity)return XTP_OK;
    size_t n=r->Capacity?r->Capacity:256;
    if(n>r->Need)n=r->Need;
    while(n<need){if(n>r->Need/2){n=r->Need;break;}n*=2;}
    if(n>r->Config.MaxStorageBytes-sizeof(*r) || r->Capacity>r->Config.MaxStorageBytes-sizeof(*r)-n)return XTP_E_LIMIT;
    size_t peak=sizeof(*r)+r->Capacity+n;if(peak>r->Peak)r->Peak=peak;
    bool fresh=r->Buffer==NULL;void* p=r->Config.Realloc(r->Config.AllocatorUser,r->Buffer,n);
    if(!p)return XTP_E_MEMORY;
    r->Buffer=p;r->Capacity=n;if(fresh)memcpy(r->Buffer,r->Header,32);return XTP_OK;
}
static void xtpReaderClear(xtpreader* r)
{
    r->Used=r->Need=r->Prefix=r->Discard=0;r->PrefixChecked=false;r->Deadline=0;
    if(r->Capacity>r->Config.RetainBytes)xtpReaderRelease(r);
}
static xtpresult xtpReaderBad(xtpreader* r,xtpresult reason,xtpreadprogress* p)
{
    ++r->Dropped;++r->Consecutive;if(p)++p->Dropped;
    if(r->Events.Drop)r->Events.Drop(r,reason,r->User);
    if(r->Dropped>=r->Config.MaxBad||r->Consecutive>=r->Config.MaxConsecutiveBad){r->Failure=reason;return reason;}
    return XTP_DROPPED;
}
static xtpresult xtpReaderClock(xtpreader* r,uint64_t now)
{
    if(now<r->LastNow)return XTP_E_ARGUMENT;
    r->LastNow=now;
    if(r->Failure!=XTP_OK)return r->Failure;
    if(r->Ended)return XTP_END;
    if(r->Deadline && now>=r->Deadline){r->Failure=XTP_E_DEADLINE;return r->Failure;}
    return XTP_OK;
}
static void xtpReaderStart(xtpreader* r,uint64_t now)
{if(!r->Deadline)r->Deadline=UINT64_MAX-now<r->Config.ReceiveTimeoutUs?UINT64_MAX:now+r->Config.ReceiveTimeoutUs;}
static const unsigned char* xtpReaderData(xtpreader* r)
{return r->Buffer?r->Buffer:r->Header;}
static xtpresult xtpReaderDispatch(xtpreader* r,const xtpframeview* f)
{
    xtpflow flow=r->Events.Frame(r,f,r->User);
    if(flow==XTP_FLOW_PAUSE){r->Paused=true;r->Deadline=0;return XTP_PAUSED;}
    if(flow!=XTP_FLOW_ACCEPT&&flow!=XTP_FLOW_STOP){r->Failure=XTP_E_STATE;return r->Failure;}
    ++r->Frames;r->Consecutive=0;
    if(flow==XTP_FLOW_STOP){r->Ended=true;return XTP_CLOSED;}
    return XTP_OK;
}
static xtpresult xtpReaderRun(xtpreader* r,xbytesview in,bool end,uint64_t now,xtpreadprogress* p)
{
    xtpresult result=xtpReaderClock(r,now);if(result<0||result==XTP_END)return result;
    if(r->Paused){if(end&&!in.Size)r->PendingEnd=true;return XTP_PAUSED;}
    if(r->PendingEnd && in.Size)return XTP_E_STATE;
    bool boundary=end||r->PendingEnd;
    while(p->Frames<r->Config.MaxFrames && p->WorkBytes<r->Config.MaxWorkBytes){
        size_t available=in.Size-p->Used,budget=r->Config.MaxWorkBytes-p->WorkBytes;
        const unsigned char* input=available?in.Data+p->Used:NULL;
        if(r->DropRecord){size_t take=available<budget?available:budget;p->Used+=take;p->WorkBytes+=take;break;}
        if(r->Discard){size_t take=r->Discard<available?r->Discard:available;if(take>budget)take=budget;
            p->Used+=take;p->WorkBytes+=take;r->Discard-=take;if(r->Discard)break;
            xtpReaderClear(r);continue;}
        if(!r->Used && available>=32){
            size_t frame=0;result=xtp__Envelope(input,&r->Config.Codec,&frame);
            if(result!=XTP_OK){if(r->Config.Mode==XTP_INPUT_STREAM){r->Failure=result;return result;}
                xtpReaderStart(r,now);
                r->DropRecord=true;result=xtpReaderBad(r,result,p);if(result<0)return result;continue;}
            size_t prefix=32+xtp__U16(input+18)+xtp__U32(input+20);
            if(available>=frame && budget>=prefix){xtpframeview f;result=xtp__DecodePrefix(input,frame,&r->Config.Codec,&f);p->WorkBytes+=prefix;
                if(result!=XTP_OK){xtpReaderStart(r,now);r->Discard=frame-prefix;p->Used+=prefix;r->DropReason=result;
                    result=xtpReaderBad(r,result,p);if(result<0)return result;
                    if(!r->Discard)xtpReaderClear(r);
                    continue;}
                if(available>=frame){result=xtpReaderDispatch(r,&f);
                    if(result==XTP_PAUSED)return result;
                    p->Used+=frame;++p->Frames;if(result!=XTP_OK)return result;continue;}
            }
        }
        if(!r->Used && !available)break;
        if(!r->Used)xtpReaderStart(r,now);
        size_t target=r->Used<32?32:(!r->PrefixChecked?r->Prefix:r->Need);
        if(r->Used<target){size_t take=target-r->Used;if(take>available)take=available;if(take>budget)take=budget;
            if(!take)break;
            if(r->Used<32)memcpy(r->Header+r->Used,input,take);
            else {result=xtpReaderReserve(r,r->Used+take);if(result!=XTP_OK){r->Failure=result;return result;}
                memcpy(r->Buffer+r->Used,input,take);}
            r->Used+=take;p->Used+=take;p->WorkBytes+=take;
            if(r->Need && r->Used==r->Need)r->Deadline=0;
            if(r->Used<target)continue;
        }
        if(r->Used==32&&!r->Need){result=xtp__Envelope(r->Header,&r->Config.Codec,&r->Need);
            if(result!=XTP_OK){if(r->Config.Mode==XTP_INPUT_STREAM){r->Failure=result;return result;}
                uint64_t deadline=r->Deadline;xtpReaderClear(r);r->Deadline=deadline;
                r->DropRecord=true;result=xtpReaderBad(r,result,p);if(result<0)return result;continue;}
            r->Prefix=32+xtp__U16(r->Header+18)+xtp__U32(r->Header+20);
            if(r->Used==r->Need)r->Deadline=0;
            if(r->Buffer)memcpy(r->Buffer,r->Header,32);
        }
        if(!r->PrefixChecked && r->Used==r->Prefix){
            if(r->Config.MaxWorkBytes-p->WorkBytes<r->Prefix)break;
            result=xtp__DecodePrefix(xtpReaderData(r),r->Need,&r->Config.Codec,&r->Validated);p->WorkBytes+=r->Prefix;
            if(result!=XTP_OK){r->Discard=r->Need-r->Used;r->DropReason=result;
                result=xtpReaderBad(r,result,p);if(result<0)return result;
                if(!r->Discard)xtpReaderClear(r);
                continue;}
            r->PrefixChecked=true;
        }
        if(r->PrefixChecked && r->Used==r->Need){
            const unsigned char* data=xtpReaderData(r);xtpframeview f=r->Validated;
            f.Packet=(xbytesview){data,r->Need};f.Body=(xbytesview){data+r->Prefix,r->Need-r->Prefix};
            if(f.Command.Kind==XTP_COMMAND_NAME)f.Command.As.Name.Data=(const char*)data+32;
            f.Params._Wire.Data=data+32+xtp__U16(r->Header+18);
            result=xtpReaderDispatch(r,&f);
            if(result==XTP_PAUSED)return result;
            ++p->Frames;xtpReaderClear(r);if(result!=XTP_OK)return result;
        }
    }
    if(boundary && p->Used==in.Size){
        if(r->Used && !r->Discard && r->Need && r->Used==r->Need){r->PendingEnd=true;return XTP_AGAIN;}
        r->PendingEnd=false;
        if(r->Used&&!r->Discard){result=xtpReaderBad(r,XTP_E_TRUNCATED,p);if(result<0)return result;}
        if(r->Used||r->Discard||r->DropRecord)xtpReaderClear(r);
        r->DropRecord=false;
    }
    if(p->Used<in.Size)return XTP_AGAIN;
    if(!r->Discard && r->Used && r->Need && (r->Used==r->Need||(!r->PrefixChecked&&r->Used==r->Prefix)))return XTP_AGAIN;
    if(r->Used||r->Discard)return XTP_MORE;
    return p->Dropped?XTP_DROPPED:XTP_OK;
}
/* This validates an ADDRESS, never reads output contents. Passing the address
 * as an integer also avoids GCC's const-void* uninitialized-read inference in
 * optimized amalgamated callers whose output storage has not been filled yet. */
static bool xtpReaderOutput(xtpreader* r,uintptr_t address,size_t n)
{
    const void* output=(const void*)address;
    return xrtMemRangeValid(output,n)&&!xtp__Overlap(r,sizeof(*r),output,n)&&
        !xtp__Overlap(r->Buffer,r->Capacity,output,n);
}
XTP_API xtpresult xtpReaderInput(xtpreader* r,xbytesview in,bool end,uint64_t now,xtpreadprogress* progress)
{
    if(!r||r->Busy||!xtpReaderOutput(r,(uintptr_t)progress,sizeof(*progress))||!xtp__Bytes(in)||
        xtp__Overlap(in.Data,in.Size,r,sizeof(*r))||xtp__Overlap(in.Data,in.Size,r->Buffer,r->Capacity)||
        xtp__Overlap(in.Data,in.Size,progress,sizeof(*progress))||
        (end&&r->Config.Mode!=XTP_INPUT_RECORD)||now<r->LastNow)return XTP_E_ARGUMENT;
    xtpreadprogress p={0};r->Busy=true;xtpresult result=xtpReaderRun(r,in,end,now,&p);
    if(result==XTP_PAUSED&&end&&p.Used==in.Size)r->PendingEnd=true;
    r->Busy=false;*progress=p;return result;
}
XTP_API xtpresult xtpReaderResume(xtpreader* r)
{if(!r||r->Busy)return XTP_E_STATE;r->Paused=false;return r->Failure;}
XTP_API xtpresult xtpReaderEnd(xtpreader* r,uint64_t now)
{
    if(!r||r->Busy||now<r->LastNow)return XTP_E_ARGUMENT;
    xtpresult result=xtpReaderClock(r,now);if(result!=XTP_OK)return result;
    if(r->Paused)return XTP_PAUSED;
    if(!r->Discard&&r->Need&&r->Used==r->Need)return XTP_AGAIN;
    r->LastNow=now;r->Ended=true;
    if(r->Used||r->Discard||r->DropRecord){r->Failure=XTP_E_TRUNCATED;return r->Failure;}
    return XTP_END;
}
XTP_API xtpresult xtpReaderReset(xtpreader* r)
{
    if(!r||r->Busy)return XTP_E_STATE;
    xtpReaderClear(r);r->Failure=XTP_OK;r->Paused=r->Ended=r->DropRecord=r->PendingEnd=false;
    r->Frames=r->Dropped=0;r->Consecutive=0;return XTP_OK;
}
XTP_API xtpresult xtpReaderDrive(xtpreader* r,uint64_t now,uint64_t* due)
{
    if(!r||r->Busy||!xtpReaderOutput(r,(uintptr_t)due,sizeof(*due))||now<r->LastNow)return XTP_E_ARGUMENT;
    xtpreadprogress p={0};r->Busy=true;
    xtpresult result=xtpReaderRun(r,(xbytesview){NULL,0},false,now,&p);
    r->Busy=false;*due=r->Deadline;return result==XTP_MORE?XTP_OK:result;
}
XTP_API xtpresult xtpReaderInfo(const xtpreader* r,xtpreaderinfo* info)
{
    if(!r||!xtpReaderOutput((xtpreader*)r,(uintptr_t)info,sizeof(*info)))return XTP_E_ARGUMENT;
    *info=(xtpreaderinfo){r->Used,r->Capacity,sizeof(*r)+r->Capacity,r->Peak,r->Frames,r->Dropped,r->Deadline,
        r->Consecutive,r->Failure,r->Paused,r->Ended,r->DropRecord};return XTP_OK;
}
#endif
#endif

#endif
