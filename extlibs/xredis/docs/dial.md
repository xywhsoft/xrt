# Optional DNS and endpoint setup

`XREDIS_MODULE_XREDIS_DIAL` adds three public functions on the existing client:
`xredisClientDialConfigInit`, `xredisClientDial` and
`xredisClientDialAddresses`. It reuses public XRT Resolver/Dial APIs and the
existing HELLO/AUTH/SELECT readiness barrier. There is no new event runtime,
transport plugin, command replay, TLS or reconnection policy in this module.
The whole xredis product remains developing.

## Inputs, limits and errors

The application supplies a running Engine and an application-owned Resolver.
Host is an explicitly sized, nonempty readable view without embedded NUL.
The factory validates it, copies it and returns asynchronously; source storage
can be recycled immediately. Numeric text can use this entry point too, though
`ConnectInit` remains the smallest numeric-address leaf. Host normalization and
resolution follow XRT; IDNA conversion is not supplied by xredis.

Defaults are unspecified family, 30 s additional Dial timeout, 250 ms candidate
fallback delay, eight selected attempts, and 1024 host bytes. The maximum host
span is 65535 bytes. Additional Dial timeout 0 disables its timer, -1 is
infinite. Managed initialization defaults to strict RESP3 and a 5 s overall
deadline. Its original API-entry timestamp includes host validation/copying,
worker queueing, DNS, candidate attempts and Redis setup. Increasing only the
Dial timeout does not extend the overall initialization deadline. Finite
deadlines trigger logical failure; physical resource retirement is separate.

Prepared immutable endpoint lists must contain 1..64 distinct entries, with
nonzero ports on candidates matching Family. Ports are preserved; candidates
outside Family are filtered. MaxAttempts is 1..64 and bounds selected attempts,
including failures. XRT supplies family interleaving, fallback timing and
losing-socket retirement. Resolver request/query/cache/address bounds remain
the Resolver's configuration; MaxAttempts is not a new DNS result cap. Create
address lists through XRT, which removes duplicate endpoints. The factory
retains a prepared list until native Dial admission, then releases that hold;
the caller can immediately destroy its own list reference.

Factory rejection preserves Result and consumes no caller reference. Output
storage must not overlap descriptors or borrowed host/credential/name/endpoint
storage. A successful factory returns one client reference. Asynchronous DNS
miss, candidate failure, native Dial deadline, initialization error and limits
produce typed readiness outcomes. Resolver range/again failures map to
`XREDIS_READY_LIMIT`, allocation failures to MEMORY, other resolution/connect
errors to DISCONNECTED. Native Dial deadline reports READY_TIMEOUT; pending
business requests receive DISCONNECTED with NOT_SENT, because their own
deadlines did not necessarily expire. All initialization diagnostic payloads
retain the existing redaction policy. Inspect Status; a resolved Future alone
does not mean a successful connection.

## Queues, cancellation and lifetime

Business admission uses the same aggregate count/byte caps and monotonic
deadlines as the numeric client. Requests may expire or be cancelled while DNS
is pending, with NOT_SENT. Live requests wait through the complete managed
barrier. Cancelling a readiness observer does not cancel the shared setup.
`Close` stops admission and cancels native Dial. It can run from a Future
notification; synchronous business calls still reject engine workers.

The borrowed Resolver must outlive the client's Closed observer. Closing a
client does not stop that shared service or interrupt an already executing OS
lookup. XRT separates the cancelled subscription/callback from the underlying
query: the client and its Engine can retire while that query still runs.
Destroy/join the application-owned Resolver separately, keeping its LookupData
alive until that join. Resolver destruction may wait for the system query;
the extension does not promise bounded interruption of OS DNS.

One worker owns candidate and client state. The client retains its activation
until both native Dial callbacks and any winning Stream physically retire.
A winning reference racing with Close is aborted and awaited, not discarded.
Cancelled setup without a Stream marks transport retirement in the terminal
Dial callback. Only after pending callback, Stream, timers, requests, setup
wire and internal posts retire does Closed publish. Join the Engine before
unloading code or freeing any separate external callback storage. A retained
typed ready result/Hello reply can survive client, Engine and Resolver teardown.

## Actual costs and compile boundary

This is an optional cold connection-start handoff, not a per-request transport
vtable. Selecting it adds a private plan pointer and three flags to the client,
and one allocated descriptor plus Host.Size+1 bytes. Prepared-list mode uses
the same descriptor with one unused trailing byte and a temporary list ref.
Host mode scans for NUL once and copies the supplied bytes once; XRT Dial and
Resolver also copy/normalize host data and allocate native operations, candidate
state and optional timers. Defaults additionally allocate the existing ready
Future/promise/result, parser and HELLO wire. Native callbacks add reference
operations/posts; final cleanup releases/wipes initialization storage as before.
These are real cold-path costs, not a zero-overhead claim.

The numeric `xredis_client` leaf compiles out plan fields and callbacks, does
not depend on `net_tcp_dial`/Resolver, and retains its existing direct TCP
request/send/read paths. Its size-symbol boundary forbids `xrtNetDial`; the new
Dial leaf independently forbids TLS/HTTP/WebSocket/task executors. The product
root selects both. Public structure layout does not depend on feature selection.
Per-operation copies, locks, syscalls and allocator/resource instrumentation
still need matched final acceptance measurements.

## Reproduction

The [self-contained example](../examples/dial/main.c) demonstrates host snapshot,
prepared-address failure/fallback, a queued binary ECHO, physical Close, separate
Resolver join and retained metadata. Its custom lookup and TCP peer are test
fixtures, not a deployed Redis or OS resolver.

```sh
python tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_tests --test test_dial --cflag=-O2
python tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_tests --single-test test_single_xredis_dial --cflag=-O2
python tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis --no-single --cflag=-O2
python tools/measure_size.py --compiler gcc --config extlibs/xredis/config/size_profiles.json --profiles xredis_client,xredis_dial --kind single --report out/service-extensions/dial-size.json
python tools/measure_performance.py --config extlibs/xredis/config/performance_profiles.json --profiles xredis_dial_numeric,xredis_dial_host,xredis_dial_addresses,xredis_dial_host_stats --smoke --repeats 1 --report out/service-extensions/dial-cold.json
```

The four cold profiles run serial setup against a deterministic lookup and
HELLO peer. Numeric control is compiled in the same Dial-enabled binary; it is
not the stripped numeric leaf. Prepared lists contain one good endpoint to
measure setup without a platform-specific refusal timeout. Failed candidate
fallback is a correctness test. Factory-to-ready-observation latency includes
main-thread wakeup. Lifecycle rate also includes fixture Engine/Resolver create,
close and join. P50/P95/P99/P99.9 use nearest rank; 16-sample smoke cannot establish
tail latency. The instrumented host profile reports whole-process/window XRT
allocation counters, including fixture and teardown, not isolated client costs.
Normal profiles explicitly identify disabled/uncompiled counters. Helper bytes
are declared inputs to the benchmark fingerprint. None measures OS DNS latency,
steady-state throughput, Hiredis parity or production tail acceptance.

For real Redis and system Resolver interoperability, use the existing disposable
supervisor with the hostname-enabled consumer. Linux commands:

```sh
python tools/package.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis --kind static --verify
gcc -std=c11 -Wall -Wextra -Werror -O2 -DINTEROP_DIAL -pthread -Iinclude -Iextlibs/xredis/include extlibs/xredis/tests/interop/redis_init.c release/gnu/native/xredis/static/libxredis.a release/gnu/native/xredis/static/libxrt.a -lm -o out/service-extensions/redis-dial-interop
python extlibs/xredis/tests/interop/run_redis_init.py --client out/service-extensions/redis-dial-interop --report out/service-extensions/evidence/redis-dial-interop.json
```

This executes the existing no-auth/password/ACL matrix through `localhost` and
one application-owned system Resolver, retaining numeric manual-HELLO control.
It never connects to an existing application service. Authentication/protocol,
name/database, binary values, pipeline, failure and teardown assertions remain
the same. Windows needs a native Redis installation and platform socket link
flags. WSL evidence does not substitute for native Linux/Windows acceptance.
