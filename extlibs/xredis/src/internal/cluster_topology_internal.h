#ifndef XREDIS_INTERNAL_CLUSTER_TOPOLOGY_H
#define XREDIS_INTERNAL_CLUSTER_TOPOLOGY_H
#include <xredis/cluster_topology.h>
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_TOPOLOGY)
bool __xredisClusterTopologyOutput(const xredisclustertopology* Topology,const void* Output,size_t Bytes);
#endif
#endif
