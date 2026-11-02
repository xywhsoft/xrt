# HTTP client result storage

`xrpchttpconfig.MaxResultBlocks` and `MaxResultBytes` bound both pending and
caller-retained result allocations. Defaults are 4096 blocks and 8 MiB. Each typed
result header (including an entire batch index map) is one block; an immutable
response snapshot is another block. Ordinary/batch replies therefore normally
hold two blocks, while notifications and local terminal results hold one.
Additional Future/Responses references do not charge an allocation twice.

Header credit is reserved during admission and refusal is OVERLOADED without
publishing a Future. Snapshot size is known only after complete response decoding;
snapshot credit failure produces LIMIT and retires the connection without replay
or partial batch publication. Request cancellation and client close do not reclaim
storage held by caller references. Queued/cancelled/failed operations also retain
their preallocated typed header until the last result owner leaves.

`xrpchttpinfo.ResultBlocks/ResultBytes` reflect live allocations/reservations.
`ResultRejected` counts refused header and snapshot credit attempts; the ordinary
Rejected counter counts admission refusals. Header/map and snapshot credits are
independent: a separately retained `xrpcresponses` keeps its own charge after its
Future disappears. Credits refund after physical block free. There is no waiting
queue for result credit, and no automatic retry. Callers may explicitly submit new
work after releasing retained results if the connection remains open.

One mutex-protected, reference-counted storage ledger belongs to each client.
Each charged allocation holds one real ledger reference in an aligned prefix,
without a second allocation per block. The ledger survives the client while
results are held; it retains no client/engine/stream or worker. Last result release
can occur on any thread and destroys the ledger after its last actual reference.
The relevant product code must remain loaded through all final releases.

Result bytes include the requested block, its private quota prefix and batch maps
or response descriptors/raw-ID views/wire. `ResponsesInfo.OwnedBytes` continues to
describe the public snapshot's header/payload storage; its transport quota prefix
is additional. Allocator bookkeeping/rounding, Future/cancel/native request/error
metadata, receive staging, connection buffers and thread stacks are separate.
Request counts, per-document/configured receive limits and charged result counts
bound their ownership, but result bytes are not an RSS limit or a bound on arbitrary
user-created diagnostics. Request and result counters are separate instants, not a
joint atomic snapshot. Admission/reservations can be observed before malloc ends.

The optional product-private `xrpc_result_quota` module is selected by HTTP client.
Standalone raw response/native call selections do not pull its locking dependency.
Their uncharged allocation layouts/copies are preserved. Charged storage adds an
aligned prefix, one ledger allocation per client, a mutex acquisition and ledger
reference for each block allocation/free. JSON validation/copies are unchanged.
Native Future ownership still refuses opaque collector graphs.

The runnable `examples/http_result_quota/main.c` demonstrates refusal while a reply
is held, followed by successful reuse after release. Tests cover pending headers,
retained Futures/maps/snapshots, independently held responses after full engine
cleanup, exact block/byte refusal, later snapshot LIMIT, parallel final release,
allocation failure/credit rollback and existing constructor OOM sweeps.
The quota microbenchmark alternates actual allocation/free and exact-limit
refusal, reports all offers and returns to zero. HTTP performance remains measured
separately with the real client/server fixed-arrival harness; neither establishes
native Linux, saturation or long-run production acceptance.

Configuration/info structures have expanded; rebuild consumers with matching
headers and libraries. Public layouts are independent of feature selection;
the developing product does not promise mixed-version binary ABI stability.

HTTP submission can allocate request bookkeeping before it attempts header
credit, so a whole-API refusal may allocate. The private ledger refuses exhausted
credit without malloc; its microbenchmark does not measure full HTTP admission.

Use one consistent xrpc implementation for the required module union per loader
namespace, with all sources built using that feature selection. Declaration-only
consumers may select subsets. Combining overlapping duplicate implementations or
releasing an object through a different build is unsupported.
