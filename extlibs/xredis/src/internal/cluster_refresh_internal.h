#ifndef XREDIS_CLUSTER_REFRESH_INTERNAL_H
#define XREDIS_CLUSTER_REFRESH_INTERNAL_H
#include <xredis/cluster_refresh.h>
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_REFRESH)
bool __xredisClusterMapOutput(const xredisclustermap* Map,const void* Output,size_t Bytes);
#endif
#endif
