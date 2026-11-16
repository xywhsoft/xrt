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

/* xtp_xrudp 及其直接依赖。 */
#if defined(XTP_MODULE_ALL) || defined(XTP_MODULE_XTP_XRUDP)
#ifndef XTP_FEATURE_XTP_XRUDP
#define XTP_FEATURE_XTP_XRUDP
#endif
#ifndef XTP_MODULE_XTP_TRANSPORT
#define XTP_MODULE_XTP_TRANSPORT
#endif
#ifndef XRUDP_MODULE_RUDP_SESSION
#define XRUDP_MODULE_RUDP_SESSION
#endif
#endif

/* xtp_native 及其直接依赖。 */
#if defined(XTP_MODULE_ALL) || defined(XTP_MODULE_XTP_NATIVE)
#ifndef XTP_FEATURE_XTP_NATIVE
#define XTP_FEATURE_XTP_NATIVE
#endif
#ifndef XTP_MODULE_XTP_TCP
#define XTP_MODULE_XTP_TCP
#endif
#ifndef XTP_MODULE_XTP_TLS
#define XTP_MODULE_XTP_TLS
#endif
#ifndef XTP_MODULE_XTP_WS
#define XTP_MODULE_XTP_WS
#endif
#ifndef XRT_MODULE_NET_TCP_RETIRE
#define XRT_MODULE_NET_TCP_RETIRE
#endif
#ifndef XRT_MODULE_TLS_STREAM_RETIRE
#define XRT_MODULE_TLS_STREAM_RETIRE
#endif
#ifndef XRT_MODULE_THREAD
#define XRT_MODULE_THREAD
#endif
#endif

/* xtp_ws 及其直接依赖。 */
#if defined(XTP_MODULE_ALL) || defined(XTP_MODULE_XTP_WS)
#ifndef XTP_FEATURE_XTP_WS
#define XTP_FEATURE_XTP_WS
#endif
#ifndef XTP_MODULE_XTP_TRANSPORT
#define XTP_MODULE_XTP_TRANSPORT
#endif
#ifndef XWS_MODULE_WEBSOCKET_CONNECTION
#define XWS_MODULE_WEBSOCKET_CONNECTION
#endif
#endif

/* xtp_tls 及其直接依赖。 */
#if defined(XTP_MODULE_ALL) || defined(XTP_MODULE_XTP_TLS)
#ifndef XTP_FEATURE_XTP_TLS
#define XTP_FEATURE_XTP_TLS
#endif
#ifndef XTP_MODULE_XTP_TRANSPORT
#define XTP_MODULE_XTP_TRANSPORT
#endif
#ifndef XRT_MODULE_TLS_STREAM
#define XRT_MODULE_TLS_STREAM
#endif
#ifndef XRT_MODULE_TLS_CLIENT_VERIFY
#define XRT_MODULE_TLS_CLIENT_VERIFY
#endif
#ifndef XRT_MODULE_TLS_IDENTITY_ED25519
#define XRT_MODULE_TLS_IDENTITY_ED25519
#endif
#ifndef XRT_MODULE_TLS_IDENTITY_P256
#define XRT_MODULE_TLS_IDENTITY_P256
#endif
#ifndef XRT_MODULE_TLS_RECORD_CHACHA
#define XRT_MODULE_TLS_RECORD_CHACHA
#endif
#ifndef XRT_MODULE_TLS_RECORD_AES
#define XRT_MODULE_TLS_RECORD_AES
#endif
#ifndef XRT_MODULE_TLS_SCHEDULE_SHA256
#define XRT_MODULE_TLS_SCHEDULE_SHA256
#endif
#ifndef XRT_MODULE_TLS_KEY_EXCHANGE_X25519
#define XRT_MODULE_TLS_KEY_EXCHANGE_X25519
#endif
#endif

/* xtp_tcp 及其直接依赖。 */
#if defined(XTP_MODULE_ALL) || defined(XTP_MODULE_XTP_TCP)
#ifndef XTP_FEATURE_XTP_TCP
#define XTP_FEATURE_XTP_TCP
#endif
#ifndef XTP_MODULE_XTP_TRANSPORT
#define XTP_MODULE_XTP_TRANSPORT
#endif
#ifndef XRT_MODULE_NET_TCP
#define XRT_MODULE_NET_TCP
#endif
#endif

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
#ifndef XTP_MODULE_XTP_SESSION
#define XTP_MODULE_XTP_SESSION
#endif
#ifndef XTP_MODULE_XTP_TRANSPORT
#define XTP_MODULE_XTP_TRANSPORT
#endif
#ifndef XTP_MODULE_XTP_DELIVERY
#define XTP_MODULE_XTP_DELIVERY
#endif
#endif

/* xtp_delivery 及其直接依赖。 */
#if defined(XTP_MODULE_ALL) || defined(XTP_MODULE_XTP_DELIVERY)
#ifndef XTP_FEATURE_XTP_DELIVERY
#define XTP_FEATURE_XTP_DELIVERY
#endif
#ifndef XTP_MODULE_XTP_SESSION
#define XTP_MODULE_XTP_SESSION
#endif
#endif

/* xtp_transport 及其直接依赖。 */
#if defined(XTP_MODULE_ALL) || defined(XTP_MODULE_XTP_TRANSPORT)
#ifndef XTP_FEATURE_XTP_TRANSPORT
#define XTP_FEATURE_XTP_TRANSPORT
#endif
#ifndef XTP_MODULE_XTP_SESSION
#define XTP_MODULE_XTP_SESSION
#endif
#endif

/* xtp_session 及其直接依赖。 */
#if defined(XTP_MODULE_ALL) || defined(XTP_MODULE_XTP_SESSION)
#ifndef XTP_FEATURE_XTP_SESSION
#define XTP_FEATURE_XTP_SESSION
#endif
#ifndef XTP_MODULE_XTP_READER
#define XTP_MODULE_XTP_READER
#endif
#ifndef XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_RANDOM_SECURE
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
 * Context/code live until Destroy. NULL selects public XRT memory functions.
 * Allocator callbacks must not reenter the instance. */
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
/* public: extlibs/xtp/include/xtp/session.h */
/* ========================================================================== */

#ifndef XTP_SESSION_H
#define XTP_SESSION_H
#if defined(XTP_FEATURE_XTP_SESSION)
typedef struct xtpsession xtpsession;
typedef struct xtprequesttoken {uint64_t _Opaque[4];} xtprequesttoken;
typedef struct xtpoperationtoken {uint64_t _Opaque[2];} xtpoperationtoken;
typedef struct xtpoutputticket {uint64_t _Opaque[4];} xtpoutputticket;
typedef struct xtpmessageview {
    xtpmessagekind Type;uint64_t MessageID;xtpcommand Command;
    xtpparamsview Params;xbytesview Body,Packet;xtprequesttoken Request;
} xtpmessageview;
typedef enum xtpaction {XTP_ACTION_ACCEPT=1,XTP_ACTION_BUSY,XTP_ACTION_REJECT,XTP_ACTION_PAUSE} xtpaction;
typedef struct xtpdecision {xtpaction Action;int32_t Status;} xtpdecision;
typedef enum xtpfinish {
    XTP_FINISH_RESPONSE=1,XTP_FINISH_ACK,XTP_FINISH_SENT,XTP_FINISH_TIMEOUT,
    XTP_FINISH_CLOSED,XTP_FINISH_LOCAL_ERROR
} xtpfinish;
typedef enum xtpknowledge {
    XTP_KNOW_NOT_SENT=1,XTP_KNOW_UNKNOWN,XTP_KNOW_ACCEPTED,XTP_KNOW_REPLIED,XTP_KNOW_REJECTED
} xtpknowledge;
typedef struct xtpcompletion {
    xtpoperationtoken Operation;xtpmessagekind Type;uint64_t MessageID;
    xtpfinish Reason;xtpknowledge Knowledge;xtpresult LocalResult;
    bool HasRemoteStatus;int32_t Status;xtpparamsview Params;xbytesview Body,Packet;void* Tag;
} xtpcompletion;
typedef struct xtpfault {xtpresult Reason;uint64_t BindingID;uint32_t LaneID;bool Fatal;} xtpfault;
typedef struct xtpevents {
    xtpdecision (*OnMessage)(xtpsession*,const xtpmessageview*,void* User);
    void (*OnComplete)(xtpsession*,const xtpcompletion*,void* User);
    void (*OnFault)(xtpsession*,const xtpfault*,void* User);
} xtpevents;
typedef struct xtpsubmitoptions {uint64_t DeadlineUs;uint32_t LaneID;bool Replay;void* Tag;} xtpsubmitoptions;
typedef struct xtplane {
    uint32_t ID;xtpinputmode Mode;bool Reliable;
    size_t RecordLimit; /* RECORD complete-frame limit; required, even on WS. */
} xtplane;
/* Host-authenticated, canonical public identities; never credentials. Each
 * array must contain a nonzero byte. SessionID is exchanged by the host. */
typedef struct xtpdeliveryidentity {
    unsigned char SessionID[16],Domain[16],Local[32],Peer[32];
} xtpdeliveryidentity;
typedef struct xtpdeliveryproof {
    xtpdeliveryidentity Identity;
    uint64_t UntilUs,LifetimeUs,PeerReceiveFloor,PeerResultFloor;
    bool Resume,StateRetained,OldBindingDetached,AllowRequests;
} xtpdeliveryproof;
typedef struct xtpdeliveryconfig {
    bool Enabled,AllowRequests;
    size_t MaxReplayMessages,MaxAttempts;
    uint64_t MaxLifetimeUs,RetryMinUs,RetryMaxUs;
} xtpdeliveryconfig;
typedef struct xtpbinding {
    const xtplane* Lanes;size_t LaneCount;
    bool Authenticated,AllowPlaintext; /* Explicit trusted-local/test exception. */
    const xtpdeliveryproof* Delivery; /* Borrowed only during Bind. */
} xtpbinding;
typedef struct xtpconfig {
    xtpreaderconfig Reader;
    size_t MaxPending,MaxIncomingRequests,MaxLanes,MaxTxFrames,MaxTxBytes;
    size_t ResponseReserveBytes,ResponseReserveFrames,MaxResponseBytes,MaxSessionBytes,DedupWindow;
    uint64_t DefaultOperationTimeoutUs,IncomingRequestTimeoutUs;
    xtpdeliveryconfig Delivery;
    xtpreallocproc Realloc;void* AllocatorUser;
} xtpconfig;
typedef struct xtpoutput {
    xbytesview Data;xtpoutputticket Ticket;uint64_t BindingID,DeadlineUs;
    uint32_t LaneID;xtpinputmode Mode;bool Reliable;uint8_t Priority;
} xtpoutput;
typedef struct xtpinfo {
    uint64_t BindingID,NextMessageID,Submitted,Completed,Messages,UnknownResults,Duplicates;
    size_t Pending,IncomingRequests,TxFrames,TxBytes,StorageBytes,PeakReservedBytes;
    bool Bound,Closing,Closed;
    size_t Retained,Receipts;
    uint64_t ReceiveFloor,ResultFloor,Attempts,Retries,ResultAcks;
    bool Suspended,ReplayRequests;
} xtpinfo;
XRT_EXTERN_C_BEGIN
/* No IO/thread/global route state. NULL config uses finite defaults. Config and
 * events copied. Random instance cookies use XRT secure random once at Create.
 * Same instance: serialize calls. Cross-instance calls may run concurrently. */
XTP_API void xtpConfigInit(xtpconfig* Config);
XTP_API xtpresult xtpCreate(const xtpconfig* Config,const xtpevents* Events,void* User,xtpsession** Session);
/* Destroy requires CLOSED and no callbacks/host asynchronous tasks in flight. */
XTP_API xtpresult xtpDestroy(xtpsession* Session);
/* No transport ownership/event replacement. Copies lanes, nonzero unique IDs;
 * LaneID=0 in Submit/Output selects default/any lane. Generation never wraps.
 * Delivery uses a host-verified proof, never a wire login command. Rebind after
 * Unbind retains only opted-in work. Otherwise Unbind completes all work. */
XTP_API xtpresult xtpBind(xtpsession* Session,const xtpbinding* Binding,uint64_t NowUs,uint64_t* BindingID);
XTP_API xtpresult xtpUnbind(xtpsession* Session,uint64_t BindingID,uint64_t NowUs);
XTP_API xtpresult xtpInput(xtpsession* Session,uint64_t BindingID,uint32_t LaneID,
    xbytesview Input,bool RecordEnd,uint64_t NowUs,xtpreadprogress* Progress);
XTP_API xtpresult xtpEnd(xtpsession* Session,uint64_t BindingID,uint32_t LaneID,uint64_t NowUs);
XTP_API xtpresult xtpResume(xtpsession* Session,uint64_t BindingID,uint32_t LaneID,uint64_t NowUs);
/* Success copies all input. Batch: all accepted or none; outputs preserved on
 * failure. Optional Options/Operations arrays have Count entries. No callback
 * dispatch during Submit/Reply. Replay requires enabled, authenticated Delivery;
 * requests additionally require negotiated AllowRequests. Original deadline
 * must fit the agreed replay lifetime and authenticated identity expiry. */
XTP_API xtpresult xtpSubmit(xtpsession* Session,const xtpmessage* Message,
    const xtpsubmitoptions* Options,uint64_t NowUs,xtpoperationtoken* Operation);
XTP_API xtpresult xtpSubmitBatch(xtpsession* Session,const xtpmessage* Messages,size_t Count,
    const xtpsubmitoptions* Options,uint64_t NowUs,xtpoperationtoken* Operations);
XTP_API xtpresult xtpReply(xtpsession* Session,xtprequesttoken Request,const xtpresponse* Response,uint64_t NowUs);
/* One immutable frame suffix. STREAM permits short Advance; RECORD is all or
 * zero. Zero Advance returns AGAIN. Ticket validates binding, slot, generation
 * and offset. No reentry between Output and lower-layer acceptance/Advance.
 * Output is borrowed until the next mutating call; host must copy if retaining.
 * Output(any lane) rotates queue heads after successful Advance; select a lane to keep
 * independently writable lanes progressing under per-lane backpressure. */
XTP_API xtpresult xtpOutput(xtpsession* Session,uint32_t LaneID,xtpoutput* Output);
XTP_API xtpresult xtpAdvance(xtpsession* Session,xtpoutputticket Ticket,size_t Accepted,uint64_t NowUs);
/* Drive also services receive deadlines/complete buffered frames, bounded to
 * Reader.MaxFrames callbacks per maintenance turn. AGAIN requests another turn;
 * NextDue=0 has no timer. PAUSE requires explicit Resume and retained suffix. */
XTP_API xtpresult xtpDrive(xtpsession* Session,uint64_t NowUs,uint64_t* NextDue);
/* Callback-safe deferred close. Repeated Close/Drive drains bounded completion
 * batches until CLOSED. Tokens/output invalid immediately on close/unbind.
 * All callbacks borrow views; only OnMessage/OnComplete/OnFault may Submit,
 * Reply or request Close. Recursive Input/Drive/Advance/Bind/Destroy forbidden. */
XTP_API xtpresult xtpClose(xtpsession* Session,uint64_t NowUs);
XTP_API xtpresult xtpInfo(const xtpsession* Session,xtpinfo* Info);
XRT_EXTERN_C_END
static inline void xtpDeliveryConfigInit(xtpdeliveryconfig* c)
{if(c){memset(c,0,sizeof(*c));c->Enabled=true;c->MaxReplayMessages=256;c->MaxAttempts=5;
    c->MaxLifetimeUs=60000000;c->RetryMinUs=100000;c->RetryMaxUs=2000000;}}
static inline xtpdecision xtpAccept(void){xtpdecision d={XTP_ACTION_ACCEPT,0};return d;}
static inline xtpdecision xtpBusy(void){xtpdecision d={XTP_ACTION_BUSY,XTP_STATUS_BUSY};return d;}
static inline xtpdecision xtpReject(int32_t status){xtpdecision d={XTP_ACTION_REJECT,status};return d;}
static inline xtpdecision xtpPause(void){xtpdecision d={XTP_ACTION_PAUSE,0};return d;}
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xtp/include/xtp/transport.h */
/* ========================================================================== */

#ifndef XTP_TRANSPORT_H
#define XTP_TRANSPORT_H
#if defined(XTP_FEATURE_XTP_TRANSPORT)
/* Writer borrows Output only until return; copy/accept before reporting bytes.
 * No reentry into Session. On an error, report any truly accepted prefix too.
 * Host retains transport ownership, events, input buffers and writable timers. */
typedef xtpresult (*xtpwriteproc)(void* User,const xtpoutput* Output,size_t* Accepted);
typedef struct xtpwritebudget {size_t Bytes,Frames,ChunkBytes;} xtpwritebudget;
typedef struct xtpwriteprogress {size_t Bytes,Frames,Calls,Dropped;} xtpwriteprogress;
XRT_EXTERN_C_BEGIN
/* NULL budget:256KiB,64 frames,16KiB stream offers. RECORD is never split and
 * must fit Bytes; no waiting to form batches. AGAIN means budget/backpressure.
 * Permanent LIMIT/DEADLINE on an unsent unreliable RECORD retires only that
 * NOTIFY as LOCAL_ERROR/TIMEOUT + NOT_SENT, returns DROPPED, never SENT.
 * Other errors retain data; host reports transport failure via Unbind.
 * Now is a fresh monotonic timestamp for this bounded synchronous drain turn. */
XTP_API xtpresult xtpDrain(xtpsession* Session,uint32_t LaneID,xtpwriteproc Write,void* User,
    const xtpwritebudget* Budget,uint64_t NowUs,xtpwriteprogress* Progress);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xtp/include/xtp/tcp.h */
/* ========================================================================== */

#ifndef XTP_TCP_H
#define XTP_TCP_H
#if defined(XTP_FEATURE_XTP_TCP)
XRT_EXTERN_C_BEGIN
/* xtpDrain writer; User is a borrowed OPEN xnetstream, copies on acceptance.
 * Only STREAM. Does not replace native events or retain/take its reference. */
XTP_API xtpresult xtpTcpWrite(void* Stream,const xtpoutput* Output,size_t* Accepted);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xtp/include/xtp/tls.h */
/* ========================================================================== */

#ifndef XTP_TLS_H
#define XTP_TLS_H
#if defined(XTP_FEATURE_XTP_TLS)
XRT_EXTERN_C_BEGIN
/* Static lifetime arrays: TLS1.3 only, ChaCha preferred, optional AES128-GCM;
 * X25519, Ed25519/ECDSA-P256 signatures. No 0-RTT path. Set client/server ALPN
 * to xtp/3; normal XRT verifier/name checks and server identity are mandatory.
 * TLS policy restricts handshake signatures; chain verification stays in XRT. */
XTP_API void xtpTlsPolicyInit(xtlspolicy* Policy,bool ChaChaOnly);
/* Checks OPEN, TLS1.3, allowed cipher, ALPN xtp/3 before accepting XTP bytes.
 * User is a borrowed xtlsstream on its owning Worker. Successful short writes
 * report Written exactly. No event replacement, retain, or cleartext fallback. */
XTP_API xtpresult xtpTlsWrite(void* Stream,const xtpoutput* Output,size_t* Accepted);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xtp/include/xtp/ws.h */
/* ========================================================================== */

#ifndef XTP_WS_H
#define XTP_WS_H
#if defined(XTP_FEATURE_XTP_WS)
/* Host maps MessageBegin/Data/End into this cursor; no native event takeover.
 * On PAUSED/AGAIN retain unconsumed Data and defer MessageEnd/next message.
 * Never recursively feed XTP in a writer. Compression is disabled by default. */
typedef struct xtpwsinput {xtpsession* Session;uint64_t Binding;uint32_t Lane;bool Active;} xtpwsinput;
XRT_EXTERN_C_BEGIN
XTP_API xtpresult xtpWsBegin(xtpwsinput* Input,xwsopcode Opcode);
XTP_API xtpresult xtpWsData(xtpwsinput* Input,xbytesview Data,uint64_t NowUs,xtpreadprogress* Progress);
XTP_API xtpresult xtpWsEnd(xtpwsinput* Input,uint64_t NowUs,xtpreadprogress* Progress);
/* xtpDrain writer; User is borrowed OPEN xwsconn, subprotocol must be xtp.v3.
 * Complete Binary RECORD only. Source is copied by native xws before return. */
XTP_API xtpresult xtpWsWrite(void* Connection,const xtpoutput* Output,size_t* Accepted);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xtp/include/xtp/xrudp.h */
/* ========================================================================== */

#ifndef XTP_XRUDP_H
#define XTP_XRUDP_H
#if defined(XTP_FEATURE_XTP_XRUDP)
typedef struct xtpxrudpwriter {
    xrudpsession* Session;uint32_t StreamID,ChannelID;uint64_t NowUs;
} xtpxrudpwriter;
XRT_EXTERN_C_BEGIN
/* Borrow an authenticated/promoted xrudp session; never add TLS or another
 * fragmenter. STREAM maps to raw BYTES StreamID (0=>1); unreliable RECORD maps
 * to configured ChannelID and must contain one whole NOTIFY. Refresh NowUs
 * before each Drain. Current DatagramSendLimit is checked on every offer.
 * LIMIT is permanent for that payload: Drain drops only the unsent NOTIFY,
 * completes it LOCAL_ERROR/NOT_SENT, and reports Dropped; no stream fallback.
 * Host supplies readable stream bytes / whole datagrams to the matching XTP
 * lane, and calls native Poll after work and at native Due. Own no events/keys. */
XTP_API xtpresult xtpXrudpWrite(void* Writer,const xtpoutput* Output,size_t* Accepted);
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
    xtpreaderconfig c;
    if(!out||!events||!events->Frame||!xrtMemRangeValid(out,sizeof(*out))||
        !xrtMemRangeValid(events,sizeof(*events))||xtp__Overlap(out,sizeof(*out),events,sizeof(*events)))return XTP_E_ARGUMENT;
    if(input){if(!xrtMemRangeValid(input,sizeof(*input))||xtp__Overlap(out,sizeof(*out),input,sizeof(*input)))return XTP_E_ARGUMENT;c=*input;}
    else xtpReaderConfigInit(&c);
    if(xtp__ReaderConfig(&c)!=XTP_OK)return XTP_E_ARGUMENT;
    if(!c.Realloc)c.Realloc=xtpReaderMemory;
    xtpreader* r=c.Realloc(c.AllocatorUser,NULL,sizeof(*r));if(!r)return XTP_E_MEMORY;
    memset(r,0,sizeof(*r));r->Config=c;r->Events=*events;r->User=user;r->Peak=sizeof(*r);*out=r;return XTP_OK;
}
xtpresult xtp__ReaderConfig(const xtpreaderconfig* c)
{
    xtpcodecconfig checked;
    return xtp__CodecConfig(&c->Codec,&checked)!=XTP_OK || c->Mode>XTP_INPUT_RECORD || c->Mode<XTP_INPUT_STREAM ||
        c->MaxStorageBytes<sizeof(xtpreader)+32 || !c->MaxFrames || !c->MaxBad || !c->MaxConsecutiveBad ||
        !c->ReceiveTimeoutUs || c->MaxWorkBytes<(uint64_t)32+c->Codec.MaxCommandBytes+c->Codec.MaxMetadataBytes?XTP_E_ARGUMENT:XTP_OK;
}
static void xtpReaderRelease(xtpreader* r)
{if(r->Buffer)r->Config.Realloc(r->Config.AllocatorUser,r->Buffer,0);r->Buffer=NULL;r->Capacity=0;}
XTP_API xtpresult xtpReaderDestroy(xtpreader* r)
{
    if(!r)return XTP_OK;
    if(r->Busy)return XTP_E_STATE;
    r->Busy=true;
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
    while(p->Frames<r->Config.MaxFrames && p->Dropped<r->Config.MaxFrames-p->Frames && p->WorkBytes<r->Config.MaxWorkBytes){
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
    r->Busy=true;
    xtpReaderClear(r);r->Failure=XTP_OK;r->Paused=r->Ended=r->DropRecord=r->PendingEnd=false;
    r->Frames=r->Dropped=0;r->Consecutive=0;r->Busy=false;return XTP_OK;
}
XTP_API xtpresult xtpReaderDrive(xtpreader* r,uint64_t now,uint64_t* due)
{
    if(!r||r->Busy||!xtpReaderOutput(r,(uintptr_t)due,sizeof(*due))||now<r->LastNow)return XTP_E_ARGUMENT;
    xtpreadprogress p={0};r->Busy=true;
    xtpresult result=xtpReaderRun(r,(xbytesview){NULL,0},false,now,&p);
    r->Busy=false;*due=r->Deadline;return result==XTP_MORE?XTP_OK:result;
}
/* Session maintenance shares one work budget across independent lanes. */
xtpresult xtp__ReaderDriveBudget(xtpreader* r,uint64_t now,size_t frames,size_t* work,uint64_t* due)
{
    size_t oldframes=r->Config.MaxFrames,oldwork=r->Config.MaxWorkBytes;
    r->Config.MaxFrames=frames<oldframes?frames:oldframes;
    r->Config.MaxWorkBytes=*work<oldwork?*work:oldwork;
    xtpreadprogress p={0};r->Busy=true;
    xtpresult result=xtpReaderRun(r,(xbytesview){NULL,0},false,now,&p);
    r->Busy=false;*due=r->Deadline;*work-=p.WorkBytes;
    r->Config.MaxFrames=oldframes;r->Config.MaxWorkBytes=oldwork;
    return result==XTP_MORE?XTP_OK:result;
}
XTP_API xtpresult xtpReaderInfo(const xtpreader* r,xtpreaderinfo* info)
{
    if(!r||!xtpReaderOutput((xtpreader*)r,(uintptr_t)info,sizeof(*info)))return XTP_E_ARGUMENT;
    *info=(xtpreaderinfo){r->Used,r->Capacity,sizeof(*r)+r->Capacity,r->Peak,r->Frames,r->Dropped,r->Deadline,
        r->Consecutive,r->Failure,r->Paused,r->Ended,r->DropRecord};return XTP_OK;
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xtp/src/session/session.c */
/* ========================================================================== */

#if defined(XTP_FEATURE_XTP_SESSION)
#if defined(XTP_FEATURE_XTP_SESSION)
#define XTP_NONE SIZE_MAX
typedef union xtpmemory {size_t Size;long double Align;void* Pointer;} xtpmemory;
typedef struct xtppending {
    uint64_t ID,Serial,Deadline;void* Tag;size_t Next,Tx,Lane;
    xtpmessagekind Type;bool Active,Sent;
} xtppending;
typedef struct xtptx {
    unsigned char* Bytes;unsigned char Control[32];
    size_t Capacity,Size,Offset,Charge,Next,Previous,Lane,Pending;
    uint64_t ID,Serial,Deadline;void* Tag;
    xtpmessagekind Type;bool Active,Queued,Incoming,Waiting,Notify,Sent;
#if defined(XTP_FEATURE_XTP_DELIVERY)
    bool Replay,RetireAfterSend,RoundStarted;
    size_t RetainNext,RetainPrevious,Attempts;
    uint64_t RetryAt,KeepUntil,Ticket;
#endif
} xtptx;
#if defined(XTP_FEATURE_XTP_DELIVERY)
typedef struct xtpreceipt {uint64_t ID,Until;size_t Lane;unsigned State;} xtpreceipt;
#define XTP_REPLAY(t) ((t)->Replay)
#define XTP_TICKET(t) ((t)->Ticket)
#else
#define XTP_REPLAY(t) false
#define XTP_TICKET(t) ((t)->Serial)
#endif
typedef struct xtphistory {uint64_t ID;int32_t Status;xtpmessagekind Type;} xtphistory;
typedef struct xtpsessionlane {
    xtplane Config;xtpreader* Reader;xtpsession* Session;size_t Head,Tail,Index;
} xtpsessionlane;
struct xtpsession {
    xtpconfig Config;xtpevents Events;void* User;
    xtppending* Pending;xtptx* Tx;xtphistory* History;xtpsessionlane* Lanes;size_t* Buckets;
    size_t BucketCount,FreePending,FreeTx,PendingCount,TxCount,TxBytes,NormalBytes,NormalFrames,Incoming,Storage,Peak,LaneCount,TurnFrames,NextLane;
    uint64_t Cookie[2],NextID,NextSerial,Binding,LastNow,HighID;
    uint64_t Submitted,Completed,Messages,Unknown,Duplicates;
    bool Bound,Busy,Callback,Closing,Closed,Unbinding,CloseRequested;
#if defined(XTP_FEATURE_XTP_DELIVERY)
    xtpreceipt* Receipts;xtpdeliveryidentity Identity;
    size_t RetainHead,RetainTail,Retained,ReceiptCount;
    uint64_t AuthUntil,Lifetime,ResultFloor,Attempts,Retries,ResultAcks;
    bool DeliveryStarted,Suspended,ReplayRequests;
#endif
};
static void* xtpSessionDefault(void* user,void* memory,size_t n)
{(void)user;if(!n){xrtFree(memory);return NULL;}return xrtRealloc(memory,n);}
/* Every owned allocation, including Reader buffers and temporary growth, shares
 * this ledger. Realloc reserves old+new, never just their difference. */
static void* xtpSessionMemory(void* user,void* memory,size_t n)
{
    xtpsession* s=user;xtpmemory* old=memory?(xtpmemory*)memory-1:NULL;size_t oldn=old?old->Size:0;
    if(!n){if(old){s->Storage-=oldn;s->Config.Realloc(s->Config.AllocatorUser,old,0);}return NULL;}
    if(n>SIZE_MAX-sizeof(xtpmemory))return NULL;
    size_t total=n+sizeof(xtpmemory);
    if(total>s->Config.MaxSessionBytes-s->Storage)return NULL;
    size_t peak=s->Storage+total;if(peak>s->Peak)s->Peak=peak;
    xtpmemory* p=s->Config.Realloc(s->Config.AllocatorUser,old,total);if(!p)return NULL;
    p->Size=total;s->Storage=s->Storage-oldn+total;return p+1;
}
bool xtp__SessionAddress(const xtpsession* s,uintptr_t address,size_t n)
{
    const void* p=(const void*)address;
    if(!xrtMemRangeValid(p,n)||xtp__Overlap(s,sizeof(*s),p,n)||
        xtp__Overlap(s->Tx,s->Config.MaxTxFrames*sizeof(*s->Tx),p,n)||
        xtp__Overlap(s->Pending,s->Config.MaxPending*sizeof(*s->Pending),p,n)||
        xtp__Overlap(s->Lanes,s->Config.MaxLanes*sizeof(*s->Lanes),p,n)||
        xtp__Overlap(s->History,s->Config.DedupWindow*sizeof(*s->History),p,n)||
        xtp__Overlap(s->Buckets,s->BucketCount*sizeof(*s->Buckets),p,n))return false;
#if defined(XTP_FEATURE_XTP_DELIVERY)
    if(s->Receipts&&xtp__Overlap(s->Receipts,s->Config.DedupWindow*sizeof(*s->Receipts),p,n))return false;
#endif
    for(size_t i=0;i<s->Config.MaxTxFrames;++i)if(s->Tx[i].Active&&
        xtp__Overlap(s->Tx[i].Bytes,s->Tx[i].Capacity,p,n))return false;
    return true;
}
xtpresult xtp__TransportWrite(xtpsession* s,const xtpoutput* out,
    xtpresult (*write)(void*,const xtpoutput*,size_t*),void* user,size_t* accepted)
{
    if(s->Busy)return XTP_E_STATE;
    s->Busy=true;xtpresult result=write(user,out,accepted);s->Busy=false;return result;
}
uint64_t xtp__SessionTime(const xtpsession* s){return s->LastNow;}
xtpresult xtp__SessionObserve(xtpsession* s,uint64_t now)
{if(s->Busy)return XTP_E_STATE;if(now<s->LastNow)return XTP_E_ARGUMENT;s->LastNow=now;return XTP_OK;}
static uint64_t xtpSessionDue(uint64_t now,uint64_t delay)
{return UINT64_MAX-now<delay?UINT64_MAX:now+delay;}
static void xtpSessionFault(xtpsession* s,xtpresult reason,size_t lane,bool fatal)
{
    if(fatal)s->CloseRequested=true;
    if(s->Events.OnFault){xtpfault f={reason,s->Binding,lane<s->LaneCount?s->Lanes[lane].Config.ID:0,fatal};
        s->Callback=true;s->Events.OnFault(s,&f,s->User);s->Callback=false;}
}
XTP_API void xtpConfigInit(xtpconfig* c)
{
    if(!c)return;
    memset(c,0,sizeof(*c));xtpReaderConfigInit(&c->Reader);
    c->MaxPending=64;c->MaxIncomingRequests=64;c->MaxLanes=4;c->MaxTxFrames=256;c->MaxTxBytes=1048576;
    c->ResponseReserveBytes=262144;c->ResponseReserveFrames=16;c->MaxResponseBytes=65536;
    c->MaxSessionBytes=16777216;c->DedupWindow=1024;
    c->DefaultOperationTimeoutUs=c->IncomingRequestTimeoutUs=5000000;
    xtpDeliveryConfigInit(&c->Delivery);c->Delivery.Enabled=false;
}
static void xtpSessionFreeTables(xtpsession* s)
{
    xtpSessionMemory(s,s->Lanes,0);xtpSessionMemory(s,s->Pending,0);xtpSessionMemory(s,s->Tx,0);
    xtpSessionMemory(s,s->History,0);xtpSessionMemory(s,s->Buckets,0);
#if defined(XTP_FEATURE_XTP_DELIVERY)
    xtpSessionMemory(s,s->Receipts,0);
#endif
}
XTP_API xtpresult xtpCreate(const xtpconfig* input,const xtpevents* events,void* user,xtpsession** out)
{
    xtpconfig c;xtpcodecconfig codec;
    if(!out||!events||!events->OnMessage||!xrtMemRangeValid(out,sizeof(*out))||
        !xrtMemRangeValid(events,sizeof(*events))||xtp__Overlap(out,sizeof(*out),events,sizeof(*events)))return XTP_E_ARGUMENT;
    if(input){if(!xrtMemRangeValid(input,sizeof(*input))||xtp__Overlap(out,sizeof(*out),input,sizeof(*input)))return XTP_E_ARGUMENT;c=*input;}
    else xtpConfigInit(&c);
    if(xtp__CodecConfig(&c.Reader.Codec,&codec)!=XTP_OK||!c.MaxPending||c.MaxPending>4096||
        !c.MaxIncomingRequests||c.MaxIncomingRequests>4096||!c.MaxLanes||c.MaxLanes>64||
        c.MaxTxFrames<2||c.MaxTxFrames>16384||c.MaxResponseBytes<32||c.MaxResponseBytes>codec.MaxFrameBytes||
        c.MaxTxBytes<c.MaxResponseBytes||c.ResponseReserveBytes<32||c.ResponseReserveBytes>=c.MaxTxBytes||
        !c.ResponseReserveFrames||c.ResponseReserveFrames>=c.MaxTxFrames||c.MaxSessionBytes<sizeof(xtpsession)||
        !c.DedupWindow||c.DedupWindow>65536||!c.DefaultOperationTimeoutUs||!c.IncomingRequestTimeoutUs||
        !c.Reader.MaxFrames||c.Reader.MaxFrames>65536||!c.Reader.MaxBad||!c.Reader.MaxConsecutiveBad||
        xtp__ReaderConfig(&c.Reader)!=XTP_OK)return XTP_E_ARGUMENT;
    if(c.Delivery.Enabled){
#if defined(XTP_FEATURE_XTP_DELIVERY)
        if(!c.Delivery.MaxReplayMessages||c.Delivery.MaxReplayMessages>c.MaxTxFrames||
            !c.Delivery.MaxAttempts||c.Delivery.MaxAttempts>64||!c.Delivery.MaxLifetimeUs||
            c.Delivery.MaxLifetimeUs>3600000000ULL||!c.Delivery.RetryMinUs||
            c.Delivery.RetryMinUs>c.Delivery.RetryMaxUs||c.Delivery.RetryMaxUs>c.Delivery.MaxLifetimeUs)return XTP_E_ARGUMENT;
#else
        return XTP_E_STATE;
#endif
    }
    if(!c.Realloc)c.Realloc=xtpSessionDefault;
    xtpsession* s=c.Realloc(c.AllocatorUser,NULL,sizeof(*s));if(!s)return XTP_E_MEMORY;
    memset(s,0,sizeof(*s));s->Config=c;s->Events=*events;s->User=user;s->Storage=s->Peak=sizeof(*s);
    s->BucketCount=1;while(s->BucketCount<c.MaxPending*2)s->BucketCount*=2;
    s->Lanes=xtpSessionMemory(s,NULL,c.MaxLanes*sizeof(*s->Lanes));
    s->Pending=xtpSessionMemory(s,NULL,c.MaxPending*sizeof(*s->Pending));
    s->Tx=xtpSessionMemory(s,NULL,c.MaxTxFrames*sizeof(*s->Tx));
    s->History=xtpSessionMemory(s,NULL,c.DedupWindow*sizeof(*s->History));
    s->Buckets=xtpSessionMemory(s,NULL,s->BucketCount*sizeof(*s->Buckets));
#if defined(XTP_FEATURE_XTP_DELIVERY)
    s->RetainHead=s->RetainTail=XTP_NONE;s->ResultFloor=1;
    if(c.Delivery.Enabled){s->Receipts=xtpSessionMemory(s,NULL,c.DedupWindow*sizeof(*s->Receipts));
        if(!s->Receipts)goto memory;
        memset(s->Receipts,0,c.DedupWindow*sizeof(*s->Receipts));}
#endif
    if(!s->Lanes||!s->Pending||!s->Tx||!s->History||!s->Buckets)goto memory;
    memset(s->Lanes,0,c.MaxLanes*sizeof(*s->Lanes));memset(s->Pending,0,c.MaxPending*sizeof(*s->Pending));
    memset(s->Tx,0,c.MaxTxFrames*sizeof(*s->Tx));memset(s->History,0,c.DedupWindow*sizeof(*s->History));
    for(size_t i=0;i<c.MaxPending;++i)s->Pending[i].Next=i+1<c.MaxPending?i+1:XTP_NONE;
    for(size_t i=0;i<c.MaxTxFrames;++i)s->Tx[i].Next=i+1<c.MaxTxFrames?i+1:XTP_NONE;
    for(size_t i=0;i<s->BucketCount;++i)s->Buckets[i]=XTP_NONE;
    if(!xrtSecureRandom(s->Cookie,sizeof(s->Cookie))||!s->Cookie[0]||!s->Cookie[1]){
        xtpSessionFreeTables(s);c.Realloc(c.AllocatorUser,s,0);return XTP_E_STATE;}
    s->NextID=s->NextSerial=1;*out=s;return XTP_OK;
memory:
    xtpSessionFreeTables(s);c.Realloc(c.AllocatorUser,s,0);return XTP_E_MEMORY;
}
static size_t xtpSessionLane(xtpsession* s,uint32_t id)
{
    if(!id)return s->LaneCount?0:XTP_NONE;
    for(size_t i=0;i<s->LaneCount;++i)if(s->Lanes[i].Config.ID==id)return i;
    return XTP_NONE;
}
bool xtp__RecordLane(xtpsession* s,uint64_t binding,uint32_t lane)
{
    if(s->Busy||!s->Bound||binding!=s->Binding)return false;
    size_t i=xtpSessionLane(s,lane);return i!=XTP_NONE&&s->Lanes[i].Config.Mode==XTP_INPUT_RECORD&&s->Lanes[i].Config.Reliable;
}
static size_t xtpPendingFind(xtpsession* s,uint64_t id)
{
    size_t i=s->Buckets[(size_t)id&(s->BucketCount-1)];
    while(i!=XTP_NONE){if(s->Pending[i].ID==id)return i;i=s->Pending[i].Next;}return XTP_NONE;
}
static void xtpTxUnqueue(xtpsession* s,size_t i)
{
    xtptx* t=&s->Tx[i];if(!t->Queued)return;
    xtpsessionlane* l=&s->Lanes[t->Lane];
    if(t->Previous!=XTP_NONE)s->Tx[t->Previous].Next=t->Next;else l->Head=t->Next;
    if(t->Next!=XTP_NONE)s->Tx[t->Next].Previous=t->Previous;else l->Tail=t->Previous;
    t->Queued=false;
}
static bool xtpTxQueue(xtpsession* s,size_t i)
{
    xtptx* t=&s->Tx[i];xtpsessionlane* l=&s->Lanes[t->Lane];
    t->Queued=true;t->Previous=l->Tail;t->Next=XTP_NONE;
    if(l->Tail!=XTP_NONE)s->Tx[l->Tail].Next=i;else l->Head=i;l->Tail=i;return true;
}
static size_t xtpTxTake(xtpsession* s,size_t lane,size_t charge,bool normal)
{
    if(s->FreeTx==XTP_NONE||!s->NextSerial||charge>s->Config.MaxTxBytes-s->TxBytes||
        (normal&&(s->NormalFrames>=s->Config.MaxTxFrames-s->Config.ResponseReserveFrames||
        charge>s->Config.MaxTxBytes-s->Config.ResponseReserveBytes-s->NormalBytes)))return XTP_NONE;
    size_t i=s->FreeTx;xtptx* t=&s->Tx[i];s->FreeTx=t->Next;memset(t,0,sizeof(*t));
    t->Active=true;t->Lane=lane;t->Charge=charge;t->Pending=XTP_NONE;t->Serial=s->NextSerial++;
    t->Next=t->Previous=XTP_NONE;t->Notify=normal; /* normal Type finalized before publishing. */
#if defined(XTP_FEATURE_XTP_DELIVERY)
    t->Ticket=t->Serial;t->RetainNext=t->RetainPrevious=XTP_NONE;
#endif
    ++s->TxCount;s->TxBytes+=charge;if(normal){++s->NormalFrames;s->NormalBytes+=charge;}return i;
}
static void xtpTxFree(xtpsession* s,size_t i)
{
    xtptx* t=&s->Tx[i];xtpTxUnqueue(s,i);xtpSessionMemory(s,t->Bytes,0);
#if defined(XTP_FEATURE_XTP_DELIVERY)
    if(t->Replay){
        if(t->RetainPrevious!=XTP_NONE)s->Tx[t->RetainPrevious].RetainNext=t->RetainNext;else s->RetainHead=t->RetainNext;
        if(t->RetainNext!=XTP_NONE)s->Tx[t->RetainNext].RetainPrevious=t->RetainPrevious;else s->RetainTail=t->RetainPrevious;
        --s->Retained;}
#endif
    s->TxBytes-=t->Charge;--s->TxCount;
    if(t->Type==XTP_REQUEST||t->Type==XTP_PUSH||t->Notify){s->NormalBytes-=t->Charge;--s->NormalFrames;}
    if(t->Waiting)--s->Incoming;
    t->Active=false;t->Waiting=false;t->Bytes=NULL;t->Next=s->FreeTx;s->FreeTx=i;
}
static void xtpSessionComplete(xtpsession* s,size_t pi,xtpcompletion* c)
{
    xtppending p=s->Pending[pi];size_t bucket=(size_t)p.ID&(s->BucketCount-1),*link=&s->Buckets[bucket];
    while(*link!=pi)link=&s->Pending[*link].Next;
    *link=p.Next;s->Pending[pi].Active=false;s->Pending[pi].Next=s->FreePending;s->FreePending=pi;--s->PendingCount;
    if(p.Tx!=XTP_NONE&&s->Tx[p.Tx].Active){
#if defined(XTP_FEATURE_XTP_DELIVERY)
        xtptx* t=&s->Tx[p.Tx];
        if(t->Replay&&t->Queued&&t->Offset&&t->Offset<t->Size&&!s->Unbinding&&!s->Closing&&!s->CloseRequested){
            t->Pending=XTP_NONE;t->RetireAfterSend=true;
        }else
#endif
        xtpTxFree(s,p.Tx);}
#if defined(XTP_FEATURE_XTP_DELIVERY)
    if(s->Receipts){xtpreceipt* r=&s->Receipts[p.ID%s->Config.DedupWindow];
        if(r->ID==p.ID&&r->State==1){r->State=0;--s->ReceiptCount;}}
#endif
    c->Operation=(xtpoperationtoken){{s->Cookie[0],p.Serial}};c->Type=p.Type;c->MessageID=p.ID;c->Tag=p.Tag;
    ++s->Completed;if(s->Events.OnComplete){s->Callback=true;s->Events.OnComplete(s,c,s->User);s->Callback=false;}
}
static void xtpNotifyComplete(xtpsession* s,size_t i,xtpfinish reason,xtpresult error)
{
    xtptx t=s->Tx[i];xtpcompletion c={0};c.Operation=(xtpoperationtoken){{s->Cookie[0],t.Serial}};
    c.Type=XTP_NOTIFY;c.Tag=t.Tag;c.Reason=reason;c.Knowledge=t.Sent?XTP_KNOW_UNKNOWN:XTP_KNOW_NOT_SENT;
    c.LocalResult=error;xtpTxFree(s,i);++s->Completed;
    if(s->Events.OnComplete){s->Callback=true;s->Events.OnComplete(s,&c,s->User);s->Callback=false;}
}
#if defined(XTP_FEATURE_XTP_DELIVERY)
static void xtpRetain(xtpsession* s,size_t i,uint64_t until)
{
    xtptx* t=&s->Tx[i];t->Replay=true;t->KeepUntil=until;t->RetainPrevious=s->RetainTail;t->RetainNext=XTP_NONE;
    if(s->RetainTail!=XTP_NONE)s->Tx[s->RetainTail].RetainNext=i;else s->RetainHead=i;
    s->RetainTail=i;++s->Retained;
}
static bool xtpRoundReset(xtpsession* s,xtptx* t)
{
    if(!s->NextSerial){s->CloseRequested=true;return false;}
    t->Offset=0;t->RoundStarted=false;t->RetryAt=0;t->Ticket=s->NextSerial++;return true;
}
static bool xtpKeep(const xtpsession* s,const xtptx* t)
{return s->Unbinding&&!s->Closing&&!s->CloseRequested&&t->Replay&&!t->RetireAfterSend;}
static bool xtpRetriable(const xtpsession* s,const xtptx* t)
{return !t->Replay||(t->Queued&&t->RoundStarted)||t->Attempts<s->Config.Delivery.MaxAttempts;}
static void xtpReceiptExpire(xtpsession* s,xtpreceipt* r)
{
    if(r->ID>=s->ResultFloor)s->ResultFloor=r->ID==UINT64_MAX?UINT64_MAX:r->ID+1;
    r->State=0;--s->ReceiptCount;
}
#else
#define xtpKeep(s,t) false
#define xtpRetriable(s,t) true
#endif
static xtpresult xtpSessionDrain(xtpsession* s)
{
    size_t budget=s->TurnFrames;
    for(size_t i=0;i<s->Config.MaxPending&&budget;++i)if(s->Pending[i].Active){
        if(s->Pending[i].Tx!=XTP_NONE&&xtpKeep(s,&s->Tx[s->Pending[i].Tx]))continue;
        xtpcompletion c={0};c.Reason=XTP_FINISH_CLOSED;c.Knowledge=s->Pending[i].Sent?XTP_KNOW_UNKNOWN:XTP_KNOW_NOT_SENT;
        xtpSessionComplete(s,i,&c);--budget;}
    for(size_t i=0;i<s->Config.MaxTxFrames&&budget;++i)if(s->Tx[i].Active){
        if(xtpKeep(s,&s->Tx[i])){xtpTxUnqueue(s,i);continue;}
        if(s->Tx[i].Notify)xtpNotifyComplete(s,i,XTP_FINISH_CLOSED,XTP_OK);else xtpTxFree(s,i);--budget;}
    bool remaining=false;
    for(size_t i=0;i<s->Config.MaxPending;++i)if(s->Pending[i].Active&&
        (s->Pending[i].Tx==XTP_NONE||!xtpKeep(s,&s->Tx[s->Pending[i].Tx])))remaining=true;
    for(size_t i=0;i<s->Config.MaxTxFrames;++i)if(s->Tx[i].Active&&!xtpKeep(s,&s->Tx[i]))remaining=true;
    if(remaining)return XTP_AGAIN;
    for(size_t i=0;i<s->LaneCount;++i){xtpReaderDestroy(s->Lanes[i].Reader);s->Lanes[i].Reader=NULL;}
#if defined(XTP_FEATURE_XTP_DELIVERY)
    s->Suspended=s->Unbinding&&s->DeliveryStarted&&!s->Closing&&!s->CloseRequested;
    if(!s->Suspended)s->LaneCount=0;
    if(s->Closing||s->CloseRequested){if(s->Receipts)memset(s->Receipts,0,s->Config.DedupWindow*sizeof(*s->Receipts));s->ReceiptCount=0;}
#else
    s->LaneCount=0;
#endif
    s->Bound=false;s->Unbinding=false;
    if(s->CloseRequested)s->Closing=true;
    if(s->Closing)s->Closed=true;
    return s->Closed?XTP_CLOSED:XTP_OK;
}
static xtpresult xtpSessionLeave(xtpsession* s,xtpresult result)
{
    if(s->CloseRequested){s->Closing=true;s->Bound=false;}
    if(s->Closing||s->Unbinding)result=xtpSessionDrain(s);
    s->Busy=false;return result;
}
static void xtpHistoryPut(xtpsession* s,uint64_t id,xtpmessagekind type,int32_t status)
{
    if(id>s->HighID)s->HighID=id;
    s->History[id%s->Config.DedupWindow]=(xtphistory){id,status,type};
}
static bool xtpDecisionValid(xtpdecision d)
{
    if(d.Action==XTP_ACTION_ACCEPT||d.Action==XTP_ACTION_PAUSE)return d.Status==0;
    if(d.Action==XTP_ACTION_BUSY)return d.Status==XTP_STATUS_BUSY;
    return d.Action==XTP_ACTION_REJECT&&xtp__Status(d.Status)&&d.Status!=0&&
        d.Status!=XTP_STATUS_BUSY&&d.Status!=XTP_STATUS_STATE_LOST&&d.Status!=XTP_STATUS_DEADLINE_EXCEEDED;
}
static bool xtpControl(xtpsession* s,size_t i,xtpmessagekind type,uint64_t id,int32_t status)
{
    xtptx* t=&s->Tx[i];xtppacket p={0};p.Type=type;p.MessageID=id;p.Status=status;size_t n;
    if(t->Waiting){t->Waiting=false;--s->Incoming;}
    xtpSessionMemory(s,t->Bytes,0);t->Bytes=NULL;t->Capacity=0;
    s->TxBytes-=t->Charge-32;t->Charge=t->Size=32;t->ID=id;t->Type=type;
    t->Deadline=xtpSessionDue(s->LastNow,s->Config.DefaultOperationTimeoutUs);
#if defined(XTP_FEATURE_XTP_DELIVERY)
    if(t->Replay)t->Deadline=t->KeepUntil;
#endif
    if(xtpEncode(&p,1,&s->Config.Reader.Codec,t->Control,32,&n)!=XTP_OK)return false;
    if(s->Bound)xtpTxQueue(s,i);
    return true;
}
static xtpflow xtpSendControl(xtpsession* s,size_t lane,const xtpframeview* f,int32_t status)
{
    size_t i=xtpTxTake(s,lane,32,false);if(i==XTP_NONE)return XTP_FLOW_PAUSE;
    if(!xtpControl(s,i,f->Type==XTP_PUSH?XTP_ACK:XTP_RESPONSE,f->MessageID,status)){
        xtpTxFree(s,i);xtpSessionFault(s,XTP_E_STATE,lane,true);return XTP_FLOW_STOP;}
    return XTP_FLOW_ACCEPT;
}
#if defined(XTP_FEATURE_XTP_DELIVERY)
static bool xtpResultAck(xtpsession* s,size_t lane,uint64_t id)
{
    /* Coalesce queued receipts without owning another queue. */
    for(size_t j=0;j<s->Config.MaxTxFrames;++j){xtptx* q=&s->Tx[j];
        if(q->Active&&q->Type==XTP_ACK&&q->ID==id&&q->Lane==lane&&
            !q->Bytes&&(q->Control[17]&XTP_FLAG_RESULT_ACK))return true;}
    size_t i=xtpTxTake(s,lane,32,false);if(i==XTP_NONE)return false;
    if(!xtpControl(s,i,XTP_ACK,id,0)){xtpTxFree(s,i);return false;}
    s->Tx[i].Control[17]|=XTP_FLAG_RESULT_ACK;++s->ResultAcks;return true;
}
static void xtpRetireResult(xtpsession* s,size_t i)
{
    xtptx* t=&s->Tx[i];
    if(t->Queued&&t->Offset&&t->Offset<t->Size)t->RetireAfterSend=true;else xtpTxFree(s,i);
}
static void xtpRetryBusy(xtpsession* s,size_t i)
{
    xtptx* t=&s->Tx[i];
    /* A duplicate BUSY cannot restart a partial round or extend its deadline. */
    if(t->Queued||t->RetryAt)return;
    uint64_t delay=s->Config.Delivery.RetryMinUs;
    for(size_t k=1;k<t->Attempts&&delay<s->Config.Delivery.RetryMaxUs;++k){
        delay=delay>s->Config.Delivery.RetryMaxUs/2?s->Config.Delivery.RetryMaxUs:delay*2;}
    uint64_t jitter=(t->ID^s->Cookie[0]^t->Attempts)%(delay/4+1);
    if(xtpRoundReset(s,t)){t->RetryAt=xtpSessionDue(s->LastNow,delay-delay/4+jitter);++s->Retries;}
}
#endif
static xtpflow xtpSessionFrame(xtpreader* reader,const xtpframeview* f,void* user)
{
    xtpsessionlane* lane=user;xtpsession* s=lane->Session;size_t li=lane->Index;(void)reader;
    if(s->TurnFrames)--s->TurnFrames;
    if(s->CloseRequested)return XTP_FLOW_STOP;
#if defined(XTP_FEATURE_XTP_DELIVERY)
    if(s->DeliveryStarted&&s->LastNow>=s->AuthUntil){xtpSessionFault(s,XTP_E_DEADLINE,li,true);return XTP_FLOW_STOP;}
#endif
    if(!lane->Config.Reliable&&f->Type!=XTP_NOTIFY){xtpSessionFault(s,XTP_E_TYPE,li,false);return XTP_FLOW_ACCEPT;}
    if(f->Type==XTP_RESPONSE||f->Type==XTP_ACK){
#if defined(XTP_FEATURE_XTP_DELIVERY)
        if(f->ResultAck){
            for(size_t i=s->RetainHead;i!=XTP_NONE;i=s->Tx[i].RetainNext){xtptx* t=&s->Tx[i];
                if(t->Incoming&&t->ID==f->MessageID&&t->Lane==li&&!t->Waiting&&t->Sent){xtpRetireResult(s,i);return XTP_FLOW_ACCEPT;}}
            ++s->Unknown;return XTP_FLOW_ACCEPT;}
#endif
        size_t pi=xtpPendingFind(s,f->MessageID);
#if defined(XTP_FEATURE_XTP_DELIVERY)
        if(pi==XTP_NONE&&f->Type==XTP_RESPONSE&&s->Receipts){xtpreceipt* r=&s->Receipts[f->MessageID%s->Config.DedupWindow];
            if(r->State==2&&r->ID==f->MessageID&&r->Lane==li&&r->Until>s->LastNow){
                if(!xtpResultAck(s,li,f->MessageID))return XTP_FLOW_PAUSE;
                ++s->Duplicates;return XTP_FLOW_ACCEPT;}}
#endif
        if(f->ResultAck||pi==XTP_NONE||s->Pending[pi].Lane!=li||
            (s->Pending[pi].Type==XTP_REQUEST)!=(f->Type==XTP_RESPONSE)){
            ++s->Unknown;return XTP_FLOW_ACCEPT;}
        xtppending* p=&s->Pending[pi];
        /* Host reports transport acceptance before delivering its resulting
         * network events. An unsolicited result cannot complete an unsent ID. */
        if(p->Tx!=XTP_NONE&&!(XTP_REPLAY(&s->Tx[p->Tx])&&p->Sent)){++s->Unknown;
            if(s->Tx[p->Tx].Offset){xtpSessionFault(s,XTP_E_STATE,li,true);return XTP_FLOW_STOP;}
            return XTP_FLOW_ACCEPT;}
#if defined(XTP_FEATURE_XTP_DELIVERY)
        if(p->Tx!=XTP_NONE&&s->Tx[p->Tx].Replay&&s->LastNow<p->Deadline){
            if(f->Status==XTP_STATUS_BUSY){xtpRetryBusy(s,p->Tx);return XTP_FLOW_ACCEPT;}
            if(f->Type==XTP_RESPONSE){
                if(!xtpResultAck(s,li,f->MessageID))return XTP_FLOW_PAUSE;
                xtpreceipt* r=&s->Receipts[p->ID%s->Config.DedupWindow];r->State=2;
                r->Until=xtpSessionDue(s->LastNow,s->Lifetime);if(r->Until>s->AuthUntil)r->Until=s->AuthUntil;}}
#endif
        xtpcompletion c={0};c.Reason=f->Type==XTP_RESPONSE?XTP_FINISH_RESPONSE:XTP_FINISH_ACK;
        c.HasRemoteStatus=true;c.Status=f->Status;
        c.Knowledge=f->Status==XTP_STATUS_STATE_LOST?XTP_KNOW_UNKNOWN:
            f->Type==XTP_RESPONSE?XTP_KNOW_REPLIED:f->Status?XTP_KNOW_REJECTED:XTP_KNOW_ACCEPTED;
        if(f->Type==XTP_RESPONSE){c.Params=f->Params;c.Body=f->Body;c.Packet=f->Packet;}
        if(s->LastNow>=p->Deadline){c=(xtpcompletion){0};c.Reason=XTP_FINISH_TIMEOUT;
            c.LocalResult=XTP_E_DEADLINE;c.Knowledge=XTP_KNOW_UNKNOWN;}
        xtpSessionComplete(s,pi,&c);return s->CloseRequested?XTP_FLOW_STOP:XTP_FLOW_ACCEPT;
    }
    if(f->Type!=XTP_NOTIFY){
        xtphistory* h=&s->History[f->MessageID%s->Config.DedupWindow];
        for(size_t i=0;i<s->Config.MaxTxFrames;++i)if(s->Tx[i].Active&&s->Tx[i].Incoming&&s->Tx[i].ID==f->MessageID){
            ++s->Duplicates;
#if defined(XTP_FEATURE_XTP_DELIVERY)
            xtptx* t=&s->Tx[i];
            if(t->Replay&&!t->Waiting&&!t->Queued&&t->KeepUntil>s->LastNow&&t->Lane==li&&xtpRetriable(s,t)){
                if(xtpRoundReset(s,t))xtpTxQueue(s,i);}
#endif
            return XTP_FLOW_ACCEPT;}
        if(h->ID==f->MessageID){++s->Duplicates;
            return xtpSendControl(s,li,f,h->Type==XTP_PUSH&&f->Type==XTP_PUSH?h->Status:XTP_STATUS_STATE_LOST);}
        if(s->HighID>=s->Config.DedupWindow&&f->MessageID<=s->HighID-s->Config.DedupWindow){
            ++s->Duplicates;return xtpSendControl(s,li,f,XTP_STATUS_STATE_LOST);}
        if(f->Replayable){
#if defined(XTP_FEATURE_XTP_DELIVERY)
            if(!s->Config.Delivery.Enabled||!s->DeliveryStarted||s->LastNow>=s->AuthUntil||
                (f->Type==XTP_REQUEST&&!s->ReplayRequests))
#endif
            return xtpSendControl(s,li,f,XTP_STATUS_UNSUPPORTED);}
    }
    size_t ti=XTP_NONE;
    if(f->Type==XTP_PUSH){ti=xtpTxTake(s,li,32,false);if(ti==XTP_NONE)return XTP_FLOW_PAUSE;}
    if(f->Type==XTP_REQUEST){
#if defined(XTP_FEATURE_XTP_DELIVERY)
        if(f->Replayable&&s->Retained>=s->Config.Delivery.MaxReplayMessages)return xtpSendControl(s,li,f,XTP_STATUS_BUSY);
#endif
        if(s->Incoming>=s->Config.MaxIncomingRequests)return xtpSendControl(s,li,f,XTP_STATUS_BUSY);
        ti=xtpTxTake(s,li,s->Config.MaxResponseBytes,false);if(ti==XTP_NONE)return xtpSendControl(s,li,f,XTP_STATUS_BUSY);
        xtptx* t=&s->Tx[ti];t->Bytes=xtpSessionMemory(s,NULL,s->Config.MaxResponseBytes);
        if(!t->Bytes){xtpTxFree(s,ti);return xtpSendControl(s,li,f,XTP_STATUS_BUSY);}
        t->Capacity=s->Config.MaxResponseBytes;t->Type=XTP_RESPONSE;t->ID=f->MessageID;t->Incoming=t->Waiting=true;
        t->Deadline=xtpSessionDue(s->LastNow,s->Config.IncomingRequestTimeoutUs);++s->Incoming;
#if defined(XTP_FEATURE_XTP_DELIVERY)
        if(f->Replayable){uint64_t until=xtpSessionDue(s->LastNow,s->Lifetime);if(until>s->AuthUntil)until=s->AuthUntil;
            xtpRetain(s,ti,until);if(t->Deadline>until)t->Deadline=until;}
#endif
    }
    xtpmessageview m={0};m.Type=f->Type;m.MessageID=f->MessageID;m.Command=f->Command;m.Params=f->Params;m.Body=f->Body;m.Packet=f->Packet;
    if(f->Type==XTP_REQUEST)m.Request=(xtprequesttoken){{s->Cookie[0],s->Cookie[1],ti+1,s->Tx[ti].Serial}};
    s->Callback=true;xtpdecision d=s->Events.OnMessage(s,&m,s->User);s->Callback=false;++s->Messages;
    bool replied=ti!=XTP_NONE&&f->Type==XTP_REQUEST&&!s->Tx[ti].Waiting;
    if(!xtpDecisionValid(d)||(replied&&d.Action!=XTP_ACTION_ACCEPT)){
        xtpSessionFault(s,XTP_E_STATE,li,true);return XTP_FLOW_STOP;}
    if(s->CloseRequested)return XTP_FLOW_STOP;
    if(d.Action==XTP_ACTION_PAUSE){if(ti!=XTP_NONE)xtpTxFree(s,ti);return XTP_FLOW_PAUSE;}
    if(f->Type==XTP_NOTIFY)return XTP_FLOW_ACCEPT;
    if(d.Action!=XTP_ACTION_BUSY)xtpHistoryPut(s,f->MessageID,f->Type,d.Status);
#if defined(XTP_FEATURE_XTP_DELIVERY)
    if(d.Action==XTP_ACTION_BUSY&&ti!=XTP_NONE&&s->Tx[ti].Replay){
        /* BUSY has no accepted result to retain. Reuse the released slot. */
        xtpTxFree(s,ti);ti=xtpTxTake(s,li,32,false);
        if(ti==XTP_NONE){xtpSessionFault(s,XTP_E_STATE,li,true);return XTP_FLOW_STOP;}}
#endif
    if(f->Type==XTP_PUSH||d.Action!=XTP_ACTION_ACCEPT){
        if(!xtpControl(s,ti,f->Type==XTP_PUSH?XTP_ACK:XTP_RESPONSE,f->MessageID,d.Status)){
            xtpSessionFault(s,XTP_E_STATE,li,true);return XTP_FLOW_STOP;}}
    return XTP_FLOW_ACCEPT;
}
static void xtpSessionDrop(xtpreader* reader,xtpresult reason,void* user)
{xtpsessionlane* lane=user;(void)reader;if(lane->Session->TurnFrames)--lane->Session->TurnFrames;
    xtpSessionFault(lane->Session,reason,lane->Index,false);}
#if defined(XTP_FEATURE_XTP_DELIVERY)
static bool xtpIdentityPart(const unsigned char* bytes,size_t n)
{unsigned char any=0;for(size_t i=0;i<n;++i)any|=bytes[i];return any!=0;}
static bool xtpProof(xtpsession* s,const xtpbinding* b,uint64_t now)
{
    const xtpdeliveryproof* p=b->Delivery;
    if(!s->Config.Delivery.Enabled)return p==NULL;
    if(!b->Authenticated||!p||!xrtMemRangeValid(p,sizeof(*p))||
        p->UntilUs<=now||p->UntilUs==UINT64_MAX||!p->LifetimeUs||p->LifetimeUs>s->Config.Delivery.MaxLifetimeUs||
        !p->PeerReceiveFloor||!p->PeerResultFloor||!xtpIdentityPart(p->Identity.SessionID,16)||
        !xtpIdentityPart(p->Identity.Domain,16)||!xtpIdentityPart(p->Identity.Local,32)||!xtpIdentityPart(p->Identity.Peer,32))return false;
    if(!s->DeliveryStarted)return !p->Resume&&p->StateRetained;
    if(!s->Suspended||!p->Resume||!p->StateRetained||!p->OldBindingDetached||s->AuthUntil<=now||
        memcmp(&p->Identity,&s->Identity,sizeof(s->Identity))||p->LifetimeUs!=s->Lifetime||
        (p->AllowRequests&&s->Config.Delivery.AllowRequests)!=s->ReplayRequests)return false;
    for(size_t i=s->RetainHead;i!=XTP_NONE;i=s->Tx[i].RetainNext){const xtptx* t=&s->Tx[i];
        if((!t->Incoming&&t->Type==XTP_PUSH&&t->ID<p->PeerReceiveFloor)||
            (t->Incoming&&!t->Waiting&&t->ID<p->PeerResultFloor))return false;}
    return true;
}
#endif
XTP_API xtpresult xtpBind(xtpsession* s,const xtpbinding* b,uint64_t now,uint64_t* binding)
{
    if(!s||s->Busy||s->Bound||s->Closing||s->CloseRequested||s->Closed||s->Unbinding||!b||
        !xrtMemRangeValid(b,sizeof(*b))||!b->Lanes||!b->LaneCount||b->LaneCount>s->Config.MaxLanes||
        !xrtMemRangeValid(b->Lanes,b->LaneCount*sizeof(*b->Lanes))||(!b->Authenticated&&!b->AllowPlaintext)||
        !xtp__SessionAddress(s,(uintptr_t)binding,sizeof(*binding))||xtp__Overlap(binding,sizeof(*binding),b,sizeof(*b))||
        xtp__Overlap(binding,sizeof(*binding),b->Lanes,b->LaneCount*sizeof(*b->Lanes))||s->Binding==UINT64_MAX||now<s->LastNow)return XTP_E_ARGUMENT;
    for(size_t i=0;i<b->LaneCount;++i){const xtplane* l=&b->Lanes[i];
        if(!l->ID||(l->Mode!=XTP_INPUT_STREAM&&l->Mode!=XTP_INPUT_RECORD)||
            (!l->Reliable&&l->Mode!=XTP_INPUT_RECORD)||
            (l->Mode==XTP_INPUT_RECORD&&(l->RecordLimit<32||(l->Reliable&&l->RecordLimit<s->Config.MaxResponseBytes))))return XTP_E_ARGUMENT;
        for(size_t j=0;j<i;++j)if(b->Lanes[j].ID==l->ID)return XTP_E_ARGUMENT;}
#if defined(XTP_FEATURE_XTP_DELIVERY)
    if(b->Delivery&&(!xrtMemRangeValid(b->Delivery,sizeof(*b->Delivery))||
        xtp__Overlap(binding,sizeof(*binding),b->Delivery,sizeof(*b->Delivery))))return XTP_E_ARGUMENT;
    if(!xtpProof(s,b,now)){
        if(s->DeliveryStarted)s->CloseRequested=true;
        return XTP_E_STATE;}
    if(s->Suspended){
        if(b->LaneCount!=s->LaneCount)return XTP_E_ARGUMENT;
        for(size_t i=0;i<s->LaneCount;++i){xtplane a=s->Lanes[i].Config,z=b->Lanes[i];
            if(a.ID!=z.ID||a.Mode!=z.Mode||a.Reliable!=z.Reliable||a.RecordLimit!=z.RecordLimit)return XTP_E_ARGUMENT;}}
#else
    if(b->Delivery)return XTP_E_STATE;
#endif
    s->Busy=true;size_t made=0;xtpresult result=XTP_OK;
    for(;made<b->LaneCount;++made){xtpsessionlane* l=&s->Lanes[made];memset(l,0,sizeof(*l));
        l->Config=b->Lanes[made];l->Session=s;l->Index=made;l->Head=l->Tail=XTP_NONE;
        xtpreaderconfig c=s->Config.Reader;c.Mode=l->Config.Mode;c.Realloc=xtpSessionMemory;c.AllocatorUser=s;
        if(c.Mode==XTP_INPUT_RECORD&&c.Codec.MaxFrameBytes>l->Config.RecordLimit)c.Codec.MaxFrameBytes=l->Config.RecordLimit;
        xtpreaderevents e={xtpSessionFrame,xtpSessionDrop};result=xtpReaderCreate(&c,&e,l,&l->Reader);if(result!=XTP_OK)break;}
    if(result!=XTP_OK){for(size_t i=0;i<made;++i){xtpReaderDestroy(s->Lanes[i].Reader);s->Lanes[i].Reader=NULL;}}
    else {s->LaneCount=made;s->Bound=true;++s->Binding;s->LastNow=now;*binding=s->Binding;
#if defined(XTP_FEATURE_XTP_DELIVERY)
        if(s->Config.Delivery.Enabled){const xtpdeliveryproof* p=b->Delivery;
            if(!s->DeliveryStarted){s->Identity=p->Identity;s->AuthUntil=p->UntilUs;s->Lifetime=p->LifetimeUs;
                s->ReplayRequests=p->AllowRequests&&s->Config.Delivery.AllowRequests;s->DeliveryStarted=true;}
            else if(p->UntilUs<s->AuthUntil)s->AuthUntil=p->UntilUs;
            s->Suspended=false;
            /* Cached results precede resubmissions. Retained order is stable. */
            for(unsigned pass=0;pass<2;++pass)for(size_t i=s->RetainHead;i!=XTP_NONE;i=s->Tx[i].RetainNext){xtptx* t=&s->Tx[i];
                if((pass==0)!=t->Incoming||t->Waiting)continue;
                t->Queued=false;
                if(t->Deadline>now&&xtpRetriable(s,t)&&xtpRoundReset(s,t))xtpTxQueue(s,i);}}
#endif
    }
    s->Busy=false;return result;
}
XTP_API xtpresult xtpUnbind(xtpsession* s,uint64_t binding,uint64_t now)
{
    if(!s||s->Busy||!s->Bound||s->Binding!=binding||now<s->LastNow)return XTP_E_STATE;
    s->LastNow=now;s->Busy=true;s->TurnFrames=s->Config.Reader.MaxFrames;
    s->Bound=false;s->Unbinding=true;return xtpSessionLeave(s,XTP_OK);
}
XTP_API xtpresult xtpInput(xtpsession* s,uint64_t binding,uint32_t id,xbytesview in,bool end,uint64_t now,xtpreadprogress* p)
{
    if(!s||s->Busy||!s->Bound||binding!=s->Binding||s->CloseRequested||now<s->LastNow)return XTP_E_STATE;
    size_t li=xtpSessionLane(s,id);if(li==XTP_NONE||!xtp__SessionAddress(s,(uintptr_t)p,sizeof(*p))||
        !xtp__SessionAddress(s,(uintptr_t)in.Data,in.Size))return XTP_E_ARGUMENT;
    s->Busy=true;s->LastNow=now;s->TurnFrames=s->Config.Reader.MaxFrames;
    xtpresult r=xtpReaderInput(s->Lanes[li].Reader,in,end,now,p);
    if(r<0&&r!=XTP_E_ARGUMENT)xtpSessionFault(s,r,li,true);
    return xtpSessionLeave(s,r);
}
XTP_API xtpresult xtpEnd(xtpsession* s,uint64_t binding,uint32_t id,uint64_t now)
{
    if(!s||s->Busy||!s->Bound||binding!=s->Binding||s->CloseRequested||now<s->LastNow)return XTP_E_STATE;
    size_t li=xtpSessionLane(s,id);if(li==XTP_NONE)return XTP_E_ARGUMENT;
    s->Busy=true;s->LastNow=now;s->TurnFrames=s->Config.Reader.MaxFrames;
    xtpresult r=xtpReaderEnd(s->Lanes[li].Reader,now);
    if(r<0)xtpSessionFault(s,r,li,true);
    return xtpSessionLeave(s,r);
}
XTP_API xtpresult xtpResume(xtpsession* s,uint64_t binding,uint32_t id,uint64_t now)
{
    if(!s||s->Busy||!s->Bound||binding!=s->Binding||s->CloseRequested||now<s->LastNow)return XTP_E_STATE;
    size_t li=xtpSessionLane(s,id);if(li==XTP_NONE)return XTP_E_ARGUMENT;
    s->LastNow=now;return xtpReaderResume(s->Lanes[li].Reader);
}
/* Operations output must never overwrite a descriptor or any borrowed input. */
static bool xtpSubmitOutput(const xtpmessage* m,const xtpsubmitoptions* o,size_t count,
    xtpoperationtoken* out)
{
    if(!out)return true;
    size_t n=count*sizeof(*out);
    if(xtp__Overlap(out,n,m,count*sizeof(*m))||(o&&xtp__Overlap(out,n,o,count*sizeof(*o))))return false;
    for(size_t i=0;i<count;++i){
        if(xtp__Overlap(out,n,m[i].Body.Data,m[i].Body.Size)||
            (m[i].Command.Kind==XTP_COMMAND_NAME&&xtp__Overlap(out,n,m[i].Command.As.Name.Data,m[i].Command.As.Name.Size)))return false;
        const xtpparams* p=&m[i].Params;
        if(p->Kind==XTP_PARAMS_ARRAY){
            if(xtp__Overlap(out,n,p->As.Array,p->Count*sizeof(*p->As.Array)))return false;
            for(size_t j=0;j<p->Count;++j)if(xtp__Overlap(out,n,p->As.Array[j].Data,p->As.Array[j].Size))return false;
        }else{
            if(xtp__Overlap(out,n,p->As.Table,p->Count*sizeof(*p->As.Table)))return false;
            for(size_t j=0;j<p->Count;++j)if(xtp__Overlap(out,n,p->As.Table[j].Key.Data,p->As.Table[j].Key.Size)||
                xtp__Overlap(out,n,p->As.Table[j].Value.Data,p->As.Table[j].Value.Size))return false;
        }
    }return true;
}
XTP_API xtpresult xtpSubmitBatch(xtpsession* s,const xtpmessage* messages,size_t count,
    const xtpsubmitoptions* options,uint64_t now,xtpoperationtoken* operations)
{
    if(!s||!s->Bound||s->Closing||s->CloseRequested||s->Closed||s->Unbinding||
        (s->Busy&&!s->Callback)||now<s->LastNow)return XTP_E_STATE;
#if defined(XTP_FEATURE_XTP_DELIVERY)
    if(s->DeliveryStarted&&now>=s->AuthUntil)return XTP_E_DEADLINE;
#endif
    if(!count||count>s->Config.Reader.Codec.MaxBatch||!messages||
        !xrtMemRangeValid(messages,count*sizeof(*messages))||
        (options&&!xrtMemRangeValid(options,count*sizeof(*options)))||
        (operations&&!xtp__SessionAddress(s,(uintptr_t)operations,count*sizeof(*operations))))return XTP_E_ARGUMENT;
    xtppacket packets[XTP_BATCH_MAX];size_t sizes[XTP_BATCH_MAX],lanes[XTP_BATCH_MAX],taken[XTP_BATCH_MAX];
    uint64_t due[XTP_BATCH_MAX],id=s->NextID;size_t reliable=0,total=0;
#if defined(XTP_FEATURE_XTP_DELIVERY)
    size_t replay=0;
#endif
    for(size_t i=0;i<count;++i){const xtpmessage* m=&messages[i];xtpsubmitoptions o={0};if(options)o=options[i];
        if(m->Type!=XTP_REQUEST&&m->Type!=XTP_PUSH&&m->Type!=XTP_NOTIFY)return XTP_E_TYPE;
        if(o.Replay){
#if defined(XTP_FEATURE_XTP_DELIVERY)
            if(!s->Config.Delivery.Enabled||!s->DeliveryStarted||m->Type==XTP_NOTIFY||
                (m->Type==XTP_REQUEST&&!s->ReplayRequests))return XTP_E_STATE;
            ++replay;
#else
            return XTP_E_STATE;
#endif
        }
        lanes[i]=xtpSessionLane(s,o.LaneID);if(lanes[i]==XTP_NONE)return XTP_E_ARGUMENT;
        if(m->Type!=XTP_NOTIFY&&!s->Lanes[lanes[i]].Config.Reliable)return XTP_E_STATE;
        due[i]=o.DeadlineUs?o.DeadlineUs:xtpSessionDue(now,s->Config.DefaultOperationTimeoutUs);
        if(due[i]<=now)return XTP_E_DEADLINE;
#if defined(XTP_FEATURE_XTP_DELIVERY)
        if(o.Replay&&(due[i]>s->AuthUntil||due[i]>xtpSessionDue(now,s->Lifetime)))return XTP_E_LIMIT;
        if(o.Replay&&m->Type==XTP_REQUEST){
            if(s->Receipts[id%s->Config.DedupWindow].State)return XTP_E_LIMIT;
            for(size_t j=0;j<i;++j)if(options[j].Replay&&messages[j].Type==XTP_REQUEST&&
                packets[j].MessageID%s->Config.DedupWindow==id%s->Config.DedupWindow)return XTP_E_LIMIT;}
#endif
        if(m->Type!=XTP_NOTIFY){if(!id)return XTP_E_LIMIT;++reliable;}
        packets[i]=(xtppacket){0};packets[i].Type=m->Type;packets[i].Command=m->Command;
        packets[i].MessageID=m->Type==XTP_NOTIFY?0:id++;packets[i].Params=m->Params;packets[i].Body=m->Body;
        packets[i].Replayable=o.Replay;
        xtpresult r=xtpEncodeSize(&packets[i],1,&s->Config.Reader.Codec,&sizes[i]);if(r!=XTP_OK)return r;
        if(s->Lanes[lanes[i]].Config.Mode==XTP_INPUT_RECORD&&sizes[i]>s->Lanes[lanes[i]].Config.RecordLimit)return XTP_E_LIMIT;
        if(sizes[i]>s->Config.MaxTxBytes-total)return XTP_E_LIMIT;
        total+=sizes[i];
    }
    if(!xtpSubmitOutput(messages,options,count,operations))return XTP_E_ARGUMENT;
#if defined(XTP_FEATURE_XTP_DELIVERY)
    if(replay>s->Config.Delivery.MaxReplayMessages-s->Retained)return XTP_E_LIMIT;
#endif
    if(reliable>s->Config.MaxPending-s->PendingCount||count>s->Config.MaxTxFrames-s->TxCount||
        count>s->Config.MaxTxFrames-s->Config.ResponseReserveFrames-s->NormalFrames||
        total>s->Config.MaxTxBytes-s->TxBytes||total>s->Config.MaxTxBytes-s->Config.ResponseReserveBytes-s->NormalBytes||
        !s->NextSerial||count-1>UINT64_MAX-s->NextSerial)return XTP_E_LIMIT;
    bool outerbusy=s->Busy,outercallback=s->Callback;
    s->Busy=true;s->Callback=false; /* Allocator callbacks cannot reenter. */
    size_t made=0;xtpresult result=XTP_E_MEMORY;
    for(;made<count;++made){taken[made]=xtpTxTake(s,lanes[made],sizes[made],true);
        if(taken[made]==XTP_NONE){result=XTP_E_LIMIT;break;}
        xtptx* t=&s->Tx[taken[made]];t->Bytes=xtpSessionMemory(s,NULL,sizes[made]);
        if(!t->Bytes){xtpTxFree(s,taken[made]);break;}
        t->Capacity=t->Size=sizes[made];size_t written;
        result=xtpEncode(&packets[made],1,&s->Config.Reader.Codec,t->Bytes,t->Capacity,&written);
        if(result!=XTP_OK){xtpTxFree(s,taken[made]);break;}
    }
    if(made<count){while(made)xtpTxFree(s,taken[--made]);s->Busy=outerbusy;s->Callback=outercallback;
        return result==XTP_OK?XTP_E_MEMORY:result;}
    for(size_t i=0;i<count;++i){xtptx* t=&s->Tx[taken[i]];t->Type=messages[i].Type;t->ID=packets[i].MessageID;
        t->Deadline=due[i];t->Notify=t->Type==XTP_NOTIFY;t->Tag=options?options[i].Tag:NULL;
#if defined(XTP_FEATURE_XTP_DELIVERY)
        if(packets[i].Replayable){xtpRetain(s,taken[i],due[i]);
            if(t->Type==XTP_REQUEST){xtpreceipt* r=&s->Receipts[t->ID%s->Config.DedupWindow];
                *r=(xtpreceipt){t->ID,0,t->Lane,1};++s->ReceiptCount;}}
#endif
        if(!t->Notify){size_t pi=s->FreePending;xtppending* p=&s->Pending[pi];s->FreePending=p->Next;
            memset(p,0,sizeof(*p));p->Active=true;p->ID=t->ID;p->Type=t->Type;p->Serial=t->Serial;
            p->Deadline=t->Deadline;p->Tag=t->Tag;p->Lane=t->Lane;p->Tx=taken[i];t->Pending=pi;
            size_t bucket=(size_t)p->ID&(s->BucketCount-1);p->Next=s->Buckets[bucket];s->Buckets[bucket]=pi;++s->PendingCount;}
        xtpTxQueue(s,taken[i]);if(operations)operations[i]=(xtpoperationtoken){{s->Cookie[0],t->Serial}};
    }
    s->NextID=id;s->LastNow=now;s->Submitted+=count;s->Busy=outerbusy;s->Callback=outercallback;return XTP_OK;
}
XTP_API xtpresult xtpSubmit(xtpsession* s,const xtpmessage* m,const xtpsubmitoptions* o,uint64_t now,xtpoperationtoken* operation)
{return xtpSubmitBatch(s,m,1,o,now,operation);}
XTP_API xtpresult xtpReply(xtpsession* s,xtprequesttoken token,const xtpresponse* response,uint64_t now)
{
    if(!s||s->Closing||s->CloseRequested||(s->Busy&&!s->Callback)||now<s->LastNow||
        token._Opaque[0]!=s->Cookie[0]||token._Opaque[1]!=s->Cookie[1]||!token._Opaque[2]||token._Opaque[2]>s->Config.MaxTxFrames)return XTP_E_STATE;
    size_t i=(size_t)token._Opaque[2]-1;xtptx* t=&s->Tx[i];
    if(!t->Active||!t->Incoming||!t->Waiting||t->Serial!=token._Opaque[3])return XTP_E_STATE;
    if(!s->Bound&&!XTP_REPLAY(t))return XTP_E_STATE;
    if(now>=t->Deadline)return XTP_E_DEADLINE;
#if defined(XTP_FEATURE_XTP_DELIVERY)
    if(s->DeliveryStarted&&now>=s->AuthUntil)return XTP_E_DEADLINE;
#endif
    if(!response||!xrtMemRangeValid(response,sizeof(*response)))return XTP_E_ARGUMENT;
    if(XTP_REPLAY(t)&&response->Status==XTP_STATUS_BUSY)return XTP_E_ARGUMENT;
    xtppacket p={0};p.Type=XTP_RESPONSE;p.MessageID=t->ID;p.Status=response->Status;p.Params=response->Params;p.Body=response->Body;
    size_t n;xtpresult r=xtpEncodeSize(&p,1,&s->Config.Reader.Codec,&n);if(r!=XTP_OK)return r;
    if(n>t->Capacity)return XTP_E_LIMIT;
    r=xtpEncode(&p,1,&s->Config.Reader.Codec,t->Bytes,t->Capacity,&n);if(r!=XTP_OK)return r;
    s->LastNow=now;s->TxBytes-=t->Charge-n;t->Charge=t->Size=n;t->Waiting=false;--s->Incoming;
    t->Deadline=xtpSessionDue(now,s->Config.DefaultOperationTimeoutUs);
#if defined(XTP_FEATURE_XTP_DELIVERY)
    if(t->Replay)t->Deadline=t->KeepUntil;
#endif
    if(s->Bound)xtpTxQueue(s,i);
    return XTP_OK;
}
XTP_API xtpresult xtpOutput(xtpsession* s,uint32_t id,xtpoutput* out)
{
    if(!s||s->Busy||!s->Bound||s->Closing||s->CloseRequested)return XTP_E_STATE;
#if defined(XTP_FEATURE_XTP_DELIVERY)
    if(s->DeliveryStarted&&s->LastNow>=s->AuthUntil)return XTP_E_DEADLINE;
#endif
    if(!xtp__SessionAddress(s,(uintptr_t)out,sizeof(*out)))return XTP_E_ARGUMENT;
    size_t i=XTP_NONE,li=xtpSessionLane(s,id);if(id&&li==XTP_NONE)return XTP_E_ARGUMENT;
    if(id)i=s->Lanes[li].Head;
    else for(size_t j=0;j<s->LaneCount;++j){size_t h=s->Lanes[(s->NextLane+j)%s->LaneCount].Head;
        if(h!=XTP_NONE&&s->Tx[h].Deadline>s->LastNow&&xtpRetriable(s,&s->Tx[h])){i=h;break;}}
    if(i==XTP_NONE)return XTP_AGAIN;
    xtptx* t=&s->Tx[i];xtplane* l=&s->Lanes[t->Lane].Config;xtpoutput o={0};
    if(t->Deadline<=s->LastNow||!xtpRetriable(s,t))return XTP_AGAIN; /* Drive retires expired heads. */
    o.Data=(xbytesview){(t->Bytes?t->Bytes:t->Control)+t->Offset,t->Size-t->Offset};
    o.Ticket=(xtpoutputticket){{s->Binding,i+1,XTP_TICKET(t),t->Offset}};o.BindingID=s->Binding;
    o.DeadlineUs=t->Deadline;o.LaneID=l->ID;o.Mode=l->Mode;o.Reliable=l->Reliable;
    o.Priority=t->Incoming||t->Type==XTP_ACK?1:0;*out=o;return XTP_OK;
}
XTP_API xtpresult xtpAdvance(xtpsession* s,xtpoutputticket ticket,size_t accepted,uint64_t now)
{
    if(!s||s->Busy||!s->Bound||s->CloseRequested||ticket._Opaque[0]!=s->Binding||
        !ticket._Opaque[1]||ticket._Opaque[1]>s->Config.MaxTxFrames||now<s->LastNow)return XTP_E_STATE;
    size_t i=(size_t)ticket._Opaque[1]-1;xtptx* t=&s->Tx[i];
    if(!t->Active||!t->Queued||s->Lanes[t->Lane].Head!=i||XTP_TICKET(t)!=ticket._Opaque[2]||
        t->Offset!=ticket._Opaque[3]||accepted>t->Size-t->Offset||
        (s->Lanes[t->Lane].Config.Mode==XTP_INPUT_RECORD&&accepted&&accepted!=t->Size))return XTP_E_ARGUMENT;
    s->LastNow=now;if(!accepted)return XTP_AGAIN;s->Busy=true;s->TurnFrames=s->Config.Reader.MaxFrames;
    s->NextLane=(t->Lane+1)%s->LaneCount;
#if defined(XTP_FEATURE_XTP_DELIVERY)
    if(t->Replay&&!t->RoundStarted){t->RoundStarted=true;++t->Attempts;++s->Attempts;}
#endif
    t->Sent=true;t->Offset+=accepted;
    if(t->Pending!=XTP_NONE)s->Pending[t->Pending].Sent=true;
    if(t->Offset==t->Size){if(t->Notify){--s->TurnFrames;xtpNotifyComplete(s,i,now>=t->Deadline?XTP_FINISH_TIMEOUT:XTP_FINISH_SENT,now>=t->Deadline?XTP_E_DEADLINE:XTP_OK);}
        else{
#if defined(XTP_FEATURE_XTP_DELIVERY)
            if(t->Replay&&!t->RetireAfterSend)xtpTxUnqueue(s,i);else
#endif
            {if(t->Pending!=XTP_NONE)s->Pending[t->Pending].Tx=XTP_NONE;xtpTxFree(s,i);}}}
    return xtpSessionLeave(s,XTP_OK);
}
xtpresult xtp__DropNotify(xtpsession* s,const xtpoutputticket* ticket,xtpresult reason,uint64_t now)
{
    if(s->Busy||!s->Bound||s->CloseRequested||ticket->_Opaque[0]!=s->Binding||!ticket->_Opaque[1]||
        ticket->_Opaque[1]>s->Config.MaxTxFrames||now<s->LastNow)return XTP_E_STATE;
    size_t i=(size_t)ticket->_Opaque[1]-1;xtptx* t=&s->Tx[i];
    if(!t->Active||!t->Notify||!t->Queued||t->Offset||s->Lanes[t->Lane].Head!=i||
        XTP_TICKET(t)!=ticket->_Opaque[2]||ticket->_Opaque[3])return XTP_E_STATE;
    s->LastNow=now;s->Busy=true;s->TurnFrames=s->Config.Reader.MaxFrames-1;
    xtpNotifyComplete(s,i,reason==XTP_E_DEADLINE?XTP_FINISH_TIMEOUT:XTP_FINISH_LOCAL_ERROR,reason);
    return xtpSessionLeave(s,XTP_OK);
}
static void xtpDueMin(uint64_t* next,uint64_t due)
{if(due&&(!*next||due<*next))*next=due;}
XTP_API xtpresult xtpDrive(xtpsession* s,uint64_t now,uint64_t* next)
{
    if(!s||s->Busy||!xtp__SessionAddress(s,(uintptr_t)next,sizeof(*next))||now<s->LastNow)return XTP_E_ARGUMENT;
    s->LastNow=now;s->Busy=true;s->TurnFrames=s->Config.Reader.MaxFrames;
    uint64_t due=0;size_t work=s->Config.Reader.MaxWorkBytes;bool again=false;
#if defined(XTP_FEATURE_XTP_DELIVERY)
    if(s->DeliveryStarted&&!s->Closed){if(now>=s->AuthUntil)s->CloseRequested=true;else xtpDueMin(&due,s->AuthUntil);}
#endif
    if(s->Closing||s->Unbinding||s->CloseRequested){xtpresult r=xtpSessionLeave(s,XTP_OK);*next=0;return r;}
    if(s->Closed){s->Busy=false;*next=0;return XTP_CLOSED;}
    for(size_t i=0;i<s->Config.MaxPending;++i)if(s->Pending[i].Active){xtppending* p=&s->Pending[i];
        if(p->Deadline<=now){if(!s->TurnFrames){again=true;continue;}--s->TurnFrames;
            if(p->Tx!=XTP_NONE&&s->Tx[p->Tx].Queued&&s->Tx[p->Tx].Offset&&s->Tx[p->Tx].Offset<s->Tx[p->Tx].Size)
                xtpSessionFault(s,XTP_E_DEADLINE,p->Lane,true);
            xtpcompletion c={0};c.Reason=XTP_FINISH_TIMEOUT;c.LocalResult=XTP_E_DEADLINE;
            c.Knowledge=p->Sent?XTP_KNOW_UNKNOWN:XTP_KNOW_NOT_SENT;xtpSessionComplete(s,i,&c);
        }else {
            xtpDueMin(&due,p->Deadline);
#if defined(XTP_FEATURE_XTP_DELIVERY)
            if(p->Tx!=XTP_NONE){xtptx* t=&s->Tx[p->Tx];
                if(t->Replay&&t->RetryAt){
                    if(t->RetryAt<=now&&s->Bound){
                        if(!s->TurnFrames){again=true;continue;}--s->TurnFrames;
                        if(t->Attempts>=s->Config.Delivery.MaxAttempts){xtpcompletion c={0};c.Reason=XTP_FINISH_LOCAL_ERROR;
                            c.LocalResult=XTP_E_LIMIT;c.Knowledge=XTP_KNOW_REJECTED;xtpSessionComplete(s,i,&c);}
                        else {t->RetryAt=0;xtpTxQueue(s,p->Tx);}}
                    else if(s->Bound)xtpDueMin(&due,t->RetryAt);}}
#endif
        }if(s->CloseRequested)break;}
    for(size_t i=0;i<s->Config.MaxTxFrames&&!s->CloseRequested;++i)if(s->Tx[i].Active){xtptx* t=&s->Tx[i];
        if(t->Pending!=XTP_NONE)continue;
        if(t->Deadline<=now){if(!s->TurnFrames){again=true;continue;}--s->TurnFrames;
            if(t->Queued&&t->Offset&&t->Offset<t->Size)xtpSessionFault(s,XTP_E_DEADLINE,t->Lane,true);
            if(t->Waiting){
#if defined(XTP_FEATURE_XTP_DELIVERY)
                if(t->Replay&&t->KeepUntil<=now){xtpTxFree(s,i);continue;}
#endif
                if(!xtpControl(s,i,XTP_RESPONSE,t->ID,XTP_STATUS_DEADLINE_EXCEEDED))xtpSessionFault(s,XTP_E_STATE,t->Lane,true);}
            else if(t->Notify)xtpNotifyComplete(s,i,XTP_FINISH_TIMEOUT,XTP_E_DEADLINE);else xtpTxFree(s,i);
        }else xtpDueMin(&due,t->Deadline);}
    for(size_t i=0;s->Bound&&i<s->LaneCount&&!s->CloseRequested;++i){uint64_t rd=0;
        xtpresult r=xtp__ReaderDriveBudget(s->Lanes[i].Reader,now,s->TurnFrames,&work,&rd);xtpDueMin(&due,rd);
        if(r==XTP_AGAIN)again=true;else if(r<0)xtpSessionFault(s,r,i,true);}
    /* A callback may have submitted into a previously visited slot, and an
     * expired incoming request may just have become a queued timeout response. */
    for(size_t i=0;i<s->Config.MaxPending;++i)if(s->Pending[i].Active)xtpDueMin(&due,s->Pending[i].Deadline);
    for(size_t i=0;i<s->Config.MaxTxFrames;++i)if(s->Tx[i].Active){xtpDueMin(&due,s->Tx[i].Deadline);
#if defined(XTP_FEATURE_XTP_DELIVERY)
        if(s->Bound)xtpDueMin(&due,s->Tx[i].RetryAt);
#endif
    }
#if defined(XTP_FEATURE_XTP_DELIVERY)
    if(s->Receipts)for(size_t i=0;i<s->Config.DedupWindow;++i){xtpreceipt* r=&s->Receipts[i];
        if(r->State!=2)continue;
        if(r->Until<=now){if(!s->TurnFrames){again=true;continue;}--s->TurnFrames;xtpReceiptExpire(s,r);}
        else xtpDueMin(&due,r->Until);}
#endif
    xtpresult result=xtpSessionLeave(s,again?XTP_AGAIN:XTP_OK);*next=s->Closed?0:due;return result;
}
XTP_API xtpresult xtpClose(xtpsession* s,uint64_t now)
{
    if(!s||now<s->LastNow)return XTP_E_ARGUMENT;
    if(s->Busy&&!s->Callback)return XTP_E_STATE;
    s->LastNow=now;if(s->Closed)return XTP_CLOSED;s->CloseRequested=true;
    if(s->Callback)return XTP_OK;
    s->Busy=true;s->TurnFrames=s->Config.Reader.MaxFrames;return xtpSessionLeave(s,XTP_OK);
}
XTP_API xtpresult xtpDestroy(xtpsession* s)
{
    if(!s)return XTP_OK;
    if(s->Busy||s->Callback||!s->Closed)return XTP_E_STATE;
    s->Busy=true;
    xtpreallocproc f=s->Config.Realloc;void* u=s->Config.AllocatorUser;xtpSessionFreeTables(s);f(u,s,0);return XTP_OK;
}
XTP_API xtpresult xtpInfo(const xtpsession* s,xtpinfo* info)
{
    if(!s||!xtp__SessionAddress(s,(uintptr_t)info,sizeof(*info)))return XTP_E_ARGUMENT;
    *info=(xtpinfo){s->Binding,s->NextID,s->Submitted,s->Completed,s->Messages,s->Unknown,s->Duplicates,
        s->PendingCount,s->Incoming,s->TxCount,s->TxBytes,s->Storage,s->Peak,s->Bound,s->Closing||s->CloseRequested,s->Closed,
        0,0,s->HighID>=s->Config.DedupWindow?s->HighID-s->Config.DedupWindow+1:1,1,0,0,0,false,false};
#if defined(XTP_FEATURE_XTP_DELIVERY)
    info->Retained=s->Retained;info->Receipts=s->ReceiptCount;info->ResultFloor=s->ResultFloor;
    info->Attempts=s->Attempts;info->Retries=s->Retries;info->ResultAcks=s->ResultAcks;
    info->Suspended=s->Suspended;info->ReplayRequests=s->ReplayRequests;
#endif
    return XTP_OK;
}
#undef XTP_NONE
#undef XTP_REPLAY
#undef XTP_TICKET
#if !defined(XTP_FEATURE_XTP_DELIVERY)
#undef xtpKeep
#undef xtpRetriable
#endif
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xtp/src/transport/transport.c */
/* ========================================================================== */

#if defined(XTP_FEATURE_XTP_TRANSPORT)
#if defined(XTP_FEATURE_XTP_TRANSPORT)
XTP_API xtpresult xtpDrain(xtpsession* s,uint32_t lane,xtpwriteproc write,void* user,
    const xtpwritebudget* input,uint64_t now,xtpwriteprogress* progress)
{
    if(!s||!write||!progress||!xtp__SessionAddress(s,(uintptr_t)progress,sizeof(*progress))||
        (input&&(!xrtMemRangeValid(input,sizeof(*input))||xtp__Overlap(input,sizeof(*input),progress,sizeof(*progress)))))return XTP_E_ARGUMENT;
    xtpwritebudget b={262144,64,16384};if(input)b=*input;
    if(!b.Bytes||!b.Frames||!b.ChunkBytes||now<xtp__SessionTime(s))return XTP_E_ARGUMENT;
    xtpresult observed=xtp__SessionObserve(s,now);if(observed!=XTP_OK)return observed;
    xtpwriteprogress p={0};xtpresult result=XTP_AGAIN;
    while(p.Frames<b.Frames&&p.Bytes<b.Bytes){xtpoutput out;result=xtpOutput(s,lane,&out);
        if(result==XTP_AGAIN){result=XTP_OK;break;}if(result!=XTP_OK)break;
        size_t remaining=b.Bytes-p.Bytes,whole=out.Data.Size;
        if(out.Mode==XTP_INPUT_RECORD){if(whole>remaining){result=p.Calls?XTP_AGAIN:XTP_E_LIMIT;break;}}
        else {if(out.Data.Size>remaining)out.Data.Size=remaining;if(out.Data.Size>b.ChunkBytes)out.Data.Size=b.ChunkBytes;}
        size_t n=0;xtpresult sent=xtp__TransportWrite(s,&out,write,user,&n);++p.Calls;
        if(n>out.Data.Size||(out.Mode==XTP_INPUT_RECORD&&n&&n!=whole)){result=XTP_E_STATE;break;}
        if(n){uint64_t latest=xtp__SessionTime(s);result=xtpAdvance(s,out.Ticket,n,now<latest?latest:now);p.Bytes+=n;if(n==whole)++p.Frames;
            if(result!=XTP_OK)break;}
        if(!n&&!out.Reliable&&out.Mode==XTP_INPUT_RECORD&&(sent==XTP_E_LIMIT||sent==XTP_E_DEADLINE)){
            uint64_t latest=xtp__SessionTime(s);result=xtp__DropNotify(s,&out.Ticket,sent,now<latest?latest:now);
            if(result==XTP_OK){++p.Frames;++p.Dropped;result=XTP_DROPPED;}break;}
        if(sent!=XTP_OK){result=sent;break;}if(!n){result=XTP_AGAIN;break;}
        result=XTP_AGAIN;
    }
    *progress=p;return result;
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xtp/src/transport/tcp.c */
/* ========================================================================== */

#if defined(XTP_FEATURE_XTP_TCP)
#if defined(XTP_FEATURE_XTP_TCP)
XTP_API xtpresult xtpTcpWrite(void* stream,const xtpoutput* out,size_t* n)
{
    if(!stream||!out||!n||out->Mode!=XTP_INPUT_STREAM||!out->Reliable)return XTP_E_ARGUMENT;
    *n=0;size_t limit=xrtNetStreamWriteLimit(stream);
    size_t take=out->Data.Size<limit?out->Data.Size:limit;if(!take)return XTP_E_LIMIT;
    xnetresult r=xrtNetStreamSend(stream,out->Data.Data,take);
    if(r==XNET_RESULT_OK){*n=take;return XTP_OK;}
    return r==XNET_RESULT_AGAIN?XTP_AGAIN:r==XNET_RESULT_CLOSED?XTP_CLOSED:XTP_E_STATE;
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xtp/src/transport/tls.c */
/* ========================================================================== */

#if defined(XTP_FEATURE_XTP_TLS)
#if defined(XTP_FEATURE_XTP_TLS)
XTP_API void xtpTlsPolicyInit(xtlspolicy* p,bool only)
{
    static const xtlsversion versions[]={XTLS_VERSION_13};
    static const xtlscipher ciphers[]={XTLS_CHACHA20_POLY1305_SHA256,XTLS_AES_128_GCM_SHA256};
    static const uint16 groups[]={XTLS_GROUP_X25519};
    static const xtlssignature signatures[]={XTLS_SIGNATURE_ED25519,XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256};
    if(p)*p=(xtlspolicy){versions,1,ciphers,only?1u:2u,groups,1,signatures,2,XTLS_KEY_SHARE_PREFER_GROUP};
}
XTP_API xtpresult xtpTlsWrite(void* stream,const xtpoutput* out,size_t* n)
{
    if(!stream||!out||!n||out->Mode!=XTP_INPUT_STREAM||!out->Reliable)return XTP_E_ARGUMENT;
    *n=0;if(xrtTlsStreamState(stream)!=XTLS_STREAM_OPEN)return XTP_CLOSED;
    xtlssession* session=xrtTlsStreamSession(stream);xbytesview protocol={0};
    xtlscipher cipher=xrtTlsSessionCipher(session);
    if(xrtTlsSessionVersion(session)!=XTLS_VERSION_13||
        (cipher!=XTLS_CHACHA20_POLY1305_SHA256&&cipher!=XTLS_AES_128_GCM_SHA256)||
        !xrtTlsSessionProtocol(session,&protocol)||protocol.Size!=5||memcmp(protocol.Data,"xtp/3",5))return XTP_E_STATE;
    xtlsresult r=xrtTlsStreamSend(stream,out->Data.Data,out->Data.Size,n);
    return r==XTLS_OK?XTP_OK:r==XTLS_AGAIN?XTP_AGAIN:r==XTLS_CLOSED?XTP_CLOSED:XTP_E_STATE;
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xtp/src/transport/ws.c */
/* ========================================================================== */

#if defined(XTP_FEATURE_XTP_WS)
#if defined(XTP_FEATURE_XTP_WS)
XTP_API xtpresult xtpWsBegin(xtpwsinput* in,xwsopcode opcode)
{if(!in||!in->Session||in->Active||!xtp__RecordLane(in->Session,in->Binding,in->Lane))return XTP_E_STATE;
    if(opcode!=XWS_OPCODE_BINARY)return XTP_E_TYPE;
    in->Active=true;return XTP_OK;}
XTP_API xtpresult xtpWsData(xtpwsinput* in,xbytesview bytes,uint64_t now,xtpreadprogress* p)
{if(!in||!in->Active)return XTP_E_STATE;return xtpInput(in->Session,in->Binding,in->Lane,bytes,false,now,p);}
XTP_API xtpresult xtpWsEnd(xtpwsinput* in,uint64_t now,xtpreadprogress* p)
{
    if(!in||!in->Active)return XTP_E_STATE;
    xtpresult r=xtpInput(in->Session,in->Binding,in->Lane,(xbytesview){NULL,0},true,now,p);
    if(r!=XTP_AGAIN&&r!=XTP_PAUSED&&r>=0)in->Active=false;
    return r;
}
XTP_API xtpresult xtpWsWrite(void* connection,const xtpoutput* out,size_t* n)
{
    if(!connection||!out||!n||out->Mode!=XTP_INPUT_RECORD||!out->Reliable)return XTP_E_ARGUMENT;
    *n=0;xstrview protocol=xrtWsConnProtocol(connection);
    if(protocol.Size!=6||memcmp(protocol.Data,"xtp.v3",6))return XTP_E_STATE;
    xnetresult r=xrtWsConnBinary(connection,out->Data);
    if(r==XNET_RESULT_OK){*n=out->Data.Size;return XTP_OK;}
    return r==XNET_RESULT_AGAIN?XTP_AGAIN:r==XNET_RESULT_CLOSED?XTP_CLOSED:XTP_E_STATE;
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xtp/src/transport/xrudp.c */
/* ========================================================================== */

#if defined(XTP_FEATURE_XTP_XRUDP)
#if defined(XTP_FEATURE_XTP_XRUDP)
static xtpresult xtpXrudpResult(xrudpresult r)
{
    return r==XRUDP_OK?XTP_OK:r==XRUDP_BACKPRESSURE?XTP_AGAIN:r==XRUDP_LIMIT?XTP_E_LIMIT:
        r==XRUDP_NO_MEMORY?XTP_E_MEMORY:r==XRUDP_CLOSED||r==XRUDP_CANCELLED||r==XRUDP_DONE?XTP_CLOSED:XTP_E_STATE;
}
XTP_API xtpresult xtpXrudpWrite(void* user,const xtpoutput* out,size_t* accepted)
{
    xtpxrudpwriter* w=user;
    if(!w||!w->Session||!out||!accepted)return XTP_E_ARGUMENT;
    *accepted=0;
    if(out->Mode==XTP_INPUT_STREAM&&out->Reliable)
        return xtpXrudpResult(xrtRudpSessionStreamWrite(w->Session,w->StreamID?w->StreamID:1,out->Data.Data,out->Data.Size,accepted));
    if(out->Mode!=XTP_INPUT_RECORD||out->Reliable||!w->ChannelID||out->Data.Size<32||out->Data.Data[16]!=XTP_NOTIFY)return XTP_E_ARGUMENT;
    if(out->DeadlineUs<=w->NowUs)return XTP_E_DEADLINE;
    if(out->DeadlineUs-w->NowUs>XRUDP_DATAGRAM_LIFETIME_MAX_US)return XTP_E_LIMIT;
    xrudpsessionstate state;xrudpresult r=xrtRudpSessionState(w->Session,&state);
    if(r!=XRUDP_OK)return xtpXrudpResult(r);
    if(state.Reason!=XRUDP_OK)return XTP_CLOSED;
    if(out->Data.Size>state.DatagramSendLimit)return XTP_E_LIMIT;
    uint64_t sequence;r=xrtRudpSessionDatagramWrite(w->Session,w->ChannelID,out->Data.Data,out->Data.Size,w->NowUs,out->DeadlineUs,&sequence);
    if(r==XRUDP_OK)*accepted=out->Data.Size;
    return xtpXrudpResult(r);
}
#endif
#endif

#endif
