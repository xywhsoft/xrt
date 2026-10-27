# XRT service extensions development contract

This is the execution plan for xredis, xrpc, xmqtt and xcron. The human requested
independent pure-C implementations, production reliability, high performance,
Windows/Linux server acceptance, and automatic commits for each verified small
feature. This file preserves the agreed target; completing a first codec does
not complete a product.

## Source and commit policy

- Start from `1d4109aff9d52d0452742669e81caa8bef626922` in the managed
  `service-extensions` worktree, branch `codex/service-extensions`.
- Preserve the dirty primary checkout and its staged rename. Do not stash,
  reset, commit, or copy its unrelated work into this branch.
- Implement public specifications independently. Reference clients and servers
  are interoperability/performance comparators, not implementation sources.
- Each locally verified feature is a separate commit. Stage explicit task-owned
  paths, inspect the staged diff, and include the validation in the commit body.
- Changes to general XRT capabilities require a reproduced concrete gap. Keep
  fixes independent of protocol features and do not build a new shared runtime.
- Product manifests contain actual assets and actual current capabilities;
  unfinished products remain `developing`. Never register empty feature stubs as
  implemented. Generated outputs are committed with their actual inputs.

## Final product scope

| Product | Required final scope |
| --- | --- |
| xredis | RESP2/3, binary commands, bounded incremental replies, async/sync calls, pipelines, pools, TLS, transactions, blocking commands, Pub/Sub, Sentinel, Cluster, tracking |
| xrpc | JSON-RPC 2.0 client/server, lossless IDs, notifications, batches, bounded dispatch, bidirectional calls, HTTP, WebSocket and explicitly framed stdio, negotiated deadline/cancellation extensions |
| xmqtt | MQTT 5.0 primary and 3.1.1 compatibility, full client packet/property rules, QoS 0/1/2, authentication, sessions, expiry, aliases, subscriptions, reconnect, durable state, TCP/TLS/WebSocket |
| xcron | Explicit Unix/seconds dialects, optional separate Quartz dialect, compiled expressions, IANA time zones, DST rules, clock jumps, misfires, overlap policies, executors and durable recovery |

Redis server, MQTT broker, arbitrary business exactly-once effects and distributed
scheduler coordination are outside these four products. Protocol completeness
and resource correctness are not weakened to obtain a benchmark result.

## Execution contracts

- One worker owns each mutable connection. Cross-thread submissions are bounded;
  callbacks and ownership transfer document the actual execution thread.
- Use XRT public APIs only. Blocking business work and storage synchronization do
  not run inside network callbacks. Sync facades reject self-worker deadlocks.
- Queueing, connection setup, retries and processing share one monotonic deadline.
- Borrowed data has an explicit scope; asynchronous input is copied or held by an
  explicit ownership contract. Success-only transfer and failure ownership are
  documented. Close is observable and physical duties survive local cancellation.
- Limits cover local AND aggregate bytes, requests, frames, nesting, retries,
  subscriptions, offline queues and storage. Overload produces a counted result.
- Redis/RPC disconnection after send may mean unknown remote outcome; do not
  silently replay potentially side-effecting operations. Redis FIFO obligations
  survive a caller timeout until drained or the connection is retired.
- MQTT persistence orders durable state and protocol acknowledgements explicitly.
  Volatile and durable modes have separate guarantees and benchmarks.
- Cron wall-clock calculation is separate from monotonic waiting. Missing/repeated
  local times, overlap, missed runs and restart delivery are explicit policies.

## Modules and delivery

Each product owns `include/`, `src/`, `config/modules.json`, `tests/`, `examples/`,
`bench/` and `docs/`. All products use the root tools and independent package
ownership. One host supplies one XRT. Compile-time selection cannot alter public
structure layout. Tests/diagnostics are outside production dependency roots.

Planned module families (not declarations of implemented modules):

- xredis: command, reader, client, pool, pubsub, sentinel, cluster, tracking.
- xrpc: codec, client, server, dispatch, http, websocket, stdio.
- xmqtt: codec311, codec5, client, session, store, websocket.
- xcron: expression, calendar, timezone, scheduler, store, engine adapter.

## Ordered work packages

1. P0: freeze scope, source identity, protocol matrices, lifecycle contracts and
   reproducible performance workloads; inventory public-API gaps.
2. P1: binary Redis command encoding, then bounded incremental RESP parsing;
   independent JSON-RPC/MQTT codecs and Cron expression/calendar calculation.
3. P2: complete the xredis request/response, deadlines, backpressure, reconnect
   and close vertical slice; measure all overhead before extending its model.
4. P3: xrpc transports/dispatch, xmqtt sessions/persistence and xcron time zone/
   scheduler/recovery; finish Redis HA/tracking and the final feature matrices.
5. P4: interoperability, faults/OOM/lifetimes, performance acceptance, native
   Windows/Linux long runs and final package consumers for each product.

## Acceptance evidence

- A feature row records specification, implementation, positive/negative tests,
  fault cases, examples, performance workload and exact source/tool identity.
- Fragmentation/coalescing, overflow/limits, every observed allocation failure,
  timeout/cancel/close races, callback reentry and repeated lifecycle are tested.
- Separate protocol errors, server errors, local timeout, cancellation, overload
  and uncertain remote outcome. No stale ambient error supplies another task's
  result. Security material is runtime-only and is not recorded in evidence.
- Linux ASan/UBSan and applicable TSan, Windows handle/resource checks, independent
  protocol peers and crash-recovery tests remain required even after smoke passes.
- Final candidates require native Windows and Linux mixed-load runs of at least
  72 continuous hours, plus separately recorded failure-injection runs. Process
  kill is not represented as proof of power-loss durability.
- Product destruction returns resources to a documented baseline: live operations
  and handles are zero; retained core caches have explicit ownership/accounting.

## Performance contract

Measure codec, runtime and end-to-end paths separately. Record throughput,
P50/P95/P99/P99.9, CPU, memory, allocation/copy/synchronization/syscall counts,
failure/admission counts and recovery times. Include queue wait in end-to-end
latency; fixed-arrival-rate tests cannot silently omit delayed requests.

Profiles vary workers 1/2/4/8 and connections 1/100/1,000/10,000, payload size,
pipeline/batch depth, slow consumers and reconnect. Match TLS, QoS, durability,
validation, thread/CPU budgets and server configuration across comparators.
Redis compares against Hiredis; MQTT against Paho C/libmosquitto. RPC and Cron
also retain explicit minimal-XRT/algorithm baselines. References are version-pinned.

Steady-state codecs target zero heap allocations with caller-owned storage;
Future/result/retained-message, cold-path and topology costs remain counted.
Batching has count, byte and maximum-wait limits; latency mode does not wait just
to fill a batch. Never claim end-to-end zero-copy across TLS or retained views.

Freeze absolute objectives only after P0 hardware measurements. The planned
same-environment regression limits are 5% throughput loss and 10% P99 increase,
after demonstrating noise below the decision threshold. An unpinned smoke report
is build/workload evidence, not a release-performance acceptance.

## Initial baseline limitations

At the start revision, the core maturity checker fails because `net_file_future`
lacks single-test closure coverage (377 implementation nodes, 376 covered).
Core features are current. This is a pre-existing baseline issue, not a failure
introduced by the four extensions. Source-only local work cannot certify Linux,
native production throughput, protocol peer compatibility or long-run acceptance.

Live status, completed small features and commands are in
[service-extensions-progress.md](service-extensions-progress.md).
