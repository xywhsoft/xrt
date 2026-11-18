#ifndef XRPC_STDIO_H
#define XRPC_STDIO_H
#include <xrpc/duplex.h>
#include <xrpc/frame.h>
#if defined(XRPC_FEATURE_XRPC_STDIO) && (!defined(XRPC_FEATURE_XRPC_DUPLEX) || !defined(XRPC_FEATURE_XRPC_FRAME))
#error "xrpc stdio requires duplex and frame"
#endif
#if defined(XRPC_FEATURE_XRPC_STDIO)

typedef struct xrpcstdio xrpcstdio;
typedef enum xrpcstdioio {
    XRPC_STDIO_IO_ERROR=-1,XRPC_STDIO_IO_AGAIN=0,XRPC_STDIO_IO_PROGRESS=1,XRPC_STDIO_IO_EOF
} xrpcstdioio;
/* Callbacks run on the serialized owner and must return without waiting.
 * PROGRESS sets Count to 1..Size bytes. Other outcomes leave Count zero.
 * EOF is valid for Read only. Spans expire on return. Write progress means
 * the host accepted that prefix and now owns any physical I/O obligations. */
typedef xrpcstdioio (*xrpcstdioread)(ptr User,bytes Data,size_t Size,size_t* Count);
typedef xrpcstdioio (*xrpcstdiowrite)(ptr User,cbytes Data,size_t Size,size_t* Count);
typedef enum xrpcstdioreason {
    XRPC_STDIO_OPEN=0,XRPC_STDIO_EOF,XRPC_STDIO_CLOSED,XRPC_STDIO_TRUNCATED,
    XRPC_STDIO_FRAME,XRPC_STDIO_IO,XRPC_STDIO_LIMIT,XRPC_STDIO_MEMORY,
    XRPC_STDIO_TIMEOUT,XRPC_STDIO_DUPLEX
} xrpcstdioreason;
typedef struct xrpcstdioconfig {
    size_t Size;
    xrpcduplexconfig Duplex;
    xrpcframeconfig Frame;
    xrpcstdioread Read;
    xrpcstdiowrite Write;
    ptr User;
    size_t ReadBytes,WriteChunkBytes,MaxIOWork;
    int64 DrainTimeout;
} xrpcstdioconfig;
typedef struct xrpcstdioinfo {
    size_t Size;
    xrpcduplexinfo Duplex;
    xrpcframeinfo Frame;
    xrpcstdioreason Reason;
    size_t FixedBytes,BufferedInputBytes;
    double NextDeadline;
    bool InputEnded,WantRead,WantWrite,More,Drained;
} xrpcstdioinfo;

XRT_EXTERN_C_BEGIN
/* Content-Length, 16 KiB input and write chunks, 64 I/O units per turn,
 * 5 s clean-EOF drain. Host owns and drives the two byte streams. A borrowed
 * Server and callback context must remain alive through Destroy. */
XRPC_API bool xrpcStdioConfigInit(xrpcstdioconfig* Config);
XRPC_API bool xrpcStdioCreate(const xrpcstdioconfig* Config,xrpcstdio** Result);
/* Serialized owner. Applies the duplex cancellation opt-in and offer to this
 * framed channel, including a borrowed endpoint from the subprocess adapter. */
XRPC_API bool xrpcStdioEnableCancellation(xrpcstdio* Stdio);
XRPC_API xrpcduplexadmission xrpcStdioNegotiateCancellation(xrpcstdio* Stdio,int64 Timeout,xfuture** Future);
/* Concurrent. Only ACCEPTED writes Future. Deadlines start at entry. The
 * transport stops accepting submissions when input EOF is observed. */
XRPC_API xrpcduplexadmission xrpcStdioCall(xrpcstdio* Stdio,xstrview Method,xstrview Params,int64 Timeout,xfuture** Future);
XRPC_API xrpcduplexadmission xrpcStdioNotify(xrpcstdio* Stdio,xstrview Method,xstrview Params,int64 Timeout,xfuture** Future);
XRPC_API xrpcduplexadmission xrpcStdioBatch(xrpcstdio* Stdio,const xrpcduplexitem* Items,size_t Count,int64 Timeout,xfuture** Future);
/* Step/Info/Destroy have one owner. Drive after Wake or host I/O readiness and
 * at NextDeadline. More asks for an immediate turn. After AGAIN, arm host
 * readiness only in requested directions. MaxIOWork limits progress units.
 * A blocked direction is paused for the turn. Each newly assembled input frame
 * allows one retry of a started write before dispatch. This budget is independent
 * of Duplex.MaxWork. I/O callbacks must not reenter Step/Destroy. */
XRPC_API bool xrpcStdioStep(xrpcstdio* Stdio);
XRPC_API bool xrpcStdioInfo(xrpcstdio* Stdio,xrpcstdioinfo* Info);
/* Concurrent abortive seal. Use TRANSPORT for out-of-band host I/O failure.
 * If the current frame was started, discard the native channel on error.
 * Stop new API entries before Destroy. Drained covers library duties. The
 * host independently retires its native handles and any queued I/O. */
XRPC_API bool xrpcStdioClose(xrpcstdio* Stdio,xrpccallstatus Reason);
XRPC_API bool xrpcStdioDestroy(xrpcstdio* Stdio);
XRT_EXTERN_C_END
#endif
#endif
