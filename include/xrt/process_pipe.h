#ifndef XRT_PROCESS_PIPE_H
#define XRT_PROCESS_PIPE_H

#include <xrt/process.h>

#if defined(XRT_FEATURE_PROCESS_PIPE) && (!defined(XRT_FEATURE_PROCESS) || !defined(XRT_FEATURE_ATOMIC))
#error "XRT_FEATURE_PROCESS_PIPE requires process and atomic"
#endif

#if defined(XRT_FEATURE_PROCESS_PIPE)
typedef struct xprocesspipe xprocesspipe;
typedef enum xprocesspipeio {
    XPROCESS_PIPE_ERROR=-1,
    XPROCESS_PIPE_AGAIN=0,
    XPROCESS_PIPE_PROGRESS=1,
    XPROCESS_PIPE_EOF
} xprocesspipeio;

#define XPROCESS_PIPE_MAX_CHUNK (1024u*1024u)

XRT_EXTERN_C_BEGIN
/* Replaces stdin/stdout in a borrowed Config, preserving stderr. Terminal
 * configurations are rejected. The child receives blocking standard streams.
 * ChunkBytes bounds each I/O offer and each Windows staging buffer. The two
 * results have independent ownership and are written only on success.
 * The pipe adds no thread. Use the Process result for wait, status and stop. */
XRT_API bool xrtProcessPipeSpawn(const xprocessconfig* Config,size_t ChunkBytes,
    xprocess** Process,xprocesspipe** Pipe);
/* One serialized owner exclusively drives Read/Write/Wait/CloseInput/Destroy.
 * AGAIN borrows no buffer. Retry that direction with the same Size and, for
 * Write, the same bytes until it makes progress, ends or is retired. PROGRESS
 * reports the completed prefix. EOF is returned by Read only. AGAIN and EOF
 * report zero bytes. ERROR preserves Count. Size must be 1..ChunkBytes. */
XRT_API xprocesspipeio xrtProcessPipeRead(xprocesspipe* Pipe,bytes Data,size_t Size,size_t* Count);
XRT_API xprocesspipeio xrtProcessPipeWrite(xprocesspipe* Pipe,cbytes Data,size_t Size,size_t* Count);
/* Wait for the requested directions or a wake, then retry the desired I/O.
 * Timeout is relative milliseconds, -1 forever. A direction with an immediately
 * available result can wake the owner without blocking. No result grants I/O
 * progress until the corresponding nonblocking operation reports it. */
XRT_API xwaitresult xrtProcessPipeWait(xprocesspipe* Pipe,bool Read,bool Write,int64 Timeout);
/* Concurrent and nonblocking. Coalesces signals. Stop wake producers before
 * Destroy. No other endpoint operations may overlap the serialized owner. */
XRT_API bool xrtProcessPipeWake(xprocesspipe* Pipe);
/* Half-close stdin, cancelling and joining any still-pending native write.
 * For a clean EOF, finish all writes first. Idempotent. Output stays readable. */
XRT_API bool xrtProcessPipeCloseInput(xprocesspipe* Pipe);
/* Retires native operations before freeing their buffers and closes both streams.
 * Does not wait for or stop the process. NULL is valid. */
XRT_API void xrtProcessPipeDestroy(xprocesspipe* Pipe);
XRT_EXTERN_C_END
#endif
#endif
