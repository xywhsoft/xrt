#ifndef XREDIS_EXAMPLE_CLUSTER_REPLICA_POLICY_H
#define XREDIS_EXAMPLE_CLUSTER_REPLICA_POLICY_H

#include <xredis/cluster_client.h>
#include <xredis/pool_shared.h>

/* This APP chooses the first online replica for a caller-classified read.
 * The Nodes factory still applies endpoint and credential policy. */
static xredisclusterpoolstatus appReplicaPool(xredisclusterclient* Client, xredisclusternodes* Nodes,
    uint16 Slot, int64 Timeout, xredispool** Pool, uint16* Port)
{
    xredisclustermap* Map = NULL;
    bool Stale;
    if (xredisClusterClientSnapshot(Client, &Map, &Stale) != XREDIS_CLUSTER_VALUE)
        return XREDIS_CLUSTER_POOL_WAIT;

    xredisclustermapinfo MapInfo = {.Size = sizeof(MapInfo)};
    xredisclustertopologyinfo Topology = {.Size = sizeof(Topology)};
    xredisclusternode Primary;
    xredisclusterpoolstatus Status = XREDIS_CLUSTER_POOL_DENIED;
    if (xredisClusterMapInfo(Map, &MapInfo) && xredisClusterTopologyInfo(MapInfo.Topology, &Topology) &&
        xredisClusterTopologyLookup(MapInfo.Topology, Slot, &Primary) == XREDIS_CLUSTER_VALUE) {
        for (size_t Index = 0; Index < Topology.Nodes; ++Index) {
            xredisclusternode Node;
            if (!xredisClusterTopologyNode(MapInfo.Topology, Index, &Node)) break;
            if (Node.Shard != Primary.Shard || Node.Role != XREDIS_CLUSTER_REPLICA ||
                (Node.Health != XREDIS_CLUSTER_ONLINE && Node.Health != XREDIS_CLUSTER_UNREPORTED) ||
                Node.Endpoint == XREDIS_CLUSTER_ENDPOINT_UNKNOWN) continue;
            xredisclusteraddress Endpoint = {
                .Host = Node.Endpoint == XREDIS_CLUSTER_ENDPOINT_SOURCE ? MapInfo.Origin.Host : Node.Host,
                .Port = MapInfo.Origin.Tls && Topology.Format != XREDIS_CLUSTER_LEGACY_SLOTS ?
                    Node.TlsPort : Node.Port,
                .Tls = MapInfo.Origin.Tls, .Slot = Slot, .Reason = XREDIS_CLUSTER_REPLICA_READ
            };
            if (!Endpoint.Port || !Endpoint.Host.Size) continue;
            Status = xredisClusterNodesGetPool(&Endpoint, Timeout, Nodes, Pool);
            if (Status == XREDIS_CLUSTER_POOL_READY) *Port = Endpoint.Port;
            break;
        }
    }
    xredisClusterMapRelease(Map);
    return Status;
}
#endif
