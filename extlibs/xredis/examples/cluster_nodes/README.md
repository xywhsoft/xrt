# Maintained Cluster node cache example

Run from the repository root:

```
python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_cluster_nodes --exclude-test "*" --no-single
```

The self-contained local TCP peers implement managed readiness and binary ECHO.
Authorize checks every lookup; Connect supplies a managed private client for the
exact copied address/profile. Replace those two fixture callbacks with service
policy and numeric/DNS/TLS/auth factories. Route.GetPool can be assigned directly
to xredisClusterNodesGetPool with Route.Data=Cache; each concurrent route caller
holds a cache ref. Cache creation starts no I/O; the first lookup lazily starts a
maintained shared node pool. READY means the pool exists; native overload waits
remain subject to each route's original deadline. Close then await Closed before
releasing external factory material/resolver/signer resources. See
[contracts](../../docs/cluster_nodes.md).
