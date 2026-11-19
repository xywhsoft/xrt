# Fetch a Cluster command catalogue and route a cached GET

Build and run with the extension manifest. The example starts an owned
loopback fixture, sends `COMMAND` on the managed primary route, and uses
the published snapshot to route a binary-key GET.

```sh
python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_cluster_command_cache_tests
```

See the [catalogue and lifetime contract](../../docs/cluster_command_cache.md).
