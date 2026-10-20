# Native Engine ownership (candidate v212)

Engine has an actual strong count distinct from operational LiveObjects. The
creator, each object/Pin and every graph hold own one reference; embedded workers
are not separate RC nodes. Successful retirement consumes only the creator.
The counted shell and stable worker descriptors survive independent graph holds.

OwnedV1 Post/Schedule transfer one certified context reference on success only.
The accepted command slot or timer owns it through callback and Drop, including
timer recycling and reentrancy. Failed submission consumes nothing. The borrowed
APIs retain their contracts and pending borrowed callbacks refuse graph views.

Frozen inspection reads actual MPSC slots and timer heap entries. Worker turns
publish a parked boundary around callback-free port waits; producer submissions,
lifecycle and real counts participate before locks. Active bodies and thread-key
cleanup refuse inspection. Read-only thread completion permits revalidation after
exit without requiring another Prepare first. Prepare drains and joins; Clear and
Finish never consume a creator reference or confuse worker occupancy with RC.

Calling the public xrtNetWorkerPort API exposes a raw external capability and
therefore keeps graph admission conservatively refused until the port is
retired. XRT's TCP, UDP and file transports use a private, immediate and
non-escaping worker-port borrow instead; that borrow must never be returned to
user code or stored beyond the operation that requested it. Embedded borrowed
commands and live pool blocks remain uncertified and explicitly refuse graph
admission. Transport/buffer ownership, complete native service graphs,
generated module integration and OS-level code unload proofs are still required
work, not waived by this Engine foundation.

The creator and an operational Pin are different duties. Pin adds one physical
reference AND one live-object admission; Unpin releases both. Plan Hold adds one
reference only, so it cannot keep native workers running or fake an operational
root. Creator Destroy and typed Finish can retire resources in either order,
but independent holds retain the shell until their own Drop. Restart is legal
after ordinary Stop, not after ownership retirement or a claimed collection.

Adapter resolution admits exact task and timer descriptor identities separately
before any child Count/Trace. A byte-identical descriptor is not interchangeable.
The graph visits the accepted MPSC slot before a timer is inserted, and its one
heap slot afterwards. Node-cache allocations, worker descriptors, timer hash
links and port implementation buffers are uniquely owned storage, not fictional
reference-counted nodes. User resource duties exposed through a raw port or live
pool block still require the forthcoming native contracts and are not discarded.

DEMO6 v212 discovery kept the first GCC/O2 fixture indentation warning as a
failed build; TCC ran all 21 programs successfully. With only that formatting
fixed, GCC O0/O2 and TCC each passed all 21 focused programs. Each new ownership
program covers 12 idle/pin/held-shell cases, 18 actual command/timer graphs,
18 legacy/raw-port refusals, six real thread-key exit windows and 24 allocation
budgets (three actual failures), with exact memory and invalid-access balance.

The unchanged XLIR planner also passed 96 owner cycles per compiler, including
48 explicitly observed timer-heap graphs, 48 policy identity refusals and 48
resurrection/abort/retry cases. Each cycle performs one task, timer notification,
language finalizer and actual Root free. The constructor's root is physically
dropped, and a resurrected external owner is retained until a new plan pins it;
no fabricated internal slot or reference-count subtraction is used.

Verification evidence is recorded by DEMO6's v212 private candidate runner.
These focused results are not a full native/library/module completion claim.
No production amalgamation or release DLL is replaced by this native candidate.

## Managed worker exit and restartable stop (2026-10-20)

`xrtNetEngineCreateOwnedExitV1` transfers one context only on successful
creation. The exact permanent policy has an Exit callback on every actual
worker, including partially started engines, and a single Drop after physical
run-resource retirement. Stop/restart does not drop the context. Worker identity
remains available through Exit and TLS cleanup; neither a parked marker nor a
finished callback body substitutes for actual native thread completion.

`xrtNetEngineOwnershipAdapterV2` requires the exact admitted exit-policy pointer
before following its one context edge. V1 remains valid for ordinary engines
but refuses the new context without invoking its Count/Trace callbacks. Drop
runs outside the lifecycle lock and ownership mutation, while Retiring still
prevents reentrant consumption. Clear/Finish still do not consume the creator.

`xrtNetEngineTryStop` is nonwaiting and nonconsuming. Live objects keep the
engine running and return BUSY; otherwise it progresses STOPPING to STOPPED,
allowing restart only after real worker exits, joins and run-resource cleanup.
BUSY/READY preserve the ambient error. Failure preserves the pending object
for explicit retry. Callers serialize lifecycle controls. Between joining
steps, pending STOPPING can enter the ownership graph only if all existing
worker/queue/timer/port/pool checks also pass; it is not blanket-quiescent.

`xrtNetEngineGetConfig` copies requested settings and buffer-pool values into
caller storage, never publishing an internal pointer. Actual per-worker backend
metadata is separate: `xrtNetEngineBackendInfo` only reads a stable running
worker's port and does not expose the raw-port capability or disable collection.
Compiled driver metadata is not proof that a backend can start on a host.

Focused GCC/TCC O0/O2 Windows/Linux tests cover the old graph contract, new exit
tails, stop/restart, pending-stop graph adoption, exact policy refusal, requested
versus actual backend, failed creation/start prefixes and balanced memory.
This is lifecycle acceptance without sockets, network traffic or security tests;
it does not certify the future language Engine facade or complete transports.
