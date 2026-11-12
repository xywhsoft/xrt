#ifndef XREDIS_CLUSTER_NODES_INTERNAL_H
#define XREDIS_CLUSTER_NODES_INTERNAL_H
#include <xredis/cluster_nodes.h>
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_CLIENT_SYNC)
/* Stable cache owner. Gate protects Engine pointer and its existing pin through
 * the Current query, including a concurrent physical close. No callback. */
bool __xredisClusterNodesCurrent(xredisclusternodes* Cache);
#endif
#endif
