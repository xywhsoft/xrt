# Pooled transactions and nested WATCH scopes

Select `XREDIS_MODULE_XREDIS_POOL_TRANSACTION` or
`XREDIS_MODULE_XREDIS_POOL_WATCH`. These are separate optional integrations with
the existing native transaction/WATCH implementations. They introduce no new
runtime or thread. Overall transaction/pool/product and these integrations remain
**developing** until final fault, native platform and performance acceptance.
Automatic pool repair/reconnect, repeated WATCH key extension, HA and other
remaining product scope are still required by the full plan.

## Native transaction on a lease

`xredisLeaseTransaction[Async]` uses the native staged MULTI / queue / EXEC or
DISCARD operation. It returns the same `xredistransactionresult` Future directly,
with no adapter Future, wire clone, per-command result wrapper or per-request
lease pointer. Configuration and command views are borrowed only during
admission. Timeout includes encoding/admission and is capped at the original
finite lease hold. Output changes only on ACCEPTED.

The successful native enqueue seals the lease against further ordinary calls,
transactions or WATCH starts. Earlier leased calls drain before MULTI; all
queued responses are checked before EXEC. Queue rejection suppresses execution
and requires validated DISCARD. Runtime error elements do not roll back other
elements. A null EXEC and known EXECABORT establish abort, while accepted EXEC
bytes without a validated outcome retain explicit uncertainty. No replay occurs.

Return the lease explicitly after inspecting its result. Logical completion
alone does not return a FIFO slot, storage or group quota. Lease Closed waits
for physical native wire/timer/cancel/drain obligations; malformed/unknown state
retires the connection. A Return racing native construction seals the owner at
the same client gate checked by final admission. Native byte limits include the
transaction's control wire, alignment and private state, not just business bytes.

## WATCH borrowing and parent lifetime

`xredisLeaseWatch[Async]` starts a native WATCH scope on a physically idle lease.
Busy returns OVERLOADED. Validation/OOM/admission failure before native acceptance
restores the unused lease; a concurrent Return/Close cannot be reopened by that
rollback. Successful acceptance consumes/seals the lease for the scope. Ordinary
parent calls, a second transaction and a second WATCH cannot enter its state.
Every accepted start outcome includes the native borrowed scope handle, including
failure/cancel/OOM, so its physical Closed barrier is always available.

Use `xredisWatchCall` and `xredisWatchCommit` on that handle. Scope lifetime and
request deadlines are capped at the original lease hold. No call renews either
budget. All WATCH/read/transaction/UNWATCH native reservations inherit the same
pool quota group, retaining aggregate count/bytes until actual storage release.
A locally canceled sent read/transaction cannot create capacity for another
operation until its physical FIFO duty drains or the client retires.

A zero native-request count during application think time does not authorize
pool handoff. A private nested owner temporarily replaces the slot owner under
the native client gate. The parent retains a nested-retirement marker until the
child's native duties, notifications, independent engine pin and physical Closed
publication have all retired. Only then can the pool release/reuse the slot.
This marker also covers the short interval after restoring the parent owner but
before child Closed publication; different client/pool workers cannot race past
it. Completed retained child metadata never dereferences a destroyed pool.

Normally await scope Closed and explicitly Return the lease. Returning a still
active scope requests native bounded abandonment: an idle scope validates
UNWATCH before reuse; a busy or uncertain stream may retire. Return grace,
Abort, lease hold expiry and PoolClose continue to bound parent duties and
cannot return an active child. Grace/hard-hold expiry can force physical closure.
PoolClose immediately closes native group admission and walks members in bounded
turns. A failed/rejected/malformed UNWATCH cannot yield a reusable parent. A
parent Closed observer must see the child's Closed already resolved, including
failed start and concurrent acquisition/transaction/Return races.

Matched GCC x64 probes (same TLS/auth selection) report ordinary requests 104
bytes and clients 856 bytes on Windows / 928 on WSL, unchanged from pool/WATCH
selection. Enabling nested ownership changes the private member slot from 184
to 208 bytes, and the combined WATCH scope from 616/688 to 640/712. Pool fixed
prefix stays 440/512 and lease metadata 200/272. Public client config remains
232, ordinary result 24. Parent/abandonment pointers plus the atomic retirement
marker/alignment account for the additional 24 descriptor bytes. Standalone
selections omit these fields. Allocator/Future/result/timer and cold construction
resources are additional; these are not native MSVC/Linux ABI acceptance.

## Ownership and bounds

Keep acquisition/start Futures alive while borrowing lease/scope handles, or
Retain and later Release handles explicitly. Dropping observers does not renew
lifetimes or silently release protocol duties. Outputs must not overlap input,
lease or native-client storage. Sync facades reject every worker of the engine,
including a different affinity. Callback code must not block network workers.
Join the engine before unloading callback code, including its final return tail.

Existing pool pending/request/byte/work limits, native connection/parser/push/
read/drain limits and transaction/WATCH config bounds remain effective. These
APIs do not weaken validation or erase caller-owned buffers/results. No extra
runtime handles or replay queue are introduced. Public structure layouts do not
depend on compile-time selection.

## Actual costs and current verification

The integrations reuse the existing lease API gate and native owner/admission
gates. Transaction calls add no per-call adapter resources. Nested WATCH reuses
its native scope and existing Future/timer/cancellation resources and takes an
independent native client reference/engine pin. Optional nested selection adds
parent/abandonment/retirement metadata to the private owner descriptor, shared
by slots and scopes. Ordinary native request allocation and client owner-pointer
counts do not grow. Retain/release, gate/atomic operations, quota CAS/decrements,
notifications, timing, allocation, descriptor scans, encoding/copies and network
round trips remain real costs. No complete zero-overhead or performance-parity
claim follows from reuse of a Future.

The self-contained [transaction example](../examples/pool_transaction/main.c)
and [WATCH example](../examples/pool_watch/main.c) assert native outcomes, explicit
child/parent barriers and cleanup. Prerequisites are Python 3, a supported C11
compiler and the ordinary XRT platform dependencies. From repository root:

```sh
python tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_pool_transaction --no-single
python tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_pool_watch --no-single
python tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_pool_sessions_tests
python tools/package.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_pool_transaction --kind static --verify
python tools/package.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_pool_watch --kind shared --verify
python tools/measure_performance.py --config extlibs/xredis/config/performance_profiles.json --profiles xredis_transaction,xredis_watch,xredis_pool_transaction,xredis_pool_watch
```

Tests cover plain/TLS binary calls and owned input, staged/error/null/opaque EXEC,
queue metadata quota, preceding requests, busy WATCH refusal, nested quota held
after local cancellation, rejected/malformed/held cleanup, startup cancel, scope
and parent deadlines, parent abort/close, 146 admission budget windows and
rollback, different client/pool workers, parent-before-child observation checks,
sync refusal and 20 rounds of four-thread WATCH/transaction/call/Return races.
Retained results/handles survive complete pool/engine destruction. Individual
codec/native/pool regression suites still apply; no narrow check replaces them.

An opt-in disposable Redis matrix adds native leased SET/GET EXEC, scoped reads
and conflicting writes, scope-before-lease closure, repeated acquisition of the
same physical member followed by plain EXEC after an external write (no inherited
WATCH), and ACL-denied nested UNWATCH followed by physical retirement:

```sh
python extlibs/xredis/tests/interop/run_redis_tls_auth.py --pool-sessions --key rsa --build --report out/pool-sessions-redis.json
```

The exact-source runner also retains the preceding pool/transaction/WATCH
oracles and independent Windows TLS startup health. It creates/deletes ephemeral
certificates; requires local Redis with TLS and OpenSSL; never uses Redis
implementation source. Native Windows clients may run against a WSL peer;
that does not certify native Linux deployment. The performance profiles measure
complete warm session cycles including acquisition, child/parent physical
barriers and observer release, with TCP/pool construction excluded. Unpinned
local C-peer samples retain unchanged noise gates and are diagnostic. Matched
native Windows/Linux/Hiredis, full workload/cost sweeps, recovery/fault and final
>=72-hour mixed/fault acceptance remain pending.
