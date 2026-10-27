# xredis (developing)

Independent C Redis client extension on public XRT APIs. The final scope is
RESP2/3, bounded incremental replies, asynchronous requests, pipelines/pools,
TLS, transactions/blocking/PubSub, Sentinel, Cluster and Tracking.
**Only binary command encoding is currently implemented. No network client,
reply decoder or production/performance acceptance is claimed.**

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
