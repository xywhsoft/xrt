# Leased ASKING and query

From the repository root, select the compiler installed on your machine:

```powershell
python tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_pool_asking --no-single
```

Runs a local deterministic peer, obtains a native pool lease, submits one
ASKING/binary query group, checks both outcomes, physically returns the lease
and proves the retained query reply survives engine destruction. No real Redis
or production credentials are required. See [contracts](../../docs/pool_asking.md).
