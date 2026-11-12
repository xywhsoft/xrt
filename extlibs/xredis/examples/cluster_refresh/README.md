# Coalesced Cluster refresh example

Run from the repository root:

```
python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_cluster_refresh --exclude-test "*" --no-single
```

The local TCP fixture supplies managed readiness, SHARDS and binary ECHO.
Create a node cache with service-specific Authorize/Connect callbacks, then
provide explicit authorized seeds to RefreshCreate. Start returns a coalesced
Future without I/O. One serialized driver calls Step and observes Pending or
NextWake. A retained Map supplies Topology, copied Origin.Host and Origin.Tls
for RouteCreate; SOURCE requires that actual origin. Invalidate on new topology
demand; a racing refresh stays stale. Fresh !Force Start reuses the completed
cohort. A joining caller can use its own WaitFor budget without cancelling the
shared cohort. Close the coordinator, then close the separately owned node
cache and join its engine. See [contracts](../../docs/cluster_refresh.md).
