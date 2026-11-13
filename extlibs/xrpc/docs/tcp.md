# Framed bidirectional TCP (developing)

Select `XRPC_MODULE_XRPC_TCP` or suite `xrpc_tcp`. This adapter connects numeric
IPv4/IPv6 endpoints or attaches a native accepted OPEN stream on its owning
worker. It drives `xrpc_duplex` and `xrpc_frame` on an existing host-owned XRT
Engine, with a borrowed shared `xrpc_server` for business execution. It creates
no new Engine or thread. Both endpoints can call, notify and batch concurrently.
See the [runnable example](../examples/tcp/README.md).

`xrpcTcpConfigInit` selects Content-Length framing, 1 MiB bodies, a 4 KiB input
scratch area, 64 KiB read/write byte budgets, 16 KiB copy-send chunks and 32
read-frame/send-offer units per owner turn. UINT32_BE framing is explicit on
both peers; there is no protocol sniffing or negotiation. Duplex and Server
JSON/message/response quotas must be aligned with framing limits. The native
Stream has its own bounded buffers and high/low water marks. A peer's reported
FixedBytes excludes its separately reported Duplex storage and retained Frame
capacity, native buffers, executor and independent result quotas; it is not RSS.

Call/Notify/Batch copy and encode input before returning. Only ACCEPTED writes
the Future output. Up to 128 facade submissions can construct concurrently by
default, additionally bounded by Duplex operations/IDs/workspace. API-entry
monotonic deadlines include facade-lock waiting, encoding and native send queue
waiting. The numeric connect timeout also includes the initial worker-post wait.
No reconnect, replay, remote deadline or remote cancellation extension is emitted.
A cancelled or expired call that was offered to the native stream can have
executed remotely. Notification completion means native acceptance of the whole
framed message, not business acknowledgement. Retained typed results outlive the
peer, shared server and Engine.

`Duplex.Wake` and `Duplex.User` are reserved and must be NULL. The adapter installs
a coalesced embedded native Post. Business callbacks execute on the existing
shared executor, never a network worker. RequestContext is copied once per peer
(up to 16 KiB) and supplied to Server's ordinary per-job copied context; pointed-to
objects inside those bytes are not recursively copied. The peer copy is securely
erased at Destroy. Keep the shared Server, code and caller owner alive until
peer retirement and destruction. Attaching requires exclusive ownership of the
stream protocol, reads and writes. Successful Attach consumes exactly the caller's
one stream reference and replaces its event binding; failure consumes nothing.
Attach explicitly handles already buffered input.

Reading copies bounded native chunks to scratch. Fragmented frames use the
framer's reusable body buffer; complete frames can borrow scratch only through
synchronous Duplex admission, which snapshots retained data. Sending copies a
canonical framing header and body in bounded chunks into the native write queue;
it is not a zero-copy path. A whole JSON message is selected at a time; there
is no interleaving of frames. Started is marked immediately before the first
header/body send offer, and Sent only after every byte has been accepted.
A stopped partially offered message closes the channel, preserving delivery
uncertainty rather than emitting a malformed next message.

Incoming admission overload is counted and closes the channel with LIMIT.
There is no unbounded input queue, silently dropped call or implicit retry. The
30-second default frame timeout begins at the first observed buffered byte,
not after the final fragment. Coalesced queued frames conservatively share their
original observation time. Protocol errors, oversize frames, allocation failure,
truncated EOF, native transport failure and exhausted scheduling resources close
the channel. EOF is terminal: complete preceding response frames are delivered,
but half-close is not a promise to drain incoming business responses. Applications
must keep the read direction open while expecting those responses.

`xrpcTcpClose` is aborting, idempotent and concurrent. Stop external API entries,
Close, WaitFor (or PollRetired), then Destroy. Retirement covers Duplex jobs and
result construction, submitters, native event bindings, timers, posts and the
native Stream's physical tails. Held noncooperative business callbacks keep
retirement pending even after call cancellation. WaitFor rejects every worker
of the same Engine; use it from a host thread. PollRetired never waits for a
mutex; it can nudge pending close work. The sole caller owner must remain stable.
Destroy never waits and retains ownership on failure. A public Engine pin is
released only at successful Destroy: keep the Engine RUNNING through this sequence.
Server physical join and the application's listener/other-object retirement are
separate host duties before Engine shutdown and code unload.

One public relative Engine timer supplies the earliest exact Duplex/connect/frame
deadline, rounded up and rechecked against monotonic time. Timer allocation/quota
failure closes with LIMIT and increments TimerFailures. Closing then uses callback
wakes and Poll/Wait nudges, without a spinning timer-retry loop. Engine shutdown
before peer destruction violates the host lifetime contract; PostFailures remains
observable. Info is concurrent; its Duplex accounting is the last owner snapshot
and is not simultaneous with facade counters.

Verification includes native two-way sockets, both framing profiles, 1-byte
partial sends, bounded read turns, backpressure, call/error/notify/batch, original
deadlines, cancellation, four concurrent producers, physical held-handler close,
retained results, malformed/oversize/truncated/slow frames, final response before
EOF, timer exhaustion and constructor allocation limits. The independent Python
socket peer exercises Unicode/NUL IDs, exact large integers, batches and native
callback calls. These local tests do not constitute production acceptance.
TLS/DNS/prepared-candidate duplex transport, reconnect policy, WebSocket/stdio,
remote deadline/cancel negotiation, authentication, broad hostile-peer campaigns,
fixed-arrival P99, connection scaling and native Windows/Linux >=72h gates remain
open. Loopback performance is only the explicitly scoped baseline below.

Build `bench/bench_tcp.c` using `xrpc_tcp,memory_stats`; run `2000 1 0` or
`2000 32 0`. It measures one connection with a closed-loop pipeline of one or a
barrier window of 32, shared business executor and native Content-Length TCP.
Only pipeline one reports observed closed-loop P50/P99 (including host wait and
collection); coordinated omission prevents using it as a fixed-arrival tail gate.
Process CPU includes all threads. A third argument of 1 enables allocation counts
and is a separate run. No TLS, remote network, connection-scale or native Linux
claim follows from these unpinned local measurements.
