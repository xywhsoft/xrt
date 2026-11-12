# Asynchronous Cluster client

This runnable loopback fixture creates maintained Nodes, creates the client with
an explicit seed, submits a binary ECHO before bootstrap, waits on its typed
Future and closes client, shared Nodes and engines in that order. No Step loop
is required. `cfCreate` is test support; applications supply their own authorized
Nodes factory and runtime endpoint configuration. See
[contracts](../../docs/cluster_client.md).

    python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_cluster_client
