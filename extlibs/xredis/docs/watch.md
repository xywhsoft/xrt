# Exclusive WATCH scopes

Select `XREDIS_MODULE_XREDIS_WATCH`; it depends on `xredis_transaction` and the
existing client, not a new scheduler, executor, engine, socket stack or pool.
The module and the overall product remain **developing**. Local protocol and
lifetime checks do not constitute production or performance acceptance.

## Session and ownership

1. Connect with a managed factory and await readiness. Retire outstanding
   native requests before starting a scope. Start claims only an OPEN, ready,
   idle connection; not-ready/busy/already claimed returns OVERLOADED. Raw
   `xredisClientConnect` is refused. Admission outputs change only on ACCEPTED.
2. Pass one or more binary keys to `xredisClientWatch[Async]`. Views/descriptors
   are borrowed only during admission; the native request owns encoded bytes.
   Read the returned `xrediswatchresult`, and check `Request.Status` before
   issuing commands on `Watch`. Every accepted outcome exposes a borrowed
   handle, including failed/cancelled starts, so the caller can always await
   its physical Closed barrier. Only validated WATCH `+OK` permits commands.
3. Keep the start Future alive while borrowing `Watch`, or explicitly Retain
   and later Release it. `xredisWatchCall[Async]` performs ordinary one-reply
   commands on this dedicated connection. Other public ClientCall/Transaction
   admissions cannot enter its server-side state. Application writes retain
   their normal Redis effects; the scope provides no rollback.
4. Submit one `xredisWatchCommit[Async]` with transaction descriptors. Its
   successful native enqueue seals the scope atomically. Calls racing through
   allocation/encoding recheck ownership and state at enqueue; later calls and
   duplicate commits are refused. Earlier reads drain before MULTI.
5. Inspect the immutable `xredistransactionresult`. A null EXEC is ABORTED,
   distinct from execution uncertainty and runtime error children. No automatic
   retry/replay occurs. The staged transaction validates MULTI and all queued
   replies before deciding EXEC versus DISCARD.
6. Await `xredisWatchClosed`. Its typed `Reason` preserves the first terminal
   reason; `Reusable` certifies the physical clearing/idle boundary. Commit's
   logical Future alone is not the connection-release barrier. A retained
   completed scope/result/Closed observer does not keep its client or engine
   alive. The final callback can still be returning; stop/join the engine before
   unloading code, as required by the underlying client contract.

`xredisWatchAbort` is idempotent and nonblocking. Release alone does not abort.
Dropping all observers still leaves a finite lifetime and bounded cleanup.
Every sync facade refuses every worker of this client's engine, including a
worker with a different affinity. Future callbacks must not block workers.

## Deadline, bounds and cleanup

Initialize `xrediswatchconfig`. Defaults: 5000 ms total lifetime, 5000 ms cleanup
request timeout, 256 keys, 64 KiB WATCH wire, transaction limit 256 commands.
The total lifetime must be positive and no greater than 24 hours; cleanup
request timeout must be positive and at most 60 seconds. The key hard limit is
65536. Transaction config retains its own hard bound. Native client request,
byte, parser, push, work-turn and drain limits continue to apply.

The total monotonic deadline starts at the public start API entry, including
validation/encoding/admission, WATCH, application think time, reads and commit.
Every scoped call/commit caps its original deadline at this deadline. Calls do
not renew the lifetime. A separate native timer enforces it even with no active
request. Logical cancellation is not proof that bytes were not sent.

An idle abort/expiry or rejected MULTI sends UNWATCH and requires exactly `+OK`
before releasing the claim. Validated EXEC (including null abort), recognized
EXECABORT and a completed queue-error DISCARD provide clearing boundaries.
Malformed/rejected/timed-out UNWATCH retires the connection. An unknown EXEC
error retains uncertain execution and also retires it. Abandoning while native
requests/constructors still exist may retire the dedicated connection; no
attempt is made to reuse a possibly dirty stream. Once UNWATCH itself is the
last request, repeated Abort allows its existing bounded cleanup to finish.
The cleanup request's logical deadline is followed by the configured client
physical drain grace when necessary.

External ClientClose is observed through its physical barrier. The scope
separately pins the existing engine so native-client teardown cannot invalidate
its posts/timer. Physical closure removes completion/cancel watches, retires
all scoped request storage and API constructors, releases the exclusive claim,
then drops the client reference and independent engine pin. No new work can
enter the scope during this transition.

## Actual costs

WATCH and pool share one optional private owner pointer in the client; selecting
both does not add a second pointer. Pool selection adds a quota pointer to the
private owner descriptor, zero for these independent WATCH scopes. Ordinary
request allocation/tag layout does not grow beyond the transaction selection.
Admission gains owner/state checks under the existing client Gate and repeats
that check at publication. Physical idle transitions temporarily retain/wake
an owner when present. These are real bookkeeping costs, not a zero-overhead
claim; the selected ordinary-pipeline profile uses the identical existing
`bench_client.c` workload to help measure them.

Each live scope allocates its state with a mutex, embedded post and four
embedded Future watches; start/Closed producer-observer pairs and preallocated
results; one start cancellation watch; an existing engine timer; and the
ordinary native WATCH/read/transaction/UNWATCH requests as needed. A temporary
key descriptor array is copied during admission and promptly freed. Payloads
are encoded into native-owned wire once; explicit scope sizing and existing
command validation still scan descriptors/lengths. Completion notifications
use existing allocation-free embedded posts plus reference bookkeeping. The
scope adds no per-read Future, wire clone, per-request scope pointer, dedicated
thread or runtime beyond the native request that the operation already needs.
The complete cycle benchmark includes start/read/commit, physical Closed and
observer release. The C fixture is not an independent performance comparator.

## Examples and checks

From the repository root, prerequisites are Python 3, a supported C11 compiler
and the ordinary XRT platform dependencies:

```sh
python tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_watch
python tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_watch_tests
python tools/package.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_watch --kind static --verify
python tools/package.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_watch --kind shared --verify
python tools/measure_performance.py --config extlibs/xredis/config/performance_profiles.json --profiles xredis_watch,xredis_watch_ordinary
```

The self-contained [example](../examples/watch/main.c) asserts WATCH, scoped
read, EXEC and reusable physical retirement and closes every resource. Tests
include plain/TLS, binary snapshots, conflicting/null and error EXEC, MULTI
rejection, queue DISCARD, malformed/rejected/held cleanup, pending-start cancel,
idle/busy lifetime expiry, external close, execution uncertainty, controls,
busy/raw admission, storage/output aliases, cross-worker sync refusal, dropped
pending/completed observers, constructor quota windows, worker OOM at WATCH /
UNWATCH / EXEC, and eight-thread call/commit races.

An opt-in independent Redis matrix adds two authenticated connections, real
conflicting writes, binary GET/EXEC, idle abort/expiry and ACL-denied UNWATCH:

```sh
python extlibs/xredis/tests/interop/run_redis_tls_auth.py --watch --key rsa --build --report out/watch-redis.json
```

This requires a disposable local Redis with TLS and OpenSSL. The runner creates
and deletes ephemeral certificates, including distinct server/client CAs, and
pins exact source/header/helper/compiler/binary hashes. Native Windows clients
may use the existing WSL peer runner and independent Windows TLS health probe;
that is not native Linux acceptance. No server implementation source is used.
The existing source-receipt checks refuse stale clients and invalidate old
success before a new run.

Semantics follow the official [WATCH](https://redis.io/docs/latest/commands/watch/),
[UNWATCH](https://redis.io/docs/latest/commands/unwatch/) and
[transaction](https://redis.io/docs/latest/develop/using-commands/transactions/)
contracts. WATCH/read/decision is a separate optional API; repeated WATCH key
extension and pooling/lease integration are not supplied by this version.
Production release still requires the complete project contract, native
Windows/Linux acceptance, independent fault/recovery and sustained mixed load,
and stable matched performance baselines with unchanged noise gates.
