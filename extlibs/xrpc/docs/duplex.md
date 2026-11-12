# Bidirectional message runtime (developing)

Select `XRPC_MODULE_XRPC_DUPLEX`. `xrpcDuplexConfigInit` and
`xrpcDuplexCreate` build one bounded JSON-RPC message endpoint. Both ends can
originate calls while serving the other end. A borrowed `xrpcserver` supplies
its existing bounded Core executor; NULL is client-only. No new shared runtime,
engine, timer, transport, listener or thread is created. The shared server and
callback code/context must remain alive until successful endpoint destruction.
This implements the message state machine; automatic TCP/stdio/WebSocket
adapters and negotiated cancellation/deadline extensions are still required.

The public [JSON-RPC 2.0 specification](https://www.jsonrpc.org/specification),
sections 4-6, is the protocol source. Duplex envelope admission is an explicit
local policy: a whole document has one direction; method plus result/error is
ambiguous and rejected. The spec allows an implementation to fill both roles
but does not prescribe the connection runtime or these transport policies.

## Ownership and driving

`xrpcDuplexCall`, `xrpcDuplexNotify` and `xrpcDuplexBatch` are concurrent bounded
caller-thread admission. Inputs are validated, encoded and copied before return;
ACCEPTED alone writes the independent native Future. A single endpoint gate
serializes construction/publication: encoding can occupy this gate, so concurrent
submit latency is part of the performance cost, not claimed zero overhead.
Timeout starts before lock acquisition, validation or encoding and is passed
unchanged to the native call producer. Fresh monotonically allocated string IDs
never repeat within the endpoint; exhaustion refuses admission. Ordinary calls
and batch maps use the existing native typed `xrpccallresult` and
`xrpcbatchcallresult`. Dropping the observer does not cancel accepted work.
Native `xrtFutureCancel` is supported from any thread; raw cancel-token requests
need an explicit owner signal/Step. No operation is replayed.

Exactly one serialized owner calls `xrpcDuplexReceive`, `xrpcDuplexStep`,
`xrpcDuplexNext`, `xrpcDuplexStarted`, `xrpcDuplexSent`, `xrpcDuplexInfo` and
`xrpcDuplexDestroy`. A shared engine worker can be that owner; other threads can
submit directly. Wake only posts/signals owner work, with no waits or semantic
reentry. A posted task needs its own lifetime discipline beyond Wake return.
Completion Watches on call Futures execute synchronously on this owner. They
must not block it; owner mutation/Destroy reentry is rejected. Concurrent-style
Submit and Close from a completion Watch are supported.

`xrpcDuplexStep` uses round robin across executor completions, submit attachment,
call stop/retirement, close work and the exceptional watch-registration fallback.
The rotation persists with MaxWork=1. More means schedule another owner turn
immediately. Info.NextDeadline is an absolute `xrtTimer` deadline: drive after
Wake/input and by that deadline. Native cancellation Wake references and incoming
Future Watch release tails pin the endpoint. A registration failure retains the
actual accepted job and polls its terminal Future before retirement; it never
pretends failed registration completed business work.

A call scan cohort is bounded by MaxWork across turns, with exact deadline and
cancellation sampling in the native producer. It is a rotating O(live calls)
scan, not one timer per call or a new scheduler; Info computes the minimum live
deadline in O(live calls). Large pending windows therefore still require measured
optimization. Executor completions signal the owner, with no normal polling.
Retained wire-credit stalls and exceptional registration failures retry at a
1 ms scheduling hint; this is backpressure retry, not a business deadline reset.

## Transport contract

Next returns 1 with an independent immutable output reference and never-reused
selection token, 0 empty without outputs, -1 local/API failure. Only one output
is selected at a time. The adapter must retain the output through the physical
borrow; it can take more refs with `xrpcDuplexOutputRetain` and must eventually
call `xrpcDuplexOutputRelease`. `xrpcDuplexOutputWire` borrows immutable bytes
through that reference. The final release never accesses the endpoint/server.

Call Started immediately before the first send offer (including framing header
bytes) and only offer bytes after it succeeds. It conservatively records
MAY_HAVE_EXECUTED even if that offer is refused; it never proves execution.
No Step or other owner mutation may intervene between successful Started and
that first offer. Sent is called only after the entire framed message has been
accepted; it releases the endpoint wire reference and notification delivery
completes. A notification result confirms transport acceptance, never business
success. Use explicit EOF/disconnection to Close(TRANSPORT). A timeout/cancel
while the selected message is partially sent closes the whole endpoint: adapter
MUST abort that transport, preserving framing integrity. A fully sent call can
stop independently; fresh IDs allow later replies to be ignored safely.

The runnable [two-peer example](../examples/duplex/README.md) pumps complete
messages in memory and joins the shared server after both endpoints drain.
It does not implement a production socket/process driver. Framing can be supplied
by the independent [frame leaf](frame.md); WebSocket itself provides messages.

## Complete-document admission

Receive validates the entire document in DUPLEX mode before any business entry
or ID removal. Invalid/mixed response groups close PROTOCOL without partial
correlation. An invalid object with method or neither result nor error is
handled as an invalid request; an object carrying result/error is response-like.
Malformed JSON and invalid requests are submitted to Server for standard errors.
Client-only endpoints refuse incoming requests with OVERLOADED. Accepted requests
copy both input and optional handler context; the incoming monotonic deadline
starts before local decoding. Server overload retains nothing and is counted.
Adapters must bound any retry/input queue and pause or refuse according to their
transport binding. They must not turn local resource failures into wire Invalid
Request or silently drop an accepted document.

Response documents are copied into one immutable quota snapshot, then all IDs
are probed before removal. A live response group must be fully sent, have the
original object/array shape, contain every expected ID exactly once, and contain
no extra/unknown IDs. Batch order is arbitrary; notification positions map to
SIZE_MAX. Multiple independently sent request documents cannot be combined into
one response group. A valid unknown-only document is ignored/counted, including
late replies; no old ID can match a later locally generated request. Complete
strict group validation is local policy over the spec's batch array behavior.

After association, native completion samples deadline/cancel again. An already
stopped response records REPLIED knowledge without retaining its payload. Local
close/transport/protocol/limit/memory statuses stay distinct. Checked completion
publication failure retains the request and response reference for retry; a
false Step/Receive does not authorize freeing physical duties.

## Bounds and costs

Fixed endpoint/scratch/pending storage is capped by MaxFixedBytes (default 16 MiB)
and reported in OwnedBytes; three constant independent ledger headers and Core
allocator caches/headers are outside that number. MaxOperations bounds submitted,
live native producers and incoming documents; MaxPendingCalls counts expected
response IDs including not-yet-attached calls; MaxIncoming bounds borrowed-server
jobs. MaxControlBytes charges request/job metadata and quota prefixes, retaining
credit through captured native cancellation tails. Native Core call/Future/cancel
control blocks have the independently finite MaxOperations bound; they are not
misreported as bytes in that metadata ledger. Pending storage derives finite
ID/key/message limits from the configured response decoder.

MaxOutputs/MaxOutputBytes charge physically retained output objects plus wire
bytes and quota prefixes, even after logical completion or endpoint destruction.
Outgoing wire is owned in that block. An incoming reply reuses its immutable
server result Future without another payload copy; its actual retained header/
capacity remains charged to the shared Server's result ledger. Endpoint output
credit additionally charges its exposed payload bytes, not that server capacity.
A temporary aggregate credit stall retains the job source for retry. A reply
that cannot fit even an empty wire ledger closes LIMIT instead of retrying forever;
OOM closes MEMORY. Business may already have executed; neither implies rollback.

MaxResults/MaxResultBytes charge native preallocated result boxes/maps and retained
response snapshots through their physical final reference. Extra immutable refs
do not double charge a block. Retained results own no endpoint/server references.
Temporary encoding descriptors and ID text are one caller-thread allocation,
serialized under the gate and bounded by MaxTemporaryBytes. Each request allocates
its own metadata, encoded output, native producer/Future/result/cancel machinery;
receive does a classification scan, response-mode snapshot scan, two exact-ID
preflights for atomic probe/removal, and one input snapshot copy. Outgoing Size
and Write independently validate and quote; no zero-allocation/zero-copy claim.

Info exposes logical accepted/rejected/completed counts, live operations/IDs/jobs,
independent control/wire/result block and byte ledgers and their refusal counts.
The counters are sampled under separate native locks, not a global atomic snapshot
or RSS budget. Output refusals include retained-credit retries. Align shared
Server input/dispatch/workspace/result limits with the endpoint decoder/encoder
and output budgets. Each group is bounded by configured messages/bytes as well as
the owner work-unit limit; a unit can process a complete bounded document.

## Close and qualification

`xrpcDuplexClose` is concurrent, nonblocking, first reason wins. Step publishes
native terminal results, removes pending IDs/wires and cooperatively cancels
incoming jobs. It never closes the shared server. Info.Drained observes no
remaining endpoint/context callback duties; retained independent wire/results do
not delay it. Stop concurrent API entries before Destroy. Destroy is owner-only,
nonblocking and requires Close plus Drained; false retains ownership for retry.
The shared Server's own physical worker/executor join is still an external duty.

`tests/test_duplex.c` and its single-header counterpart exercise simultaneous
reverse calls, batches/notifications, reordered maps, whole-document invalid
responses, unsent/late IDs, timeout/cancel/delivery, partial-send close, physically
retained output quota, shared executor/context/malformed requests, live-handler
close, four cross-thread producers and admission-close races, 4096 admission OOM
budget prefixes, constructor failure thresholds, completion reentry and an
impossible reply bound. They are local runtime evidence. Independent transport
peer interop, broader transport fault injection, scalable contention/tail-latency,
real TCP/stdio/WebSocket and native 72-hour acceptance remain pending.

`bench/bench_duplex.c` records closed-loop pipeline1 wall rate, average call cost,
all-process-thread CPU and separate allocation runs. Mode0 is a native producer
with a prebuilt response snapshot (cost reference; it omits runtime semantics).
Mode1 includes duplex admission and a synthetic peer decode/encode; Mode2 uses
two endpoints and the existing shared executor with in-process message transfer.
Busy owner scheduling CPU is included. These are not fixed-arrival/network/tail
or same-function parity tests; an unpinned baseline cannot establish release
performance. Current source-bound evidence is in
`tests/results/duplex-20261113.json` with raw files under
`out/service-extensions/xrpc-duplex-20261113/`.
