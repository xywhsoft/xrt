# Framed stdio subprocesses

Select XRPC_MODULE_XRPC_STDIO_PROCESS to launch a child and drive a framed RPC
endpoint using the existing XRT process pipes. The child uses ordinary blocking
stdin/stdout. The owner uses native readiness, cross-thread wakes and deadlines.
The adapter adds no worker, timer thread or shared queue. The plain stdio leaf
retains its host callback entry points and its original dependency closure.

## Launch and ownership

xrpcStdioProcessSpawn takes a borrowed xprocessconfig and optional
xrpcstdioconfig. It supplies its own Read, Write, User and Duplex.Wake/User.
Leave those fields null. Set the frame format explicitly on both ends and
optionally supply a shared server for reverse requests. Direct launch uses the
program, argument vector, environment and working directory in the process
configuration. An explicit shell target retains XRT's shell semantics.
Terminal and merged stderr modes are invalid. Inherit stderr, redirect it,
or consume a separate stderr pipe through the returned process object.

Only success publishes both independently owned results. The process object
provides exit status, waiting and explicit stops. The adapter owns the parent
pipe ends and the RPC endpoint. xrpcStdioProcessEndpoint borrows the endpoint
for concurrent xrpcStdioCall, xrpcStdioNotify, xrpcStdioBatch and xrpcStdioClose.
The borrowed endpoint's I/O and destruction belong to the process adapter.

## Owner loop

xrpcStdioProcessStep drives one owner turn and fills the initialized
xrpcstdioinfo snapshot. xrpcStdioProcessWait uses that snapshot and returns
XWAIT_OK for requested I/O, a producer wake, immediate work or a due RPC
deadline. XWAIT_TIMEOUT means the user's wait expired first, and XWAIT_CLOSED
means a Step observed the drain. A native wait error seals the endpoint with
TRANSPORT. The owner continues to drive retirement. The initial Wait can
request the first Step.

When the transport reaches a terminal reason, Step closes child stdin and
retires pending writes. Drain progress continues through server completion
and cancellation callbacks. xrpcStdioProcessDestroy requires the caller to
stop concurrent entries and reach Drained, then retires the remaining native
I/O. Retained Future results remain readable. The caller manages the
independent xprocess lifetime and decides whether a running child needs an
explicit stop.

The host stages one read and one write on Windows, each bounded by the larger
configured chunk. POSIX uses the callback spans directly. A write advances
only after the native layer reports its completed prefix. The duplex result
preserves uncertainty on partial send, timeout or cancellation.

[Process-pipe contracts](../../../docs/api/process_pipe.md).


The serialized owner can negotiate [remote cancellation](cancellation.md) and
[deadline propagation](deadline.md) through the borrowed stdio endpoint.

## Example and independent peer

Build and run the example from the repository root.

```sh
python tools/build.py --compiler gcc --manifest extlibs/xrpc/config/modules.json --suite xrpc_stdio_process
```

With no arguments the example launches itself as a blocking echo child and
validates a round trip. Use text or be to choose the framing profile, followed
by the call count and window size. An optional program and argument list after
the window selects a custom echo peer.

The independent Python fixture verifies actual launch, both framing
profiles, the echo result and clean child exit. It fragments the first reply.
Add --perf-count 4096 to collect three short runs per profile per platform.
Measurements include the C caller and Python child with a closed-loop window
of 32. Process launch and the first group are outside timed measurements.

```sh
python extlibs/xrpc/tests/interop/run_stdio_process.py <executable> out/xrpc-stdio-process.json
```

[Recorded stage evidence](../tests/results/stdio-process-20261119.json).
