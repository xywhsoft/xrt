# Selected live service (developing)

The optional `xcron_service_selected` mode uses the existing service owner,
command tickets, event waiter, retry heap and reserved completion channel. It
adds complete fold/missing job selection and immutable nominal worker metadata.
Use the typed APIs in `service_selected.h`; generic cancellation, Info/Reap
requests, close/wait/failure/wake/destroy retain their original contracts.

An accepted command is distinct from an admitted job. Selected job admission
copies/retains without recurrence traversal. Initial and later calculation
faults belong to the admitted job's full `Info.LastIssue`, rather than turning an
admitted Data duty into a failed-ADD reply. Data remains borrowed until retirement.
Each owner poll has five global search dimensions capped by Scheduler.WorkLimit.
Calculation still progresses when output credit is full.

SubmissionBatchLimit is explicit, positive and at most64. Only already-ready
leases are grouped; the owner never waits to fill a group. Native batch capacity
is one worker queue. Each group member counts one attempt; no-effect AGAIN is
retried individually under that lease's original deadline and attempt limit.
Every worker outcome and unsent cancellation/rejection/expiry returns the original
nominal label and complete candidate count without exposing borrowed Data.

Ordinary public structure layouts remain unchanged. Selected sidecars consume
bounded arena bytes. Active selected runs reuse the future completion storage
through a C11 union with a common initial Run; Finish snapshots the label before
writing the completion record. The complete union type is visible wherever the
common initial sequence is read; anonymous structure promotion and common initial
sequence access follow C11 6.7.2.1p13 and 6.5.2.3p6 in the
[WG14 draft](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf).
The implementation targets C11. This is not a C++ object-lifetime claim.

Read the [runnable example](../examples/service_selected/README.md). Factory width1
selects scalar admission; larger widths cap the already-ready initial group.
This is separate from BatchLimit (owner scratch/work limit), CompletionLimit
(reserved result credits) and the borrowed pool's per-worker queue capacity.
The worker sees a borrowed immutable token view; retain its values by copying,
and retain business Data through a full retirement receipt. A full selected reply
is required for Info/Reap, including when requests use the shared async APIs.

The selected component benchmark advances a custom clock in64-due UTC bursts.
It measures recurrence, owner scheduling, actual workers, full nominal validation,
two shared callback atomics and physical receipt collection. Width1/64 and1/4
workers are distinct explicit workloads. This fixture does not measure real
fixed-arrival lateness or timer accuracy. Source fingerprints, individual samples,
unchanged noise gates and preserved binaries distinguish observations from
qualified performance. No production throughput or scaling claim is made.

No new worker/runtime or per-operation successful-path heap allocation is added.
Mode checks, metadata copies, sidecars and rejection-error references have costs;
this is not a zero-overhead claim. Diagnostic/error cold paths can allocate.
Compensation, durable recovery and full production/performance qualification
remain open.

## Measured x64 storage

The original public config/reply/completion/stats remain136/296/80/384 bytes.
The selected counterparts are144/520/136/448 bytes, identically declared by the
modular and declaration-only single headers. The440-byte private command and
184-byte reservation retain their original sizes and checked offsets; selected
metadata occupies the active run's future completion space rather than widening
that reservation. The service owner gains one8-byte pointer in an extended build.

Against an ordinary instance in the same extended build, selected service arena
storage adds120 bytes of mode state,232 per command ticket,56 per completion
credit and112 per BatchLimit, plus alignment if necessary. The two typed batch
buffers replace the original buffers. The pre-existing selected kernel adds376
bytes per job query, and the selected adapter adds56 bytes per live lease slot.
The extended dependency closure also has the existing16-byte kernel-owner and
8-byte adapter-owner differences versus lean leaves; include those when comparing
whole instances against a lean ordinary build.

For JobLimit/RunLimit/CommandLimit/CompletionLimit/BatchLimit=2/2/3/2/2, Windows
measures lean ordinary arena/kernel/adapter=2880/1592/400 bytes, extended ordinary
2888/1608/408, selected4040/2360/520. Linux/WSL has280 extra service-owner arena
bytes in each mode (3160/3168/4320 respectively), with the same kernel/adapter
measurements and the same1152-byte selected service arena difference. Native thread/runtime bootstrap, borrowed
Data/zones/contexts and native waiter handles remain outside these requested
component-byte sums. The owner keeps the existing retained-byte/startup-budget
contract. Callback views, result copying, mode checks and cold diagnostics must
also be counted in any end-to-end overhead comparison.

## Validation record

[Stage evidence](../tests/results/service-selected-20261109.json) preserves both
platforms'33-program root suites, ordinary-leaf and selected tests, four WSL TSan
invocations, examples, root/isolated static/shared consumers, exact frozen oracle
results, public/private layouts,90 tool tests and generated/documentation checks.
Final validation and measurement share2556 unchanged hashed inputs;51 relevant
source/config/header copies and all eight formal executables are retained.

Each formal process validates64000 callbacks and independent ordinal checksum
2048032000; original five repeats, one warmup,3% relative MAD and6% central-range
noise gates remain unchanged. Reports retain12 Windows and16 WSL failed noise
checks. The separate two-round mechanism check uses the preserved executables:
scalar widths issue no native groups; batch widths publish two groups/128 leases.
These are unqualified component observations. Four-worker scalar throughput and
real fixed-arrival/native production performance still require investigation.
