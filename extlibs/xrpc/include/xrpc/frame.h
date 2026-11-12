#ifndef XRPC_FRAME_H
#define XRPC_FRAME_H
#include <xrpc/features.h>
#include <xrpc/export.h>
#include <xrt/memory.h>
#if defined(XRPC_FEATURE_XRPC_FRAME)
typedef struct xrpcframereader xrpcframereader;
typedef enum xrpcframeformat {XRPC_FRAME_CONTENT_LENGTH=1,XRPC_FRAME_UINT32_BE} xrpcframeformat;
typedef enum xrpcframestatus {
    XRPC_FRAME_ERROR=-1,XRPC_FRAME_MORE=0,XRPC_FRAME_READY=1,
    XRPC_FRAME_PROTOCOL,XRPC_FRAME_LIMIT,XRPC_FRAME_MEMORY,XRPC_FRAME_UNSUPPORTED,
    XRPC_FRAME_TRUNCATED,XRPC_FRAME_END
} xrpcframestatus;
typedef struct xrpcframeconfig {
    size_t Size;xrpcframeformat Format;
    size_t MaxBodyBytes,MaxHeaderBytes,MaxLineBytes,MaxFields,MaxStorageBytes;
} xrpcframeconfig;
typedef struct xrpcframeview {
    xbytesview Body;size_t HeaderBytes,WireBytes;bool Buffered;
} xrpcframeview;
typedef struct xrpcframeinfo {
    size_t Size,BaseBytes,HeaderCapacity,BodyCapacity,StorageBytes,PeakStorageBytes;
    size_t HeaderBytes,BodyBytes,ContentLength;bool InBody,Ready,Ended;
    xrpcframestatus Failure;
} xrpcframeinfo;
XRT_EXTERN_C_BEGIN
/* Content-Length default; 1MiB body,8KiB head,4KiB line,64 fields,4MiB storage.
 * Body<=1GiB,head/line<=1MiB,fields<=65536,storage<=4GiB (SIZE_MAX on32bit).
 * Headers are ASCII/CRLF. Exactly one decimal byte Content-Length, no folding,
 * TE or content coding. Optional JSON/VSCode JSON-RPC Content-Type must select
 * UTF-8 (legacy utf8 accepted); syntactic unknown media parameters are ignored.
 * UINT32_BE is an explicitly selected binary length profile, never auto-detected.
 * The framer accepts opaque/empty bodies; JSON validation belongs to Decode. */
XRPC_API bool xrpcFrameConfigInit(xrpcframeconfig* Config);
/* One small allocation; no initial head/body buffer. Copied config, no callbacks,
 * locks, timers, Engine or transport. One serialized owner for every operation.
 * Limits include state, both retained capacities and old+new buffer growth reservations.
 * Failure preserves output. Caller stops entries before Destroy. */
XRPC_API bool xrpcFrameReaderCreate(const xrpcframeconfig* Config,xrpcframereader** Reader);
XRPC_API void xrpcFrameReaderDestroy(xrpcframereader* Reader);
/* Reserve optional reusable buffers. No input/state reset. Failure leaves old
 * buffers/protocol state/views unchanged (observed allocation peak may grow);
 * successful growth invalidates buffered views.
 * HeaderCapacity ignored only when zero in the binary profile. Quota refusal
 * is LIMIT, allocation failure MEMORY. Reserve failure is not sticky. */
XRPC_API xrpcframestatus xrpcFrameReaderReserve(xrpcframereader* Reader,
    size_t HeaderCapacity,size_t BodyCapacity);
/* Consumes at most one frame. On valid API calls Used is always published,
 * including a consumed failing prefix; Frame changes only on READY. MORE may
 * consume partial head/body. Next Feed after READY begins the next frame and
 * invalidates buffered views. Complete body in the current input is borrowed
 * directly (Buffered=false); fragmented bodies are copied once into reusable
 * storage (Buffered=true), plus actual realloc movement. A borrowed body needs
 * caller bytes alive/unchanged; buffered views end at next Feed/Reserve/Reset/
 * Destroy. No input is retained on MORE, and coalesced suffix is not consumed.
 * Protocol/limit/memory/unsupported failures are sticky until Reset; no recovery
 * scan guesses the next boundary. API ERROR preserves state and both outputs.
 * Input, outputs and all reader allocations must be mutually nonoverlapping.
 * Success/MORE and typed protocol/quota outcomes preserve ambient error. */
XRPC_API xrpcframestatus xrpcFrameReaderFeed(xrpcframereader* Reader,xbytesview Input,
    size_t* Used,xrpcframeview* Frame);
/* END at a frame boundary (including immediately after READY), TRUNCATED for
 * any partial frame. Sticky failure wins. Subsequent Feed consumes0 and returns
 * END/TRUNCATED. An empty Feed is not EOF. Reset discards the current frame and
 * terminal state, retains capacity and requires an explicitly trusted boundary. */
XRPC_API xrpcframestatus xrpcFrameReaderEnd(xrpcframereader* Reader);
XRPC_API bool xrpcFrameReaderReset(xrpcframereader* Reader);
XRPC_API bool xrpcFrameReaderInfo(const xrpcframereader* Reader,xrpcframeinfo* Info);
/* Canonical Content-Length: N CRLF CRLF or four big-endian bytes. Size/Write
 * validate all limits/aliases before modifying any output; no allocation on
 * success. HeaderWrite supports scatter/gather without copying body. Complete
 * Write copies opaque Body exactly once; no JSON/UTF-8 scan or trailing NUL.
 * Config NULL uses defaults. Output is caller-owned and disjoint from inputs. */
XRPC_API bool xrpcFrameHeaderSize(size_t ContentLength,const xrpcframeconfig* Config,size_t* Size);
XRPC_API bool xrpcFrameHeaderWrite(size_t ContentLength,const xrpcframeconfig* Config,
    bytes Output,size_t Capacity,size_t* Written);
XRPC_API bool xrpcFrameWrite(xbytesview Body,const xrpcframeconfig* Config,
    bytes Output,size_t Capacity,size_t* Written);
XRT_EXTERN_C_END
#endif
#endif
