#ifndef XRPC_STDIO_PROCESS_H
#define XRPC_STDIO_PROCESS_H
#include <xrpc/stdio.h>
#include <xrt/process.h>

#if defined(XRPC_FEATURE_XRPC_STDIO_PROCESS) && \
    (!defined(XRPC_FEATURE_XRPC_STDIO) || !defined(XRT_FEATURE_PROCESS_PIPE))
#error "xrpc stdio process requires stdio and XRT process_pipe"
#endif

#if defined(XRPC_FEATURE_XRPC_STDIO_PROCESS)
typedef struct xrpcstdioprocess xrpcstdioprocess;

XRT_EXTERN_C_BEGIN
/* Synchronously launch a child with blocking stdin/stdout. Stdio is a borrowed
 * configuration from xrpcStdioConfigInit, or NULL for defaults. The adapter
 * supplies Read, Write, User and Duplex.Wake/User. Those fields must be NULL.
 * Process and Adapter receive independent ownership only on success. Terminal
 * and stderr merge are invalid. Separate captured stderr is the host's duty. */
XRPC_API bool xrpcStdioProcessSpawn(const xprocessconfig* Config,const xrpcstdioconfig* Stdio,
    xprocess** Process,xrpcstdioprocess** Adapter);
/* Borrowed through Adapter destruction. Use it for concurrent Call, Notify,
 * Batch and Close. The Adapter is the exclusive I/O and lifetime owner. */
XRPC_API xrpcstdio* xrpcStdioProcessEndpoint(xrpcstdioprocess* Adapter);
/* Drive one bounded owner turn and snapshot its state. Info->Size is required.
 * A terminal transport closes child stdin, retiring any pending native write.
 * Keep driving until Info->Drained, even if the child already exited. */
XRPC_API bool xrpcStdioProcessStep(xrpcstdioprocess* Adapter,xrpcstdioinfo* Info);
/* Owner-only. XWAIT_OK requests a Step after I/O, a wake, immediate work or an
 * RPC deadline. XWAIT_TIMEOUT is the caller's relative millisecond budget,
 * with -1 meaning no outer limit. XWAIT_CLOSED means Step observed Drained.
 * Native wait failure seals the endpoint with TRANSPORT and returns ERROR. */
XRPC_API xwaitresult xrpcStdioProcessWait(xrpcstdioprocess* Adapter,int64 Timeout);
/* Caller stops concurrent submissions and Close before destroying. Requires
 * the protocol to be Drained. Success also retires all remaining native pipe
 * I/O. False retains ownership. NULL succeeds. The independent child remains
 * available for status, wait or explicit termination. */
XRPC_API bool xrpcStdioProcessDestroy(xrpcstdioprocess* Adapter);
XRT_EXTERN_C_END
#endif
#endif
