# Framed stdio

Select XRPC_MODULE_XRPC_STDIO for a JSON-RPC channel over two byte streams.
Set Read and Write in the configuration to the host's nonblocking callbacks.
The adapter combines the existing frame reader and duplex runtime. The host
owns native pipe handles, launch, stderr, readiness registration and physical
I/O retirement.

Use Content-Length for the default framing profile. UINT32_BE is selected
explicitly on both ends. The format and validation follow the same rules as the
[frame leaf](frame.md). The existing [duplex result contracts](duplex.md) apply
to calls, notifications, batches and their Future ownership.

## Driving and backpressure

Call admission and Close are concurrent. The read, frame, write and destroy
state has one serialized owner. Drive Step after Wake, native readiness and
NextDeadline. Info.More requests immediate work. A successful input callback
copies into a fixed adapter buffer. Complete frames in that buffer can be
borrowed by the parser. The remaining buffer and its current frame are kept
until admission succeeds. A fragmented frame can use the separately bounded
frame reader's storage.

Overloaded input stops reads before the next frame. The current frame stays
valid through retries. The owner retries after a bounded 1 ms scheduling hint
because the shared server can regain capacity without signaling this endpoint.
The document's incoming deadline originates at the first read containing its
bytes. A client-only endpoint rejects inbound requests as a typed limit.

Output keeps the duplex object alive while it writes the header and body in
successive bounded offers. Progress means the host has accepted exactly the
reported prefix. Callbacks borrow the input or output span through return.
An asynchronous host copies accepted output or otherwise finishes using the
borrow before returning. A zero-byte AGAIN retains the current prefix. The
first offer marks local delivery uncertain and full acceptance is Sent.

MaxIOWork limits progress units, with one additional no-progress attempt per
direction. The owner rotates between read and write across turns, including
MaxIOWork=1. ReadBytes, WriteChunkBytes, frame limits and Duplex's independent
budgets bound the work and storage. The adapter is single allocation plus its
reader and duplex endpoint. The host's own native buffers and handles are
outside those budgets. Outgoing call encoding uses the smaller of the duplex
and frame size limits. An oversized handler response seals the channel before
its framing prefix is offered.

## EOF and retirement

A clean frame boundary stops new submissions and input. Accepted handler
responses drain to the write callback. An unstarted local request retires as
TRANSPORT with its untouched delivery status. A previously started output
reaches its frame boundary before a call waiting for a reply retires. The host
can close the channel earlier. EOF drain uses the configured monotonic timeout.
The native duplex outcome still applies deadline, cancellation and delivery
precedence.

An incomplete frame is TRUNCATED. Bad framing, allocation refusal and
callback failure close the channel. When an output frame has started and
the endpoint stops partway, the host discards the native output stream so the
peer cannot mistake later bytes for the rest of that frame. Concurrent Close
seals the duplex and is observed at the next owner boundary. After Drained,
the host waits for its own retained I/O duties before releasing its handles.

The [redirected-pipe example](../examples/stdio/README.md) can run against
an independent Python peer using the integration script in tests/interop.
This stage provides host-driven stdio for duplex calls. Automated native
process attachment and its readiness lifecycle, negotiated deadline and
cancellation, and native production qualification remain open.

## Measuring the adapter

The registered xrpc_stdio profile measures one call at a time over the memory
callbacks with a scripted peer, including frame and JSON processing in both
directions. Run it with the repository performance tool.

```sh
python tools/measure_performance.py --compiler gcc --config extlibs/xrpc/config/performance_profiles.json --manifest extlibs/xrpc/config/modules.json --profiles xrpc_stdio
```

The independent Python peer can also record a width-32 closed-loop workload
over actual redirected pipes. These results include the example driver's
1 ms idle polling and Python time. The repository evidence records both
workloads and the scope of the remaining production checks.

[Stage results and verification](../tests/results/stdio-20261119.json).
