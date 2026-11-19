#ifndef XREDIS_INTERNAL_CLUSTER_REPLY_H
#define XREDIS_INTERNAL_CLUSTER_REPLY_H
#include <xredis/reply.h>
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_TOPOLOGY)
bool __xredisClusterReplyOutput(const xredisreply* Reply,const void* Output,size_t Bytes);
#endif
#endif
