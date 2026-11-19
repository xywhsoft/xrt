# Asynchronous Cluster client

This runnable loopback fixture creates maintained Nodes, creates the client with
an explicit seed and submits a binary ECHO before bootstrap. After observing the
primary route, the APP reads the topology snapshot, selects an advertised
replica, and sends a direct native request through its READONLY pool. It closes the client, shared
Nodes and engines in that order. The fixture uses ECHO as a synthetic command.
Applications classify stale-tolerant keyed reads and supply their authorized
Nodes factory and runtime endpoints. [Contracts](../../docs/cluster_client.md)

    python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_cluster_client
