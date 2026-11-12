#ifndef XREDIS_H
#define XREDIS_H

#include <xredis/features.h>

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_COMMAND)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_COMMAND"
#endif

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_READER)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_READER"
#endif

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_REPLY)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_REPLY"
#endif

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_CLIENT)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_CLIENT"
#endif

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_DIAL)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_DIAL"
#endif

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_TLS)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_TLS"
#endif

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_TLS_AUTH)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_TLS_AUTH"
#endif

#include <xredis/command.h>
#include <xredis/reader.h>
#include <xredis/reply.h>
#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_ROLE)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_ROLE"
#endif
#include <xredis/role.h>
#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_ROLE_CLIENT)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_ROLE_CLIENT"
#endif
#include <xredis/role_client.h>
#include <xredis/client.h>
#include <xredis/dial.h>
#include <xredis/tls.h>
#include <xredis/tls_auth.h>
#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_TRANSACTION"
#endif
#include <xredis/transaction.h>

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_WATCH)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_WATCH"
#endif
#include <xredis/watch.h>

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_POOL)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_POOL"
#endif
#include <xredis/pool.h>
#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_POOL_SHARED)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_POOL_SHARED"
#endif
#include <xredis/pool_shared.h>
#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_CONNECT)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_POOL_SHARED_CONNECT"
#endif
#include <xredis/pool_shared_connect.h>



#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_POOL_TRANSACTION)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_POOL_TRANSACTION"
#endif
#include <xredis/pool_transaction.h>
#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_POOL_WATCH"
#endif
#include <xredis/pool_watch.h>
#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_POOL_CONNECT"
#endif
#include <xredis/pool_connect.h>

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_TRACKING)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_TRACKING"
#endif
#include <xredis/tracking.h>

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_PUBSUB)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_PUBSUB"
#endif
#include <xredis/pubsub.h>

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_SUBSCRIBER"
#endif
#include <xredis/subscriber.h>

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY"
#endif
#include <xredis/subscriber_notify.h>

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS"
#endif
#include <xredis/subscriptions.h>

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION"
#endif
#include <xredis/subscription_session.h>

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_SENTINEL)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_SENTINEL"
#endif
#include <xredis/sentinel.h>

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_SENTINEL_POOL)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_SENTINEL_POOL"
#endif
#include <xredis/sentinel_pool.h>

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_CLUSTER_TOPOLOGY)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_CLUSTER_TOPOLOGY"
#endif
#include <xredis/cluster_topology.h>
#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_CLUSTER_ROUTE)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_CLUSTER_ROUTE"
#endif
#include <xredis/cluster_route.h>


#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_POOL_ASKING"
#endif
#include <xredis/pool_asking.h>
#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_ASKING)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_POOL_SHARED_ASKING"
#endif
#include <xredis/pool_shared_asking.h>


#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_CLUSTER_NODES)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_CLUSTER_NODES"
#endif
#include <xredis/cluster_nodes.h>

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_CLUSTER_REFRESH)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_CLUSTER_REFRESH"
#endif
#include <xredis/cluster_refresh.h>

#endif
