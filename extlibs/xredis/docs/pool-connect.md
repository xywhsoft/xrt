# Automatic pool connections and repair

Select `XREDIS_MODULE_XREDIS_POOL_CONNECT`. This optional **developing** module
adds bounded automatic construction, readiness and replacement to the native
lease pool. It does not replay calls, transactions or WATCH scopes. Sentinel,
Cluster, dedicated modes and final production/performance acceptance remain
part of the full product plan.

Initialize `xredispoolconnectconfig`, set Connect/Data and optionally Drop, then
call `xredisPoolConnect` with a target member count. Successful construction
consumes the context/drop obligation; failed construction leaves both that
obligation and the output unchanged. Config values are copied. Pool creation is
asynchronous. Each accepted acquisition retains its original monotonic deadline
and cancellation while no member is ready, including setup/backoff/outage wait.
One acquisition's timeout never cancels shared pool maintenance.

The factory runs serially on the existing pool worker, outside gates. It must
return promptly and start one of the existing managed numeric, DNS, TLS or TLS
client-auth factories. Apply the provided finite setup timeout to that factory.
Success transfers one private client reference; false transfers nothing. Each
stable slot number has its own saturating attempt counter, enabling credential/
endpoint refresh. Keep the intended service/auth/database/name/RESP profile the
same across generations. The pool validates managed ownership, engine and
explicit expected RESP2/RESP3 before admitting a member. It cannot infer erased
credentials or arbitrary peer profile changes. Config/keys/code referenced by
the context remain valid until Drop. Secrets are never captured by this module
in diagnostic errors; their context storage/erasure remains the factory owner's
responsibility. Native setup storage keeps its existing secure wipe contract.
The native factory contracts for shared resolver ownership/close/join and
callback-code retirement still apply; Drop retires the pool's context obligation.

At most MaxConnecting candidates, including failed candidates' physical close
tails, are in flight. Every slot has at most one client generation. No successor
starts until a failed predecessor has closed, its requests, lease and nested
WATCH duties have retired, its watches have joined and its native claim has
been released. Ready callbacks only queue work; a candidate is claimed before
initialization can expose it to business calls. A ready result must match the
configured RESP protocol. Setup failures, transport loss, lease Abort and forced
retirement replenish the slot. Existing Futures preserve their original typed
outcome, including unknown remote execution after send.

Defaults are four concurrent candidates, five seconds per attempt and a retry
range of 100 ms..10 seconds. Concurrency is bounded by the target member count.
Setup and retry times have hard positive maxima of 60 seconds. Consecutive
failures increase a per-slot exponential cap up to RetryMax. Actual delay is
selected from RetryMin through that cap using non-cryptographic
jitter; successful readiness resets the failure streak. An outage does not
retain an unbounded retry history. Counters saturate at UINT64_MAX. The factory
is never asked to replay failed business operations.

Maintenance uses a preallocated indexed min-heap and intrusive runnable queue,
O(log members) deadline changes, bounded work per worker turn and one shared
timer. Due retry entries move to the runnable queue even at the concurrency
limit, so they cannot hide later candidate timeouts in the heap. No additional
thread, executor or runtime is created. Timer allocation failure closes the pool
and records MEMORY rather than spinning through an unbounded OOM retry loop.
Client factories retain their own bounded native setup resources.
The business command quota excludes setup wire and per-member parser/push
storage. Include every member's configured native bounds and transferred result
storage separately when budgeting aggregate memory.

PoolClose stops new attempt admissions, closes existing candidates/members,
retires retries/timers/source notifications and invokes Drop exactly once on
the pool worker outside gates before PoolClosed. A factory already admitted
concurrently with Close can finish, but its candidate is drained. Drop must not
block. Completed retained metadata holds no context/client/engine reference;
the final current completion/post tail may still return, so join the engine
before unloading callback code. PoolConnectInfo reports in-flight candidates,
waiting slots, attempts/failures/connected generations and last typed failure.
PoolInfo.Retired counts actual retired clients and can exceed target membership.
Invalid factories are contract violations; transferred invalid candidates are
retired instead of becoming ready members.

The [runnable example](../examples/pool_connect/main.c) starts a local protocol
fixture, acquires and calls a member, aborts it, automatically creates a
replacement, then closes/releases every resource. Prerequisites: Python 3, C11
compiler and ordinary XRT platform libraries. Run from the repository root:

```sh
python tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_pool_connect --no-single
python tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_pool_connect_tests
python tools/package.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_pool_connect --kind static --verify
python tools/package.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_pool_connect --kind shared --verify
python tools/measure_performance.py --config extlibs/xredis/config/performance_profiles.json --profiles xredis_pool_connect
```

Matched GCC x64 probes with the same TLS/auth closure retain 104-byte native
requests, 856/928-byte Windows/WSL clients, 184-byte slots (208 with nested
WATCH), and 200/272-byte leases. Selecting the optional connector adds one
8-byte pool pointer: prefixes become 448/520 bytes. Automatic construction
allocates one 200-byte maintenance prefix plus 128-byte entry and 8-byte heap
cell per slot. Public connector config/info are 72/56 bytes in both probes.
Allocator headers, native Futures, clients/setup and timer nodes are additional.
Fixed-only selection omits the pointer and maintenance code/storage.

The performance profile measures warm lease cycles both with maintenance
enabled and with fixed membership, using the same source and feature closure,
identical peer, input, workers and timed loop. It separately measures
forced replacement cycles including physical retirement, retry,
TCP/HELLO/readiness and successor reuse. Pool startup is outside timing; all
warm-cycle acquire/call/return/observer costs remain inside. These local fixture
measurements do not establish Hiredis parity, final tail latency or native
Windows/Linux deployment acceptance. All added heap/state storage, callbacks,
references/pins, gate/atomic operations, clocks and timer changes are real costs.

## Longer samples and explicit CPU placement

`xredis_pool_connect_sustained` keeps the same timed loop and peer, with 100000
cycles per fixed/automatic warm sample and 1000 per physical repair sample.
The original shorter profile remains available. Both retain five repeats plus
one warmup and the existing 3% relative MAD / 6% central-range limits. Process
CPU time per cycle is also reported; its Windows counter granularity still
limits short samples. Startup, affinity setup, sample sorting and teardown are
outside timing, while acquire/call/return and observer release remain inside.

The benchmark executable accepts `COUNT MODE [CPU_MASK]`, where MODE is 0 for
automatic warm cycles, 1 for forced repair and 2 for fixed membership. An
optional nonzero mask (for example `0x110` on a compatible machine) is validated
against the process's allowed CPUs and applied before any engine worker starts.
The measured program prints its actual placement. Windows supports a mask in
the current processor group; Linux supports the low 64 CPU bits and workers
inherit the main thread's restricted set. Other platforms reject an explicit
mask. Invalid/unavailable masks fail before construction. An omitted mask
leaves placement unchanged. This changes only the benchmark process, with no
priority, power-policy or timer-resolution-setting change.

Select distinct physical cores using the host's actual topology, rather than
assuming adjacent logical CPUs are separate cores. A process mask does not
isolate the host, fix individual workers to particular cores or pin frequency.
Record these limits and preserve failed noise gates. A paired comparison must
use the same benchmark/peer/input/worker/CPU budgets and identical current
sources except for the deliberately pinned IOCP translation unit. Keep the
original short/unpinned results; new placement or longer samples do not turn
them into accepted performance evidence.
