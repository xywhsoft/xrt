# Connection initialization

The client implements an explicit managed setup barrier on the existing TCP
request runtime. It follows the primary [RESP handshake specification](https://redis.io/docs/latest/develop/reference/protocol-spec/),
[HELLO](https://redis.io/docs/latest/commands/hello/),
[AUTH](https://redis.io/docs/latest/commands/auth/),
[CLIENT SETNAME](https://redis.io/docs/latest/commands/client-setname/) and
[SELECT](https://redis.io/docs/latest/commands/select/) command contracts.
The implementation is independent C using public XRT APIs. No Redis client or
server implementation is embedded.

## Options and state

Initialize `xredisclientinitconfig` with `xredisClientInitConfigInit`, then pass
it to `xredisClientConnectInit`. A NULL config selects strict RESP3, no optional
steps, a 5000 ms total initialization timeout and 64 KiB aggregate setup wire.
The factory admits asynchronous setup; a true return does not mean Redis has
authenticated the connection. `xredisClientReady` returns a retained shared
Future whose immutable `xredisreadyresult.Status` must be checked. A resolved
Future can carry failure. Cancelling this observer does not cancel the shared
connection; call Close to do that.

| Mode | Commands | Success |
| --- | --- | --- |
| RESP3 (default) | HELLO 3, optional combined AUTH/SETNAME, optional SELECT | Validated RESP3 HELLO map, all following steps acknowledged |
| AUTO (explicit) | Same, or legacy fallback only for unknown HELLO/NOPROTO | Validated RESP3, or new connection's default RESP2 plus required legacy steps |
| RESP2 (explicit) | Optional AUTH, CLIENT SETNAME and SELECT | All selected legacy steps acknowledged; HELLO not required |

Set `XREDIS_INIT_CLUSTER_READONLY` when connecting to a selected Redis Cluster
replica. It adds one 18-byte RESP command and one setup round trip. The command
runs after negotiation, authentication, naming, database selection and any
requested [ROLE verification](role_client.md). Readiness requires an exact
simple-string `+OK`. An explicit unknown-command error reports
`XREDIS_READY_UNSUPPORTED`; authentication errors keep their typed status
and other Redis errors report `XREDIS_READY_SERVER_ERROR`. A failure identifies
`XREDIS_INIT_READONLY` as the setup step. RESP2 fallback still
runs this required step. The usual setup deadline and wire caps include it.
Each replacement connection created by a pool factory repeats the setup.

Use database zero for Cluster. Redis may return stale data from a replica
and redirects keys outside the upstream primary's slot range. The flag records
connection mode. Add `XREDIS_INIT_REQUIRE_REPLICA` to verify the role during
setup. Replication lag, read authorization, and selection of the target
replica remain application concerns for an explicitly configured connection.
[Redis Cluster replica reads](https://redis.io/docs/latest/operate/oss_and_stack/reference/cluster-spec/#scaling-reads-using-replica-nodes)

An AUTH flag distinguishes an empty password from no authentication. USERNAME
requires AUTH; without USERNAME, HELLO uses `default` and legacy AUTH uses the
single-password form. SETNAME permits an explicitly empty name, and SELECT an
explicit database zero. Unselected nonempty fields, unknown flags, invalid
spans and output aliases are rejected before ownership transfer. The server
validates database and name semantics. RESP3/AUTO require a RESP3 reader.

Only explicit `NOPROTO` or a canonical unknown-command HELLO error permits AUTO
fallback. Authentication failures, syntax errors, malformed replies and other
server errors retire the connection. A fallback never replays a business
operation. Strict RESP3 reports unsupported peers without fallback.
WRONGPASS, NOAUTH and the legacy invalid-password reply identify authentication
rejection. Other AUTH command errors retain SERVER_ERROR rather than pretending
that a transient or unsupported-command failure proves invalid credentials.

HELLO success requires one map with unique `server`, `version` and integer
`proto = 3` fields of the specified types. Unknown fields and ordered attributes
are preserved. Legacy steps require exact simple-string OK acknowledgments.
On success, the Future owns HELLO metadata; it can outlive client and engine.
On any subsequent setup failure, no HELLO payload is published. Typed status,
failed step, selected protocol, negotiation and fallback remain available even
if diagnostic allocation fails. Authentication error reply text is discarded;
the retained Cause uses fixed redacted text rather than copying server errors.

The client remains CONNECTING through setup; OPEN means readiness succeeded.
Info.Ready records successful initialization even after a later close; use State
to distinguish an open connection from one that has retired.
Ordinary calls may queue, cancel or expire in the meantime. Their deadlines
start at API entry and are never restarted after initialization. None is sent
until every step succeeds. A setup rejection completes surviving requests as
INITIALIZATION/NOT_SENT. Native transport/parser/resource failures keep their
own typed categories. Managed connections reject later HELLO/AUTH/SELECT/CLIENT
SETNAME controls so their recorded setup state remains meaningful. Opting into
managed READONLY also reserves READONLY and READWRITE for the initializer. The original
raw Connect API permits manual controls and reports transport readiness with
UNKNOWN protocol and no negotiation.

One monotonic initialization deadline includes encoding, worker scheduling,
TCP establishment and every step. Zero expires before TCP starts, and -1 opts
out. Setup has an independent timer and bounded preencoded storage. Each
command must fit the stream write limit; the aggregate cap includes the AUTO
fallback commands, even when not used. Initialization failure or Close stops
admission before publishing readiness, so notification reentry is safe.

## Credentials, ownership and costs

Input username/password/name views are borrowed only during ConnectInit. The
client preencodes one bounded wire allocation rather than retaining pointers
or constructing commands on a network worker. Strict modes copy selected
credentials/name once; AUTO reserves both HELLO and legacy forms, copying them
twice. Binary inputs and empty fields use explicit lengths. SELECT formatting
uses the existing XRT integer writer; there is no new shared runtime.

The initialization owner and accepted TCP send references independently protect
that buffer. The final owner securely zeroes the entire encoded storage before
freeing it, including unused fallback commands and failure/close paths.
`Info.InitBytes = 0` is published only after that free. Closed also waits for
the timer and setup send-reference tail. Caller buffers, peer/OS/lower-transport
buffers and arbitrary response/application payloads are outside this wipe
contract. Raw AUTH/HELLO request storage is also wiped at its final release.

Every connection, including raw ones, now allocates an additional readiness
Future/promise and preallocated typed result. Managed setup adds its bounded
wire allocation, reference bookkeeping and (unless infinite) an engine timer.
HELLO validation walks map fields, reply ownership copies bytes under the
existing frame caps, and wiping adds a full cold-path storage pass. Normal
response dispatch checks the readiness barrier. Existing queue locks, atomic
references, Future synchronization, TCP metadata, socket copies and timers
remain real costs. No zero-overhead or production-performance claim is made.

## Runnable evidence

The [self-contained example](../examples/client_init/main.c) needs no external
service. It asserts AUTH/name/SELECT/READONLY ordering, a queued request, immediate
caller credential cleanup, physical close and metadata after engine teardown.

```sh
python tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_tests --test test_client_init --cflag=-O2
python tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_tests --single-test test_single_xredis_client_init --cflag=-O2
python tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis --no-single --cflag=-O2
```

The deterministic peer checks exact binary command bytes and ordering. Cases
include strict/default/AUTO/legacy modes, default/explicit ACL users, empty
password/name/database zero, map attributes/unknown fields, held replies and
pushes, queued deadline/cancel, observer cancellation, server/auth/shape errors,
no fallback on auth failure, transport close, total/worker-queue timeouts,
aggregate caps, invalid options/aliases, quota thresholds, worker/diagnostic
OOM, reentrant notification and result retention. It is a fixture, not Redis.

For real-server interoperability, build the independent static packages and
the dedicated external consumer, then run the disposable-service supervisor:

```sh
python tools/package.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis --kind static --verify
gcc -std=c11 -Wall -Wextra -Werror -O2 -pthread -Iinclude -Iextlibs/xredis/include extlibs/xredis/tests/interop/redis_init.c release/gnu/native/xredis/static/libxredis.a release/gnu/native/xredis/static/libxrt.a -lm -o out/service-extensions/redis-init-interop
python extlibs/xredis/tests/interop/run_redis_init.py --client out/service-extensions/redis-init-interop --report out/service-extensions/evidence/redis-init-interop.json
```

The supervisor requires an installed Redis binary and starts only new loopback
instances with persistence disabled. It tests unauthenticated, default-user
password and ACL configurations, terminates its own processes and removes
temporary data. The report pins executable/source hashes, server version and
platform. This check is opt-in because an external Redis executable is not a
library build dependency. Linux commands above use default native output paths;
Windows needs native Redis and the appropriate socket libraries/compiler.

The separate managed READONLY fixture starts three loopback Cluster primaries
and one replica. The native probe checks RESP2/RESP3 MOVED on connections
without READONLY, binary GET on managed READONLY connections and pipelined reads
through a maintained two-connection pool. The runner waits for replication
and for both SHARDS and SLOTS to advertise the replica. The probe also checks
managed topology selection and fallback with both topology reply formats and
protocol versions. The report captures server identity, binary hashes,
metadata replies and per-process logs.

```sh
python tools/package.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_cluster_client,xredis_role_client --kind static --verify
lib=release/gnu/native/xredis_cluster_client,xredis_role_client/static
mkdir -p out/service-extensions
gcc -std=c11 -Wall -Wextra -Werror -O2 -pthread -Iinclude -Iextlibs/xredis/include extlibs/xredis/tests/interop/redis_cluster_readonly.c "$lib/libxredis.a" "$lib/libxrt.a" -lm -o out/service-extensions/redis-cluster-readonly
python extlibs/xredis/tests/interop/run_redis_cluster_readonly.py --client out/service-extensions/redis-cluster-readonly --report out/service-extensions/evidence/redis-cluster-readonly.json
```

The cold setup workloads `xredis_client_init` and `xredis_client_init_readonly`
record ConnectInit-entry-to-ready-observation P50/P95/P99/P99.9 against the
same scripted peer. Both use HELLO AUTH and SELECT. The second adds READONLY.
The lifecycle rate also includes fixture/engine creation and destruction.
The default 256 samples per repeat are a local comparison. Larger runs on
pinned deployment hosts are needed for tail-latency acceptance.

```sh
python tools/measure_performance.py --compiler gcc --manifest extlibs/xredis/config/modules.json --config extlibs/xredis/config/performance_profiles.json --profiles xredis_client_init,xredis_client_init_readonly --warmups 1 --repeats 3 --report out/service-extensions/evidence/redis-readonly-cold.json
```

The ordinary client workload still measures a closed-loop pipeline. Pinned
open-loop baselines, native Linux/Windows long runs, larger peer/version
matrices, TLS, reconnect, pools, dedicated modes, HA and Tracking remain pending.

Current local evidence: Windows x64 GCC 16.2 O2 modular and all five leaf single
tests pass on IOCP. Ubuntu/WSL x64 GCC 15.2 modular and direct initialization
single tests pass with ASan+UBSan on epoll. The initialization single test also
passes O1 TSan with `-Wno-error=tsan`; GCC retains a warning that generic XRT
atomic_thread_fence is not instrumented. Both platforms run the root examples,
dependency trim probes and independent static/shared consumers. API inventory
is 34 functions, 110 constants and 34 types with none missing from registered
documentation; PE/ELF exports contain the same 34 public functions.

The real-server test passed all three configurations with Redis 8.0.5 under
WSL (jemalloc 5.3.0, build d47fc40702b1fad8, executable SHA-256
`0f3f5d696b7dabd9af484c2d8844dbb0c9bc7722ddf794f63b911a9795509293`).
It checks strict RESP3, explicit RESP2 and AUTO success, actual client name/db,
binary SET/GET, 32-request pipelines, bad name/database, rejected auth/no-auth,
manual raw HELLO and metadata after engine destruction. This one real Redis
version does not establish old-server fallback interoperability; the exact
legacy fallback paths currently have deterministic peer evidence. Native Linux,
native Windows real-server tests and broader version matrices remain required.
An initial supervisor run resolved a redis-server symlink to its argv[0]-selected
RDB checker. The runner now preserves the invocation name and records the
resolved binary separately; the corrected complete test passes.
