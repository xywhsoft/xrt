# xredis (developing)

Independent C Redis client extension on public XRT APIs. The final scope is
RESP2/3, bounded incremental replies, asynchronous requests, pipelines/pools,
TLS, transactions/blocking/PubSub, Sentinel, Cluster and Tracking.
**The command encoder, incremental RESP2/3 reader, owned reply builder and one
raw TCP request/response connection are implemented. Advanced transports/modes,
HA and production/performance acceptance remain pending.**

## Binary command encoding

Select `XREDIS_MODULE_XREDIS_COMMAND` before including `<xredis.h>`.
`xredisCommandSize` measures the exact RESP array-of-bulk-strings wire size.
`xredisCommandWrite` writes it to caller storage without a trailing NUL.
Empty arguments and embedded NUL/CRLF bytes are supported. Inputs are borrowed
only for the call; output must not overlap them. All validation precedes output
mutation. Failures preserve output parameters and report `xrt.redis` errors.
If diagnostic allocation itself fails, XRT's allocation-free `XERR_MEMORY`
fallback is retained; output atomicity still holds.
Successful calls preserve the ambient XRT error and perform no allocation.
The contiguous writer copies payload bytes once; a later vectored path will be
evaluated separately, not advertised as already present.

## Incremental response reader

Select `XREDIS_MODULE_XREDIS_READER`. Create with a copied explicit limit config,
feed arbitrary fragments, and consume BEGIN/DATA/END events. All RESP2/3 types,
attributes, pushes and the RESP3 streamed string/array/map/set forms are parsed.
DATA borrows the current input only during the callback; retain bytes explicitly
when needed. One allocation at creation holds the reader and bounded depth stack;
successful parsing does not allocate or copy payload bytes. It does validate
numeric text; the owned builder supplies lazy double conversion.

`Feed` stops at one complete semantic frame, a callback pause or its byte work
budget. Resume with the unconsumed suffix, including an empty suffix after an
END pause. Attributes augment the following value without consuming array/map
items. Do not publish partial events as a successful reply until READY. Limits,
malformed wire, callback rejection and truncated EOF are terminal until Reset.
Call `Finish` at EOF; it detects orphan attributes and incomplete frames.
Readers are thread confined; mutating callback reentry is rejected. See the
[reader contract and evidence](docs/reader.md) and [fragmented-input example](examples/reader/main.c).

## Owned replies

Select `XREDIS_MODULE_XREDIS_REPLY` for immutable owned frames. Feed transfers one
reply reference only after the whole semantic frame passes validation. Source
buffers can be recycled immediately; replies survive reader Reset/Destroy.
Index-based nodes preserve all types, binary payloads, duplicate keys, streamed
flags and ordered root/nested attributes. Completed replies support concurrent
read access and atomic Retain/Release after a synchronized handoff.

Small scalars use one allocation with inline storage. Larger frames grow node
and byte vectors under a per-frame **peak** byte cap, including temporary growth
copies. No per-node allocation or final flattening is required. Failure releases
partial storage and preserves a typed cause even if diagnostic allocation fails.
`ReplyDouble` converts raw numeric text lazily through public XRT number APIs.
See the [ownership, limits and actual costs](docs/reply.md) and
[buffer-recycling example](examples/reply/main.c).

## TCP requests

Select `XREDIS_MODULE_XREDIS_CLIENT` to connect to a numeric endpoint on an
existing XRT engine. Async admission copies/encodes arguments, bounds count and
bytes, and includes setup/queue wait in one monotonic deadline. Ordinary replies
follow the sent FIFO; RESP3 pushes use a separate bounded ownership ring. A sent
request's local timeout/cancel retains a physical FIFO slot until drained or the
connection is retired, with explicit execution uncertainty and no silent replay.

Accepted Futures always resolve with a typed `xredisrequestresult`; inspect
Status/Delivery, not only generic Future state. Sync calls reject the engine's
workers. Close has a separate physical-ownership observer; retained response
results survive engine destruction. See the [client contract and real costs](docs/client.md)
and [local TCP example with cleanup](examples/client/main.c).

```sh
python tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_tests
python tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis --trim-only
python tools/package.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis --kind static --verify
python tools/package.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis --kind shared --verify
python tools/check_release_maturity.py --manifest extlibs/xredis/config/modules.json
python tools/amalgamate.py --manifest extlibs/xredis/config/modules.json --check
python tools/measure_performance.py --config extlibs/xredis/config/performance_profiles.json --smoke
```

Normal maturity audits may pass for the currently registered assets; `--release`
must fail while the overall product is developing. Performance smoke verifies
the workload, not parity with another client or production throughput.

See [the offline example](examples/command/main.c),
[public reference](docs/api/reference.md), and
[command evidence](docs/coverage.md). The final target is recorded in
[the full development contract](../../docs/development/service-extensions-plan.md).
The public command frame follows the
[Redis protocol specification](https://redis.io/docs/latest/develop/reference/protocol-spec/).
