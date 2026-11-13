# Asynchronous framed TCP connection sources

The developing `xrpc_tcp_dial` leaf adds `xrpcTcpDialConfigInit`, `xrpcTcpDial`
and `xrpcTcpDialAddresses` to the existing bidirectional peer. It uses public Core
resolver/candidate APIs and the host Engine/shared business Server. The facade
does not create a Resolver, Engine, thread or second runtime.

DNS names are copied before return, bounded by default1024/hard65535 bytes and
reject embedded NUL. IDNA conversion is the caller's duty. A borrowed host
Resolver executes lookups outside network workers. Prepared immutable lists
retain one native reference before return, preserve each endpoint port and never
consult a Resolver. At most64 entries/attempts, family filtering,250ms fallback
and8 attempts are defaults. Selected endpoints require nonzero ports.

`Config.Stream.ConnectTimeout` is one absolute deadline from original factory
entry through construction, owner queueing, DNS, candidate racing and losing
resource retirement. Zero expires immediately and -1 disables that deadline.
Core Dial's separate relative deadline is disabled. Calls/notifications/batches
retain their own entry deadlines/cancellation while setup is pending. Expiring
an unsent call does not poison a healthy pending connection. No uncertain RPC is
replayed and no TLS policy is inferred.

Candidates have no peer event callbacks. Only the transferred winner receives
the permanent owned framing binding; already buffered input is driven explicitly.
Readiness is published under the facade gate with a concurrent Close recheck.
Timeout, native resource admission, memory and transport failures close with
typed peer/call statuses. `xrpctcpinfo` now unconditionally contains copied native
error kind/code/domain and truncation state, including after retirement. Its
public layout is independent of feature selection; rebuild consumers for the new
pre-release layout. It does not reproduce a native error's complete cause graph.

The cold source has one allocation for plan/name/list/controller, with actual
creator and owned callback references. A successful Core Done first transfers
the winner after candidate resource drain. Its physical owned Drop releases the
peer's source pointer and native controller owner; source metadata is not kept
for the whole open connection. This hook is cold setup only, without a hot IO
vtable. A failed native submission transfers no callback reference.

Close cancels pending setup, fails queued operations and preserves physical
source/candidate/callback duties until they retire. Core may retire a cancelled
dial's owned callback while a shared backend lookup still runs. Keep the borrowed
Resolver valid through peer retirement, and lookup code/data resident through
the Resolver's separate actual worker/thread join. Peer,
shared business Server, Engine and external callers have distinct joins before
code unload. Successful factory transfers exactly one peer owner; false changes
no output and consumes no caller alias. Existing peer admission/result ownership,
frame bounds and Close/Poll/Wait/Destroy contracts remain in force.

The [local example](../examples/tcp_dial/README.md) uses OS localhost resolution.
`bench/bench_tcp_dial.c`, built with `xrpc_tcp_dial,memory_stats`, takes `count`
`mode` `heap`: modes0/1/2 are numeric/prepared/DNS; e.g. `100 2 0`. It validates
each echoed byte, includes both peer physical waits/destroys and records all
process-thread CPU. Host construction/final joins are outside the timed loop.
Closed-loop P50/P95/P99/P99.9 have coordinated omission; tiny samples cannot
establish extreme tails. System DNS may cache despite the app cache being off.
No fixed-arrival/scale/native-Linux/72h or equal-contract speedup is established.
Framed TLS DNS/prepared setup, stdio/WebSocket, negotiation/credentials/recovery
and full four-product production gates remain required.
