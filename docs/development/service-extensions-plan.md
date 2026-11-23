# XRT service extensions development contract

This is the execution plan for xredis, xrpc, xmqtt and xcron. The human requested
independent pure-C implementations, production reliability, high performance,
Windows/Linux server acceptance, and automatic commits for each verified small
feature. This file preserves the agreed target; completing a first codec does
not complete a product.

## Source and commit policy

- Historical origin: `1d4109aff9d52d0452742669e81caa8bef626922` in the managed
  service-extensions worktree. Its committed history is integrated into master.
- Current user instruction (2026-11-08): work directly in `D:/GIT/xrt` on master
  by default; use a branch/worktree only for a concrete special necessity.
  Preserve concurrent edits/index entries and stage only this task's paths.
- During migration, retain source identities and ignored validation evidence,
  preserve both histories and verify against actual mainline Core/tools.
- Implement public specifications independently. Reference clients and servers
  are interoperability/performance comparators, not implementation sources.
- Each locally verified feature is a separate commit. Stage explicit task-owned
  paths, inspect the staged diff, and include the validation in the commit body.
- Changes to general XRT capabilities require a reproduced concrete gap. Keep
  fixes independent of protocol features and do not build a new shared runtime.
- Product manifests contain actual assets and actual current capabilities;
  unfinished products remain `developing`. Never register empty feature stubs as
  implemented. Generated outputs are committed with their actual inputs.

## Current product boundaries

The approved November 19 scope puts business execution, deployment policy and
application data in host applications. The libraries own protocol behavior,
bounded resources and the lifetime of their own state.

| Product | Library scope | APP examples |
| --- | --- | --- |
| xcron | Unix five-field and seconds-first six-field expressions, UTC/civil recurrence, optional host-driven UTC callback scheduler | Poll and blocking wait loops, executor submission, task configuration and restart, local time and catch-up policies |
| xrpc | Host-driven JSON-RPC 2.0 messages, dispatch, request tokens, response correlation, notification/batch behavior and local timeouts | Transport/framing, authentication, subprocesses, host execution and optional private wire policies |
| xmqtt client | MQTT 5.0 and optional 3.1.1, TCP/TLS and thin WebSocket transport, QoS/session recovery, bounded reconnect, externally supplied subscriptions and optional protocol state persistence | Storage, business Inbox/queues, application acknowledgement, log rotation and compaction |
| xmqtt server | Selectable single-node broker for MQTT 5.0 and optional 3.1.1, native accepted connections, QoS 0/1/2, subscription matching and fan-out, retained messages, Wills, session takeover and expiration, bounded offline delivery and optional protocol state persistence | Credentials and authorization policy, persistence backend, configuration, deployment, cluster/bridging and business integrations |
| xredis client | RESP2/3, generic calls and pipelines, TLS, native pools and leases, transactions, Pub/Sub, tracking notifications, base Sentinel and explicit-route Cluster | Cache policy, replica selection, dynamic Sentinel discovery and automatic command catalogues |

Migrate existing APP use cases to runnable examples using public XRT and
extension APIs. Examples may combine reloading configuration, timers and
executors. Remove the old library entry points after validating their intended
uses against the reduced contract.

The updated scope adds the MQTT server as an independently selectable runtime.
A Redis connection, including Sentinel and Cluster control traffic, always uses
the Redis client role.

### MQTT server delivery

The server owns MQTT connection and session semantics. The application provides
listeners and TLS credentials through XRT, admission and topic authorization
callbacks, and an optional protocol-state backend. The library supplies in-memory
state with bounded connection, packet, subscription, retained, offline and
in-flight resources. Expose exhaustion distinctly and preserve QoS obligations
until completion or a documented session termination. In particular, negotiate
MQTT 5 Receive Maximum and Maximum Packet Size, and expose a bounded admission
budget for each authenticated principal or session. Application callbacks never
block network workers. Application policy, account databases and file or database
engines stay in examples.

Start with CONNECT version selection and a socket-backed handshake/retirement
path. Then add one-session QoS flows and exact subscription delivery, followed
by wildcard and shared subscriptions, MQTT 5 subscription options and retained
messages. Add persistent subscriptions, replay, duplicate Client ID takeover,
Wills and expiry with independently tested timers and storage callbacks. Reuse
the wire codecs and role-neutral QoS transitions where their contracts match.
TCP/TLS and WebSocket front doors share one MQTT session implementation. Complete
independent protocol interoperability, hostile-input and overload tests before
load acceptance. The normative baselines are [MQTT 5.0](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html)
and [MQTT 3.1.1](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html).

A connection stays on one XRT worker. Measure connection-local batching and
immutable retained publish data before adding cross-worker scheduling. Route
by indexed filter matching and avoid scanning all connections or serializing
every publish through a global lock. Preserve per-session ordering and impose
explicit cross-worker queue and fan-out budgets. Report the cost of copies,
allocations and dispatches per delivered message separately from ingress rate.

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
- xrpc: codec, dispatch, request correlation, host-driven endpoint.
- xmqtt: codec311, codec5, client, broker, protocol session and recovery, selectable transports.
- xcron: expression, calendar and optional scheduler.

## Ordered work packages

1. Reduce xcron to expression/calendar and the optional callback scheduler.
   Validate both drive modes and the APP examples before retiring the old service.
2. Complete host-driven asynchronous replies in xrpc, then remove the embedded
   transport and execution runtimes. Validate the protocol endpoint without them.
3. Consolidate xmqtt connection configuration and QoS paths. Separate APP storage
   and compose protocol recovery with host-supplied subscription intent. Develop
   the independent broker in handshake, routing/QoS, recovery and load stages.
4. Simplify xredis paired commands and Sentinel discovery. Exercise explicit
   Cluster routing and retire automatic command catalogue management from the library.

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
- Final Redis client and MQTT client and broker candidates require native Windows and Linux mixed-load runs of at least
  72 continuous hours, plus separately recorded failure-injection runs. Process
  kill is not represented as proof of power-loss durability.
- Mixed-load journals record uptime, awake time, observer time and UTC.
  Each new run must cover the requested duration while awake, stay within
  the observation gap, and reject changes in the suspend counter beyond
  the measurement tolerance. The verifier replays both elapsed clocks.
  UTC offsets remain available for investigating host time corrections.
  The offline checker accepts legacy journals for diagnostic replay.
  Native 72-hour qualification requires both uptime and awake-time evidence.
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
Redis compares against Hiredis. MQTT client compares against Paho C or
libmosquitto. Compare broker workloads to a pinned standalone broker such as
Mosquitto with the same capabilities and queue limits. Include idle connection
memory, connection churn, one-to-one routing and fan-out, exact versus wildcard
and shared filters, retained and offline buildup, slow consumers, reconnection
storms, and saturation/recovery. Report ingress and actual deliveries per
second, end-to-end latency and overload outcomes for each case. RPC and Cron
retain explicit minimal-XRT/algorithm baselines. References are version-pinned.

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
