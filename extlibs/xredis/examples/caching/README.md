# Adjacent CLIENT CACHING and query

From the repository root:

```text
python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_tracking --exclude-test "*" --no-single
```

This local protocol fixture performs managed RESP3 setup, enables observations
and OPTIN, and submits one binary GET with `xredisClientCachingCall`. It checks
both statuses and reads the retained result after closing Client and Engine.
It requires no Redis installation and is not an interoperability result.

On an actual server, configure OPTIN and use `Caching=true`, or configure OPTOUT
and use `Caching=false`. The asynchronous form supports shared producers. Both
commands have one queue position, deadline and Future. A prefix error does not
prevent execution of its already-pipelined query. Do not cache the query unless
both statuses succeed, and compare its received sequence with invalidations.
The library does not implement an application cache or replay uncertain calls.

See [the full contract](../../docs/caching.md), including limits and cancellation.
