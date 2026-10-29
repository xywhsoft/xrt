# Bounded exclusive connection pools

Select `XREDIS_MODULE_XREDIS_POOL`. This is the fixed-membership lease core on
existing managed clients; the module and product remain **developing**. Automatic
connection creation/replacement/reconnect, HA routing, final performance and production acceptance remain required by the
full project plan. This core does not redefine that final scope.

Optional [pooled transaction/WATCH integrations](pool-sessions.md) reuse this
core's native quota and physical lease barriers without per-call Future adapters.

## Construction and session profile

Create clients with the existing numeric, DNS or TLS factories, await readiness
and retire outstanding requests. `xredisPoolCreate` borrows an array of 1..65536
distinct managed clients on one running engine. Every client must be OPEN, ready
and physically idle. The negotiated RESP version must match. Caller-supplied
members must share the intended service/credentials/database/session profile;
the pool cannot reconstruct erased credentials or infer arbitrary server state.
Different endpoints may represent that same logical service. The constructor
sorts a temporary pointer copy to reject duplicates, then claims every member.
A failure rolls back all earlier claims and preserves the output and caller refs.

On success the pool retains/claims members. It holds one engine pin per claim
and one for its runtime, and uses one existing worker plus embedded posts for
bookkeeping. Members keep their own original worker affinity. There is no new
socket implementation, executor or thread. Direct ClientCall/Transaction/WATCH
admissions cannot enter claimed members. Original caller refs, Close observers
and an external client Close remain valid. The pool closes its members on
PoolClose. The caller must Close then Release the pool; Release alone does not
close a live object.

## Acquisition, direct calls and physical return

`xredisPoolAcquire[Async]` has bounded admission. Initialize the config; defaults
are 256 concurrent acquisition constructors/queued requests, 4096 native command
reservations, 64 MiB encoded command storage, 30 s maximum lease hold, 5 s return
grace and 64 work items per queue/category per worker turn. Pending/work limits
have hard maxima 65536; lease hold must be positive and at most 24 hours, return
grace positive and at most 60 seconds. Acquire Timeout is signed milliseconds
(-1 forever, zero already due), starting at API entry through allocation, queue
wait and grant. FIFO means accepted enqueue order, not the entry order of two
overlapping API calls still constructing their state. Idle members also rotate
through a FIFO. Queued expiry/cancellation does not block the next waiter.

Every accepted acquisition resolves an immutable `xredisleaseresult`. Only OK
contains a borrowed Lease; keep its Future alive or explicitly Retain/Release
the lease. Admission outputs change only on ACCEPTED. OVERLOADED and CLOSED are
normal admission results. All-dead membership resolves queued/new accepted
acquisitions with DISCONNECTED; no failed operation is replayed. Cancellation
is observed before final grant and does not revoke a completed grant.

`xredisLeaseCall[Async]` returns the native request Future/result directly. It
copies/encodes input with the client admission contract; it adds no wire clone,
per-call adapter Future or per-request lease pointer. Pipeline ordinary one-reply
commands within one lease. A blocking command owns that dedicated stream/FIFO
slot; other leases cannot inject work into it. Known protocol, subscription,
connection/profile and manual transaction controls are rejected. Unknown module
commands must obey the documented one-reply/profile contract. The effective
request deadline is capped at the original lease hold deadline; calls do not
renew it. Sync facades refuse every worker of the engine, including one with a
different affinity. No user completion runs while a pool/lease gate is held.

Return seals new calls and waits for every API constructor and native request
reservation to physically retire. A canceled/timed-out sent call still owns its
FIFO/wire/timer/cancel obligations and pool quota. Return grace is capped by the
original lease hold; expiry retires a busy stream. Abort requests immediate
connection retirement. An idle hold expiry can return a clean member. Release
alone does not return a lease; finite expiry recovers dropped observers. Old
returned handles cannot address a later lease generation. Abort/Return are
idempotent, and PoolClose prevents new native admissions via the quota group's
atomic close flag before walking members in bounded worker turns.

`xredisLeaseClosed` resolves with Reason/Reusable after the actual slot boundary.
Reusable certifies the clean release decision, not future availability or freedom
from concurrent external closure. Await physical return before assuming a FIFO
slot/quota is available. PoolClosed resolves NULL after all members physically
close/unclaim, pending constructors/acquisitions and lease duties retire, and
notifications retire. Completed retained lease/acquisition/pool metadata pins no
client or engine. The current final completion/post return tail can still be
returning; join the engine before unloading callback code. Lifetime tests wait
boundedly for actual allocation counters to reach zero after dropping observers.

## Aggregate bounds and actual costs

The hard aggregate native request/command-byte counters reserve before native
construction and release after actual storage free. They cover constructor
failure, queued/sent work, cancellation, immutable wire and timer/cancel tails,
not just logical Futures. Native local request/byte/parser/push/read/work/drain
limits also remain active. The maximum member count bounds their aggregate
configured storage; retained results/replies transferred to callers remain
caller-owned. Acquisition/result metadata is bounded in the live backend by
MaxPending and member count; arbitrary caller retention is not silently charged
to an unrelated later request.

WATCH and pool share one optional private client owner pointer; selecting both
does not add a second pointer. No ordinary request layout growth is required.
When pool is selected, the private owner descriptor also has a quota-group
pointer (zero for standalone WATCH scopes). Client admission checks owner/state
at reservation and publication. Pool-owned reservations use shared atomic
request/byte CAS counters and physical-release decrements; failed reservations
roll back. These are real synchronization/contention costs.

Each lease uses a gate, producer/observer pairs and preallocated acquisition/
return results, one acquisition cancellation watch, a queue timer when needed
and a finite hold timer. API entry/exit uses the lease gate. Active calls avoid
unnecessary pool posts on ordinary API return; sealing/closure wakes the pool,
and native physical idle transitions coalesce slot events. Pool bookkeeping
uses bounded O(1) idle/waiter/event queues. Cold construction allocates slot
storage, sorts a temporary pointer array, registers client-close watches and
takes references/pins. No zero-overhead, complete allocation/syscall attribution
or independent performance parity is claimed.

Matched GCC x64 layout probes (same TLS/auth closure) report:

| Private/public storage | Windows | WSL |
| --- | ---: | ---: |
| Ordinary native request allocation base | 104 | 104 |
| Client with pool or WATCH or both | 856 | 928 |
| Same client without either owner feature | 848 | 920 |
| Pool fixed prefix, including quota group | 440 | 512 |
| Per-member slot, including owner/close watch | 184 | 184 |
| Per-acquisition/lease metadata | 200 | 272 |
| Embedded quota group (included in pool prefix) | 40 | 40 |
| WATCH scope with both features selected | 616 | 688 |

Public client config remains 232 bytes and ordinary request result 24. Pool
selection alone leaves wire offset 102; transaction/WATCH selection uses 103
within the same 104-byte allocation base. Sizes omit allocator headers, Future/
result/timer storage and temporary constructor arrays. These measurements do not
establish native MSVC or native Linux deployment ABI/performance acceptance.

## Reproduction and evidence scope

Prerequisites: Python 3, a C11 compiler and normal XRT platform dependencies.
From the repository root:

```sh
python tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_pool --no-single
python tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_pool_tests
python tools/package.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_pool --kind static --verify
python tools/package.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_pool --kind shared --verify
python tools/measure_performance.py --config extlibs/xredis/config/performance_profiles.json --profiles xredis_pool_ordinary,xredis_pool_managed,xredis_pool_leased,xredis_pool_cycle
```

The [example](../examples/pool/main.c) creates two local fixture members, acquires
both, executes a binary native request and asserts physical return/close while
releasing every resource. Tests include FIFO and bounded queue admission;
queue cancel/expiry and zero timeout; aggregate count/byte quotas held after
logical cancellation; graceful pending-command drain; idle/busy hold expiry,
return expiry, forced abort, external/pool close; old generations and retained
metadata; dropped observers; partial constructor rollback; protocol/engine
profile boundaries; invalid/alias/control inputs; cross-worker sync refusal;
constructor/acquisition/native-call budget windows; cold worker timer OOM; and
eight producers pipelining over four members. The OOM fixture sets the existing
engine NodeCacheBytes=0 to force actual timer allocation; examples, benchmarks
and the independent peer retain the ordinary cache defaults.

Performance profiles include the original raw pipeline with pool selected,
identical compiled managed-client versus held-lease pipelines (same payload,
count/depth/timestamps/checksum), and a complete warm acquire/PING/physical
return/observer-release cycle. The latter two separate per-call lease costs
from checkout/return; neither measures pool/TCP construction. Unpinned local
C-peer runs are diagnostic and preserve the unchanged noise gates. They do not
replace matched Hiredis, fixed-arrival/multiconnection/worker sweeps or native
Windows/Linux sustained performance acceptance.

The opt-in disposable real Redis runner adds two pooled authenticated TLS
members and a separate admin, RESP2/3 binary calls, queued cancellation, a
server-confirmed blocked BLPOP, aggregate quota retained across logical cancel,
LPUSH-unblocked physical drain and safe FIFO handoff:

```sh
python extlibs/xredis/tests/interop/run_redis_tls_auth.py --pool --key rsa --build --report out/pool-redis.json
```

It requires local Redis with TLS and OpenSSL, creates/deletes ephemeral keys,
and pins exact source/header/helper/compiler/binary identity. Native Windows
clients can use the existing WSL peer/Windows health probe; WSL is not native
Linux deployment acceptance. The semantics follow official
[BLPOP](https://redis.io/docs/latest/commands/blpop/) and
[CLIENT LIST](https://redis.io/docs/latest/commands/client-list/) documentation;
no server implementation source is used. Final fault/recovery, native Linux,
MSVC where applicable and >=72-hour mixed/fault acceptance remain pending.
