# Cluster topology lookup

Run from the repository root (select your compiler path as needed):

```powershell
python tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_cluster_topology --no-single
```

This offline example parses full SLOTS, releases reader/source ownership,
computes the slot of `orders:{42}`, and looks up its retained endpoint. It opens
no connection. See [contracts](../../docs/cluster_topology.md) for SHARDS, TLS,
health, fallback hosts and the current automatic-client scope.
