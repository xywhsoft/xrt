/*
 * MIT License
 *
 * Copyright (c) 2025 xLeaves [xywhsoft] <xywhsoft@qq.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* 此文件由 tools/amalgamate.py 生成，请勿直接修改。 */
/* Supply XRT and selected extension dependencies before this header. */
#if !defined(XRT_CORE_H)
#error "xredis requires XRT; include xrt.h or xrt_decl.h first"
#endif
#if defined(XREDIS_IMPLEMENTATION) && defined(_MSC_VER) && \
	!defined(_CRT_SECURE_NO_WARNINGS)
	#define _CRT_SECURE_NO_WARNINGS
#endif
#if defined(XREDIS_IMPLEMENTATION) && \
	!defined(_WIN32) && !defined(_WIN64)
	#if defined(__linux__) && !defined(_GNU_SOURCE)
		#define _GNU_SOURCE 1
	#endif
	#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
		#define _DARWIN_C_SOURCE 1
	#endif
	#if !defined(_POSIX_C_SOURCE)
		#define _POSIX_C_SOURCE 200809L
	#endif
	#if !defined(_FILE_OFFSET_BITS)
		#define _FILE_OFFSET_BITS 64
	#endif
#endif
#ifndef XREDIS_SINGLE_HEADER_H
#define XREDIS_SINGLE_HEADER_H
#define XREDIS_SINGLE_HEADER 1


/* ========================================================================== */
/* feature selection: extlibs/xredis/include/xredis/features.h */
/* ========================================================================== */

/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XREDIS_FEATURES_H
#define XREDIS_FEATURES_H

/* xredis 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS)
#ifndef XREDIS_FEATURE_XREDIS
#define XREDIS_FEATURE_XREDIS
#endif
#ifndef XREDIS_MODULE_XREDIS_COMMAND
#define XREDIS_MODULE_XREDIS_COMMAND
#endif
#ifndef XREDIS_MODULE_XREDIS_READER
#define XREDIS_MODULE_XREDIS_READER
#endif
#ifndef XREDIS_MODULE_XREDIS_REPLY
#define XREDIS_MODULE_XREDIS_REPLY
#endif
#ifndef XREDIS_MODULE_XREDIS_CLIENT
#define XREDIS_MODULE_XREDIS_CLIENT
#endif
#ifndef XREDIS_MODULE_XREDIS_DIAL
#define XREDIS_MODULE_XREDIS_DIAL
#endif
#ifndef XREDIS_MODULE_XREDIS_TLS
#define XREDIS_MODULE_XREDIS_TLS
#endif
#ifndef XREDIS_MODULE_XREDIS_TLS_AUTH
#define XREDIS_MODULE_XREDIS_TLS_AUTH
#endif
#ifndef XREDIS_MODULE_XREDIS_TRANSACTION
#define XREDIS_MODULE_XREDIS_TRANSACTION
#endif
#ifndef XREDIS_MODULE_XREDIS_WATCH
#define XREDIS_MODULE_XREDIS_WATCH
#endif
#ifndef XREDIS_MODULE_XREDIS_POOL
#define XREDIS_MODULE_XREDIS_POOL
#endif
#ifndef XREDIS_MODULE_XREDIS_POOL_TRANSACTION
#define XREDIS_MODULE_XREDIS_POOL_TRANSACTION
#endif
#ifndef XREDIS_MODULE_XREDIS_POOL_WATCH
#define XREDIS_MODULE_XREDIS_POOL_WATCH
#endif
#ifndef XREDIS_MODULE_XREDIS_POOL_CONNECT
#define XREDIS_MODULE_XREDIS_POOL_CONNECT
#endif
#ifndef XREDIS_MODULE_XREDIS_TRACKING
#define XREDIS_MODULE_XREDIS_TRACKING
#endif
#ifndef XREDIS_MODULE_XREDIS_PUBSUB
#define XREDIS_MODULE_XREDIS_PUBSUB
#endif
#ifndef XREDIS_MODULE_XREDIS_SUBSCRIBER
#define XREDIS_MODULE_XREDIS_SUBSCRIBER
#endif
#ifndef XREDIS_MODULE_XREDIS_SUBSCRIBER_NOTIFY
#define XREDIS_MODULE_XREDIS_SUBSCRIBER_NOTIFY
#endif
#ifndef XREDIS_MODULE_XREDIS_SUBSCRIPTIONS
#define XREDIS_MODULE_XREDIS_SUBSCRIPTIONS
#endif
#ifndef XREDIS_MODULE_XREDIS_SUBSCRIPTION_SESSION
#define XREDIS_MODULE_XREDIS_SUBSCRIPTION_SESSION
#endif
#ifndef XREDIS_MODULE_XREDIS_ROLE
#define XREDIS_MODULE_XREDIS_ROLE
#endif
#ifndef XREDIS_MODULE_XREDIS_ROLE_CLIENT
#define XREDIS_MODULE_XREDIS_ROLE_CLIENT
#endif
#ifndef XREDIS_MODULE_XREDIS_SENTINEL
#define XREDIS_MODULE_XREDIS_SENTINEL
#endif
#ifndef XREDIS_MODULE_XREDIS_SENTINEL_POOL
#define XREDIS_MODULE_XREDIS_SENTINEL_POOL
#endif
#ifndef XREDIS_MODULE_XREDIS_CLUSTER_TOPOLOGY
#define XREDIS_MODULE_XREDIS_CLUSTER_TOPOLOGY
#endif
#ifndef XREDIS_MODULE_XREDIS_POOL_ASKING
#define XREDIS_MODULE_XREDIS_POOL_ASKING
#endif
#ifndef XREDIS_MODULE_XREDIS_POOL_SHARED
#define XREDIS_MODULE_XREDIS_POOL_SHARED
#endif
#ifndef XREDIS_MODULE_XREDIS_POOL_SHARED_CONNECT
#define XREDIS_MODULE_XREDIS_POOL_SHARED_CONNECT
#endif
#ifndef XREDIS_MODULE_XREDIS_POOL_SHARED_ASKING
#define XREDIS_MODULE_XREDIS_POOL_SHARED_ASKING
#endif
#ifndef XREDIS_MODULE_XREDIS_CLUSTER_ROUTE
#define XREDIS_MODULE_XREDIS_CLUSTER_ROUTE
#endif
#ifndef XREDIS_MODULE_XREDIS_CLUSTER_NODES
#define XREDIS_MODULE_XREDIS_CLUSTER_NODES
#endif
#ifndef XREDIS_MODULE_XREDIS_CLUSTER_REFRESH
#define XREDIS_MODULE_XREDIS_CLUSTER_REFRESH
#endif
#ifndef XREDIS_MODULE_XREDIS_CLUSTER_CLIENT
#define XREDIS_MODULE_XREDIS_CLUSTER_CLIENT
#endif
#ifndef XREDIS_MODULE_XREDIS_COMMAND_KEYS
#define XREDIS_MODULE_XREDIS_COMMAND_KEYS
#endif
#ifndef XREDIS_MODULE_XREDIS_CLUSTER_COMMAND
#define XREDIS_MODULE_XREDIS_CLUSTER_COMMAND
#endif
#endif

/* xredis_cluster_command 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_CLUSTER_COMMAND)
#ifndef XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND
#define XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND
#endif
#ifndef XREDIS_MODULE_XREDIS_CLUSTER_CLIENT
#define XREDIS_MODULE_XREDIS_CLUSTER_CLIENT
#endif
#ifndef XREDIS_MODULE_XREDIS_COMMAND_KEYS
#define XREDIS_MODULE_XREDIS_COMMAND_KEYS
#endif
#endif

/* xredis_command_keys 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_COMMAND_KEYS)
#ifndef XREDIS_FEATURE_XREDIS_COMMAND_KEYS
#define XREDIS_FEATURE_XREDIS_COMMAND_KEYS
#endif
#ifndef XREDIS_MODULE_XREDIS_REPLY
#define XREDIS_MODULE_XREDIS_REPLY
#endif
#endif

/* xredis_cluster_client 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_CLUSTER_CLIENT)
#ifndef XREDIS_FEATURE_XREDIS_CLUSTER_CLIENT
#define XREDIS_FEATURE_XREDIS_CLUSTER_CLIENT
#endif
#ifndef XREDIS_MODULE_XREDIS_CLUSTER_REFRESH
#define XREDIS_MODULE_XREDIS_CLUSTER_REFRESH
#endif
#endif

/* xredis_cluster_refresh 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_CLUSTER_REFRESH)
#ifndef XREDIS_FEATURE_XREDIS_CLUSTER_REFRESH
#define XREDIS_FEATURE_XREDIS_CLUSTER_REFRESH
#endif
#ifndef XREDIS_MODULE_XREDIS_CLUSTER_NODES
#define XREDIS_MODULE_XREDIS_CLUSTER_NODES
#endif
#endif

/* xredis_cluster_nodes 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_CLUSTER_NODES)
#ifndef XREDIS_FEATURE_XREDIS_CLUSTER_NODES
#define XREDIS_FEATURE_XREDIS_CLUSTER_NODES
#endif
#ifndef XREDIS_MODULE_XREDIS_CLUSTER_ROUTE
#define XREDIS_MODULE_XREDIS_CLUSTER_ROUTE
#endif
#ifndef XREDIS_MODULE_XREDIS_POOL_SHARED_CONNECT
#define XREDIS_MODULE_XREDIS_POOL_SHARED_CONNECT
#endif
#endif

/* xredis_cluster_route 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_CLUSTER_ROUTE)
#ifndef XREDIS_FEATURE_XREDIS_CLUSTER_ROUTE
#define XREDIS_FEATURE_XREDIS_CLUSTER_ROUTE
#endif
#ifndef XREDIS_MODULE_XREDIS_CLUSTER_TOPOLOGY
#define XREDIS_MODULE_XREDIS_CLUSTER_TOPOLOGY
#endif
#ifndef XREDIS_MODULE_XREDIS_POOL_SHARED_ASKING
#define XREDIS_MODULE_XREDIS_POOL_SHARED_ASKING
#endif
#endif

/* xredis_pool_shared_asking 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_POOL_SHARED_ASKING)
#ifndef XREDIS_FEATURE_XREDIS_POOL_SHARED_ASKING
#define XREDIS_FEATURE_XREDIS_POOL_SHARED_ASKING
#endif
#ifndef XREDIS_MODULE_XREDIS_POOL_SHARED
#define XREDIS_MODULE_XREDIS_POOL_SHARED
#endif
#ifndef XREDIS_MODULE_XREDIS_POOL_ASKING
#define XREDIS_MODULE_XREDIS_POOL_ASKING
#endif
#endif

/* xredis_pool_shared_connect 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_POOL_SHARED_CONNECT)
#ifndef XREDIS_FEATURE_XREDIS_POOL_SHARED_CONNECT
#define XREDIS_FEATURE_XREDIS_POOL_SHARED_CONNECT
#endif
#ifndef XREDIS_MODULE_XREDIS_POOL_SHARED
#define XREDIS_MODULE_XREDIS_POOL_SHARED
#endif
#ifndef XREDIS_MODULE_XREDIS_POOL_CONNECT
#define XREDIS_MODULE_XREDIS_POOL_CONNECT
#endif
#endif

/* xredis_pool_shared 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_POOL_SHARED)
#ifndef XREDIS_FEATURE_XREDIS_POOL_SHARED
#define XREDIS_FEATURE_XREDIS_POOL_SHARED
#endif
#ifndef XREDIS_MODULE_XREDIS_POOL
#define XREDIS_MODULE_XREDIS_POOL
#endif
#endif

/* xredis_pool_asking 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_POOL_ASKING)
#ifndef XREDIS_FEATURE_XREDIS_POOL_ASKING
#define XREDIS_FEATURE_XREDIS_POOL_ASKING
#endif
#ifndef XREDIS_MODULE_XREDIS_POOL
#define XREDIS_MODULE_XREDIS_POOL
#endif
#endif

/* xredis_cluster_topology 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_CLUSTER_TOPOLOGY)
#ifndef XREDIS_FEATURE_XREDIS_CLUSTER_TOPOLOGY
#define XREDIS_FEATURE_XREDIS_CLUSTER_TOPOLOGY
#endif
#ifndef XREDIS_MODULE_XREDIS_REPLY
#define XREDIS_MODULE_XREDIS_REPLY
#endif
#endif

/* xredis_sentinel_pool 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_SENTINEL_POOL)
#ifndef XREDIS_FEATURE_XREDIS_SENTINEL_POOL
#define XREDIS_FEATURE_XREDIS_SENTINEL_POOL
#endif
#ifndef XREDIS_MODULE_XREDIS_SENTINEL
#define XREDIS_MODULE_XREDIS_SENTINEL
#endif
#ifndef XREDIS_MODULE_XREDIS_POOL_CONNECT
#define XREDIS_MODULE_XREDIS_POOL_CONNECT
#endif
#endif

/* xredis_sentinel 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_SENTINEL)
#ifndef XREDIS_FEATURE_XREDIS_SENTINEL
#define XREDIS_FEATURE_XREDIS_SENTINEL
#endif
#ifndef XREDIS_MODULE_XREDIS_CLIENT
#define XREDIS_MODULE_XREDIS_CLIENT
#endif
#ifndef XREDIS_MODULE_XREDIS_ROLE
#define XREDIS_MODULE_XREDIS_ROLE
#endif
#endif

/* xredis_role_client 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_ROLE_CLIENT)
#ifndef XREDIS_FEATURE_XREDIS_ROLE_CLIENT
#define XREDIS_FEATURE_XREDIS_ROLE_CLIENT
#endif
#ifndef XREDIS_MODULE_XREDIS_CLIENT
#define XREDIS_MODULE_XREDIS_CLIENT
#endif
#ifndef XREDIS_MODULE_XREDIS_ROLE
#define XREDIS_MODULE_XREDIS_ROLE
#endif
#endif

/* xredis_role 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_ROLE)
#ifndef XREDIS_FEATURE_XREDIS_ROLE
#define XREDIS_FEATURE_XREDIS_ROLE
#endif
#ifndef XREDIS_MODULE_XREDIS_REPLY
#define XREDIS_MODULE_XREDIS_REPLY
#endif
#endif

/* xredis_subscription_session 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_SUBSCRIPTION_SESSION)
#ifndef XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION
#define XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION
#endif
#ifndef XREDIS_MODULE_XREDIS_SUBSCRIPTIONS
#define XREDIS_MODULE_XREDIS_SUBSCRIPTIONS
#endif
#endif

/* xredis_subscriptions 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_SUBSCRIPTIONS)
#ifndef XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS
#define XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS
#endif
#ifndef XREDIS_MODULE_XREDIS_SUBSCRIBER
#define XREDIS_MODULE_XREDIS_SUBSCRIBER
#endif
#endif

/* xredis_subscriber_notify 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_SUBSCRIBER_NOTIFY)
#ifndef XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY
#define XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY
#endif
#ifndef XREDIS_MODULE_XREDIS_SUBSCRIBER
#define XREDIS_MODULE_XREDIS_SUBSCRIBER
#endif
#endif

/* xredis_subscriber 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_SUBSCRIBER)
#ifndef XREDIS_FEATURE_XREDIS_SUBSCRIBER
#define XREDIS_FEATURE_XREDIS_SUBSCRIBER
#endif
#ifndef XREDIS_MODULE_XREDIS_CLIENT
#define XREDIS_MODULE_XREDIS_CLIENT
#endif
#ifndef XREDIS_MODULE_XREDIS_PUBSUB
#define XREDIS_MODULE_XREDIS_PUBSUB
#endif
#ifndef XRT_MODULE_HASH64
#define XRT_MODULE_HASH64
#endif
#endif

/* xredis_pubsub 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_PUBSUB)
#ifndef XREDIS_FEATURE_XREDIS_PUBSUB
#define XREDIS_FEATURE_XREDIS_PUBSUB
#endif
#ifndef XREDIS_MODULE_XREDIS_COMMAND
#define XREDIS_MODULE_XREDIS_COMMAND
#endif
#ifndef XREDIS_MODULE_XREDIS_REPLY
#define XREDIS_MODULE_XREDIS_REPLY
#endif
#endif

/* xredis_tracking 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_TRACKING)
#ifndef XREDIS_FEATURE_XREDIS_TRACKING
#define XREDIS_FEATURE_XREDIS_TRACKING
#endif
#ifndef XREDIS_MODULE_XREDIS_CLIENT
#define XREDIS_MODULE_XREDIS_CLIENT
#endif
#endif

/* xredis_tls_auth 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_TLS_AUTH)
#ifndef XREDIS_FEATURE_XREDIS_TLS_AUTH
#define XREDIS_FEATURE_XREDIS_TLS_AUTH
#endif
#ifndef XREDIS_MODULE_XREDIS_TLS
#define XREDIS_MODULE_XREDIS_TLS
#endif
#ifndef XRT_MODULE_TLS_CLIENT_AUTH
#define XRT_MODULE_TLS_CLIENT_AUTH
#endif
#endif

/* xredis_tls 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_TLS)
#ifndef XREDIS_FEATURE_XREDIS_TLS
#define XREDIS_FEATURE_XREDIS_TLS
#endif
#ifndef XREDIS_MODULE_XREDIS_DIAL
#define XREDIS_MODULE_XREDIS_DIAL
#endif
#ifndef XRT_MODULE_TLS_STREAM
#define XRT_MODULE_TLS_STREAM
#endif
#ifndef XRT_MODULE_TLS_CLIENT_RESUME
#define XRT_MODULE_TLS_CLIENT_RESUME
#endif
#ifndef XRT_MODULE_TLS_RECORD_AES
#define XRT_MODULE_TLS_RECORD_AES
#endif
#ifndef XRT_MODULE_TLS_RECORD_CHACHA
#define XRT_MODULE_TLS_RECORD_CHACHA
#endif
#ifndef XRT_MODULE_TLS_SCHEDULE_SHA256
#define XRT_MODULE_TLS_SCHEDULE_SHA256
#endif
#ifndef XRT_MODULE_TLS_SCHEDULE_SHA384
#define XRT_MODULE_TLS_SCHEDULE_SHA384
#endif
#ifndef XRT_MODULE_TLS_KEY_EXCHANGE_X25519
#define XRT_MODULE_TLS_KEY_EXCHANGE_X25519
#endif
#ifndef XRT_MODULE_TLS_KEY_EXCHANGE_P256
#define XRT_MODULE_TLS_KEY_EXCHANGE_P256
#endif
#ifndef XRT_MODULE_TLS_KEY_EXCHANGE_P384
#define XRT_MODULE_TLS_KEY_EXCHANGE_P384
#endif
#endif

/* xredis_dial 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_DIAL)
#ifndef XREDIS_FEATURE_XREDIS_DIAL
#define XREDIS_FEATURE_XREDIS_DIAL
#endif
#ifndef XREDIS_MODULE_XREDIS_CLIENT
#define XREDIS_MODULE_XREDIS_CLIENT
#endif
#ifndef XRT_MODULE_NET_TCP_DIAL
#define XRT_MODULE_NET_TCP_DIAL
#endif
#endif

/* xredis_pool_transaction 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_POOL_TRANSACTION)
#ifndef XREDIS_FEATURE_XREDIS_POOL_TRANSACTION
#define XREDIS_FEATURE_XREDIS_POOL_TRANSACTION
#endif
#ifndef XREDIS_MODULE_XREDIS_POOL
#define XREDIS_MODULE_XREDIS_POOL
#endif
#ifndef XREDIS_MODULE_XREDIS_TRANSACTION
#define XREDIS_MODULE_XREDIS_TRANSACTION
#endif
#endif

/* xredis_pool_watch 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_POOL_WATCH)
#ifndef XREDIS_FEATURE_XREDIS_POOL_WATCH
#define XREDIS_FEATURE_XREDIS_POOL_WATCH
#endif
#ifndef XREDIS_MODULE_XREDIS_POOL
#define XREDIS_MODULE_XREDIS_POOL
#endif
#ifndef XREDIS_MODULE_XREDIS_WATCH
#define XREDIS_MODULE_XREDIS_WATCH
#endif
#endif

/* xredis_watch 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_WATCH)
#ifndef XREDIS_FEATURE_XREDIS_WATCH
#define XREDIS_FEATURE_XREDIS_WATCH
#endif
#ifndef XREDIS_MODULE_XREDIS_TRANSACTION
#define XREDIS_MODULE_XREDIS_TRANSACTION
#endif
#endif

/* xredis_transaction 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_TRANSACTION)
#ifndef XREDIS_FEATURE_XREDIS_TRANSACTION
#define XREDIS_FEATURE_XREDIS_TRANSACTION
#endif
#ifndef XREDIS_MODULE_XREDIS_CLIENT
#define XREDIS_MODULE_XREDIS_CLIENT
#endif
#endif

/* xredis_pool_connect 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_POOL_CONNECT)
#ifndef XREDIS_FEATURE_XREDIS_POOL_CONNECT
#define XREDIS_FEATURE_XREDIS_POOL_CONNECT
#endif
#ifndef XREDIS_MODULE_XREDIS_POOL
#define XREDIS_MODULE_XREDIS_POOL
#endif
#endif

/* xredis_pool 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_POOL)
#ifndef XREDIS_FEATURE_XREDIS_POOL
#define XREDIS_FEATURE_XREDIS_POOL
#endif
#ifndef XREDIS_MODULE_XREDIS_CLIENT
#define XREDIS_MODULE_XREDIS_CLIENT
#endif
#endif

/* xredis_client 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_CLIENT)
#ifndef XREDIS_FEATURE_XREDIS_CLIENT
#define XREDIS_FEATURE_XREDIS_CLIENT
#endif
#ifndef XREDIS_MODULE_XREDIS_COMMAND
#define XREDIS_MODULE_XREDIS_COMMAND
#endif
#ifndef XREDIS_MODULE_XREDIS_REPLY
#define XREDIS_MODULE_XREDIS_REPLY
#endif
#ifndef XRT_MODULE_NET_TCP
#define XRT_MODULE_NET_TCP
#endif
#ifndef XRT_MODULE_FUTURE
#define XRT_MODULE_FUTURE
#endif
#ifndef XRT_MODULE_NUMBER_INTEGER
#define XRT_MODULE_NUMBER_INTEGER
#endif
#endif

/* xredis_reply 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_REPLY)
#ifndef XREDIS_FEATURE_XREDIS_REPLY
#define XREDIS_FEATURE_XREDIS_REPLY
#endif
#ifndef XREDIS_MODULE_XREDIS_READER
#define XREDIS_MODULE_XREDIS_READER
#endif
#ifndef XRT_MODULE_ATOMIC
#define XRT_MODULE_ATOMIC
#endif
#ifndef XRT_MODULE_NUMBER_FLOAT
#define XRT_MODULE_NUMBER_FLOAT
#endif
#endif

/* xredis_reader 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_READER)
#ifndef XREDIS_FEATURE_XREDIS_READER
#define XREDIS_FEATURE_XREDIS_READER
#endif
#endif

/* xredis_command 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_COMMAND)
#ifndef XREDIS_FEATURE_XREDIS_COMMAND
#define XREDIS_FEATURE_XREDIS_COMMAND
#endif
#endif

#endif /* XREDIS_FEATURES_H */


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/export.h */
/* ========================================================================== */

#ifndef XREDIS_EXPORT_H
#define XREDIS_EXPORT_H


#if defined(_WIN32) || defined(_WIN64)
 #if defined(XREDIS_BUILD_SHARED)
  #define XREDIS_API __declspec(dllexport)
 #elif defined(XREDIS_USE_SHARED)
  #define XREDIS_API __declspec(dllimport)
 #else
  #define XREDIS_API
 #endif
#elif defined(__GNUC__) || defined(__clang__)
 #if defined(XREDIS_BUILD_SHARED)
  #define XREDIS_API __attribute__((visibility("default")))
 #else
  #define XREDIS_API
 #endif
#else
 #define XREDIS_API
#endif

#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/command.h */
/* ========================================================================== */

#ifndef XREDIS_COMMAND_H
#define XREDIS_COMMAND_H


#if defined(XREDIS_FEATURE_XREDIS_COMMAND)

/* These error codes belong to the xrt.redis domain, not server error replies. */
typedef enum xrediscommanderror {
	XREDIS_COMMAND_ARGUMENT = 1,
	XREDIS_COMMAND_OVERFLOW,
	XREDIS_COMMAND_CAPACITY,
	XREDIS_COMMAND_OVERLAP
} xrediscommanderror;

XRT_EXTERN_C_BEGIN

/* Measure one RESP array of bulk strings, shared by RESP2 and RESP3 commands.
 * Count must be positive; argument zero is the command name. Arguments borrow
 * stable, readable memory for this call. NULL/0 is a valid empty argument;
 * NULL/nonzero is invalid. Embedded NUL and CRLF are ordinary payload bytes.
 * A length/count greater than INT64_MAX or size/address arithmetic overflow is
 * rejected. Required is unchanged on failure and must not alias an input.
 * Success allocates nothing and preserves the ambient XRT error. */
XREDIS_API bool xredisCommandSize(const xbytesview* Arguments, size_t Count,
	size_t* Required);

/* Write the exact measured wire bytes into caller-owned Output; no trailing NUL.
 * Written is optional. Output and Written must not overlap input descriptors,
 * input payloads, or each other. Only the actual wire span is an output span.
 * All validation precedes the first write: Output and Written are unchanged on
 * failure. Success copies each payload exactly once, allocates nothing, retains
 * nothing and preserves the ambient error. The caller supplies actual readable/
 * writable objects; address-wrap checks do not validate arbitrary addresses.
 * This contiguous convenience encoder is not a zero-copy send API. */
XREDIS_API bool xredisCommandWrite(ptr Output, size_t Capacity,
	const xbytesview* Arguments, size_t Count, size_t* Written);

XRT_EXTERN_C_END

#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/reader.h */
/* ========================================================================== */

#ifndef XREDIS_READER_H
#define XREDIS_READER_H


#if defined(XREDIS_FEATURE_XREDIS_READER)

typedef struct xredisreader xredisreader;

typedef enum xredisprotocol {
	XREDIS_RESP2 = 2,
	XREDIS_RESP3 = 3
} xredisprotocol;

typedef enum xredisreplytype {
	XREDIS_REPLY_SIMPLE_STRING = 1,
	XREDIS_REPLY_SIMPLE_ERROR,
	XREDIS_REPLY_INTEGER,
	XREDIS_REPLY_BULK_STRING,
	XREDIS_REPLY_ARRAY,
	XREDIS_REPLY_NULL,
	XREDIS_REPLY_BOOLEAN,
	XREDIS_REPLY_DOUBLE,
	XREDIS_REPLY_BIG_NUMBER,
	XREDIS_REPLY_BULK_ERROR,
	XREDIS_REPLY_VERBATIM,
	XREDIS_REPLY_MAP,
	XREDIS_REPLY_ATTRIBUTE,
	XREDIS_REPLY_SET,
	XREDIS_REPLY_PUSH
} xredisreplytype;

#define XREDIS_REPLY_NULL_VALUE 0x01u
#define XREDIS_REPLY_STREAMED 0x02u
#define XREDIS_LENGTH_UNKNOWN UINT64_MAX

/* Optional compatibility with pre-7.2 libc NaN forms: case insensitive,
 * signed nan and nan([A-Za-z0-9_]*). The default follows normalized RESP3. */
#define XREDIS_READER_LEGACY_NAN 0x01u

typedef enum xredisreadkind {
	XREDIS_READ_BEGIN = 1,
	XREDIS_READ_DATA,
	XREDIS_READ_END
} xredisreadkind;

/* Depth is zero for the root. Length is payload bytes for scalars and logical
 * elements/pairs for aggregates (maps/attributes count pairs). Scalar BEGIN
 * lengths are unknown, except null; bulk/aggregate BEGIN has the declared size
 * or UNKNOWN for streaming. END always has the actual size, null has size 0.
 * DATA contains nonempty raw wire payload, including numeric text and the four
 * verbatim format bytes; it excludes framing/chunk headers. Integer/Boolean
 * are meaningful only on END for the corresponding types. Data and the event
 * itself are borrowed ONLY until this callback returns. No input is retained. */
typedef struct xredisreadevent {
	xredisreadkind Kind;
	xredisreplytype Type;
	uint32 Flags;
	size_t Depth;
	uint64 Length;
	int64 Integer;
	bool Boolean;
	xbytesview Data;
} xredisreadevent;

typedef enum xredisreadaction {
	XREDIS_READ_NEXT = 0,
	XREDIS_READ_PAUSE,
	XREDIS_READ_FAIL
} xredisreadaction;

typedef xredisreadaction (*xredisreadproc)(const xredisreadevent* Event, ptr User);

typedef enum xredisreadererror {
	XREDIS_READER_OK = 0,
	XREDIS_READER_ARGUMENT = 1001,
	XREDIS_READER_STATE,
	XREDIS_READER_PROTOCOL,
	XREDIS_READER_LIMIT,
	XREDIS_READER_CALLBACK,
	XREDIS_READER_TRUNCATED
} xredisreadererror;

typedef enum xredisreadstatus {
	XREDIS_READER_ERROR = -1,
	XREDIS_READER_MORE = 0,
	XREDIS_READER_READY,
	XREDIS_READER_PAUSED,
	XREDIS_READER_YIELD
} xredisreadstatus;

/* Initialize with ConfigInit, then change explicit limits. Zero is a real
 * limit, not unlimited. MaxFrameBytes, MaxValues and MaxFeedBytes must be >0;
 * uint64 limits must be <=INT64_MAX. MaxDepth counts open aggregates, not
 * scalar nesting. MaxLineBytes bounds BOTH scalar lines and numeric headers.
 * MaxContainerItems counts direct elements/pairs, excluding attributes.
 * MaxValues includes every value and attribute in the semantic frame. */
typedef struct xredisreaderconfig {
	size_t Size;
	xredisprotocol Protocol;
	uint32 Flags;
	size_t MaxDepth;
	uint64 MaxFrameBytes;
	uint64 MaxBulkBytes;
	uint64 MaxLineBytes;
	uint64 MaxContainerItems;
	uint64 MaxValues;
	size_t MaxFeedBytes;
} xredisreaderconfig;

typedef struct xredisreaderinfo {
	size_t Size;
	uint64 StreamBytes;
	uint64 FrameBytes;
	uint64 FrameValues;
	uint64 LastFrameBytes;
	uint64 LastFrameValues;
	uint64 CompletedFrames;
	size_t OpenContainers;
	bool Paused;
	xredisreadererror Error;
	/* Absolute stream position at detection (next unconsumed byte), not always
	 * the start of an invalid token. Reset restarts all counters at zero. */
	uint64 ErrorOffset;
} xredisreaderinfo;

XRT_EXTERN_C_BEGIN

/* Defaults: RESP3, normalized NaN, depth 64, frame 64 MiB, bulk 16 MiB,
 * line 64 KiB, container/value counts 1 million, Feed budget 64 KiB. */
XREDIS_API bool xredisReaderConfigInit(xredisreaderconfig* Config);

/* One bounded XRT allocation including the depth stack. NULL Config selects
 * defaults. Callback is required; User is borrowed until Destroy. Config is
 * copied. Result is unchanged on failure and must not overlap Config. */
XREDIS_API bool xredisReaderCreate(const xredisreaderconfig* Config,
	xredisreadproc Callback, ptr User, xredisreader** Result);

/* Single-owner, thread-confined reader; callers serialize all access. Feed,
 * Reset, Finish and Destroy cannot reenter a callback; Info can. Callback
 * reentry failure does not poison the outer parse or free the reader.
 * Destroy(NULL) succeeds; valid Destroy releases its one allocation. */
XREDIS_API bool xredisReaderDestroy(xredisreader* Reader);
XREDIS_API bool xredisReaderReset(xredisreader* Reader);
XREDIS_API bool xredisReaderInfo(const xredisreader* Reader, xredisreaderinfo* Info);

/* Consume at most MaxFeedBytes and stop after ONE complete semantic frame.
 * READY includes any preceding attributes and exactly one root value/push.
 * Attributes do not consume parent items; pushes must be top-level, nonempty
 * and start with a non-null simple/bulk string. RESP2 rejects RESP3 types.
 * MORE needs more bytes; YIELD leaves bytes in Input after the work budget;
 * PAUSED means the callback requested pause AFTER the event was delivered.
 * Resume with the unconsumed suffix, possibly empty: pending events and READY
 * may occur with Consumed==0. Events are never repeated after pause.
 * DATA may precede a later malformed terminator: discard partial frame state
 * on ERROR, and publish an assembled result only after READY. Protocol, limit,
 * callback and truncated-EOF failures are terminal until Reset. Invalid API
 * arguments leave reader state and Consumed unchanged. Other results set
 * Consumed, including ERROR. Consumed must not overlap Input or reader storage.
 * No successful Feed/Reset/Info/Finish allocates, copies payload or clears the
 * ambient error (callbacks can change it). Error diagnostics are cold-path XRT
 * allocations with XERR_MEMORY fallback. Actual readable objects are required;
 * address-span checks cannot validate arbitrary addresses. */
XREDIS_API xredisreadstatus xredisReaderFeed(xredisreader* Reader,
	xbytesview Input, size_t* Consumed);

/* End-of-stream check. Succeeds only at a frame boundary. Partial input and
 * orphan attributes fail terminally. A paused reader must first be resumed. */
XREDIS_API bool xredisReaderFinish(xredisreader* Reader);

XRT_EXTERN_C_END

#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/reply.h */
/* ========================================================================== */

#ifndef XREDIS_REPLY_H
#define XREDIS_REPLY_H


#if defined(XREDIS_FEATURE_XREDIS_REPLY) && \
	(!defined(XREDIS_FEATURE_XREDIS_READER) || !defined(XRT_FEATURE_ATOMIC) || !defined(XRT_FEATURE_NUMBER_FLOAT))
#error "XREDIS_FEATURE_XREDIS_REPLY requires reader, atomic and number_float features"
#endif

#if defined(XREDIS_FEATURE_XREDIS_REPLY)

typedef struct xredisreply xredisreply;
typedef struct xredisreplyreader xredisreplyreader;

#define XREDIS_REPLY_NONE SIZE_MAX

typedef enum xredisreplyerror {
	XREDIS_REPLY_OK = 0,
	XREDIS_REPLY_ARGUMENT = 2001,
	XREDIS_REPLY_STATE,
	XREDIS_REPLY_PROTOCOL,
	XREDIS_REPLY_LIMIT,
	XREDIS_REPLY_MEMORY,
	XREDIS_REPLY_TRUNCATED
} xredisreplyerror;

typedef enum xredisreplystatus {
	XREDIS_REPLY_ERROR = -1,
	XREDIS_REPLY_MORE = 0,
	XREDIS_REPLY_READY,
	XREDIS_REPLY_YIELD
} xredisreplystatus;

/* Indices belong to one immutable reply. NONE ends every chain. ChildCount
 * counts actual children (twice Length for maps/attributes); metadata maps do
 * not consume children. Attributes is an ordered chain of attribute nodes,
 * using Next just like the ordinary child chain. Preserve duplicate map keys,
 * wire order, null kinds and streamed flags. Data contains owned raw scalar
 * payload, including numeric lexemes and verbatim fmt:, without framing or
 * a trailing NUL. Integer/Boolean are native values of those types only. */
typedef struct xredisreplyvalue {
	xredisreplytype Type;
	uint32 Flags;
	uint64 Length;
	int64 Integer;
	bool Boolean;
	xbytesview Data;
	size_t ChildCount;
	size_t FirstChild;
	size_t Next;
	size_t Attributes;
} xredisreplyvalue;

typedef struct xredisreplyinfo {
	size_t Size;
	size_t Root;
	size_t Nodes;
	size_t PayloadBytes;
	/* Requested XRT bytes, including header/inline storage and unused vector
	 * capacity. Peak also includes old+new buffers during geometric growth.
	 * Allocator headers, size-class rounding and diagnostics are excluded. */
	size_t ReservedBytes;
	size_t PeakBytes;
} xredisreplyinfo;

typedef struct xredisreplyreaderconfig {
	size_t Size;
	xredisreaderconfig Reader;
	/* Per-frame peak requested owned bytes, including temporary growth copies.
	 * Does not include reader/control stacks or already transferred replies.
	 * Zero is invalid. Small limits can reject even an empty scalar. */
	size_t MaxOwnedBytes;
} xredisreplyreaderconfig;

typedef struct xredisreplyreaderinfo {
	size_t Size;
	xredisreaderinfo Reader;
	xredisreplyerror Error;
	size_t PartialNodes;
	size_t PartialPayloadBytes;
	size_t PartialReservedBytes;
	/* Current/most recently completed or failed frame; Reset clears it. */
	size_t PeakOwnedBytes;
} xredisreplyreaderinfo;

XRT_EXTERN_C_BEGIN

/* Default event-reader limits, plus 64 MiB peak owned bytes per frame. */
XREDIS_API bool xredisReplyReaderConfigInit(xredisreplyreaderconfig* Config);

/* Config is copied; NULL selects defaults. Two XRT allocations hold control
 * and bounded depth stacks. Result is unchanged on failure. Creation may
 * report the nested Reader's validation errors. All reader access is confined
 * to one thread. No user callback or input pointer is retained. */
XREDIS_API bool xredisReplyReaderCreate(const xredisreplyreaderconfig* Config,
	xredisreplyreader** Result);
XREDIS_API void xredisReplyReaderDestroy(xredisreplyreader* Reader);
XREDIS_API bool xredisReplyReaderReset(xredisreplyreader* Reader);
XREDIS_API bool xredisReplyReaderInfo(const xredisreplyreader* Reader, xredisreplyreaderinfo* Info);

/* Consume at most Reader.MaxFeedBytes and publish ONE fully validated semantic
 * frame on READY, transferring one strong reference. Prior attributes belong
 * to that root, including pushes. Result is unchanged on all other outcomes.
 * Resume YIELD with the unconsumed suffix; MORE needs more input. No retained
 * source bytes, per-node allocation, second parse or final flatten/copy.
 * Scalar bytes are copied into owned storage; vector growth can copy them
 * again. One small scalar needs one allocation. All resource/protocol/OOM/EOF
 * errors are terminal until Reset, release partial ownership immediately and
 * keep their typed cause in Info even if diagnostics cannot allocate.
 * Invalid API calls leave state/Consumed/Result unchanged. Otherwise Consumed
 * is always written, including ERROR. All outputs must be disjoint from each
 * other, Input and reader/partial-frame storage. Successful calls preserve
 * the ambient error. Actual readable objects are required, not just spans. */
XREDIS_API xredisreplystatus xredisReplyReaderFeed(xredisreplyreader* Reader,
	xbytesview Input, size_t* Consumed, xredisreply** Result);
XREDIS_API bool xredisReplyReaderFinish(xredisreplyreader* Reader);

/* Immutable replies may be shared across threads after a synchronized handoff.
 * Each user must already own a live reference when retaining/accessing it.
 * Retain uses a bounded 32-bit atomic count; overflow leaves it unchanged.
 * Release(NULL) is allowed. Final release frees all owned storage on its thread.
 * Refcounting does not publish a pointer or make a concurrent final release
 * safe without a reference. Views remain valid until that last release. */
XREDIS_API bool xredisReplyRetain(xredisreply* Reply);
XREDIS_API void xredisReplyRelease(xredisreply* Reply);
XREDIS_API bool xredisReplyInfo(const xredisreply* Reply, xredisreplyinfo* Info);
XREDIS_API bool xredisReplyGet(const xredisreply* Reply, size_t Index, xredisreplyvalue* Value);

/* Lazy, locale-independent correctly rounded binary64 conversion of a DOUBLE
 * node. Raw text stays lossless; legacy NaN payloads normalize to signed NaN.
 * Finite overflow reports the public XRT number error and preserves Value;
 * it does not retroactively invalidate a syntactically valid wire reply.
 * Successful conversion allocates nothing and preserves the ambient error. */
XREDIS_API bool xredisReplyDouble(const xredisreply* Reply, size_t Index, double* Value);

XRT_EXTERN_C_END

#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/client.h */
/* ========================================================================== */

#ifndef XREDIS_CLIENT_H
#define XREDIS_CLIENT_H

#if defined(XREDIS_FEATURE_XREDIS_CLIENT) && \
	(!defined(XREDIS_FEATURE_XREDIS_COMMAND) || !defined(XREDIS_FEATURE_XREDIS_REPLY) || \
	 !defined(XRT_FEATURE_NET_TCP) || !defined(XRT_FEATURE_FUTURE) || !defined(XRT_FEATURE_NUMBER_INTEGER))
#error "XREDIS_FEATURE_XREDIS_CLIENT requires command, reply, net_tcp, future and number_integer features"
#endif

#if defined(XREDIS_FEATURE_XREDIS_CLIENT)
typedef struct xredisclient xredisclient;

typedef enum xredisclientstate {
	XREDIS_CLIENT_CONNECTING = 0, XREDIS_CLIENT_OPEN, XREDIS_CLIENT_CLOSING, XREDIS_CLIENT_CLOSED
} xredisclientstate;

typedef enum xredisrequeststatus {
	XREDIS_REQUEST_OK = 0, XREDIS_REQUEST_SERVER_ERROR, XREDIS_REQUEST_TIMEOUT,
	XREDIS_REQUEST_CANCELLED, XREDIS_REQUEST_CLOSED, XREDIS_REQUEST_DISCONNECTED,
	XREDIS_REQUEST_PROTOCOL, XREDIS_REQUEST_LIMIT, XREDIS_REQUEST_MEMORY,
	XREDIS_REQUEST_INITIALIZATION
} xredisrequeststatus;

typedef enum xredisdelivery {
	XREDIS_NOT_SENT = 0, XREDIS_MAY_HAVE_EXECUTED, XREDIS_REPLIED
} xredisdelivery;

typedef enum xredisadmission {
	XREDIS_ADMISSION_ERROR = -1, XREDIS_ADMISSION_ACCEPTED = 0,
	XREDIS_ADMISSION_OVERLOADED, XREDIS_ADMISSION_CLOSED
} xredisadmission;

typedef enum xredisclienterror {
	XREDIS_CLIENT_ARGUMENT = 3001, XREDIS_CLIENT_STATE, XREDIS_CLIENT_COMMAND_MODE,
	XREDIS_CLIENT_INITIALIZATION_FAILED
} xredisclienterror;

/* Every accepted call resolves its Future with this immutable typed outcome,
 * including cancellation/timeout/transport failure. Future cancellation asks
 * the producer to stop; XFUTURE_RESOLVED itself does not imply request success.
 * Reply (OK/SERVER_ERROR) and optional explicit Cause are borrowed from that
 * Future. Retain a reply before dropping its Future. Status/Delivery remain
 * exact even if error diagnostics cannot allocate. Delivery states knowledge
 * at logical completion: accepting bytes into TCP is MAY_HAVE_EXECUTED, not
 * proof that the server received/executed them. No operation is auto-replayed. */
typedef struct xredisrequestresult {
	xredisrequeststatus Status;
	xredisdelivery Delivery;
	xredisreply* Reply;
	const xerror* Cause;
} xredisrequestresult;

typedef struct xredisclientconfig {
	size_t Size;
	xnetstreamconfig Stream;
	xredisreplyreaderconfig Replies;
	size_t MaxRequests;
	size_t MaxCommandBytes;
	size_t MaxCommandBytesTotal;
	size_t MaxInFlight;
	size_t MaxPushes;
	size_t MaxPushBytes;
	size_t WorkRequests;
	size_t ReadBytes;
	size_t ReadFrames;
	/* Positive grace after a sent request is locally cancelled/timed out.
	 * Keep its FIFO slot until drained; retire the connection if grace expires. */
	int64 DrainTimeout;
} xredisclientconfig;

typedef enum xredisinitmode {
	XREDIS_INIT_AUTO = 0, XREDIS_INIT_RESP2 = 2, XREDIS_INIT_RESP3 = 3
} xredisinitmode;

#define XREDIS_INIT_AUTH 0x01u
#define XREDIS_INIT_USERNAME 0x02u
#define XREDIS_INIT_SETNAME 0x04u
#define XREDIS_INIT_SELECT 0x08u
#define XREDIS_PROTOCOL_UNKNOWN ((xredisprotocol)0)

typedef enum xredisinitstep {
	XREDIS_INIT_NONE = 0, XREDIS_INIT_HELLO, XREDIS_INIT_AUTHENTICATE,
	XREDIS_INIT_NAME, XREDIS_INIT_DATABASE, XREDIS_INIT_DONE, XREDIS_INIT_ROLE
} xredisinitstep;

typedef enum xredisreadystatus {
	XREDIS_READY_OK = 0, XREDIS_READY_AUTH_FAILED, XREDIS_READY_UNSUPPORTED,
	XREDIS_READY_SERVER_ERROR, XREDIS_READY_PROTOCOL, XREDIS_READY_TIMEOUT,
	XREDIS_READY_CLOSED, XREDIS_READY_DISCONNECTED, XREDIS_READY_LIMIT,
	XREDIS_READY_MEMORY, XREDIS_READY_ROLE_MISMATCH
} xredisreadystatus;

/* Initialize before use. Views borrow stable readable input only for ConnectInit.
 * Flags make empty passwords/names and an explicit SELECT 0 unambiguous. With
 * AUTH but no USERNAME use the default user (legacy AUTH password on RESP2).
 * Unselected nonempty fields, USERNAME without AUTH, and a nonzero Database
 * without SELECT are invalid. Server validates client-name/database semantics.
 * Timeout includes factory encoding, worker wait, TCP setup and all steps.
 * MaxWireBytes bounds ALL preencoded setup commands, including AUTO fallback.
 * Default strict RESP3, no optional steps, 5000 ms, 64 KiB. */
typedef struct xredisclientinitconfig {
	size_t Size;
	xredisinitmode Mode;
	uint32 Flags;
	xbytesview Username;
	xbytesview Password;
	xbytesview Name;
	uint32 Database;
	int64 Timeout;
	size_t MaxWireBytes;
} xredisclientinitconfig;

/* Borrowed from the readiness Future. Protocol can be UNKNOWN before selection.
 * Negotiated means a validated HELLO response; legacy RESP2 uses a new server
 * connection's default. Hello preserves the successful RESP3 map/attributes on
 * OK only. Error responses are not retained or copied into Cause, since a peer
 * may echo authentication arguments. Typed status/step survive diagnostic OOM.
 * Retained readiness outcomes do not keep the client/engine alive. */
typedef struct xredisreadyresult {
	xredisreadystatus Status;
	xredisinitstep Step;
	xredisprotocol Protocol;
	bool Negotiated;
	bool Downgraded;
	xredisreply* Hello;
	const xerror* Cause;
} xredisreadyresult;

typedef struct xredisclientinfo {
	size_t Size;
	xredisclientstate State;
	/* Includes concurrent construction and wire/timer/cancel-release tails.
	 * Completed results retained by callers do not retain request/client/engine. */
	size_t Requests;
	size_t CommandBytes;
	size_t Pushes;
	size_t PushBytes;
	uint64 Accepted;
	uint64 Rejected;
	uint64 Completed;
	uint64 DiscardedReplies;
	xredisrequeststatus CloseStatus;
	bool Ready;
	xredisprotocol Protocol;
	bool Negotiated;
	bool Downgraded;
	xredisinitstep InitStep;
	/* Encoded setup storage still owned, including accepted TCP reference tails;
	 * zero is published only after the owned wire is wiped and freed. */
	size_t InitBytes;
} xredisclientinfo;

XRT_EXTERN_C_BEGIN
XREDIS_API bool xredisClientConfigInit(xredisclientconfig* Config);
XREDIS_API bool xredisClientInitConfigInit(xredisclientinitconfig* Init);

/* One TCP connection on an existing running XRT engine, fixed worker affinity.
 * Numeric address only. Config is copied; NULL supplies finite defaults.
 * Asynchronous start permits calls while connecting. Result unchanged on
 * failure. No AUTH/HELLO negotiation, DNS, TLS or reconnect is implicit.
 * Connect/Call may finish before their creation function returns. The caller
 * owns one reference and must Close then Release. Engine is pinned until all
 * physical requests/posts retire; the engine outlives these APIs/callbacks.
 * This module handles ordinary one-reply commands. Known PubSub/replication/
 * MONITOR/CLIENT REPLY mode switches are rejected before admission; dedicated
 * modes and unknown module-command cardinality need their own contract. */
XREDIS_API bool xredisClientConnect(xnetengine* Engine, const xnetaddr* Remote,
	uint64 Affinity, const xredisclientconfig* Config, xredisclient** Result);

/* Managed setup barrier. NULL Init selects strict RESP3 defaults. HELLO 3 can
 * combine AUTH/SETNAME; optional SELECT follows its validated reply. AUTO falls
 * back ONLY on explicit NOPROTO or unknown HELLO errors, then AUTH/name/SELECT.
 * RESP2 uses legacy AUTH/name/SELECT directly, without requiring HELLO support.
 * RESP3/AUTO require a RESP3 reader. Business requests may queue with their own
 * original deadlines, but none is sent until setup succeeds. Setup failure
 * retires the connection; surviving calls report INITIALIZATION/NOT_SENT.
 * Options are copied as encoded wire; no credential view is retained. AUTO
 * preencoding copies auth/name twice; strict modes once. Owned setup bytes are
 * securely wiped after final ownership release, including error/close paths.
 * This does not erase caller, peer, OS or lower transport buffers. No implicit
 * TLS/DNS/reconnect; the application chooses its transport/security policy. */
XREDIS_API bool xredisClientConnectInit(xnetengine* Engine, const xnetaddr* Remote,
	uint64 Affinity, const xredisclientconfig* Config, const xredisclientinitconfig* Init,
	xredisclient** Result);

/* Shared readiness observer, resolved with xredisreadyresult on every terminal
 * outcome. It does not cancel setup: use Close for connection cancellation.
 * Raw Connect reports transport readiness with UNKNOWN protocol/no negotiation.
 * CONNECTING remains until this barrier succeeds; OPEN means Ready. */
XREDIS_API xfuture* xredisClientReady(xredisclient* Client);
XREDIS_API bool xredisClientRetain(xredisclient* Client);
XREDIS_API void xredisClientRelease(xredisclient* Client);

/* Thread-safe bounded admission; copies/encodes arguments during this call.
 * Timeout is signed milliseconds (-1 forever, 0 already due), measured from
 * API entry through encoding, submission, setup and processing. Future output
 * changes only on ACCEPTED. OVERLOADED/CLOSED are normal counted admission
 * results, not request outcomes. All outputs are disjoint from input/client.
 * Successful submission does not retain any source argument/storage.
 * Limits include abandoned sent requests and actual retained command buffers.
 * xrtFutureCancel wakes the worker without running request completion inline.
 * Cancellation wins when observed before completion; deadline equality expires.
 * Local completion does not remove a sent request's FIFO/wire obligation.
 * Future watch/continuation notifications execute on XRT completion paths:
 * application callbacks must not block a network worker. Managed connections
 * reserve HELLO/AUTH/SELECT/CLIENT SETNAME for setup. Raw Connect still permits
 * explicit controls. Raw AUTH/HELLO command storage is wiped after its final
 * send/FIFO/cancel owner releases it. */
XREDIS_API xredisadmission xredisClientCallAsync(xredisclient* Client,
	const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future);

/* Same operation/deadline, then waits for the typed outcome. Rejects calls
 * from ANY worker of the same engine (including a different affinity), since
 * blocking an engine worker is not a supported sync facade. */
XREDIS_API xredisadmission xredisClientCall(xredisclient* Client,
	const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future);

/* Nonblocking ownership transfer of one RESP3 PUSH. Empty returns false and
 * preserves Reply/ambient error. A full count/byte queue retires the connection
 * rather than silently losing pushes or shifting response correlation.
 * Ordinary replies without a sent FIFO slot are protocol errors. */
XREDIS_API bool xredisClientTakePush(xredisclient* Client, xredisreply** Reply);
XREDIS_API bool xredisClientInfo(xredisclient* Client, xredisclientinfo* Info);

/* Idempotent, allocation-free close request. Accepted requests are completed
 * with explicit statuses; physical duties remain alive until TCP closure and
 * data-bearing request/control ownership retires. Closed returns a retained
 * observer of that barrier, resolved with NULL. The current notification tail
 * may still be returning; stop/join the engine before unloading callback code.
 * Closed does not itself request close. */
XREDIS_API bool xredisClientClose(xredisclient* Client);
XREDIS_API xfuture* xredisClientClosed(xredisclient* Client);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/pool.h */
/* ========================================================================== */

#ifndef XREDIS_POOL_H
#define XREDIS_POOL_H
#if defined(XREDIS_FEATURE_XREDIS_POOL) && !defined(XREDIS_FEATURE_XREDIS_CLIENT)
#error "XREDIS_FEATURE_XREDIS_POOL requires XREDIS_FEATURE_XREDIS_CLIENT"
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL)
typedef struct xredispool xredispool;
typedef struct xredislease xredislease;
typedef enum xredispoolstate { XREDIS_POOL_OPEN, XREDIS_POOL_CLOSING, XREDIS_POOL_CLOSED } xredispoolstate;
typedef struct xredispoolconfig {
    size_t Size;
    /* Includes concurrent acquire constructors and queued acquisitions. */
    size_t MaxPending;
    size_t MaxRequests;
    size_t MaxCommandBytes;
    /* Finite maximum lease hold, including calls and application think time.
     * Default 30 s, hard maximum 24 hours. No renewal on calls. */
    int64 LeaseTimeout;
    /* Graceful Return seals admission and drains, default 5 s, max 60 s.
     * Expiry retires a still-busy connection. */
    int64 ReturnTimeout;
    size_t WorkItems;
} xredispoolconfig;
typedef struct xredispoolinfo {
    size_t Size;
    xredispoolstate State;
    size_t Members, Live, Idle, Leased, Pending;
    uint64 Requests, CommandBytes, Acquired, Rejected, Returned, Retired;
} xredispoolinfo;
/* Borrowed from the acquisition Future. Only OK has a Lease. Retain it before
 * dropping that Future. Failed acquisition performs no Redis command. */
typedef struct xredisleaseresult {
    xredisrequeststatus Status;
    xredislease* Lease;
} xredisleaseresult;
typedef struct xredisleaseclosedresult {
    xredisrequeststatus Reason;
    bool Reusable;
} xredisleaseclosedresult;

XRT_EXTERN_C_BEGIN
XREDIS_API bool xredisPoolConfigInit(xredispoolconfig* Config);
/* Fixed membership, 1..65536 distinct OPEN/ready/physically idle managed
 * clients with the same negotiated RESP version on this running Engine.
 * The caller supplies a common service/credentials/database/session profile;
 * the pool cannot infer erased credentials or server-side profile changes.
 * Existing numeric/DNS/TLS factories
 * supply members. Success holds refs and exclusively claims them; caller refs
 * are borrowed, never consumed. Failure rolls back every partial claim and
 * leaves output unchanged. Busy members return OVERLOADED. No connection is
 * created/reconnected/replayed by this core. PoolClose closes its members.
 * Do not use member clients directly while pooled; Close observers and an
 * external close remain valid. Count/argument storage is borrowed for Create.
 * Limits are hard across all members, including request constructor/wire/
 * timer/cancel tails. Result/reply storage retained by callers remains theirs.
 * Each native parser/push/work limit also remains in force. */
XREDIS_API xredisadmission xredisPoolCreate(xnetengine* Engine, uint64 Affinity,
    xredisclient* const* Clients, size_t Count, const xredispoolconfig* Config, xredispool** Pool);
XREDIS_API bool xredisPoolRetain(xredispool* Pool);
/* Caller must Close then Release; Release alone does not close a live pool. */
XREDIS_API void xredisPoolRelease(xredispool* Pool);
/* Bounded FIFO admission. Timeout (-1 forever, 0 due) includes allocation,
 * queue wait and grant. A separate finite lease hold starts at grant. Cancel
 * requests stop; every accepted acquisition resolves a typed immutable result.
 * No live member yields DISCONNECTED for fixed membership; optional automatic
 * maintenance instead waits within the original deadline through outages.
 * Output changes only on ACCEPTED.
 * Sync rejects every worker of the same engine. */
XREDIS_API xredisadmission xredisPoolAcquireAsync(xredispool* Pool, int64 Timeout, xfuture** Future);
XREDIS_API xredisadmission xredisPoolAcquire(xredispool* Pool, int64 Timeout, xfuture** Future);
XREDIS_API bool xredisPoolInfo(xredispool* Pool, xredispoolinfo* Info);
XREDIS_API bool xredisPoolClose(xredispool* Pool);
XREDIS_API xfuture* xredisPoolClosed(xredispool* Pool);
XREDIS_API bool xredisLeaseRetain(xredislease* Lease);
XREDIS_API void xredisLeaseRelease(xredislease* Lease);
/* Direct native request/Future, with no second per-call Future or wire clone.
 * Ordinary one-reply and blocking commands use the same FIFO and physical
 * obligations as the client. Effective deadline is min(call deadline, lease
 * hold deadline). Connection/mode/transaction controls require their own APIs.
 * Local cancel/timeout does not release a sent request's FIFO obligation. */
XREDIS_API xredisadmission xredisLeaseCallAsync(xredislease* Lease,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future);
XREDIS_API xredisadmission xredisLeaseCall(xredislease* Lease,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future);
/* Idempotent, nonblocking seal. Return drains within ReturnTimeout and the
 * original hold deadline; Abort retires the connection. Releasing a handle
 * alone does not return it; finite hold expiry recovers dropped observers.
 * Closed resolves only after API constructors and native request storage
 * retire. Reusable certifies the slot-release boundary; it does not promise
 * another caller has not acquired it since. Completed retained metadata pins
 * no client/pool/engine. Join the engine before unloading callback code. */
XREDIS_API bool xredisLeaseReturn(xredislease* Lease);
XREDIS_API bool xredisLeaseAbort(xredislease* Lease);
XREDIS_API xfuture* xredisLeaseClosed(xredislease* Lease);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/pool_connect.h */
/* ========================================================================== */

#ifndef XREDIS_POOL_CONNECT_H
#define XREDIS_POOL_CONNECT_H
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT) && !defined(XREDIS_FEATURE_XREDIS_POOL)
#error "XREDIS_FEATURE_XREDIS_POOL_CONNECT requires XREDIS_FEATURE_XREDIS_POOL"
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
/* Called serially on the pool worker, without a pool/client gate. Must not
 * block. Start an existing managed numeric/DNS/TLS/auth factory with Timeout
 * as its total setup limit. Success transfers ONE private client reference;
 * failure transfers nothing and leaves Client unchanged. No business commands
 * or sharing/claims are permitted before transfer. Slot is stable, Attempt is
 * a saturating per-slot counter. Choose the same service/session/RESP profile
 * on every attempt. No command from an old connection is ever replayed. */
typedef bool (*xredispoolconnectproc)(xnetengine* Engine, size_t Slot,
    uint64 Attempt, int64 Timeout, ptr Data, xredisclient** Client);
typedef void (*xredispoolconnectdrop)(ptr Data);
typedef struct xredispoolconnectconfig {
    size_t Size;
    xredispoolconnectproc Connect;
    xredispoolconnectdrop Drop;
    ptr Data;
    xredisprotocol Protocol;
    size_t MaxConnecting;
    int64 AttemptTimeout;
    int64 RetryMin, RetryMax;
} xredispoolconnectconfig;
typedef struct xredispoolconnectinfo {
    size_t Size;
    size_t Connecting, Waiting;
    uint64 Attempts, Failures, Connected;
    xredisrequeststatus LastFailure;
} xredispoolconnectinfo;
XRT_EXTERN_C_BEGIN
/* Defaults: RESP3, 4 simultaneous attempts, 5 s setup, 100 ms..10 s retry.
 * Connect is mandatory. Bounds: concurrency 1..65536 (capped by Count),
 * setup 1..60000 ms, retry 1..60000 ms, min <= max. */
XREDIS_API bool xredisPoolConnectConfigInit(xredispoolconnectconfig* Config);
/* Start/refill Count slots. No I/O/factory callback runs inline, although a
 * worker may run before this API returns. ACCEPTED consumes the Data/Drop
 * obligation; every failure preserves it and Result. Config values are copied.
 * Attempts, their physical failure tails and retry queues are bounded. Every
 * failed generation physically closes before its slot can reconnect. Backoff
 * is exponential with bounded non-cryptographic jitter, reset on readiness.
 * Acquisition deadlines include outage/retry wait; zero Live does not cause
 * DISCONNECTED in this mode. Each acquisition still has its own finite/forever
 * timeout and cancellation; it does not cancel shared maintenance.
 * PoolClose stops new attempts, closes candidates/members and retires timers/
 * callbacks before Drop and PoolClosed. Drop runs once on the pool worker,
 * outside gates, must not block and must outlive the pool. Context, resolver,
 * verifier/signer code and referenced material remain valid until Drop.
 * Maintenance timer OOM fails the pool closed; it never busy-retries OOM.
 * PoolInfo.Retired counts actual retired client generations (can exceed Count).
 * Retained closed pool metadata holds no context/client/engine reference. */
XREDIS_API xredisadmission xredisPoolConnect(xnetengine* Engine, uint64 Affinity,
    size_t Count, const xredispoolconfig* Config,
    const xredispoolconnectconfig* Connect, xredispool** Result);
XREDIS_API bool xredisPoolConnectInfo(xredispool* Pool, xredispoolconnectinfo* Info);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/transaction.h */
/* ========================================================================== */

#ifndef XREDIS_TRANSACTION_H
#define XREDIS_TRANSACTION_H

#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION) && !defined(XREDIS_FEATURE_XREDIS_CLIENT)
#error "XREDIS_FEATURE_XREDIS_TRANSACTION requires XREDIS_FEATURE_XREDIS_CLIENT"
#endif

#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
#define XREDIS_TRANSACTION_COMMANDS_DEFAULT 256u
#define XREDIS_TRANSACTION_COMMANDS_MAX 65536u

typedef struct xredistransactioncommand {
    const xbytesview* Arguments;
    size_t Count;
} xredistransactioncommand;

typedef struct xredistransactionconfig {
    size_t Size;
    size_t MaxCommands;
} xredistransactionconfig;

/* Execution certainty is independent from local timeout/cancellation and from
 * error elements inside a valid EXEC array. Redis does not roll those back. */
typedef enum xredistransactionexecution {
    XREDIS_TRANSACTION_NOT_EXECUTED = 0,
    XREDIS_TRANSACTION_MAY_HAVE_EXECUTED,
    XREDIS_TRANSACTION_EXECUTED,
    XREDIS_TRANSACTION_ABORTED
} xredistransactionexecution;

/* Owned by the returned Future; Request is the first member. Request.Reply is
 * the final EXEC array/error or a rejected MULTI reply. QueueError retains the
 * first queue error; other queue errors are counted and promptly released.
 * Queue rejection uses SERVER_ERROR, no EXEC reply, and NOT_EXECUTED after a
 * validated DISCARD. Counts describe knowledge at logical completion and do
 * not change while an abandoned operation drains. Retain replies explicitly
 * to use them after destroying the Future. Results do not retain the client. */
typedef struct xredistransactionresult {
    xredisrequestresult Request;
    xredistransactionexecution Execution;
    size_t Commands;
    size_t Queued;
    size_t QueueErrors;
    xredisreply* QueueError;
} xredistransactionresult;

XRT_EXTERN_C_BEGIN
XREDIS_API bool xredisClientTransactionConfigInit(xredistransactionconfig* Config);

/* One exclusive MULTI / queue / EXEC (or DISCARD) operation on a managed
 * connection. Raw Connect is refused. MULTI is acknowledged before business
 * commands are sent; preceding requests drain, and later requests cannot enter
 * the transaction. Input is borrowed only during admission and encoded once
 * into owned storage. Count=0 is an empty transaction. Config=NULL uses bounded
 * defaults. Client request/byte limits include transaction wire and metadata.
 * Timeout includes admission/encoding/worker/setup/transaction/cleanup time.
 * Before EXEC is accepted, cancellation/timeout suppresses EXEC and drains
 * DISCARD; afterwards execution may be uncertain. DrainTimeout still bounds
 * physical cleanup. Cleanup rejection/malformed or unknown EXEC replies retire
 * the connection; a recognized EXECABORT confirms abort and permits reuse.
 * No automatic replay, rollback, WATCH/read/decision session or retry is added.
 * Connection/mode controls are not transaction payload commands. Unknown module
 * commands must obey Redis transactional queue/reply semantics. Output changes
 * only on ACCEPTED. Future cancellation returns a typed immutable result. */
XREDIS_API xredisadmission xredisClientTransactionAsync(xredisclient* Client,
    const xredistransactioncommand* Commands, size_t Count,
    const xredistransactionconfig* Config, int64 Timeout, xfuture** Future);

/* Same deadline and outcome, then waits. Rejects every worker of this engine. */
XREDIS_API xredisadmission xredisClientTransaction(xredisclient* Client,
    const xredistransactioncommand* Commands, size_t Count,
    const xredistransactionconfig* Config, int64 Timeout, xfuture** Future);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/watch.h */
/* ========================================================================== */

#ifndef XREDIS_WATCH_H
#define XREDIS_WATCH_H

#if defined(XREDIS_FEATURE_XREDIS_WATCH) && !defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
#error "XREDIS_FEATURE_XREDIS_WATCH requires XREDIS_FEATURE_XREDIS_TRANSACTION"
#endif
#if defined(XREDIS_FEATURE_XREDIS_WATCH)
typedef struct xrediswatch xrediswatch;
typedef struct xrediswatchconfig {
    size_t Size;
    /* Positive total lifetime, including admission, application think time,
     * reads and commit. No renewal; at most 24 hours. Default 5000 ms. */
    int64 Timeout;
    /* Separate bounded UNWATCH request deadline, default 5000 ms (max 60 s).
     * Physical retirement also includes the client's existing drain grace. */
    int64 CleanupTimeout;
    size_t MaxKeys;
    size_t MaxWireBytes;
    xredistransactionconfig Transaction;
} xrediswatchconfig;

/* Owned by the start Future. Watch is borrowed on EVERY accepted outcome;
 * only OK permits commands. On failure it still exposes the physical Closed
 * barrier. Retain it before destroying the Future. A retained completed result
 * does not pin the engine. Request.Reply is the WATCH response, not EXEC. */
typedef struct xrediswatchresult {
    xredisrequestresult Request;
    xrediswatch* Watch;
} xrediswatchresult;
/* Owned by Closed's Future. Reason is the scope's first terminal reason;
 * Reusable certifies physical idle plus a validated clearing boundary. */
typedef struct xrediswatchclosedresult {
    xredisrequeststatus Reason;
    bool Reusable;
} xrediswatchclosedresult;

XRT_EXTERN_C_BEGIN
XREDIS_API bool xredisWatchConfigInit(xrediswatchconfig* Config);
/* Claims an OPEN, ready, idle managed client. A busy/not-ready/already claimed
 * client returns OVERLOADED. Ordinary ClientCall/Transaction admissions are
 * refused while claimed. Keys are binary and copied only during admission.
 * Count must be 1..MaxKeys (default 256, hard limit 65536); default wire 64 KiB.
 * Config is copied; output changes only on ACCEPTED. No implicit retry.
 * Cancelling a pending start Future abandons the scope. */
XREDIS_API xredisadmission xredisClientWatchAsync(xredisclient* Client,
    const xbytesview* Keys, size_t Count, const xrediswatchconfig* Config, xfuture** Future);
XREDIS_API xredisadmission xredisClientWatch(xredisclient* Client,
    const xbytesview* Keys, size_t Count, const xrediswatchconfig* Config, xfuture** Future);
XREDIS_API bool xredisWatchRetain(xrediswatch* Watch);
XREDIS_API void xredisWatchRelease(xrediswatch* Watch);
/* Ordinary one-reply reads/commands within this connection's WATCH scope.
 * Connection/mode/transaction controls are reserved. The effective deadline
 * is min(call entry + Timeout, scope deadline). Release alone does not abort.
 * Application writes have their ordinary Redis effects and are not rolled back. */
XREDIS_API xredisadmission xredisWatchCallAsync(xrediswatch* Watch,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future);
XREDIS_API xredisadmission xredisWatchCall(xrediswatch* Watch,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future);
/* Exactly one accepted commit seals the scope against new calls/commits.
 * Earlier reads drain before MULTI. Result is xredistransactionresult; a null
 * EXEC is ABORTED, never an automatic retry. Logical completion precedes
 * physical Closed. Cancelling an accepted commit may retire the connection. */
XREDIS_API xredisadmission xredisWatchCommitAsync(xrediswatch* Watch,
    const xredistransactioncommand* Commands, size_t Count, int64 Timeout, xfuture** Future);
XREDIS_API xredisadmission xredisWatchCommit(xrediswatch* Watch,
    const xredistransactioncommand* Commands, size_t Count, int64 Timeout, xfuture** Future);
/* Idempotent nonblocking abandonment. Idle scopes send UNWATCH; abandoning
 * while physical requests remain may retire the connection. No dirty reuse.
 * Closed is a retained observer, not a request to abort. It resolves only
 * after scoped request storage, timers, notifications and the claim retire.
 * Sync facades reject every worker of the client's engine. Stop/join the
 * engine before unloading code, including the final notification return tail. */
XREDIS_API bool xredisWatchAbort(xrediswatch* Watch);
XREDIS_API xfuture* xredisWatchClosed(xrediswatch* Watch);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/pool_watch.h */
/* ========================================================================== */

#ifndef XREDIS_POOL_WATCH_H
#define XREDIS_POOL_WATCH_H
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH) && (!defined(XREDIS_FEATURE_XREDIS_POOL) || !defined(XREDIS_FEATURE_XREDIS_WATCH))
#error "XREDIS_FEATURE_XREDIS_POOL_WATCH requires pool and watch"
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
XRT_EXTERN_C_BEGIN
/* Nested WATCH scope on a physically idle lease. Busy returns OVERLOADED;
 * admission failure restores the unused lease. Acceptance consumes/seals this
 * lease for the scope; ordinary/transaction/second-WATCH admissions cannot
 * enter it. Result is the native xrediswatchresult on every accepted outcome,
 * including a failed start's physical Closed observer. All scope requests and
 * transaction/control metadata retain the pool's hard aggregate quota. Both
 * total scope/call deadlines are capped at the original lease hold.
 * Return/Abort/hold expiry/PoolClose cannot release a live nested owner. Return
 * requests bounded scope abandonment (UNWATCH when clean); grace expiry or
 * uncertain/busy cleanup retires the client. Explicitly Return the lease after
 * scope Closed. Retained completed scope/lease metadata pins no pool/engine.
 * Same input/output/ownership and sync-worker rules as standalone WATCH. */
XREDIS_API xredisadmission xredisLeaseWatchAsync(xredislease* Lease,
    const xbytesview* Keys, size_t Count, const xrediswatchconfig* Config, xfuture** Future);
XREDIS_API xredisadmission xredisLeaseWatch(xredislease* Lease,
    const xbytesview* Keys, size_t Count, const xrediswatchconfig* Config, xfuture** Future);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/pool_transaction.h */
/* ========================================================================== */

#ifndef XREDIS_POOL_TRANSACTION_H
#define XREDIS_POOL_TRANSACTION_H
#if defined(XREDIS_FEATURE_XREDIS_POOL_TRANSACTION) && (!defined(XREDIS_FEATURE_XREDIS_POOL) || !defined(XREDIS_FEATURE_XREDIS_TRANSACTION))
#error "XREDIS_FEATURE_XREDIS_POOL_TRANSACTION requires pool and transaction"
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_TRANSACTION)
XRT_EXTERN_C_BEGIN
/* Direct staged transaction on this lease. Accepted enqueue seals the lease
 * against further calls/transactions; earlier leased calls drain before MULTI.
 * Config/commands are borrowed only during admission. Output changes only on
 * ACCEPTED. Native xredistransactionresult preserves execution uncertainty and
 * queue/runtime errors. Group request/byte quotas include wire/control/state.
 * Deadline is capped at the original lease hold. Return the lease explicitly;
 * its physical Closed barrier waits for actual DISCARD/EXEC/drain/retirement.
 * Return never authorizes replay. Sync rejects all workers of this engine. */
XREDIS_API xredisadmission xredisLeaseTransactionAsync(xredislease* Lease,
    const xredistransactioncommand* Commands, size_t Count, const xredistransactionconfig* Config,
    int64 Timeout, xfuture** Future);
XREDIS_API xredisadmission xredisLeaseTransaction(xredislease* Lease,
    const xredistransactioncommand* Commands, size_t Count, const xredistransactionconfig* Config,
    int64 Timeout, xfuture** Future);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/dial.h */
/* ========================================================================== */

#ifndef XREDIS_DIAL_H
#define XREDIS_DIAL_H

#if defined(XREDIS_FEATURE_XREDIS_DIAL) && (!defined(XREDIS_FEATURE_XREDIS_CLIENT) || !defined(XRT_FEATURE_NET_TCP_DIAL))
#error "XREDIS_FEATURE_XREDIS_DIAL requires client and net_tcp_dial"
#endif

#if defined(XREDIS_FEATURE_XREDIS_DIAL)
#define XREDIS_DIAL_ADDRESSES_MAX 64u
#define XREDIS_DIAL_HOST_MAX 65535u

/* Network policy only; the client Config.Stream remains the sole TCP limits.
 * Timeout=0 disables the additional Dial timer, -1 is infinite. Managed Init's
 * original deadline still spans host copying, DNS, candidate race and HELLO.
 * Default dual stack, 30000 ms, 250 ms fallback, eight candidates, 1024 host
 * bytes. Maximum candidate/address count 64; maximum borrowed host span 65535.
 * No IDNA conversion, TLS, reconnect or application-command replay is implied. */
typedef struct xredisclientdialconfig {
	size_t Size;
	xnetfamily Family;
	int64 Timeout;
	int64 FallbackDelay;
	uint32 MaxAttempts;
	size_t MaxHostBytes;
} xredisclientdialconfig;

XRT_EXTERN_C_BEGIN
XREDIS_API bool xredisClientDialConfigInit(xredisclientdialconfig* Config);

/* Async hostname setup on the existing Engine/Resolver. Host is a bounded,
 * nonempty readable view without embedded NUL; copied before return. Resolver
 * is application-owned and must outlive Closed (then retire/join it separately).
 * It is shared; closing a client does not close the service or stop an OS
 * lookup already running there. All client callback/data ownership retires
 * through the client Close barrier. NULL Init selects strict RESP3 defaults.
 * Calls can queue, expire/cancel through DNS and TCP setup. Result unchanged on
 * failure. Successful submission owns one Client reference: Close then Release.
 * Core XRT supplies bounded family interleaving, fallback and losing-socket
 * retirement before transfer. No business operation is retried or replayed. */
XREDIS_API bool xredisClientDial(xnetengine* Engine, xnetresolver* Resolver,
	xstrview Host, uint16 Port, uint64 Affinity, const xredisclientconfig* Config,
	const xredisclientinitconfig* Init, const xredisclientdialconfig* Dial,
	xredisclient** Result);

/* Already resolved immutable endpoints; no Resolver is consulted. Each port
 * is preserved. At most 64 entries; Family filters candidates and MaxAttempts
 * bounds selected attempts. Retains the list only until native Dial admission;
 * caller may release its reference immediately. Output storage must be disjoint
 * from descriptors and borrowed host/credential/name/endpoint storage. */
XREDIS_API bool xredisClientDialAddresses(xnetengine* Engine,
	xnetaddrlist* Addresses, uint64 Affinity, const xredisclientconfig* Config,
	const xredisclientinitconfig* Init, const xredisclientdialconfig* Dial,
	xredisclient** Result);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/tls.h */
/* ========================================================================== */

#ifndef XREDIS_TLS_H
#define XREDIS_TLS_H
#if defined(XREDIS_FEATURE_XREDIS_TLS)
#endif

#if defined(XREDIS_FEATURE_XREDIS_TLS) && (!defined(XREDIS_FEATURE_XREDIS_DIAL) || !defined(XRT_FEATURE_TLS_STREAM) || !defined(XRT_FEATURE_TLS_CLIENT_RESUME))
#error "XREDIS_FEATURE_XREDIS_TLS requires dial, TLS stream and verified/resumable TLS client"
#endif

#if defined(XREDIS_FEATURE_XREDIS_TLS) && (!defined(XRT_FEATURE_TLS_RECORD_AES) || !defined(XRT_FEATURE_TLS_RECORD_CHACHA) || \
	!defined(XRT_FEATURE_TLS_SCHEDULE_SHA256) || !defined(XRT_FEATURE_TLS_SCHEDULE_SHA384) || \
	!defined(XRT_FEATURE_TLS_KEY_EXCHANGE_X25519) || !defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P256) || !defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P384))
#error "XREDIS_FEATURE_XREDIS_TLS requires its declared AES/ChaCha, SHA256/SHA384 and X25519/P256/P384 backends"
#endif

#if defined(XREDIS_FEATURE_XREDIS_TLS)
/* All views are borrowed only during the factory. Native TLS owns a snapshot
 * of names/ALPN and shared Context/Verifier/Resume references before return.
 * Supply a Verifier (or explicit ResumeOnly with a valid Resume); no implicit
 * insecure verification mode. Verifier callback code/context follow XRT's
 * shared-verifier lifetime, including Engine join after client Closed.
 * Defaults: native TLS policy/stream limits, normal Dial defaults, 64 KiB
 * metadata, 64 KiB plaintext write work per client turn, infer DNS SNI/name.
 * Numeric hosts infer VerifyName only; prepared endpoints need an explicit
 * ServerName or VerifyName. Init's original deadline spans TLS too. */
typedef struct xredisclienttlsconfig {
	size_t Size;
	xtlsclientconfig Session;
	xtlsstreamconfig Stream;
	xredisclientdialconfig Dial;
	size_t MaxMetadataBytes;
	size_t WriteBytes;
	bool ServerNameFromHost;
} xredisclienttlsconfig;

XRT_EXTERN_C_BEGIN
XREDIS_API bool xredisClientTlsConfigInit(xredisclienttlsconfig* Config);
/* TLS is required. Result unchanged on rejection; no cleartext fallback or
 * operation replay. Typed readiness is published only after TLS + Redis setup.
 * Resolver is application owned through Closed, and joined separately. */
XREDIS_API bool xredisClientTlsDial(xnetengine* Engine, xnetresolver* Resolver,
	xstrview Host, uint16 Port, uint64 Affinity, const xredisclientconfig* Config,
	const xredisclientinitconfig* Init, const xredisclienttlsconfig* TLS, xredisclient** Result);
XREDIS_API bool xredisClientTlsDialAddresses(xnetengine* Engine, xnetaddrlist* Addresses,
	uint64 Affinity, const xredisclientconfig* Config, const xredisclientinitconfig* Init,
	const xredisclienttlsconfig* TLS, xredisclient** Result);
/* Numeric endpoint convenience; TLS identity must be explicit in TLS. */
XREDIS_API bool xredisClientTlsConnect(xnetengine* Engine, const xnetaddr* Remote,
	uint64 Affinity, const xredisclientconfig* Config, const xredisclientinitconfig* Init,
	const xredisclienttlsconfig* TLS, xredisclient** Result);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/tls_auth.h */
/* ========================================================================== */

#ifndef XREDIS_TLS_AUTH_H
#define XREDIS_TLS_AUTH_H
#if defined(XREDIS_FEATURE_XREDIS_TLS_AUTH)
#endif

#if defined(XREDIS_FEATURE_XREDIS_TLS_AUTH) && (!defined(XREDIS_FEATURE_XREDIS_TLS) || !defined(XRT_FEATURE_TLS_CLIENT_AUTH))
#error "XREDIS_FEATURE_XREDIS_TLS_AUTH requires Redis TLS and native TLS client authentication"
#endif

#if defined(XREDIS_FEATURE_XREDIS_TLS_AUTH)
XRT_EXTERN_C_BEGIN
/* Configs/views are borrowed only during admission. The native session retains
 * the immutable identity before DNS or any worker callback; no caller identity
 * or auth-config borrow escapes. Signers run on the connection worker and must
 * not block it or reenter/destroy the native session. The identity's final
 * Release follows its existing last-reference thread contract. Resolver/code/
 * verifier-context lifetime and total deadlines follow the TLS Dial contract.
 * Auth is mandatory; defaults come from xrtTlsClientAuthConfigInitV1. With
 * RequireCertificate=true, no compatible server request fails readiness.
 * With false the caller explicitly permits a connection without local proof.
 * Full certificate handshakes only: explicit resumes are rejected and tickets
 * are not retained until identity-bound resumption is implemented. A signing
 * backend is selected separately; this module does not add private-key code.
 * Readiness includes the configured Redis initialization, so TLS 1.3 local
 * proof queueing alone cannot publish successful managed readiness. */
XREDIS_API bool xredisClientTlsAuthDialV1(xnetengine* Engine, xnetresolver* Resolver,
    xstrview Host, uint16 Port, uint64 Affinity, const xredisclientconfig* Config,
    const xredisclientinitconfig* Init, const xredisclienttlsconfig* TLS,
    const xtlsclientauthconfigv1* Auth, xredisclient** Result);
XREDIS_API bool xredisClientTlsAuthDialAddressesV1(xnetengine* Engine, xnetaddrlist* Addresses,
    uint64 Affinity, const xredisclientconfig* Config, const xredisclientinitconfig* Init,
    const xredisclienttlsconfig* TLS, const xtlsclientauthconfigv1* Auth, xredisclient** Result);
XREDIS_API bool xredisClientTlsAuthConnectV1(xnetengine* Engine, const xnetaddr* Remote,
    uint64 Affinity, const xredisclientconfig* Config, const xredisclientinitconfig* Init,
    const xredisclienttlsconfig* TLS, const xtlsclientauthconfigv1* Auth, xredisclient** Result);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/tracking.h */
/* ========================================================================== */

#ifndef XREDIS_TRACKING_H
#define XREDIS_TRACKING_H
#if defined(XREDIS_FEATURE_XREDIS_TRACKING) && !defined(XREDIS_FEATURE_XREDIS_CLIENT)
#error "Redis Tracking requires the existing Client"
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
#define XREDIS_TRACKING_PREFIXES_MAX 64u
#define XREDIS_TRACKING_BCAST 0x01u
#define XREDIS_TRACKING_OPTIN 0x02u
#define XREDIS_TRACKING_OPTOUT 0x04u
#define XREDIS_TRACKING_NOLOOP 0x08u
#define XREDIS_TRACKING_REDIRECT 0x10u
#define XREDIS_TRACKING_OBSERVATION_BYTES_DEFAULT ((size_t)65536u)
typedef struct xredistrackingconfig {
    size_t Size;bool Enabled;uint32 Flags;int64 Redirect;
    const xbytesview* Prefixes;size_t PrefixCount;
} xredistrackingconfig;
/* Request prefix and ownership match ordinary Client results. Sequence is
 * per Client, in actual received-frame order, not completion/collection order.
 * Zero means no reply was published. Counter exhaustion closes without wrap.
 * No Client/Engine reference survives in an observed result. */
typedef struct xredistrackingresult {xredisrequestresult Request;uint64 Sequence;} xredistrackingresult;
/* Query is the ordinary result prefix. Control independently preserves the
 * CLIENT CACHING outcome known at logical completion. Both replies/causes
 * belong to the Future; neither result retains a Client/Engine. */
typedef struct xrediscachingresult {
    xredistrackingresult Query,Control;
    bool ControlReceived;
} xrediscachingresult;
typedef struct xredistrackingpush {xredisreply* Reply;uint64 Sequence;} xredistrackingpush;
typedef struct xredistrackinginfo {size_t Size,Bytes,Reserved;uint64 Sequence,Rejected;bool Enabled,Preparing;} xredistrackinginfo;
typedef enum xredistrackingeventkind {XREDIS_TRACKING_KEYS=0,XREDIS_TRACKING_FLUSH,XREDIS_TRACKING_REDIRECT_BROKEN} xredistrackingeventkind;
typedef enum xredistrackingstatus {XREDIS_TRACKING_ERROR=-2,XREDIS_TRACKING_MALFORMED=-1,XREDIS_TRACKING_OTHER=0,XREDIS_TRACKING_EVENT=1} xredistrackingstatus;
/* Borrows Reply. Walk exactly Keys nodes using xredisReplyGet/Next, starting
 * at FirstKey: linear iteration, binary views, no allocation/copy. Retain the
 * original Reply explicitly to keep these views after its owning result. */
typedef struct xredistrackingevent {
    const xredisreply* Reply;xredistrackingeventkind Kind;size_t FirstKey,Keys;uint64 Redirect;
} xredistrackingevent;
XRT_EXTERN_C_BEGIN
/* Thread-safe one-time opt-in. MaxBytes=0 uses 64 KiB. One bounded allocation
 * (header + MaxPushes uint64 slots), retained until final Client release.
 * ACCEPTED is idempotent; concurrent installation/insufficient budget is normal
 * OVERLOADED. No command or tracking mode is sent here. Existing queued pushes
 * have sequence0 and precede all newly admitted observed calls. This activation
 * starts an observation epoch with an empty application cache. */
XREDIS_API xredisadmission xredisClientTrackingObserve(xredisclient* Client,size_t MaxBytes);
XREDIS_API bool xredisClientTrackingInfo(xredisclient* Client,xredistrackinginfo* Info);
/* Requires Observe. Same command/deadline/admission/cancel/drain semantics as
 * ordinary calls. One enlarged result allocation, no extra Future or payload
 * copy. Other callers may use ordinary requests; those results carry no order.
 * Sync form rejects every worker of this Engine before submitting. */
XREDIS_API xredisadmission xredisClientTrackingCallAsync(xredisclient* Client,
    const xbytesview* Arguments,size_t Count,int64 Timeout,xfuture** Future);
XREDIS_API xredisadmission xredisClientTrackingCall(xredisclient* Client,
    const xbytesview* Arguments,size_t Count,int64 Timeout,xfuture** Future);
/* Requires Observe and a managed connection. Copies CLIENT CACHING YES/NO
 * and one ordinary query into ONE adjacent wire group/request/Future. Other
 * producers cannot interleave commands; this is not a rollback transaction.
 * Query may execute even if Control is SERVER_ERROR. Only two successful
 * results authorize caching; invalidations still require receive-order checks.
 * Known connection/transaction/multiple-reply controls are refused as queries.
 * MaxCommandBytes/Total include both frames and aligned private group state.
 * A sent cancel/timeout is MAY_HAVE_EXECUTED even if only the prefix was sent:
 * the original group remains a bounded FIFO drain obligation, with no replay.
 * ControlReceived and both results are immutable snapshots, not later updates.
 * Timeout covers construction and both replies. Sync rejects all Engine workers. */
XREDIS_API xredisadmission xredisClientCachingCallAsync(xredisclient* Client,bool Caching,
    const xbytesview* Arguments,size_t Count,int64 Timeout,xfuture** Future);
XREDIS_API xredisadmission xredisClientCachingCall(xredisclient* Client,bool Caching,
    const xbytesview* Arguments,size_t Count,int64 Timeout,xfuture** Future);
/* Transfers one reply reference. Same push queue/credits as ordinary TakePush;
 * do not mix consumers when ordered invalidation handling is required. Empty
 * preserves output/error. Values survive Client/Engine destruction. */
XREDIS_API bool xredisClientTrackingTake(xredisclient* Client,xredistrackingpush* Push);
XREDIS_API bool xredisTrackingConfigInit(xredistrackingconfig* Config);
/* Encodes CLIENT TRACKING, including binary PREFIX, REDIRECT, BCAST, OPTIN,
 * OPTOUT and NOLOOP. Requires Observe; uses the original entry deadline.
 * Input is copied before return. Direct invalidations require managed RESP3
 * (AUTO must have selected3); RESP2 senders need positive REDIRECT. Success
 * validates a simple OK; unexpected non-error replies close as PROTOCOL.
 * The server reply/status remains explicit:
 * a sent timeout can leave mode uncertain. No replay or hidden local cache.
 * Use CachingCall for adjacent OPTIN/OPTOUT queries shared by producers;
 * two separately submitted commands do not form an atomic group. */
XREDIS_API xredisadmission xredisClientTrackingConfigureAsync(xredisclient* Client,
    const xredistrackingconfig* Config,int64 Timeout,xfuture** Future);
/* Fully validate recognized RESP3 invalidations/redirection failure, or the
 * RESP2 __redis__:invalidate Pub/Sub envelope. Null invalidation means flush.
 * OTHER/malformed preserve output and ambient error. Invalid API use reports
 * ERROR; output cannot overlap any Reply allocation. Runtime receiving of RESP2
 * Pub/Sub still requires a dedicated subscribed connection implementation. */
XREDIS_API xredistrackingstatus xredisTrackingEventRead(const xredisreply* Reply,xredistrackingevent* Event);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/pubsub.h */
/* ========================================================================== */

#ifndef XREDIS_PUBSUB_H
#define XREDIS_PUBSUB_H
#if defined(XREDIS_FEATURE_XREDIS_PUBSUB) && \
    (!defined(XREDIS_FEATURE_XREDIS_COMMAND) || !defined(XREDIS_FEATURE_XREDIS_REPLY))
#error "Pub/Sub wire operations require command and reply"
#endif
#if defined(XREDIS_FEATURE_XREDIS_PUBSUB)
#define XREDIS_PUBSUB_NAMES_MAX 1024u
#define XREDIS_PUBSUB_NAME_ABSENT 0x01u
#define XREDIS_PUBSUB_STRUCTURED 0x02u
typedef enum xredispubsubcommand {
    XREDIS_PUBSUB_CMD_SUBSCRIBE=0,XREDIS_PUBSUB_CMD_PSUBSCRIBE,XREDIS_PUBSUB_CMD_SSUBSCRIBE,
    XREDIS_PUBSUB_CMD_UNSUBSCRIBE,XREDIS_PUBSUB_CMD_PUNSUBSCRIBE,XREDIS_PUBSUB_CMD_SUNSUBSCRIBE,
    XREDIS_PUBSUB_CMD_PING
} xredispubsubcommand;
typedef enum xredispubsubkind {
    XREDIS_PUBSUB_MESSAGE=0,XREDIS_PUBSUB_PMESSAGE,XREDIS_PUBSUB_SMESSAGE,
    XREDIS_PUBSUB_SUBSCRIBE,XREDIS_PUBSUB_PSUBSCRIBE,XREDIS_PUBSUB_SSUBSCRIBE,
    XREDIS_PUBSUB_UNSUBSCRIBE,XREDIS_PUBSUB_PUNSUBSCRIBE,XREDIS_PUBSUB_SUNSUBSCRIBE,XREDIS_PUBSUB_PONG
} xredispubsubkind;
typedef enum xredispubsubstatus {
    XREDIS_PUBSUB_ERROR=-2,XREDIS_PUBSUB_MALFORMED=-1,XREDIS_PUBSUB_OTHER=0,XREDIS_PUBSUB_EVENT=1
} xredispubsubstatus;
typedef enum xredispubsuberror {
    XREDIS_PUBSUB_ARGUMENT=4001,XREDIS_PUBSUB_LIMIT,XREDIS_PUBSUB_OVERLAP,XREDIS_PUBSUB_CAPACITY
} xredispubsuberror;
/* Borrows one immutable Reply, including attributes. Copying this structure
 * does not retain it. Retain Reply explicitly before dropping its owner.
 * Envelope is the actual ARRAY/PUSH (or scalar for PongRead), not negotiated
 * protocol. ACK Subscriptions is the reported count: regular channel+pattern
 * counts and shard counts are separate server domains, not a client total.
 * Pattern ACKs use Pattern; regular/shard ACKs use Channel.
 * NAME_ABSENT denotes an unsubscribe-all ACK with a null name, distinct from
 * a subscribed empty binary name; it does not imply global count zero.
 * PayloadNode is NONE for ACKs. STRUCTURED is reserved for the legacy RESP2
 * __redis__:invalidate message's fully validated array/null payload. In that
 * case Payload is empty; inspect PayloadNode or use TrackingEventRead.
 * Normal payload views are binary, including empty/NUL/CRLF, with no scan/copy. */
typedef struct xredispubsubevent {
    const xredisreply* Reply;xredispubsubkind Kind;xredisreplytype Envelope;uint32 Flags;
    xbytesview Channel,Pattern,Payload;uint64 Subscriptions;size_t PayloadNode;
} xredispubsubevent;
XRT_EXTERN_C_BEGIN
/* Measure/write SUBSCRIBE/PSUBSCRIBE/SSUBSCRIBE, their unsubscribe variants,
 * or PING. Subscribe needs 1..1024 names, unsubscribe accepts 0..1024 (zero
 * means all in that family), PING accepts 0 or 1 binary payload. Inputs are
 * borrowed for this call only. No heap, descriptor copy, payload scan or NUL.
 * No connection mode, subscription state or command admission is performed.
 * All validation precedes writes; outputs preserve their values on failure
 * and cannot alias names/descriptors/each other. Only the actual encoded span
 * is output. Name span errors use existing command argument/overflow codes. */
XREDIS_API bool xredisPubSubCommandSize(xredispubsubcommand Command,
    const xbytesview* Names,size_t Count,size_t* Required);
XREDIS_API bool xredisPubSubCommandWrite(ptr Output,size_t Capacity,xredispubsubcommand Command,
    const xbytesview* Names,size_t Count,size_t* Written);
/* Decode complete RESP2 arrays or RESP3 pushes: all three message families,
 * all six ACKs and aggregate pong. OTHER/MALFORMED preserve Output and ambient
 * error; invalid API use reports ERROR. Output cannot overlap any Reply storage.
 * This is a view decoder, not a subscribed-connection runtime or ACK correlator.
 * Scalar PING replies require PongRead with the expected payload context. */
XREDIS_API xredispubsubstatus xredisPubSubEventRead(const xredisreply* Reply,xredispubsubevent* Output);
/* Call when a PING response is expected. NULL Expected means no PING argument;
 * non-NULL includes an explicit empty argument. Match exact binary payload in
 * RESP2 aggregate pong or an ordinary bulk echo (RESP3 or unsubscribed RESP2);
 * no-argument scalar is simple PONG.
 * Other Pub/Sub events, errors and unrecognized aggregates return OTHER for
 * separate routing; bad recognized pong/scalar responses are MALFORMED.
 * This does not correlate arbitrary business replies. Expected/output cannot
 * overlap. No allocation/copy; the same ownership/output rules apply. */
XREDIS_API xredispubsubstatus xredisPubSubPongRead(const xredisreply* Reply,
    const xbytesview* Expected,xredispubsubevent* Output);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/subscriber.h */
/* ========================================================================== */

#ifndef XREDIS_SUBSCRIBER_H
#define XREDIS_SUBSCRIBER_H
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER) && \
    (!defined(XREDIS_FEATURE_XREDIS_CLIENT) || !defined(XREDIS_FEATURE_XREDIS_PUBSUB) || !defined(XRT_FEATURE_HASH64))
#error "Subscriber requires client, Pub/Sub wire and hash64"
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
typedef struct xredissubscriberconfig {
    size_t Size;
    size_t MaxSubscriptions;
    size_t MaxNameBytes;
} xredissubscriberconfig;

/* First member is the usual immutable request outcome. Reply is the final ACK,
 * PONG or server error, not a retained vector of every ACK. Acknowledged counts
 * validated ACKs at logical completion, including duplicates/absent-name ACKs.
 * Counts are confirmed server state at that instant; later draining cannot
 * change this result. Cancellation after send can still change subscriptions. */
typedef struct xredissubscriberresult {
    xredisrequestresult Request;
    size_t Acknowledged;
    uint64 Subscriptions,ShardSubscriptions;
} xredissubscriberresult;

typedef struct xredissubscriberinfo {
    size_t Size;
    size_t Entries,NameBytes,RegistryBytes;
    uint64 Subscriptions,ShardSubscriptions;
    uint64 Acknowledgements,Messages,Sidebands;
} xredissubscriberinfo;

XRT_EXTERN_C_BEGIN
XREDIS_API bool xredisSubscriberConfigInit(xredissubscriberconfig* Config);
/* Irreversibly dedicate an idle, ready Client to Pub/Sub until Close. Existing
 * numeric/DNS/TLS factories, readiness, refs and physical close still apply.
 * Active requests, queued pushes, exclusive owners or Tracking observation
 * prevent adoption. Both admission checks and publication are under Gate.
 * No ordinary calls/transactions/session claims are admitted after adoption.
 * NULL Config defaults to4096 entries/1MiB name bytes. Entries bound confirmed
 * AND reserved distinct (family,binary name) keys; name bytes exclude fixed
 * entry headers and the cold bucket array, both bounded by MaxSubscriptions.
 * Registry allocation/limit failure before send is a typed NOT_SENT outcome. */
XREDIS_API xredisadmission xredisSubscriberStart(xredisclient* Client,const xredissubscriberconfig* Config);
/* Bounded pipelined controls using the same request/deadline/cancellation and
 * physical wire/FIFO obligations as Client. Inputs are encoded/copied during
 * admission; subscribe entries additionally own one persistent name copy.
 * All ACK names, kinds and counts are checked against confirmed state. Empty
 * unsubscribe-all ACKs and unordered unsubscribe-all families are correlated.
 * Completion Value is xredissubscriberresult. No implicit replay/reconnect.
 * Messages and unrelated RESP3 pushes use ClientTakePush (owned Reply transfer)
 * with the existing MaxPushes/MaxPushBytes bounds. Overflow retires the Client,
 * never silently drops a message. Retained replies survive all Client owners.
 * Sync rejects every worker of the same Engine. Callbacks must not block it. */
XREDIS_API xredisadmission xredisSubscriberCommandAsync(xredisclient* Client,xredispubsubcommand Command,
    const xbytesview* Names,size_t Count,int64 Timeout,xfuture** Future);
XREDIS_API xredisadmission xredisSubscriberCommand(xredisclient* Client,xredispubsubcommand Command,
    const xbytesview* Names,size_t Count,int64 Timeout,xfuture** Future);
XREDIS_API bool xredisSubscriberInfo(xredisclient* Client,xredissubscriberinfo* Info);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/subscriber_notify.h */
/* ========================================================================== */

#ifndef XREDIS_SUBSCRIBER_NOTIFY_H
#define XREDIS_SUBSCRIBER_NOTIFY_H
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY) && !defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
#error "Subscriber notification requires the native subscriber"
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY)
XRT_EXTERN_C_BEGIN
/* Nonblocking shared readiness observer, not a receive reservation. ACCEPTED
 * returns one owned Future reference. Wait/Watch it, destroy that reference,
 * then recheck the delivery queue. Its only terminal value is RESOLVED/NULL;
 * a competing consumer can have drained the queue by the time it is observed.
 * One pending epoch is shared by all observers; rearming an empty queue creates
 * at most one replacement outside Gate. Concurrent epoch construction returns
 * OVERLOADED, and allocation failure returns ERROR; both preserve Future.
 * CLOSED means physical close AND an empty queue; queued messages remain
 * readable after close. Physical close wakes every pending epoch. Retaining an
 * observer does not keep Client/Engine alive after close. FutureCancel is not
 * acted on by this producer; use your own cancel token with Core's cancelable
 * wait, or detach your Watch. Never block an Engine worker in a wait/callback.
 * Pending Watch callbacks run on the owning IO worker. Newly constructed ready
 * epochs resolve before publication, so no user callback runs under Gate or on
 * a constructor thread. Ready WatchAdd follows Core's immediate-ready contract.
 * Drain-before-arm races cannot strand available messages. Success transfers
 * no message and retains no caller storage. No notification is allocated on
 * the producer path. Outputs must be disjoint, caller-owned writable objects. */
XREDIS_API xredisadmission xredisSubscriberReadable(xredisclient* Client,xfuture** Future);
/* Transfer up to Capacity owned reply references in one Gate acquisition.
 * Only the actual prefix is written; each reference must later be released.
 * Successful transfer sets Taken >0. Empty returns false and preserves all
 * output bytes and ambient error, including after physical close. Invalid
 * arguments report an error and change no outputs/queue. Capacity is positive,
 * Replies and Taken are disjoint and cannot overlap Client/mode storage.
 * Shares FIFO delivery with ClientTakePush; neither path reserves messages for
 * a particular readiness observer. No allocation, retain, payload copy or scan. */
XREDIS_API bool xredisSubscriberTakeBatch(xredisclient* Client,xredisreply** Replies,size_t Capacity,size_t* Taken);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/subscriptions.h */
/* ========================================================================== */

#ifndef XREDIS_SUBSCRIPTIONS_H
#define XREDIS_SUBSCRIPTIONS_H
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS) && !defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
#error "Subscription recovery requires the native subscriber"
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS)
typedef struct xredissubscriptions xredissubscriptions;
typedef struct xredissubscriptionsnapshot xredissubscriptionsnapshot;
typedef struct xredissubscriptionreplay xredissubscriptionreplay;
typedef struct xredissubscriptionsconfig {size_t Size,MaxSubscriptions,MaxNameBytes,MaxChangeBytes;} xredissubscriptionsconfig;
typedef struct xredissubscriptionsinfo {
    size_t Size;uint64 Revision;size_t Entries,NameBytes,SnapshotBytes,ReferencedEntryBytes;bool Preparing;
} xredissubscriptionsinfo;
typedef struct xredissubscriptionview {xredispubsubcommand Family;xbytesview Name;} xredissubscriptionview;
typedef enum xredissubscriptionstatus {
    XREDIS_SUBSCRIPTIONS_PENDING=0,XREDIS_SUBSCRIPTIONS_OK,XREDIS_SUBSCRIPTIONS_STALE,
    XREDIS_SUBSCRIPTIONS_CANCELLED,XREDIS_SUBSCRIPTIONS_TIMEOUT,XREDIS_SUBSCRIPTIONS_CLOSED,
    XREDIS_SUBSCRIPTIONS_SERVER_ERROR,XREDIS_SUBSCRIPTIONS_PROTOCOL,
    XREDIS_SUBSCRIPTIONS_LIMIT,XREDIS_SUBSCRIPTIONS_MEMORY
} xredissubscriptionstatus;
typedef enum xredissubscriptionstep {
    XREDIS_SUBSCRIPTIONS_STEP_ERROR=-1,XREDIS_SUBSCRIPTIONS_WAIT=0,
    XREDIS_SUBSCRIPTIONS_MORE=1,XREDIS_SUBSCRIPTIONS_DONE=2
} xredissubscriptionstep;
typedef struct xredissubscriptionreplayconfig {
    size_t Size;xredissubscriberconfig Registry;size_t MaxBatchNames,MaxBatchBytes;int64 Timeout;
} xredissubscriptionreplayconfig;
/* Acknowledged/counts cover native outcomes observed by Step, not unobserved
 * remote effects. NativeStatus is the observed native failure or the corresponding
 * local stop class; the retained native observer can have its own later outcome. */
typedef struct xredissubscriptionreplayresult {
    size_t Size;xredissubscriptionstatus Status;xredisrequeststatus NativeStatus;
    uint64 Revision;size_t Acknowledged,Batches;uint64 Subscriptions,ShardSubscriptions;
    xredisreply* ErrorReply;const xerror* Cause;
} xredissubscriptionreplayresult;
XRT_EXTERN_C_BEGIN
XREDIS_API bool xredisSubscriptionsConfigInit(xredissubscriptionsconfig* Config);
/* Independent in-memory desired set. Defaults4096 names/1MiB stored names/1MiB input names per edit. Config is
 * copied; success owns one reference. No Client/Engine or new worker is owned. */
XREDIS_API bool xredisSubscriptionsCreate(const xredissubscriptionsconfig* Config,xredissubscriptions** Store);
XREDIS_API bool xredisSubscriptionsRetain(xredissubscriptions* Store);
XREDIS_API void xredisSubscriptionsRelease(xredissubscriptions* Store);
/* Atomic local intent, not a wire operation or durability acknowledgement.
 * Six subscription commands only; duplicates are idempotent. Empty unsubscribe
 * removes its family. Inputs borrowed only through return. Changed sets advance
 * revision; no-op keeps it. Failure preserves all state and Revision output.
 * One bounded constructor runs outside Gate; concurrent Apply is OVERLOADED.
 * Cold edits merge pointer vectors; unchanged binary names are shared, not copied.
 * Snapshots remain readable during edits. Outputs are caller-owned/disjoint. */
XREDIS_API xredisadmission xredisSubscriptionsApply(xredissubscriptions* Store,
    xredispubsubcommand Command,const xbytesview* Names,size_t Count,uint64* Revision);
XREDIS_API bool xredisSubscriptionsInfo(xredissubscriptions* Store,xredissubscriptionsinfo* Info);
/* O(1), no allocation or payload copy. Each snapshot reference is caller-owned;
 * a snapshot/its borrowed views outlive Store and future local edits. Retained
 * old snapshots pin old entry allocations as caller-owned results. */
XREDIS_API bool xredisSubscriptionsSnapshot(xredissubscriptions* Store,xredissubscriptionsnapshot** Snapshot);
XREDIS_API bool xredisSubscriptionSnapshotRetain(xredissubscriptionsnapshot* Snapshot);
XREDIS_API void xredisSubscriptionSnapshotRelease(xredissubscriptionsnapshot* Snapshot);
XREDIS_API bool xredisSubscriptionSnapshotInfo(const xredissubscriptionsnapshot* Snapshot,xredissubscriptionsinfo* Info);
/* Canonical family/shard-slot/hash/binary order; index-at-end false preserves output and
 * ambient error. View borrows immutable name bytes from this snapshot. */
XREDIS_API bool xredisSubscriptionSnapshotAt(const xredissubscriptionsnapshot* Snapshot,size_t Index,xredissubscriptionview* View);
XREDIS_API bool xredisSubscriptionReplayConfigInit(xredissubscriptionreplayconfig* Config);
/* Retain Store/current snapshot/Client, dedicate a fresh ready PRIVATE Client.
 * Caller keeps its original Client close/reference duty. An internal token rejects
 * other native controls until terminal result or destruction; no token is stored
 * in accepted requests. No subscription is sent
 * inline. Success alone changes Replay; failure consumes nothing. Client must
 * not be shared or receive other controls through replay completion/destruction.
 * Registry must fit the captured set. Default64 names/64KiB encoded batch/5s.
 * Every command shares the original monotonic timeout, including caller delay.
 * Shard batches stay within one cached hash slot; endpoint selection and MOVED
 * routing remain with the caller/future Cluster controller.
 * A desired-revision change returns STALE and retires the partial connection.
 * Subscription restoration cannot recover Pub/Sub messages missed while offline. */
XREDIS_API xredisadmission xredisSubscriptionReplayCreate(xredissubscriptions* Store,xredisclient* Client,
    const xredissubscriptionreplayconfig* Config,xredissubscriptionreplay** Replay);
/* Unique caller serialization, no blocking/no private scheduler. One call submits
 * OR observes at most one bounded batch; MORE requires another scheduled turn.
 * WAIT has one pending native Future. A transient physical request tail returns
 * MORE; a private idle Client admission limit is a terminal LIMIT, not busy retry.
 * Failure/cancel/timeout closes Client; OK leaves it subscribed to Result.Revision.
 * Later desired edits do not change a terminal result. Drive again on MORE or a
 * pending-Future notification; never block an Engine worker in a wait. */
XREDIS_API xredissubscriptionstep xredisSubscriptionReplayStep(xredissubscriptionreplay* Replay);
/* Retained native observer if currently pending, otherwise NULL. Does not add a
 * wrapper Future. Cancelling it cancels that control; Step then retires Client. */
XREDIS_API xfuture* xredisSubscriptionReplayPending(xredissubscriptionreplay* Replay);
XREDIS_API bool xredisSubscriptionReplayResult(xredissubscriptionreplay* Replay,xredissubscriptionreplayresult* Result);
/* Same unique-owner serialization as Step. Nonblocking; Destroy aborts unfinished
 * work and releases all helper refs. Native accepted wire owns its copied inputs,
 * so no callback/future retains Replay storage. ErrorReply/Cause borrow a retained
 * failed native Future until Destroy; retain escaping replies explicitly. */
XREDIS_API bool xredisSubscriptionReplayCancel(xredissubscriptionreplay* Replay);
XREDIS_API void xredisSubscriptionReplayDestroy(xredissubscriptionreplay* Replay);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/subscription_session.h */
/* ========================================================================== */

#ifndef XREDIS_SUBSCRIPTION_SESSION_H
#define XREDIS_SUBSCRIPTION_SESSION_H
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION) && !defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS)
#error "Subscription sessions require desired-state replay"
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION)
typedef struct xredissubscriptionsession xredissubscriptionsession;
typedef enum xredissubscriptionsessionstate {
    XREDIS_SESSION_CONNECTING=0,XREDIS_SESSION_RESTORING,XREDIS_SESSION_ONLINE,
    XREDIS_SESSION_UPDATING,XREDIS_SESSION_DRAINING,XREDIS_SESSION_BACKOFF,
    XREDIS_SESSION_CLOSING,XREDIS_SESSION_CLOSED
} xredissubscriptionsessionstate;
typedef struct xredissubscriptionsessioninfo {
    size_t Size;xredissubscriptionsessionstate State;
    uint64 Attempts,Failures,Generations,Restored,AppliedRevision,DesiredRevision;
    xredisrequeststatus LastFailure;double NextAttempt;
} xredissubscriptionsessioninfo;
/* Serialized on the controller worker, outside gates; must not block. Success
 * transfers one private managed Client reference. Failure transfers nothing.
 * Use the provided total setup limit with numeric/DNS/prepared/TLS/auth factory.
 * Every generation must use the same Engine and RESP/service/security profile. */
typedef bool (*xredissubscriptionsessionconnect)(xnetengine* Engine,uint64 Attempt,
    int64 Timeout,ptr Data,xredisclient** Client);
/* Coalesced state snapshots, not a complete transition log. Reentry is allowed.
 * Called on the controller worker; retain escaping objects explicitly. Data and
 * callback code remain resident through Drop. No per-message callback is added. */
typedef void (*xredissubscriptionsessionnotify)(xredissubscriptionsession* Session,
    const xredissubscriptionsessioninfo* Info,ptr Data);
typedef void (*xredissubscriptionsessiondrop)(ptr Data);
typedef struct xredissubscriptionsessionconfig {
    size_t Size;xredissubscriptionsessionconnect Connect;
    xredissubscriptionsessionnotify Notify;xredissubscriptionsessiondrop Drop;ptr Data;
    xredisprotocol Protocol;xredissubscriptionreplayconfig Replay;
    int64 AttemptTimeout,RetryMin,RetryMax;uint32 MaxFailures;
} xredissubscriptionsessionconfig;
/* Terminal stop reason (explicit Close is OK); LastFailure remains historical. */
typedef struct xredissubscriptionsessionclosed {
    xredisrequeststatus Status;uint64 Attempts,Generations;
} xredissubscriptionsessionclosed;
typedef struct xredissubscriptionsessioncurrent {
    xredisclient* Client;uint64 Generation,Revision;
} xredissubscriptionsessioncurrent;
XRT_EXTERN_C_BEGIN
/* RESP3, 5s per complete factory/readiness/restore attempt, 100ms..10s jittered
 * backoff, unlimited consecutive transient failures (MaxFailures=0). Positive
 * MaxFailures stops at that bound. Replay.Timeout is ignored: initial restore
 * shares the attempt's original deadline; each live diff gets AttemptTimeout. */
XREDIS_API bool xredisSubscriptionSessionConfigInit(xredissubscriptionsessionconfig* Config);
/* Retain and exclusively claim Store editing until physical close. Read-only
 * snapshots remain available; direct Store Apply is OVERLOADED while claimed.
 * All desired edits use SessionApply. Caller owns one Session reference and must
 * Close then Release it. Accepted creation consumes Data/Drop duty;
 * failure preserves it and Output. No callback runs inline on the caller, but
 * the owning worker may run before return. One candidate/generation, one native
 * control Future, three embedded Watches, one timer and one coalesced post.
 * No worker/runtime is created. Transient connect/disconnect/timeout retries only
 * after physical retirement. Invalid profile, server/protocol/limit/memory and
 * authentication failures close the controller; ordinary commands are not replayed. */
XREDIS_API xredisadmission xredisSubscriptionSessionCreate(xnetengine* Engine,uint64 Affinity,
    xredissubscriptions* Store,const xredissubscriptionsessionconfig* Config,xredissubscriptionsession** Output);
XREDIS_API bool xredisSubscriptionSessionRetain(xredissubscriptionsession* Session);
XREDIS_API void xredisSubscriptionSessionRelease(xredissubscriptionsession* Session);
/* Commit local intent and automatically schedule bounded incremental reconciliation.
 * Only removed names are unsubscribed and only added names subscribed; unchanged
 * names stay live. Finish the captured revision, then reconcile newer intent.
 * This is not a server acknowledgement. No-op edits send nothing. Native message
 * queues retain their limits; transport loss still creates an at-most-once gap. */
XREDIS_API xredisadmission xredisSubscriptionSessionApply(xredissubscriptionsession* Session,
    xredispubsubcommand Command,const xbytesview* Names,size_t Count,uint64* Revision);
XREDIS_API bool xredisSubscriptionSessionInfo(xredissubscriptionsession* Session,xredissubscriptionsessioninfo* Info);
/* ACCEPTED transfers one retained native Client for receiving/observing only.
 * Release it; do not issue commands/adopt/claim it. Controller owns close duty.
 * Available only at the last fully applied desired revision. OVERLOADED during
 * recovery/update and CLOSED after close begins, preserving Output. Old retained
 * generations may still be drained after retirement; they never become current
 * again. Native receive/readability APIs run directly, without a proxy queue. */
XREDIS_API xredisadmission xredisSubscriptionSessionCurrent(xredissubscriptionsession* Session,
    xredissubscriptionsessioncurrent* Output);
XREDIS_API bool xredisSubscriptionSessionClose(xredissubscriptionsession* Session);
/* Physical close retires accepted edits, native generations and callback access;
 * releases Store claim, runs Drop once on worker, then unpins Engine and resolves
 * this shared typed observer. Cancelling the observer does not close the session.
 * Final post/Watch release frames may finish afterwards without context/Engine
 * access. Join owner worker turns for quota-zero accounting. Retained
 * session/outcomes keep no Engine/context reference after close. */
XREDIS_API xfuture* xredisSubscriptionSessionClosed(xredissubscriptionsession* Session);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/role.h */
/* ========================================================================== */

#ifndef XREDIS_ROLE_H
#define XREDIS_ROLE_H
#if defined(XREDIS_FEATURE_XREDIS_ROLE) && !defined(XREDIS_FEATURE_XREDIS_REPLY)
#error "ROLE inspection requires immutable replies"
#endif
#if defined(XREDIS_FEATURE_XREDIS_ROLE)
typedef enum xredisrolekind {XREDIS_ROLE_MASTER=1,XREDIS_ROLE_REPLICA,XREDIS_ROLE_SENTINEL} xredisrolekind;
typedef enum xredisrolestatus {
    XREDIS_ROLE_ERROR=-1,XREDIS_ROLE_MALFORMED=-2,XREDIS_ROLE_LIMIT=-3,
    XREDIS_ROLE_OTHER=0,XREDIS_ROLE_VALUE=1,XREDIS_ROLE_SERVER_ERROR=2
} xredisrolestatus;
typedef enum xredisrolelink {
    XREDIS_ROLE_LINK_OTHER=0,XREDIS_ROLE_LINK_CONNECT,XREDIS_ROLE_LINK_CONNECTING,
    XREDIS_ROLE_LINK_SYNC,XREDIS_ROLE_LINK_CONNECTED
} xredisrolelink;
typedef struct xredisrolelimits {size_t Size,MaxEntries,MaxPayloadBytes;} xredisrolelimits;
/* Borrowed cursor: Source and all returned views require an existing live Reply
 * reference. Read consumes no reference and copies no payload. Use existing
 * ReplyRetain/Release for escaping ownership. Do not edit cursor fields. */
typedef struct xredisrolecursor {const xredisreply* Source;size_t Next,Remaining;uint32 Kind;} xredisrolecursor;
typedef struct xredisrolereplica {xbytesview Host;uint16 Port;int64 Offset;} xredisrolereplica;
typedef struct xredisroleinfo {
    xredisrolekind Kind;int64 Offset;xbytesview MasterHost,LinkText;uint16 MasterPort;
    xredisrolelink Link;xredisrolecursor Replicas,Monitored;
} xredisroleinfo;
XRT_EXTERN_C_BEGIN
/* Defaults4096 entries/1MiB scalar payload; zero is a real limit, not unlimited.
 * MaxPayloadBytes counts the whole immutable reply, including attributes and
 * numeric lexemes. MaxEntries limits the replica/monitored list, not fixed
 * replica-role fields. Output must not overlap any source reply allocation. The caller's bounded reply reader still owns parsing costs. */
XREDIS_API bool xredisRoleLimitsInit(xredisrolelimits* Limits);
/* Validate the complete recognized ROLE shape, then publish one borrowed view.
 * Master replica tuples accept decimal port/offset strings used by Redis and
 * integer numeric forms. Replica offset -1 means unavailable; master offsets
 * are nonnegative. Port0 remains represented, not invented as a usable endpoint.
 * Unknown replica link text is preserved as LINK_OTHER. Protocol role text
 * "slave" maps to REPLICA; it is a wire value, not an API spelling.
 * VALUE alone writes Info. OTHER/server-error/malformed/limit preserve output
 * and ambient error. Invalid arguments report ERROR. No successful/protocol/
 * limit path allocates, copies payload or retains ownership. Validation is
 * O(entries + decimal bytes), bounded by Limits; iteration is O(1) per
 * entry for Redis canonical numeric fields. No host/name payload is scanned. */
XREDIS_API xredisrolestatus xredisRoleRead(const xredisreply* Reply,
    const xredisrolelimits* Limits,xredisroleinfo* Info);
/* Transfer one borrowed view and advance only on success. Exhaustion preserves
 * outputs and ambient error. Pass an unmodified cursor from Read and a live source Reply. Detected invalid
 * cursor/output is an argument error;
 * cursor and output storage must be disjoint and caller-owned writable objects. */
XREDIS_API bool xredisRoleReplicaNext(xredisrolecursor* Cursor,xredisrolereplica* Replica);
XREDIS_API bool xredisRoleMasterNext(xredisrolecursor* Cursor,xbytesview* Name);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/role_client.h */
/* ========================================================================== */

#ifndef XREDIS_ROLE_CLIENT_H
#define XREDIS_ROLE_CLIENT_H
#if defined(XREDIS_FEATURE_XREDIS_ROLE_CLIENT) && (!defined(XREDIS_FEATURE_XREDIS_CLIENT) || !defined(XREDIS_FEATURE_XREDIS_ROLE))
#error "Managed role verification requires native Client and ROLE inspection"
#endif
#if defined(XREDIS_FEATURE_XREDIS_ROLE_CLIENT)
/* Choose zero or ONE requirement in ClientInitConfig.Flags. ROLE follows
 * negotiation/auth/name/database setup and must validate and match before
 * Ready or any queued business command. All phases share the original setup
 * deadline and MaxWireBytes/Stream.WriteLimit; the role command adds14 bytes.
 * Reply limits are the existing Replies.Reader MaxValues/MaxFrameBytes bounds.
 * No Role reply is retained in Ready; Hello stays the negotiated HELLO map.
 * Mismatch returns READY_ROLE_MISMATCH at INIT_ROLE; other setup failures keep
 * their existing typed statuses and redacted diagnostics. Config/result/native
 * object layouts and all old enum values stay unchanged.
 * REQUIRE_REPLICA verifies identity, not freshness or replication consistency.
 * This point-in-time check must be repeated on each new connection. Sentinel
 * discovery, endpoint replacement and Cluster routing are separate work. */
#define XREDIS_INIT_REQUIRE_MASTER 0x10u
#define XREDIS_INIT_REQUIRE_REPLICA 0x20u
#define XREDIS_INIT_REQUIRE_SENTINEL 0x40u
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/sentinel.h */
/* ========================================================================== */

#ifndef XREDIS_SENTINEL_H
#define XREDIS_SENTINEL_H
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL) && (!defined(XREDIS_FEATURE_XREDIS_CLIENT) || !defined(XREDIS_FEATURE_XREDIS_ROLE))
#error "Sentinel discovery requires native managed clients and ROLE inspection"
#endif
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL)
typedef struct xredissentineldiscovery xredissentineldiscovery;
typedef struct xredissentinelendpoint {xbytesview Host;uint16 Port;} xredissentinelendpoint;
typedef enum xredissentinelstatus {
    XREDIS_SENTINEL_PENDING=0,XREDIS_SENTINEL_OK,XREDIS_SENTINEL_UNREACHABLE,
    XREDIS_SENTINEL_UNKNOWN_SERVICE,XREDIS_SENTINEL_STALE_ROLE,XREDIS_SENTINEL_TIMEOUT,
    XREDIS_SENTINEL_CANCELLED,XREDIS_SENTINEL_AUTH_FAILED,XREDIS_SENTINEL_SERVER_ERROR,
    XREDIS_SENTINEL_PROTOCOL,XREDIS_SENTINEL_LIMIT,XREDIS_SENTINEL_MEMORY,XREDIS_SENTINEL_FACTORY
} xredissentinelstatus;
typedef enum xredissentinelstep {
    XREDIS_SENTINEL_STEP_ERROR=-1,XREDIS_SENTINEL_WAIT=0,XREDIS_SENTINEL_MORE=1,XREDIS_SENTINEL_DONE=2
} xredissentinelstep;
/* Nonblocking factories run on the Step caller. OK transfers one fresh,
 * PRIVATE managed Client reference on Engine; failure transfers nothing.
 * Select numeric/DNS/prepared/TLS, Sentinel/data credentials and trust separately.
 * Respect Timeout as the entire native setup limit; copy borrowed Endpoint input
 * before returning. No business call, external sharing or control is permitted.
 * Return a typed non-OK status on failure (no transferred reference); transient
 * TIMEOUT/UNREACHABLE retries, other statuses fail closed. The discovery performs
 * its own ROLE command; factories need no role flag. */
typedef xredissentinelstatus (*xredissentinelseedconnect)(xnetengine* Engine,size_t Seed,int64 Timeout,ptr Data,xredisclient** Client);
typedef xredissentinelstatus (*xredissentinelmasterconnect)(xnetengine* Engine,const xredissentinelendpoint* Endpoint,int64 Timeout,ptr Data,xredisclient** Client);
typedef struct xredissentinelconfig {
    size_t Size; xbytesview Service;
    size_t SeedCount,PreferredSeed,MaxRounds,MaxServiceBytes,MaxHostBytes;
    int64 Timeout,SeedTimeout,RetryDelay;
    xredisprotocol Protocol;xredisrolelimits Role;
    xredissentinelseedconnect ConnectSeed;xredissentinelmasterconnect ConnectMaster;ptr Data;
} xredissentinelconfig;
/* Host borrows Discovery until Destroy, copied once from the successful query.
 * PreferredSeed identifies the most recent positive query, for the next discovery.
 * Native outcomes are last observations, never an ambient error/peer diagnostic.
 * OK verifies a point-in-time master identity, not freshness or consensus. */
typedef struct xredissentinelresult {
    size_t Size;xredissentinelstatus Status;xredissentinelendpoint Endpoint;
    size_t PreferredSeed,Rounds,SeedAttempts,MasterAttempts,UnknownReplies,StaleReplies;
    bool ReadyObserved,RequestObserved;
    xredisreadystatus ReadyStatus;xredisrequeststatus RequestStatus;
} xredissentinelresult;
XRT_EXTERN_C_BEGIN
/* Defaults: 5s total, 300ms per seed connect+query, 100ms stale/transport retry,
 * 3 rounds, 1 seed, 64KiB service and 1024-byte host. Two factories mandatory.
 * Timeout 1..24h, SeedTimeout/RetryDelay 1..60s; seeds1..4096/rounds1..1024;
 * service1..64KiB, host1..64KiB. All limits include caller scheduling delay. */
XREDIS_API bool xredisSentinelConfigInit(xredissentinelconfig* Config);
/* Unique caller serialization for every API; no reentry on this Discovery from
 * a factory. Copies config/service, allocates one bounded host buffer and pins
 * existing Engine. Data is borrowed through Destroy. Failure consumes nothing
 * and preserves Output. No new runtime, worker, timer or wrapper Future.
 * Successful factories' transport callbacks retain their own context/security
 * material through native physical close; Discovery never assumes their duty. */
XREDIS_API xredisadmission xredisSentinelDiscoveryCreate(xnetengine* Engine,const xredissentinelconfig* Config,xredissentineldiscovery** Output);
/* Each Step advances one phase, never blocks. MORE schedules another turn.
 * WAIT observes Pending or NextWake (whichever comes first). Never busy-poll or
 * block an Engine worker. One original monotonic deadline spans all rounds,
 * factory/Ready/query/ROLE/backoff and retirement. Failed candidates physically
 * retire before another factory starts. Nil query tries next seed; wrong ROLE
 * waits then rediscovers. ACL/auth/protocol/limit/OOM fail closed. All-null seed
 * round is UNKNOWN_SERVICE; mixed unavailable/null is UNREACHABLE. No INFO
 * fallback for old servers without ROLE, business replay or implicit downgrade. */
XREDIS_API xredissentinelstep xredisSentinelDiscoveryStep(xredissentineldiscovery* Discovery);
/* One retained unresolved native Ready/command/Closed observer, otherwise NULL.
 * Release it normally. Cancelling a command observer asks that native operation
 * to stop; cancellation of the shared readiness/close observer does not abort
 * discovery. Use DiscoveryCancel for that. No notification runs a helper callback. */
XREDIS_API xfuture* xredisSentinelDiscoveryPending(xredissentineldiscovery* Discovery);
/* Absolute monotonic seconds (xrtTimer); zero on DONE, includes physical-tail
 * polling at >=1ms. Pending completion can require an earlier turn. */
XREDIS_API double xredisSentinelDiscoveryNextWake(xredissentineldiscovery* Discovery);
XREDIS_API bool xredisSentinelDiscoveryResult(xredissentineldiscovery* Discovery,xredissentinelresult* Result);
/* Once only, after DONE/OK, transfers native master reference and close duty.
 * Destroy otherwise closes it. Future reconnects must run discovery again.
 * No proxy/extra allocation/copy/synchronization on ordinary business calls. */
XREDIS_API bool xredisSentinelDiscoveryTakeClient(xredissentineldiscovery* Discovery,xredisclient** Client);
XREDIS_API bool xredisSentinelDiscoveryCancel(xredissentineldiscovery* Discovery);
/* Nonblocking abort: closes unreleased candidate, cancels/releases owned command
 * observer, unpins Engine and frees helper storage. Native physical duties own
 * copied wire and no Discovery pointer. Drive Cancel to DONE first if observing
 * physical retirement is required. All retained native observers remain valid. */
XREDIS_API void xredisSentinelDiscoveryDestroy(xredissentineldiscovery* Discovery);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/sentinel_pool.h */
/* ========================================================================== */

#ifndef XREDIS_SENTINEL_POOL_H
#define XREDIS_SENTINEL_POOL_H
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_POOL) && (!defined(XREDIS_FEATURE_XREDIS_SENTINEL) || !defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT))
#error "Sentinel pools require discovery and native pool maintenance"
#endif
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_POOL)
typedef enum xredissentinelpoolphase {
    XREDIS_SENTINEL_POOL_DISCOVERING=0,XREDIS_SENTINEL_POOL_ONLINE,
    XREDIS_SENTINEL_POOL_RECONFIGURING,XREDIS_SENTINEL_POOL_CLOSING,XREDIS_SENTINEL_POOL_CLOSED
} xredissentinelpoolphase;
typedef struct xredissentinelpoolinfo {
    size_t Size;xredissentinelpoolphase Phase;
    uint64 Generation,Changes;size_t PreferredSeed;uint16 Port;
    xredissentinelstatus LastStatus;xredispoolconnectinfo Connect;
} xredissentinelpoolinfo;
/* Coalesced snapshots on the existing pool worker, outside gates. Must not block.
 * Reentry through async/native snapshot/close APIs is allowed. Snapshot is borrowed
 * through return; retain escaping objects. Callback code/Data survive through Drop. */
typedef void (*xredissentinelpoolnotify)(xredispool* Pool,const xredissentinelpoolinfo* Info,ptr Data);
typedef struct xredissentinelpoolconfig {
    size_t Size;xredissentinelconfig Discovery;size_t MaxConnecting;
    int64 RetryMin,RetryMax;xredissentinelpoolnotify Notify;xredispoolconnectdrop Drop;
} xredissentinelpoolconfig;
XRT_EXTERN_C_BEGIN
/* Defaults: native discovery defaults, four complete concurrent discovery/retire
 * duties, jittered100ms..10s retry. Discovery.Timeout1..60000ms; other discovery
 * bounds apply. Retry1..60000ms, min<=max. Service bytes are copied once here and
 * once per bounded active discovery. Native factories keep their own limits. */
XREDIS_API bool xredisSentinelPoolConfigInit(xredissentinelpoolconfig* Config);
/* Automatic Count1..65536 native pool slots. Every missing member runs fresh
 * Sentinel discovery and target ROLE, including same-address reconnects.
 * One existing pool worker/heap/timer owns all cold state. MaxConnecting counts
 * physical failure/cancellation tails. No factory runs inline on this caller.
 * ACCEPTED consumes Discovery.Data/Drop duty; failure consumes nothing and
 * preserves Output. Factories/Notify/Drop run serially on the pool worker.
 * Discovery.Service is snapshotted; other referenced trust/resolver/materials
 * and context remain valid through Drop. Each attempt shares the discovery's
 * original deadline; acquisitions keep their own outage-inclusive deadlines.
 * TIMEOUT/UNREACHABLE/STALE_ROLE retry with bounded jitter; unknown service,
 * auth/server/protocol/limit/memory/factory failures fail the pool closed.
 * LastStatus preserves the maintenance outcome after physical close.
 *
 * A changed verified host/port atomically fences ALL old lease admissions before
 * retiring members in WorkItems batches. Already accepted calls keep native
 * delivery uncertainty; no business command is replayed. Old generations and
 * discovery/watch/cancel tails physically retire before the fence reopens.
 * An unchanged endpoint refills only missing slots. Host comparison is exact
 * bytes; no DNS alias/consensus/freshness inference is made. ROLE is point-in-time.
 * Use the ordinary PoolAcquire/LeaseCall/Return/Abort/Close/Closed APIs directly.
 * Explicit Close retires all candidates/callbacks before one Drop and Closed;
 * Context access retires before Drop; final watch/post release frames may finish
 * afterwards. Join the engine before unloading callback code. Retained closed
 * snapshots/outcomes keep no engine/context reference. */
XREDIS_API xredisadmission xredisSentinelPoolCreate(xnetengine* Engine,uint64 Affinity,size_t Count,
    const xredispoolconfig* Config,const xredissentinelpoolconfig* Sentinel,xredispool** Output);
XREDIS_API bool xredisSentinelPoolInfo(xredispool* Pool,xredissentinelpoolinfo* Info);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/cluster_topology.h */
/* ========================================================================== */

#ifndef XREDIS_CLUSTER_TOPOLOGY_H
#define XREDIS_CLUSTER_TOPOLOGY_H
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_TOPOLOGY) && !defined(XREDIS_FEATURE_XREDIS_REPLY)
#error "Cluster topology requires immutable replies"
#endif
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_TOPOLOGY)
#define XREDIS_CLUSTER_SLOTS 16384u
typedef struct xredisclustertopology xredisclustertopology;
typedef enum xredisclusterformat {XREDIS_CLUSTER_SHARDS=1,XREDIS_CLUSTER_LEGACY_SLOTS} xredisclusterformat;
typedef enum xredisclusterstatus {
    XREDIS_CLUSTER_ERROR=-1,XREDIS_CLUSTER_MALFORMED=-2,XREDIS_CLUSTER_LIMIT=-3,
    XREDIS_CLUSTER_MEMORY=-4,XREDIS_CLUSTER_SERVER_ERROR=-5,
    XREDIS_CLUSTER_ABSENT=0,XREDIS_CLUSTER_VALUE=1
} xredisclusterstatus;
typedef enum xredisclusterendpoint {XREDIS_CLUSTER_ENDPOINT_HOST=1,XREDIS_CLUSTER_ENDPOINT_SOURCE,XREDIS_CLUSTER_ENDPOINT_UNKNOWN} xredisclusterendpoint;
typedef enum xredisclusterrole {XREDIS_CLUSTER_ROLE_OTHER=0,XREDIS_CLUSTER_MASTER,XREDIS_CLUSTER_REPLICA} xredisclusterrole;
typedef enum xredisclusterhealth {XREDIS_CLUSTER_HEALTH_OTHER=0,XREDIS_CLUSTER_ONLINE,XREDIS_CLUSTER_FAILED,XREDIS_CLUSTER_LOADING,XREDIS_CLUSTER_UNREPORTED} xredisclusterhealth;
typedef enum xredisclusterredirectkind {XREDIS_CLUSTER_MOVED=1,XREDIS_CLUSTER_ASK} xredisclusterredirectkind;
typedef struct xredisclusterlimits {
    size_t Size,MaxShards,MaxNodes,MaxRanges,MaxReplyNodes,MaxPayloadBytes,MaxEndpointBytes,MaxOwnedBytes;
    bool RequireFullCoverage;
} xredisclusterlimits;
/* Views are owned by a live topology reference; no NUL terminator. SOURCE means
 * use the actual host used for the topology query. UNKNOWN ('?') must not use
 * that fallback. SHARDS has separate TCP/TLS ports (0 means unavailable);
 * legacy SLOTS Port belongs to the transport used for that query. */
typedef struct xredisclusternode {
    xbytesview Id,Host,Ip,Hostname,RoleText,HealthText;
    size_t Shard;int64 ReplicationOffset;
    uint16 Port,TlsPort;xredisclusterendpoint Endpoint;
    xredisclusterrole Role;xredisclusterhealth Health;
} xredisclusternode;
typedef struct xredisclusterrange {uint16 First,Last;size_t Master,Shard;} xredisclusterrange;
typedef struct xredisclustertopologyinfo {
    size_t Size,Shards,Nodes,Ranges,AssignedSlots,StorageBytes,RetainedReplyBytes;
    xredisclusterformat Format;
} xredisclustertopologyinfo;
/* Borrowed from the existing Reply reference. ASK targets only the current
 * query and requires exclusive ASKING+query execution, not a permanent map edit.
 * Empty Host uses the original request's host; IPv6 brackets are stripped. */
typedef struct xredisclusterredirect {xredisclusterredirectkind Kind;uint16 Slot,Port;xbytesview Host;xredisclusterendpoint Endpoint;} xredisclusterredirect;
XRT_EXTERN_C_BEGIN
/* Binary-safe CRC16/XMODEM with first-pair nonempty hash tags. O(key bytes),
 * no allocation or ownership. Empty keys are valid. Output must be disjoint. */
XREDIS_API bool xredisClusterKeySlot(xbytesview Key,uint16* Slot);
/* Classify SIMPLE_ERROR/BULK_ERROR only. MaxEndpointBytes is a real cap,
 * including brackets; slot/port decimal fields have fixed maximum lengths.
 * VALUE alone writes Redirect. Non-values preserve output and ambient error.
 * Input must remain owned; never mutates a topology or executes/replays calls. */
XREDIS_API xredisclusterstatus xredisClusterRedirectRead(const xredisreply* Reply,size_t MaxEndpointBytes,xredisclusterredirect* Redirect);
/* Defaults:4096 shards/nodes,16384 ranges,262144 reply nodes,4MiB payload,
 * 1024 endpoint bytes,32MiB total requested retained+snapshot storage, partial
 * coverage allowed. Zero is a limit. MaxNodes<=65535; shards/ranges<=16384. */
XREDIS_API bool xredisClusterLimitsInit(xredisclusterlimits* Limits);
/* Complete two-pass shape validation; detects overlapping/out-of-range slots,
 * duplicate known map fields and missing/multiple masters. SHARDS accepts RESP2
 * alternating arrays or RESP3 maps, preserves unknown fields in the owner,
 * and preserves unknown role/health text. SLOTS accepts old 2-field endpoints,
 * optional node IDs and metadata; duplicate per-range node tuples remain so.
 * Empty shard slot lists and partial maps are valid unless full coverage is
 * required. Bounded reader costs remain the caller's responsibility.
 * VALUE transfers one topology ref. One allocation owns the fixed32KiB slot
 * index and node/range descriptors, plus one retained Reply ref (no text copy).
 * MaxOwnedBytes includes that Reply's ReservedBytes. Creation scans shapes
 * twice and fills slots; no hidden runtime, threads, network, refresh or retry.
 * Output remains unchanged on failure. Source/config/output must be disjoint.
 * Invalid args/OOM may set ambient error; protocol/limit/server outcomes don't. */
XREDIS_API xredisclusterstatus xredisClusterTopologyCreate(xredisreply* Reply,xredisclusterformat Format,const xredisclusterlimits* Limits,xredisclustertopology** Topology);
/* Immutable snapshots support synchronized cross-thread handoff. A caller must
 * own a live ref before access/retain; final Release frees on its own thread. */
XREDIS_API bool xredisClusterTopologyRetain(xredisclustertopology* Topology);
XREDIS_API void xredisClusterTopologyRelease(xredisclustertopology* Topology);
XREDIS_API bool xredisClusterTopologyInfo(const xredisclustertopology* Topology,xredisclustertopologyinfo* Info);
/* Constant-time index/slot lookups, no allocations, locks or refcount changes.
 * Out-of-range arguments report ERROR; unassigned slots return ABSENT and leave
 * Node unchanged. Lookup does not filter health/unknown endpoints: caller must
 * choose an eligible node/transport before dialing. Every writable output must
 * be disjoint from the snapshot and all its retained reply allocations. */
XREDIS_API xredisclusterstatus xredisClusterTopologyLookup(const xredisclustertopology* Topology,uint16 Slot,xredisclusternode* Node);
XREDIS_API bool xredisClusterTopologyNode(const xredisclustertopology* Topology,size_t Index,xredisclusternode* Node);
XREDIS_API bool xredisClusterTopologyRange(const xredisclustertopology* Topology,size_t Index,xredisclusterrange* Range);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/pool_asking.h */
/* ========================================================================== */

#ifndef XREDIS_POOL_ASKING_H
#define XREDIS_POOL_ASKING_H
#if defined(XREDIS_FEATURE_XREDIS_POOL_ASKING) && !defined(XREDIS_FEATURE_XREDIS_POOL)
#error "Pooled ASKING requires native pools"
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
/* Both outcomes borrow their Future. Query is the ordinary result prefix.
 * ControlReceived/Control are immutable snapshots at logical completion, even
 * when a sent cancellation/timeout still has physical FIFO obligations. No
 * result retains the lease/client/engine; retain the Future for escaping views. */
typedef struct xredisaskingresult {xredisrequestresult Query,Control;bool ControlReceived;} xredisaskingresult;
XRT_EXTERN_C_BEGIN
/* Copies ASKING + one ordinary command into ONE adjacent native wire group,
 * request and Future, with no extra payload copy/Future/controller. Producers
 * sharing this exclusive lease cannot interleave between the two commands.
 * Both commands are pipelined: Query MAY EXECUTE even if Control is SERVER_ERROR.
 * Inspect both outcomes; a control error never authorizes replay of the query.
 * No address selection, redirect following, reconnect or business replay occurs.
 * Managed ready pooled clients only. Known connection/transaction/subscription
 * controls, CLIENT, QUIT, READONLY/READWRITE and ASKING are refused as queries.
 * Inputs borrow only during admission. MaxCommandBytes/Total and pool quotas
 * include the prefix and aligned private state. Timeout starts at API entry,
 * covers construction/both replies, and is capped by the original lease hold.
 * Sent cancel/timeout conservatively reports MAY_HAVE_EXECUTED even when only
 * the prefix may have gone out. Native FIFO drains both replies, with no reuse
 * until physical retirement; drain expiry/disconnect/protocol errors retire the
 * connection, so a pending one-shot ASKING flag cannot escape to a later lease.
 * Control must reply simple OK or a server error; malformed control fails the
 * connection. A validated Query reply has ordinary OK/SERVER_ERROR semantics.
 * Reply/limits/OOM/push/TLS bounds remain native. Return/Abort the lease normally.
 * Output changes only on ACCEPTED; sync rejects every worker of the same Engine. */
XREDIS_API xredisadmission xredisLeaseAskingCallAsync(xredislease* Lease,
    const xbytesview* Arguments,size_t Count,int64 Timeout,xfuture** Future);
XREDIS_API xredisadmission xredisLeaseAskingCall(xredislease* Lease,
    const xbytesview* Arguments,size_t Count,int64 Timeout,xfuture** Future);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/pool_shared.h */
/* ========================================================================== */

#ifndef XREDIS_POOL_SHARED_H
#define XREDIS_POOL_SHARED_H
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED) && !defined(XREDIS_FEATURE_XREDIS_POOL)
#error "Shared pools require native pools"
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED)
XRT_EXTERN_C_BEGIN
/* Fixed ready managed members, same ownership/profile/rollback rules as
 * PoolCreate. Uses the native pool handle and PoolInfo/Close/Closed/Retain/
 * Release. Exclusively claims members, but multiplexes ordinary calls without
 * allocating a lease. Acquire APIs reject this mode. Public config validation
 * is unchanged; MaxPending/LeaseTimeout/ReturnTimeout do not govern shared
 * calls. There is no acquisition wait queue, reconnect or business replay.
 * PoolInfo.Idle counts dispatch-eligible members, including busy ones; Leased,
 * Pending, Acquired and Returned stay zero. Requests/CommandBytes retain hard
 * native constructor/wire/timer/cancel-tail accounting. */
XREDIS_API xredisadmission xredisSharedPoolCreate(xnetengine* Engine, uint64 Affinity,
    xredisclient* const* Clients, size_t Count, const xredispoolconfig* Config, xredispool** Pool);
/* One native request/Future. Round-robin selection probes at most
 * min(dispatch-eligible members, WorkItems) candidates. Only nonaccepted
 * admissions can try another member; accepted calls never move or replay.
 * No ready/capacity candidate returns OVERLOADED without changing output.
 * Timeout starts at API entry, including selection and constructor work.
 * Ordinary client restrictions, limits, FIFO and sent-stop drain apply.
 * A blocking command also blocks later replies on its selected connection;
 * isolate blocking traffic in a separate pool. Known connection/mode/session
 * controls require exclusive APIs and are refused here. No cross-connection
 * order or key affinity is promised. Caller retains Pool across each API.
 * Result storage outlives the pool/engine. Sync rejects every engine worker. */
XREDIS_API xredisadmission xredisSharedPoolCallAsync(xredispool* Pool,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future);
XREDIS_API xredisadmission xredisSharedPoolCall(xredispool* Pool,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/pool_shared_connect.h */
/* ========================================================================== */

#ifndef XREDIS_POOL_SHARED_CONNECT_H
#define XREDIS_POOL_SHARED_CONNECT_H
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_CONNECT) && \
    (!defined(XREDIS_FEATURE_XREDIS_POOL_SHARED) || !defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT))
#error "Automatic shared pools require shared pools and native connection maintenance"
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_CONNECT)
XRT_EXTERN_C_BEGIN
/* Starts/refills Count shared slots using the same factory, bounded attempts,
 * readiness/profile checks, backoff, physical generation retirement, timer-OOM
 * failure and exactly-once Data/Drop contract as PoolConnect. No factory runs
 * inline. ACCEPTED consumes Data/Drop; failure preserves it and output.
 * Use SharedPoolCall[Async], PoolConnectInfo and ordinary PoolInfo/Close/Closed/
 * Retain/Release. Native request/Future/selection costs match fixed shared pools.
 * Ready members admit immediately; an outage or a saturated bounded probe
 * returns OVERLOADED with output untouched. There is NO outage wait queue or
 * automatic retry of accepted business requests. The call timeout does not
 * wait for maintenance. Acquisition APIs reject shared mode. Old logical stops
 * keep native FIFO/quota obligations; replacements wait for physical retirement.
 * Caller factory/context/code remain valid until Drop. Full Cluster topology,
 * address policy, redirects and failover selection are separate operations. */
XREDIS_API xredisadmission xredisSharedPoolConnect(xnetengine* Engine, uint64 Affinity,
    size_t Count, const xredispoolconfig* Config,
    const xredispoolconnectconfig* Connect, xredispool** Result);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/pool_shared_asking.h */
/* ========================================================================== */

#ifndef XREDIS_POOL_SHARED_ASKING_H
#define XREDIS_POOL_SHARED_ASKING_H
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_ASKING) && \
    (!defined(XREDIS_FEATURE_XREDIS_POOL_SHARED) || !defined(XREDIS_FEATURE_XREDIS_POOL_ASKING))
#error "Shared ASKING requires shared pools and native ASKING groups"
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_ASKING)
XRT_EXTERN_C_BEGIN
/* Native adjacent ASKING/query execution on fixed or maintained shared pools.
 * Returns ONE native request/Future borrowing an xredisaskingresult; no lease,
 * proxy Future or extra payload clone. Other producers cannot interleave
 * between this group's commands on its selected connection. Both commands are
 * pipelined: Query MAY EXECUTE even when Control is SERVER_ERROR. Inspect both
 * outcomes. No accepted query is retried/replayed, routed or refreshed here.
 * SharedPoolCall selection, bounded probes, immediate overload, entry deadline,
 * quota and close rules apply. Native ASKING command restrictions, two-reply
 * FIFO, immutable prefix knowledge, partial-wire completion and sent-stop drain
 * rules apply. Group prefix/aligned state count toward native and pool quotas.
 * A stopped group cannot leak its pending one-shot flag to following ordinary
 * calls; full wire/replies drain or the native member retires. Maintenance may
 * replace retired members but never replays the group. Completed outcomes pin
 * neither pool nor engine. Caller retains Pool across each API. No lease hold
 * applies. Output changes only on ACCEPTED; sync refuses every engine worker. */
XREDIS_API xredisadmission xredisSharedPoolAskingCallAsync(xredispool* Pool,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future);
XREDIS_API xredisadmission xredisSharedPoolAskingCall(xredispool* Pool,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/cluster_route.h */
/* ========================================================================== */

#ifndef XREDIS_CLUSTER_ROUTE_H
#define XREDIS_CLUSTER_ROUTE_H
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_ROUTE) && \
    (!defined(XREDIS_FEATURE_XREDIS_CLUSTER_TOPOLOGY) || !defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_ASKING))
#error "Cluster routes require topology and native shared ASKING"
#endif
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_ROUTE)
typedef struct xredisclusterroute xredisclusterroute;
typedef enum xredisclusterpoolstatus {
    XREDIS_CLUSTER_POOL_READY, XREDIS_CLUSTER_POOL_WAIT, XREDIS_CLUSTER_POOL_DENIED,
    XREDIS_CLUSTER_POOL_CLOSED, XREDIS_CLUSTER_POOL_ERROR, XREDIS_CLUSTER_POOL_MEMORY
} xredisclusterpoolstatus;
typedef enum xredisclusterroutestatus {
    XREDIS_CLUSTER_ROUTE_PENDING, XREDIS_CLUSTER_ROUTE_COMPLETE,
    XREDIS_CLUSTER_ROUTE_TIMEOUT, XREDIS_CLUSTER_ROUTE_CANCELLED,
    XREDIS_CLUSTER_ROUTE_NO_ROUTE, XREDIS_CLUSTER_ROUTE_POLICY,
    XREDIS_CLUSTER_ROUTE_LIMIT, XREDIS_CLUSTER_ROUTE_OVERLOADED,
    XREDIS_CLUSTER_ROUTE_CLOSED, XREDIS_CLUSTER_ROUTE_PROVIDER_ERROR,
    XREDIS_CLUSTER_ROUTE_ADMISSION_ERROR, XREDIS_CLUSTER_ROUTE_PROTOCOL,
    XREDIS_CLUSTER_ROUTE_MEMORY
} xredisclusterroutestatus;
typedef enum xredisclusterroutestep {
    XREDIS_CLUSTER_ROUTE_STEP_ERROR=-1, XREDIS_CLUSTER_ROUTE_WAIT=0,
    XREDIS_CLUSTER_ROUTE_MORE=1, XREDIS_CLUSTER_ROUTE_DONE=2
} xredisclusterroutestep;
typedef struct xredisclusteraddress {
    xbytesview Host; uint16 Port, Slot; bool Tls;
    /* Zero for the initial topology choice, otherwise validated MOVED/ASK.
     * ASK applies only to this route and must not change a shared slot map. */
    xredisclusterredirectkind Reason;
} xredisclusteraddress;
/* Runs on the Step caller, outside all library gates. Nonblocking; no reentry
 * on this Route. Authorize the exact address/transport/profile before returning
 * its fixed or maintained SHARED pool. READY transfers one retained Pool ref;
 * every other status leaves output unchanged and transfers nothing. Endpoint
 * input borrows only this invocation. WAIT requests bounded delayed admission,
 * not another business attempt. Provider owns node caches/refresh/coalescing
 * and pool/context close duties independently. Never return a pool for a
 * different endpoint/credentials/transport. No implicit DNS/port/TLS fallback. */
typedef xredisclusterpoolstatus (*xredisclusterpoolproc)(const xredisclusteraddress* Address,
    int64 Remaining, ptr Data, xredispool** Pool);
typedef struct xredisclusterrouteconfig {
    size_t Size, MaxRedirects, MaxWaits, MaxArguments, MaxCommandBytes, MaxHostBytes, MaxStorageBytes;
    int64 Timeout, RetryDelay; bool Tls;
    xredisclusterpoolproc GetPool; ptr Data;
} xredisclusterrouteconfig;
/* Views borrow Route until the next Step, or Destroy at DONE. Native
 * Query/Control/Cause additionally borrow the current
 * native Future: retain ResultFuture before escaping Route's lifetime.
 * RequestObserved is a snapshot at DONE, even if a stopped native Future later
 * resolves. COMPLETE means a terminal native outcome, including a server error
 * or uncertain transport failure; inspect Query.Status/Delivery. A parent stop
 * can coexist with an already observed native outcome. No result authorizes
 * replay after timeout/cancel/disconnect or denied ASKING control. */
typedef struct xredisclusterrouteresult {
    size_t Size; xredisclusterroutestatus Status;
    xredisclusteraddress Address, AttemptAddress;
    size_t Attempts, Redirects, Moved, Asks, Waits, ProviderCalls;
    /* Controller requested storage only; native Future/reply/physical duties
     * remain bounded by their independent pool/client limits. */
    size_t StorageBytes;
    xredisclusterpoolstatus Provider; xredisadmission Admission;
    bool AdmissionObserved, RequestObserved, UsedAsking, ControlReceived;
    xredisrequestresult Query, Control;
} xredisclusterrouteresult;
XRT_EXTERN_C_BEGIN
/* Defaults: 5s total, 5ms admission wait, 8 redirects, 1024 waits/arguments,
 * 1MiB encoded command, 1024-byte host, 2MiB controller storage. GetPool required.
 * Timeout0 is due; maximum24h, delay1..60000ms, redirects<=1024/waits<=65536,
 * arguments1..65536, host1..65536, command<=64MiB/storage<=128MiB. */
XREDIS_API bool xredisClusterRouteConfigInit(xredisclusterrouteconfig* Config);
/* Nonblocking construction. Slot is explicit: caller computes it and ensures
 * all command keys belong to that slot. Master ONLINE/UNREPORTED only; no replica
 * fallback. SHARDS chooses the explicit TCP/TLS port; SLOTS uses its query's
 * transport-specific Port. SOURCE uses OriginHost from that topology query;
 * UNKNOWN, unavailable port or missing slot finishes NO_ROUTE without a provider.
 * One bounded allocation copies descriptors/payload and owns current/last-attempt
 * host buffers sized for the initial endpoint. A longer redirect host grows
 * them once, bounded by MaxHostBytes/MaxStorageBytes; that cold allocation can
 * finish MEMORY/LIMIT without another business attempt. Address is the current
 * candidate; AttemptAddress belongs to the
 * last accepted native attempt and its observed Query/Control outcomes.
 * Topology/config/input need live only for this API; Data/code until DONE or
 * Destroy. Copies add real cost versus direct native calls. No worker, timer,
 * proxy Future or engine pin. Output changes only on ACCEPTED, including a
 * locally terminal NO_ROUTE/TIMEOUT. Failure consumes nothing. */
XREDIS_API xredisadmission xredisClusterRouteCreate(const xredisclustertopology* Topology,
    xbytesview OriginHost, uint16 Slot, const xbytesview* Arguments, size_t Count,
    const xredisclusterrouteconfig* Config, xredisclusterroute** Route);
/* Unique caller serialization for every API; never block a worker. Each Step
 * advances one bounded phase. MORE schedules another turn; WAIT observes Pending
 * or NextWake, whichever is earlier. Do not busy-poll. One original monotonic
 * deadline includes construction, caller scheduling, provider/admission waits
 * and every redirected native query. Native attempts inherit that exact deadline.
 * Only a well-formed matching-slot MOVED/ASK server reply can trigger another
 * attempt; ASK uses adjacent native ASKING/query. Control must succeed before
 * following another redirect. Other server/transport outcomes are final.
 * A MOVED can inform provider cache/refresh policy; this controller never mutates
 * its input snapshot. RetryDelay/MaxWaits bound nonaccepted provider/overload wait.
 * Caller-owned pools keep their independent maintenance lifetime. */
XREDIS_API xredisclusterroutestep xredisClusterRouteStep(xredisclusterroute* Route);
/* Retain the current native wait handle in the NATIVE phase, even if it has
 * just resolved. This closes the completion window between WAIT/NextWake and
 * subscribing to that handle. NULL during provider/backoff or after DONE. */
XREDIS_API xfuture* xredisClusterRoutePending(xredisclusterroute* Route);
/* Absolute xrtTimer seconds; zero at DONE. Pending completion may wake earlier. */
XREDIS_API double xredisClusterRouteNextWake(xredisclusterroute* Route);
XREDIS_API bool xredisClusterRouteResult(xredisclusterroute* Route, xredisclusterrouteresult* Result);
/* At DONE, retain the last accepted native attempt, or NULL if none. It can
 * still be pending after a parent stop. It is not a proxy success Future. */
XREDIS_API xfuture* xredisClusterRouteResultFuture(xredisclusterroute* Route);
/* Immediate nonblocking logical stop, asks the current native operation to
 * cancel. Independent native physical FIFO/storage duties survive this object.
 * Retained DONE metadata holds no pool/engine/context reference. */
XREDIS_API bool xredisClusterRouteCancel(xredisclusterroute* Route);
XREDIS_API void xredisClusterRouteDestroy(xredisclusterroute* Route);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/cluster_nodes.h */
/* ========================================================================== */

#ifndef XREDIS_CLUSTER_NODES_H
#define XREDIS_CLUSTER_NODES_H
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_NODES) && \
    (!defined(XREDIS_FEATURE_XREDIS_CLUSTER_ROUTE) || !defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_CONNECT))
#error "Cluster nodes require routes and maintained shared pools"
#endif
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_NODES)
typedef struct xredisclusternodes xredisclusternodes;
typedef enum xredisclusternodesstate {XREDIS_CLUSTER_NODES_OPEN,XREDIS_CLUSTER_NODES_CLOSING,XREDIS_CLUSTER_NODES_CLOSED} xredisclusternodesstate;
/* Runs on each lookup caller outside gates, including cache hits. Nonblocking,
 * thread-safe; immutable profile/authority for this cache. May inspect/close
 * Cache, but must not recursively call GetPool. No ownership transfer. */
typedef bool (*xredisclusterauthorizeproc)(const xredisclusteraddress* Address,ptr Data);
/* Runs on each node's pool worker; different nodes may call concurrently.
 * Same managed-client factory rules as PoolConnect: private reference transfers
 * on success only. Address borrows a stable copied key; Slot/Reason are zero.
 * Honor exact transport/endpoint/credentials/session profile and total per-
 * attempt Timeout; never block. Client setup may outlive one caller's route. */
typedef bool (*xredisclusterconnectproc)(const xredisclusteraddress* Address,
    xnetengine* Engine,size_t Slot,uint64 Attempt,int64 Timeout,ptr Data,xredisclient** Client);
typedef struct xredisclusternodesconfig {
    size_t Size,MaxNodes,MaxHostBytes,MaxStorageBytes,ConnectionsPerNode,MaxConnections,MaxLookups;
    xredispoolconfig Pool;
    xredisprotocol Protocol;size_t MaxConnecting;
    int64 AttemptTimeout,RetryMin,RetryMax;
    xredisclusterauthorizeproc Authorize;xredisclusterconnectproc Connect;
    xredispoolconnectdrop Drop;ptr Data;
} xredisclusternodesconfig;
typedef struct xredisclusternodesinfo {
    size_t Size;xredisclusternodesstate State;
    size_t Capacity,Nodes,Opening,APIs,StorageBytes,ConnectionBound,ConnectingBound,LookupBound;
    uint64 RequestBound,CommandBytesBound,Hits,Misses,Waits,Denied,Created,Retired,Failures;
} xredisclusternodesinfo;
XRT_EXTERN_C_BEGIN
/* Defaults:64 nodes,1024-byte hosts,1MiB controller storage,1 connection/node,
 * 64 aggregate connections,1024 concurrent lookups; native pool defaults; RESP3,4 attempts/node,
 * 5s setup and100ms..10s retry. Authorize/Connect mandatory. Fixed limits:
 * nodes/connections/lookups<=65536,host<=65536,storage<=128MiB. Aggregate native quota
 * bounds are MaxNodes times the per-node Pool limits, with checked arithmetic. */
XREDIS_API bool xredisClusterNodesConfigInit(xredisclusternodesconfig* Config);
/* One preallocation holds hash buckets/slots/copied hosts. Does not start
 * connections or call callbacks. ACCEPTED consumes Data/Drop; failure preserves
 * them and output. Cache pins Engine until Closed; close before releasing.
 * All APIs are thread-safe, each caller holds a retained cache reference. */
XREDIS_API xredisadmission xredisClusterNodesCreate(xnetengine* Engine,uint64 Affinity,
    const xredisclusternodesconfig* Config,xredisclusternodes** Cache);
XREDIS_API bool xredisClusterNodesRetain(xredisclusternodes* Cache);
XREDIS_API void xredisClusterNodesRelease(xredisclusternodes* Cache);
/* Direct Route.GetPool adapter (Data=Cache). Exact copied host/port/TLS key;
 * Slot/Reason do not create duplicate node pools. Authorization runs on every
 * call. One concurrent creator per key; others WAIT. Hits allocate nothing and
 * transfer one pool ref on READY; all other outcomes preserve output. READY
 * means a maintained pool exists, not that its members are already ready.
 * Native overload waits are bounded by the route's original deadline.
 * Remaining0 performs no setup. Shared maintenance has independent bounded
 * attempts and lasts until node retirement/cache close, not caller timeout.
 * Capacity includes opening/retiring nodes; MaxLookups bounds concurrent
 * authorizations/constructors (Close can add one mechanical API duty). Full
 * capacity/concurrency returns WAIT without invoking policy/setup; no LRU
 * aborts active business. Retired slots reuse only after physical PoolClosed.
 * Endpoint policy/factory code/data live until Drop. No hidden DNS/TLS fallback. */
XREDIS_API xredisclusterpoolstatus xredisClusterNodesGetPool(const xredisclusteraddress* Address,
    int64 Remaining,ptr Cache,xredispool** Pool);
/* Explicit endpoint fence, idempotent if absent. Stops new lookups for this key
 * until its physical close retires; closes accepted native work with the usual
 * typed/possibly uncertain outcomes. Retained selected pools may reject later
 * admissions CLOSED. Retirement never replays accepted business. */
XREDIS_API bool xredisClusterNodesRetire(xredisclusternodes* Cache,const xredisclusteraddress* Address);
XREDIS_API bool xredisClusterNodesInfo(xredisclusternodes* Cache,xredisclusternodesinfo* Info);
/* Nonblocking idempotent seal; closes every current/constructing node. Closed
 * resolves after constructors/authorization calls and all native pool close
 * notifications retire, Drop runs once outside gates, and Engine pin is gone.
 * Retained closed metadata/Future pins no context/pool/engine. Join Engine before
 * unloading callback code. Release alone does not close an open cache. */
XREDIS_API bool xredisClusterNodesClose(xredisclusternodes* Cache);
XREDIS_API xfuture* xredisClusterNodesClosed(xredisclusternodes* Cache);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/cluster_refresh.h */
/* ========================================================================== */

#ifndef XREDIS_CLUSTER_REFRESH_H
#define XREDIS_CLUSTER_REFRESH_H
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_REFRESH) && !defined(XREDIS_FEATURE_XREDIS_CLUSTER_NODES)
#error "Cluster refresh requires maintained node pools"
#endif
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_REFRESH)
typedef struct xredisclusterrefresh xredisclusterrefresh;
typedef struct xredisclustermap xredisclustermap;
typedef enum xredisclusterrefreshstatus {
    XREDIS_REFRESH_OK,XREDIS_REFRESH_STALE,XREDIS_REFRESH_TIMEOUT,XREDIS_REFRESH_CLOSED,
    XREDIS_REFRESH_POLICY,XREDIS_REFRESH_OVERLOADED,XREDIS_REFRESH_TRANSPORT,
    XREDIS_REFRESH_SERVER,XREDIS_REFRESH_PROTOCOL,XREDIS_REFRESH_LIMIT,XREDIS_REFRESH_MEMORY
} xredisclusterrefreshstatus;
typedef enum xredisclusterrefreshstep {
    XREDIS_REFRESH_STEP_ERROR=-1,XREDIS_REFRESH_IDLE,XREDIS_REFRESH_WAIT,
    XREDIS_REFRESH_MORE,XREDIS_REFRESH_DONE,XREDIS_REFRESH_BUSY
} xredisclusterrefreshstep;
typedef struct xredisclusterrefreshconfig {
    size_t Size,MaxSeeds,MaxHostBytes,MaxControlBytes,MaxSnapshotBytes,MaxAttempts,MaxWaits;
    int64 Timeout,RetryDelay;xredisclusterformat Format;xredisclusterlimits Limits;
} xredisclusterrefreshconfig;
/* All views borrow a live Map reference. Origin is the copied query endpoint;
 * its transport defines legacy SLOTS port interpretation and SOURCE fallback.
 * Epoch is a local invalidation counter, not a Redis configuration epoch. */
typedef struct xredisclustermapinfo {
    size_t Size;uint64 Generation,Epoch;xredisclusteraddress Origin;
    xredisclustertopology* Topology;size_t StorageBytes;
} xredisclustermapinfo;
/* Immutable Future value. Native/Map and Query reply/cause borrow that Future;
 * retain either object before escaping. Native can remain pending after stop.
 * QueryObserved never changes after publication. STALE means invalidation raced
 * refresh, so publication does not certify current demand. OK only certifies
 * that demand at publication; later invalidation can stale the same Map. */
typedef struct xredisclusterrefreshresult {
    size_t Size;xredisclusterrefreshstatus Status;uint64 Epoch,Generation;
    size_t Attempts,Waits,ProviderCalls;bool Published,StaleAtPublish,QueryObserved;
    xredisclusterpoolstatus Provider;xredisadmission Admission;xredisclusterstatus Parse;
    xredisclusteraddress Origin;xredisrequestresult Query;xfuture* Native;xredisclustermap* Map;
} xredisclusterrefreshresult;
typedef struct xredisclusterrefreshinfo {
    size_t Size;bool Closing,Closed,Active,Busy,Dirty;
    uint64 DesiredEpoch,PublishedEpoch,Generation,Started,Coalesced,CacheHits,Completed;
    xredisclusterrefreshstatus LastStatus;size_t Seeds,ControlBytes;
} xredisclusterrefreshinfo;
XRT_EXTERN_C_BEGIN
/* Defaults:16 seeds,1024-byte host,1MiB control,32MiB per retained snapshot,
 * 8 native metadata attempts,1024 waits,5s total/5ms delay,SHARDS/full coverage.
 * Bounds:seeds<=1024,host<=65536,control/snapshot<=128MiB,attempts<=1024,
 * waits<=65536,timeout0..24h,delay1..60000ms. Explicit SLOTS is supported;
 * no implicit command/ACL/transport fallback. */
XREDIS_API bool xredisClusterRefreshConfigInit(xredisclusterrefreshconfig* Config);
/* Copies bounded explicit host/port/TLS seeds; Slot/Reason must be zero. Retains
 * Nodes on success; never closes it. No policy/I/O/callback runs during creation.
 * Failure preserves output and ownership. Close before final Release. */
XREDIS_API xredisadmission xredisClusterRefreshCreate(xredisclusternodes* Nodes,
    const xredisclusteraddress* Seeds,size_t Count,const xredisclusterrefreshconfig* Config,
    xredisclusterrefresh** Refresh);
XREDIS_API bool xredisClusterRefreshRetain(xredisclusterrefresh* Refresh);
XREDIS_API void xredisClusterRefreshRelease(xredisclusterrefresh* Refresh);
/* Concurrent callers share one active result Future. Fresh !Force reuses the
 * completed result. Force starts a new local epoch when otherwise fresh; it
 * does not alter an already active cohort. Invalidate explicitly records newer
 * demand. One creation-entry deadline covers setup/admission/retry/query/parse.
 * Joining or cancelling an observer never extends/cancels the cohort. A waiter
 * can use its own WaitFor timeout. Start performs no policy/I/O; a serialized
 * driver must keep stepping. During cohort creation or result-publication reentry Start can return
 * OVERLOADED. Output changes only on ACCEPTED. */
XREDIS_API xredisadmission xredisClusterRefreshStart(xredisclusterrefresh* Refresh,bool Force,xfuture** Future);
XREDIS_API bool xredisClusterRefreshInvalidate(xredisclusterrefresh* Refresh);
/* Replace future query candidates with up to MaxSeeds distinct eligible
 * masters from Map, then its actual query origin and distinct original explicit
 * seeds as fallback (total <=2*MaxSeeds+1). SHARDS uses Map.Origin.Tls and its explicit port; SLOTS keeps
 * its query transport. SOURCE uses the copied query host. Unknown/unhealthy/
 * unavailable/oversize endpoints are skipped. Every later lookup still runs
 * Nodes authorization. No policy/I/O/allocation here; bounded preallocated
 * hash/plan storage and two passes avoid partial replacement.
 * Requires no active cohort/creation/driver phase; otherwise OVERLOADED.
 * Concurrent Close is included in Busy and can return CLOSED. Does not change
 * topology, generation or demand epoch; existing outcomes remain immutable.
 * A failed cohort advances the next start candidate; successful query origin
 * is preferred when it remains an eligible discovered master. */
XREDIS_API xredisadmission xredisClusterRefreshDiscover(xredisclusterrefresh* Refresh,
    const xredisclustermap* Map);
/* Start/Invalidate/Snapshot/Info/Close are thread-safe. Step/Pending/NextWake
 * have ONE caller-serialized driver; no worker blocking. Step makes one bounded
 * phase. BUSY reports another active Step/publication, not permission to spin.
 * WAIT observes Pending (including just-resolved native handles) or NextWake.
 * Only these fixed read-only metadata commands retry transport failures; no
 * business is submitted/replayed. Denied/server/malformed replies stop without
 * replacing an existing view. Original deadlines survive seed failover. */
XREDIS_API xredisclusterrefreshstep xredisClusterRefreshStep(xredisclusterrefresh* Refresh);
XREDIS_API xfuture* xredisClusterRefreshPending(xredisclusterrefresh* Refresh);
XREDIS_API double xredisClusterRefreshNextWake(xredisclusterrefresh* Refresh);
XREDIS_API xredisclusterstatus xredisClusterRefreshSnapshot(xredisclusterrefresh* Refresh,xredisclustermap** Map,bool* Stale);
XREDIS_API bool xredisClusterRefreshInfo(xredisclusterrefresh* Refresh,xredisclusterrefreshinfo* Info);
/* Nonblocking seal. Includes any running provider/driver phase, stops the last
 * native operation and publishes typed CLOSED. Its physical native duties are
 * independent. Closed releases coordinator-owned Nodes/view/result refs. Retained maps and
 * settled outcomes own no coordinator/cache/engine; pending native work keeps
 * independent physical resources. Close does not certify physical engine
 * shutdown: the owner separately closes Nodes and joins its engine. */
XREDIS_API bool xredisClusterRefreshClose(xredisclusterrefresh* Refresh);
XREDIS_API xfuture* xredisClusterRefreshClosed(xredisclusterrefresh* Refresh);
XREDIS_API bool xredisClusterMapRetain(xredisclustermap* Map);
XREDIS_API void xredisClusterMapRelease(xredisclustermap* Map);
XREDIS_API bool xredisClusterMapInfo(const xredisclustermap* Map,xredisclustermapinfo* Info);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/cluster_client.h */
/* ========================================================================== */

#ifndef XREDIS_CLUSTER_CLIENT_H
#define XREDIS_CLUSTER_CLIENT_H
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_CLIENT) && !defined(XREDIS_FEATURE_XREDIS_CLUSTER_REFRESH)
#error "Cluster client requires metadata refresh and maintained nodes"
#endif
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_CLIENT)
typedef struct xredisclusterclient xredisclusterclient;
typedef enum xredisclusterclientstatus {
    XREDIS_CLUSTER_CLIENT_COMPLETE,XREDIS_CLUSTER_CLIENT_TIMEOUT,
    XREDIS_CLUSTER_CLIENT_CANCELLED,XREDIS_CLUSTER_CLIENT_CLOSED,
    XREDIS_CLUSTER_CLIENT_MEMORY,XREDIS_CLUSTER_CLIENT_INTERNAL,XREDIS_CLUSTER_CLIENT_LIMIT,
    XREDIS_CLUSTER_CLIENT_RESOURCE
} xredisclusterclientstatus;
typedef struct xredisclusterclientconfig {
    size_t Size,MaxRequests,MaxCommandBytes,MaxStorageBytes,MaxSteps;
    int64 RefreshMin,RefreshRetryMax;
    xredisclusterrefreshconfig Refresh;xredisclusterrouteconfig Route;
} xredisclusterclientconfig;
/* COMPLETE is a terminal route, including server/uncertain transport failure.
 * Inspect Route.Status and Route.Query.Status/Delivery. Native and every Route
 * view borrow this result Future. Native can remain pending after a stop;
 * RouteObserved/Route never change afterwards. No outcome authorizes replay. */
typedef struct xredisclusterclientresult {
    size_t Size;xredisclusterclientstatus Status;uint64 Generation;
    double QueueSeconds,TotalSeconds;bool RouteObserved,RefreshObserved;
    xredisclusterrefreshstatus RefreshStatus;xredisclusterrouteresult Route;
    xfuture* Native;
} xredisclusterclientresult;
typedef struct xredisclusterclientready {
    xredisclusterclientstatus Status;uint64 Generation,Epoch;
} xredisclusterclientready;
typedef struct xredisclusterclientinfo {
    size_t Size;bool Closing,Closed,Dirty;
    size_t Requests,APIs,CommandBytes,StorageBytes,HeapEntries;
    uint64 Accepted,Completed,Rejected,Generation,Epoch,Refreshes,RefreshCompleted,RefreshFailures,Moved;
    xredisclusterrefreshstatus LastRefresh;xredisclusterclientstatus StopStatus;double NextRefresh;
} xredisclusterclientinfo;
XRT_EXTERN_C_BEGIN
/* Defaults:1024 active/reserved requests,16MiB encoded commands,64MiB request
 * controller reservations,64 request phases/turn plus one metadata phase,
 * 100ms..10s metadata refresh backoff.
 * Requests<=65536,byte caps<=1GiB,steps1..4096,delays1..60000ms.
 * Route.GetPool/Data/Tls must remain zero: exact transport comes from the query
 * origin. Route.Timeout is the default Call timeout; a Call supplies its own.
 * Node/native/Future limits remain independent. Snapshot costs are separate. */
XREDIS_API bool xredisClusterClientConfigInit(xredisclusterclientconfig* Config);
/* Copies explicit seeds through a private refresh coordinator; retains shared
 * Nodes and pins the progress Engine. Nodes may use another Engine. Failure
 * preserves output/Nodes/factory ownership. No policy/I/O inline. Once accepted,
 * one existing worker drives bootstrap, routes, watch notifications and timers.
 * Metadata transport failures retry with bounded backoff until Close. */
XREDIS_API xredisadmission xredisClusterClientCreate(xnetengine* Engine,uint64 Affinity,
    xredisclusternodes* Nodes,const xredisclusteraddress* Seeds,size_t Count,
    const xredisclusterclientconfig* Config,xredisclusterclient** Client);
XREDIS_API bool xredisClusterClientRetain(xredisclusterclient* Client);
XREDIS_API void xredisClusterClientRelease(xredisclusterclient* Client);
/* Thread-safe nonblocking admission, explicit slot/all-keys responsibility.
 * Timeout-1 uses Route.Timeout;0 is due;maximum24h. One entry deadline includes reservation/allocation,
 * queue/bootstrap and all native redirects. Warm admission copies inputs once
 * into its route; bootstrap queues add a cold copy. No accepted business replay
 * except validated matching-slot MOVED/ASK. Parent cancellation promptly stops
 * only this operation. Outputs change only on ACCEPTED; overload consumes none.
 * Caller-retained result/route/native storage leaves client quotas at logical
 * publication and stays caller-owned. Do not wait on this Engine's worker. */
XREDIS_API xredisadmission xredisClusterClientCall(xredisclusterclient* Client,
    uint16 Slot,const xbytesview* Arguments,size_t Count,int64 Timeout,xfuture** Future);
/* Ready resolves on the first valid map, or typed stop; metadata failures are
 * visible in Info while bootstrap retries. Acquired views are retained Maps.
 * MOVED from the current generation invalidates once per generation and starts
 * coalesced full-map refresh; ASK never changes the shared map. Old valid maps
 * remain usable during failed refresh. */
XREDIS_API xfuture* xredisClusterClientReady(xredisclusterclient* Client);
XREDIS_API xredisclusterstatus xredisClusterClientSnapshot(xredisclusterclient* Client,
    xredisclustermap** Map,bool* Stale);
XREDIS_API bool xredisClusterClientInvalidate(xredisclusterclient* Client);
XREDIS_API bool xredisClusterClientInfo(xredisclusterclient* Client,xredisclusterclientinfo* Info);
/* Nonblocking seal; stops this client's routes/refresh, retires its own watches,
 * posts/timer and Engine pin before Closed. Shared Nodes is never closed here.
 * Its owner separately closes Nodes and joins Engines for native physical FIFO
 * retirement/factory release. Pending native work can outlive parent results.
 * Close before final Release; keep code loaded through all result destruction. */
XREDIS_API bool xredisClusterClientClose(xredisclusterclient* Client);
XREDIS_API xfuture* xredisClusterClientClosed(xredisclusterclient* Client);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/command_keys.h */
/* ========================================================================== */

#ifndef XREDIS_COMMAND_KEYS_H
#define XREDIS_COMMAND_KEYS_H
#if defined(XREDIS_FEATURE_XREDIS_COMMAND_KEYS) && !defined(XREDIS_FEATURE_XREDIS_REPLY)
#error "Command key specifications require immutable replies"
#endif
#if defined(XREDIS_FEATURE_XREDIS_COMMAND_KEYS)
typedef struct xrediscommandcatalog xrediscommandcatalog;
typedef enum xrediskeysstatus {
    XREDIS_KEYS_ERROR=-1,XREDIS_KEYS_MALFORMED=-2,XREDIS_KEYS_LIMIT=-3,
    XREDIS_KEYS_MEMORY=-4,XREDIS_KEYS_SERVER_ERROR=-5,
    XREDIS_KEYS_OK=1,XREDIS_KEYS_NONE,XREDIS_KEYS_UNKNOWN,
    XREDIS_KEYS_UNSUPPORTED,XREDIS_KEYS_CROSS_SLOT,XREDIS_KEYS_ARGUMENTS
} xrediskeysstatus;
typedef enum xrediskeyflag {
    XREDIS_KEY_RO=1,XREDIS_KEY_RW=2,XREDIS_KEY_OW=4,XREDIS_KEY_RM=8,
    XREDIS_KEY_ACCESS=16,XREDIS_KEY_UPDATE=32,XREDIS_KEY_INSERT=64,
    XREDIS_KEY_DELETE=128,XREDIS_KEY_NOT_KEY=256,XREDIS_KEY_VARIABLE=512
} xrediskeyflag;
typedef struct xrediscommandkeylimits {
    size_t Size,MaxCommands,MaxSpecifications,MaxNameBytes,MaxDepth,
        MaxReplyNodes,MaxPayloadBytes,MaxOwnedBytes,MaxArguments,MaxArgumentBytes,MaxKeys,
        MaxKeyBytes,MaxSearchWork;
} xrediscommandkeylimits;
typedef struct xrediscommandcataloginfo {
    size_t Size,Commands,Specifications,LookupDepth,StorageBytes,RetainedReplyBytes;
} xrediscommandcataloginfo;
/* Flags are specification possibilities; VARIABLE is not an exact access decision.
 * NOT_KEY still participates in Cluster slot calculation (for example channels).
 * Specification order and repeated argument occurrences are preserved. */
typedef struct xrediscommandkey {size_t Argument;uint32 Flags;} xrediscommandkey;
typedef struct xrediscommandkeyresult {
    size_t Size,Command,Keys;uint16 Slot;bool HasSlot,Legacy,VariableFlags;
} xrediscommandkeyresult;
XRT_EXTERN_C_BEGIN
/* Defaults:8192 commands,32768 specs,256-byte names,16 levels,1M reply nodes,
 * 16MiB metadata payload,64MiB retained storage,65536 arguments/keys,64MiB args
 * and extracted key bytes,1M search inspections/matched bytes per pass.
 * Hard maxima:65535 commands,262144 specs,4096-byte names,16 levels,
 * 1M arguments/keys,1GiB byte caps. Zero is a limit, except Size. */
XREDIS_API bool xredisCommandKeyLimitsInit(xrediscommandkeylimits* Limits);
/* Complete COMMAND/COMMAND INFO collection, including nested subcommands and null
 * unknown entries. RESP3 sets are supported for non-positional collections;
 * command rows remain positional arrays. Retains Reply once; one additional immutable allocation holds
 * commands/specs/name hash. Two passes validate before publication. Legacy six-
 * field entries are supported; movable/incomplete/unknown key rules require a
 * server oracle. Unknown future metadata fields are preserved by the Reply.
 * No I/O. Failure leaves Catalog and ownership unchanged; OK includes empty. */
XREDIS_API xrediskeysstatus xredisCommandCatalogCreate(xredisreply* Reply,
    const xrediscommandkeylimits* Limits,xrediscommandcatalog** Catalog);
XREDIS_API bool xredisCommandCatalogRetain(xrediscommandcatalog* Catalog);
XREDIS_API void xredisCommandCatalogRelease(xrediscommandcatalog* Catalog);
XREDIS_API bool xredisCommandCatalogInfo(const xrediscommandcatalog* Catalog,
    xrediscommandcataloginfo* Info);
/* Arguments include the command and subcommand tokens. ASCII command/keyword
 * matching is case-insensitive; keys are binary and use Redis hash tags.
 * No allocation, lock, refcount, I/O or input retention. Immutable owners can
 * evaluate concurrently. NULL Keys/zero Capacity computes count/slot alone.
 * OK/NONE alone publish Result and optional Keys, atomically after validation;
 * short capacity and every other status leave all outputs unchanged.
 * UNKNOWN means absent metadata; UNSUPPORTED means full extraction needs
 * COMMAND GETKEYS/GETKEYSANDFLAGS (including incomplete/unknown specs/flags).
 * Neither may be treated as NONE or routed using a guessed first argument.
 * CROSS_SLOT checks every extracted routing key, including NOT_KEY. ARGUMENTS
 * checks arity and extraction indices/counts, not the command's entire grammar.
 * Flags may overapproximate access; they do not authorize replica routing.
 * All outputs must be disjoint from catalog/reply, descriptors and input bytes.
 * Successful and typed non-API outcomes preserve the ambient error. */
XREDIS_API xrediskeysstatus xredisCommandCatalogEvaluate(const xrediscommandcatalog* Catalog,
    const xbytesview* Arguments,size_t Count,xrediscommandkey* Keys,size_t Capacity,
    xrediscommandkeyresult* Result);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/cluster_command.h */
/* ========================================================================== */

#ifndef XREDIS_CLUSTER_COMMAND_H
#define XREDIS_CLUSTER_COMMAND_H
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND) && (!defined(XREDIS_FEATURE_XREDIS_CLUSTER_CLIENT) || !defined(XREDIS_FEATURE_XREDIS_COMMAND_KEYS))
#error "Cluster commands require the asynchronous client and command key catalogue"
#endif
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND)
typedef enum xredisclustercommandadmission {
    XREDIS_CLUSTER_COMMAND_ERROR=-1,XREDIS_CLUSTER_COMMAND_ACCEPTED=0,
    XREDIS_CLUSTER_COMMAND_OVERLOADED,XREDIS_CLUSTER_COMMAND_CLOSED,
    XREDIS_CLUSTER_COMMAND_UNKNOWN,XREDIS_CLUSTER_COMMAND_UNSUPPORTED,
    XREDIS_CLUSTER_COMMAND_CROSS_SLOT,XREDIS_CLUSTER_COMMAND_ARGUMENTS,
    XREDIS_CLUSTER_COMMAND_LIMIT,XREDIS_CLUSTER_COMMAND_KEYLESS
} xredisclustercommandadmission;
#define XREDIS_CLUSTER_COMMAND_NO_SLOT UINT16_MAX
XRT_EXTERN_C_BEGIN
/* Nonblocking metadata-checked Call. All routing keys must agree on one slot.
 * KeylessSlot is an explicit0..16383 fallback for NONE; NO_SLOT returns KEYLESS
 * without admission. It is ignored when actual routing keys exist. Unknown,
 * incomplete/future, cross-slot, arity/extraction or catalogue/client argument
 * limit failures return their typed admission without network work. No metadata
 * fetch, server fallback, replica inference or accepted-business replay is added.
 * Preflight refusals leave client admission counters unchanged; native capacity/
 * close decisions use existing counters and map to OVERLOADED/CLOSED/ERROR.
 * Only ACCEPTED publishes one existing xredisclusterclientresult Future.
 * Every other result preserves Future, inputs and ownership. Catalogue and
 * arguments are borrowed only during this function; no catalogue ref is held
 * afterward. Async command data is copied by the existing native Call path.
 * One function-entry monotonic deadline covers metadata/spans/slot validation,
 * allocation, bootstrap, queue/provider waits and redirects. Timeout-1 uses
 * Route.Timeout;0 is due;maximum24h. Metadata itself is a bounded synchronous
 * CPU phase, not an interruptible task; configure catalogue work/byte caps.
 * Future must be disjoint from Client/current map/Catalogue/Reply/input storage.
 * Caller holds stable Client/Catalogue owners throughout the call. Do not wait
 * on the progress Engine's workers. Existing close/native physical duties and
 * retained outcome guarantees are unchanged. Typed preflight refusals preserve
 * ambient error; ERROR reports invalid API/native admission failure as before. */
XREDIS_API xredisclustercommandadmission xredisClusterClientCallCommand(
    xredisclusterclient* Client,const xrediscommandcatalog* Catalogue,
    const xbytesview* Arguments,size_t Count,uint16 KeylessSlot,int64 Timeout,
    xfuture** Future);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis.h */
/* ========================================================================== */

#ifndef XREDIS_H
#define XREDIS_H


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

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_ROLE)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_ROLE"
#endif
#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_ROLE_CLIENT)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_ROLE_CLIENT"
#endif
#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_TRANSACTION"
#endif

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_WATCH)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_WATCH"
#endif

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_POOL)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_POOL"
#endif
#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_POOL_SHARED)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_POOL_SHARED"
#endif
#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_CONNECT)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_POOL_SHARED_CONNECT"
#endif



#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_POOL_TRANSACTION)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_POOL_TRANSACTION"
#endif
#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_POOL_WATCH"
#endif
#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_POOL_CONNECT"
#endif

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_TRACKING)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_TRACKING"
#endif

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_PUBSUB)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_PUBSUB"
#endif

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_SUBSCRIBER"
#endif

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY"
#endif

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS"
#endif

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION"
#endif

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_SENTINEL)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_SENTINEL"
#endif

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_SENTINEL_POOL)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_SENTINEL_POOL"
#endif

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_CLUSTER_TOPOLOGY)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_CLUSTER_TOPOLOGY"
#endif
#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_CLUSTER_ROUTE)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_CLUSTER_ROUTE"
#endif


#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_POOL_ASKING"
#endif
#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_ASKING)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_POOL_SHARED_ASKING"
#endif


#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_CLUSTER_NODES)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_CLUSTER_NODES"
#endif

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_CLUSTER_REFRESH)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_CLUSTER_REFRESH"
#endif

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_CLUSTER_CLIENT)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_CLUSTER_CLIENT"
#endif

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_COMMAND_KEYS)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_COMMAND_KEYS"
#endif

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND"
#endif

#endif

#endif

#if defined(XREDIS_IMPLEMENTATION) && !defined(XREDIS_IMPLEMENTATION_ONCE)
#define XREDIS_IMPLEMENTATION_ONCE 1


/* ========================================================================== */
/* internal: extlibs/xredis/src/internal/cluster_reply.h */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_REPLY) || \
	defined(XREDIS_FEATURE_XREDIS_CLUSTER_TOPOLOGY) || \
	defined(XREDIS_FEATURE_XREDIS_CLUSTER_ROUTE) || \
	defined(XREDIS_FEATURE_XREDIS_COMMAND_KEYS)
#ifndef XREDIS_INTERNAL_CLUSTER_REPLY_H
#define XREDIS_INTERNAL_CLUSTER_REPLY_H
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_TOPOLOGY) || defined(XREDIS_FEATURE_XREDIS_COMMAND_KEYS)
bool __xredisClusterReplyOutput(const xredisreply* Reply,const void* Output,size_t Bytes);
#endif
#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xredis/src/internal/client_connect.h */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_CLIENT) || \
	defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
#ifndef XREDIS_CLIENT_CONNECT_INTERNAL_H
#define XREDIS_CLIENT_CONNECT_INTERNAL_H
typedef struct xredisexclusiveowner xredisexclusiveowner;
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
enum { XREDIS_OWNER_STARTING, XREDIS_OWNER_ACTIVE, XREDIS_OWNER_SEALED,
    XREDIS_OWNER_CLOSING, XREDIS_OWNER_CLOSED };
#if defined(XREDIS_FEATURE_XREDIS_POOL)
typedef struct xredisownerquota {
    xatomic64 Requests, Bytes;
    xatomic32 Closing;
    uint64 MaxRequests, MaxBytes;
} xredisownerquota;
#endif
/* Embedded in a session or pool slot. Retain/Release are atomic-only; Wake is called
 * outside the client Gate. The owner holds a client ref and an engine Pin
 * until Unclaim, including the physical retirement of request constructors.
 * Deadline is published before ACTIVE, and changed only while sealed, idle
 * and without API constructors from the previous generation. */
struct xredisexclusiveowner {
    ptr Data;
    xatomic32 State;
    double Deadline;
    void (*Retain)(ptr Data);
    void (*Wake)(ptr Data);
    void (*Release)(ptr Data);
#if defined(XREDIS_FEATURE_XREDIS_POOL)
    xredisownerquota* Quota;
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
    xredisexclusiveowner* Parent;
    void (*Abandon)(ptr Data);
    xatomic32 Nested;
#endif
};
xredisadmission __xredisClientClaim(xredisclient* Client, xredisexclusiveowner* Owner,
    xnetengine** Engine, xnetworker** Worker, ptr Output);
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
xredisadmission __xredisClientClaimStarting(xredisclient* Client, xredisexclusiveowner* Owner, xnetengine* Engine);
#endif
bool __xredisClientUnclaim(xredisclient* Client, xredisexclusiveowner* Owner);
bool __xredisClientIsWorker(xredisclient* Client);
xredisadmission __xredisClientOwnedCall(xredisclient* Client, xredisexclusiveowner* Owner,
    const xbytesview* Arguments, size_t Count, int64 Timeout, double Entered,
    bool Control, xfuture** Future);
#if defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
xredisadmission __xredisClientOwnedAsking(xredisclient* Client,xredisexclusiveowner* Owner,
    const xbytesview* Arguments,size_t Count,int64 Timeout,double Entered,xfuture** Future);
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
xredisadmission __xredisClientClaimNested(xredisclient* Client, xredisexclusiveowner* Parent,
    xredisexclusiveowner* Owner, xnetengine** Engine, xnetworker** Worker, ptr Output);
bool __xredisClientUnclaimUnused(xredisclient* Client, xredisexclusiveowner* Owner);
bool __xredisClientOwnerReady(xredisclient* Client, xredisexclusiveowner* Parent, bool Abandon);
void __xredisClientChildDone(xredisexclusiveowner* Owner);
xredisadmission __xredisWatchBeginOwned(xredisclient* Client, xredisexclusiveowner* Parent,
    const xbytesview* Keys, size_t Count, const xrediswatchconfig* Config, xfuture** Future, double Entered);
#endif
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
xredisadmission __xredisClientTransactionAsync(xredisclient* Client,
    const xredistransactioncommand* Commands, size_t Count, const xredistransactionconfig* Config,
    int64 Timeout, double Entered, xfuture** Future);
xredisadmission __xredisClientTransaction(xredisclient* Client,
    const xredistransactioncommand* Commands, size_t Count, const xredistransactionconfig* Config,
    int64 Timeout, double Entered, xfuture** Future);
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
xredisadmission __xredisClientOwnedTransaction(xredisclient* Client, xredisexclusiveowner* Owner,
    const xredistransactioncommand* Commands, size_t Count, const xredistransactionconfig* Config,
    int64 Timeout, double Entered, xfuture** Future);
#endif
#endif
typedef struct xredisclienttlsconfig xredisclienttlsconfig;
typedef struct xtlsclientauthconfigv1 xtlsclientauthconfigv1;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
typedef struct xredistlsplan {
	xtlssession* Session;
	xtlsstreamconfig Config;
	size_t WriteBytes;
} xredistlsplan;
bool __xredisClientDialTls(xnetengine* Engine, xnetresolver* Resolver, xnetaddrlist* Addresses,
	xstrview Host, uint16 Port, uint64 Affinity, const xredisclientconfig* Config,
	const xredisclientinitconfig* Init, const xredisclienttlsconfig* TLS, double Entered, xredisclient** Result);
bool __xredisTlsPlanCreate(const xredisclienttlsconfig* TLS, const xtlsclientauthconfigv1* Auth,
	xstrview Host, xredisclient** Output, xredistlsplan* Plan);
#if defined(XREDIS_FEATURE_XREDIS_TLS_AUTH)
bool __xredisClientDialTlsAuthV1(xnetengine* Engine, xnetresolver* Resolver, xnetaddrlist* Addresses,
	xstrview Host, uint16 Port, uint64 Affinity, const xredisclientconfig* Config,
	const xredisclientinitconfig* Init, const xredisclienttlsconfig* TLS,
	const xtlsclientauthconfigv1* Auth, double Entered, xredisclient** Result);
#endif
/* Consumes TCP always, and Session only on true. Owning Worker only. */
bool __xredisClientConnectTls(xredisclient* Client, xnetstream* Transport, const xredistlsplan* Plan);
const xnetstreamevents* __xredisClientTlsPendingEvents(void);
#endif

/* Private cold connection-start handoff. No per-request dispatch, public
 * transport plugin or shared runtime. The descriptor lives in Data's owner.
 * Factory failure consumes nothing; success drops it at physical Close. */
typedef struct xredisconnectplan {
	ptr Data;
	bool (*Start)(ptr Data, xredisclient* Client, xnetengine* Engine, uint64 Affinity,
		const xnetstreamconfig* Config, const xnetstreamevents* Events);
	void (*Cancel)(ptr Data);
	void (*Drop)(ptr Data);
} xredisconnectplan;

#if defined(XREDIS_FEATURE_XREDIS_DIAL)
bool __xredisClientCreatePlanned(xnetengine* Engine, uint64 Affinity,
	const xredisclientconfig* Config, const xredisclientinitconfig* Init,
	double Entered, const xredisconnectplan* Plan, xredisclient** Result);
/* Owning Worker only. Done consumes any winning Stream reference, then the
 * source calls Retired as its final action before returning from its callback.
 * The queued client work cannot publish Close until that callback returns. */
void __xredisClientConnectDone(xredisclient* Client, xnetresult Result,
	xnetstream* Stream, const xerror* Cause);
void __xredisClientConnectRetired(xredisclient* Client);
#endif
#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xredis/src/internal/pool_admit.h */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_POOL) || \
	defined(XREDIS_FEATURE_XREDIS_POOL_ASKING) || \
	defined(XREDIS_FEATURE_XREDIS_POOL_SHARED) || \
	defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_ASKING) || \
	defined(XREDIS_FEATURE_XREDIS_CLUSTER_ROUTE) || \
	defined(XREDIS_FEATURE_XREDIS_CLUSTER_REFRESH)
#ifndef XREDIS_POOL_ADMIT_INTERNAL_H
#define XREDIS_POOL_ADMIT_INTERNAL_H
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED)
xredisadmission __xredisSharedPoolCreate(xnetengine* Engine, uint64 Affinity,
    xredisclient* const* Clients, size_t Count, const xredispoolconfig* Config, xredispool** Result);
xredisadmission __xredisSharedPoolCall(xredispool* Pool, const xbytesview* Arguments, size_t Count,
    int64 Timeout, xfuture** Future, double Entered, bool Sync);
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_ASKING)
xredisadmission __xredisSharedPoolAsking(xredispool* Pool, const xbytesview* Arguments, size_t Count,
    int64 Timeout, xfuture** Future, double Entered, bool Sync);
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_TRANSACTION)
xredisadmission __xredisLeaseTransaction(xredislease* Lease, const xredistransactioncommand* Commands,
    size_t Count, const xredistransactionconfig* Config, int64 Timeout, xfuture** Future, double Entered, bool Sync);
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
xredisadmission __xredisLeaseWatch(xredislease* Lease, const xbytesview* Keys, size_t Count,
    const xrediswatchconfig* Config, xfuture** Future, double Entered, bool Sync);
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
xredisadmission __xredisLeaseAsking(xredislease* Lease,const xbytesview* Arguments,size_t Count,
    int64 Timeout,xfuture** Future,double Entered,bool Sync);
#endif
#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xredis/src/internal/pool_connect_internal.h */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_POOL) || \
	defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_CONNECT)
#ifndef XREDIS_POOL_CONNECT_INTERNAL_H
#define XREDIS_POOL_CONNECT_INTERNAL_H
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
xredisadmission __xredisPoolConnect(xnetengine* Engine, uint64 Affinity, size_t Count,
    const xredispoolconfig* Config, const xredispoolconnectconfig* Connect, xredispool** Result);
bool __xredisPoolConnectInfo(xredispool* Pool, xredispoolconnectinfo* Info);
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_CONNECT)
xredisadmission __xredisSharedPoolConnect(xnetengine* Engine, uint64 Affinity, size_t Count,
    const xredispoolconfig* Config, const xredispoolconnectconfig* Connect, xredispool** Result);
#endif
#endif
#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xredis/src/internal/subscription_replay.h */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS)
#ifndef XREDIS_SUBSCRIPTION_REPLAY_INTERNAL_H
#define XREDIS_SUBSCRIPTION_REPLAY_INTERNAL_H
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS)
xredisadmission __xredisSubscriberReplayStart(xredisclient* Client,const xredissubscriberconfig* Config,ptr Owner);
void __xredisSubscriberReplayRelease(xredisclient* Client,ptr Owner);
xredisadmission __xredisSubscriberCommandBudget(xredisclient* Client,xredispubsubcommand Command,
    const xbytesview* Names,size_t Count,int64 Timeout,double Entered,ptr Owner,xfuture** Future);
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION)
bool __xredisSubscriptionsFits(xredissubscriptions* Store,const xredissubscriberconfig* Config);
bool __xredisSubscriptionRegistryBounds(const xredissubscriberconfig* Config);
bool __xredisSubscriptionClientProfile(xredisclient* Client,xnetengine* Engine);
xredisadmission __xredisSubscriptionsClaim(xredissubscriptions* Store,ptr Owner,ptr Output);
void __xredisSubscriptionsUnclaim(xredissubscriptions* Store,ptr Owner);
xredisadmission __xredisSubscriptionsApply(xredissubscriptions* Store,xredispubsubcommand Command,const xbytesview* Names,size_t Count,uint64* Revision,ptr Owner);
xredisadmission __xredisSubscriptionTransitionCreate(xredissubscriptions* Store,xredisclient* Client,xredissubscriptionsnapshot* Base,const xredissubscriptionreplayconfig* Config,double Entered,ptr Scope,xredissubscriptionreplay** Output);
xredissubscriptionsnapshot* __xredisSubscriptionTransitionSnapshot(xredissubscriptionreplay* Replay);
#endif
#endif
#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xredis/src/internal/cluster_slot.h */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS) || \
	defined(XREDIS_FEATURE_XREDIS_CLUSTER_TOPOLOGY) || \
	defined(XREDIS_FEATURE_XREDIS_COMMAND_KEYS)
#ifndef XREDIS_INTERNAL_CLUSTER_SLOT_H
#define XREDIS_INTERNAL_CLUSTER_SLOT_H
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_TOPOLOGY) || defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS) || defined(XREDIS_FEATURE_XREDIS_COMMAND_KEYS)
/* CRC16/XMODEM polynomial0x1021, init0, no reflection/final xor. */
static uint16 xredisClusterSlotRaw(xbytesview Key)
{
    static const uint16 Table[256]={
        0x0000u,0x1021u,0x2042u,0x3063u,0x4084u,0x50a5u,0x60c6u,0x70e7u,
        0x8108u,0x9129u,0xa14au,0xb16bu,0xc18cu,0xd1adu,0xe1ceu,0xf1efu,
        0x1231u,0x0210u,0x3273u,0x2252u,0x52b5u,0x4294u,0x72f7u,0x62d6u,
        0x9339u,0x8318u,0xb37bu,0xa35au,0xd3bdu,0xc39cu,0xf3ffu,0xe3deu,
        0x2462u,0x3443u,0x0420u,0x1401u,0x64e6u,0x74c7u,0x44a4u,0x5485u,
        0xa56au,0xb54bu,0x8528u,0x9509u,0xe5eeu,0xf5cfu,0xc5acu,0xd58du,
        0x3653u,0x2672u,0x1611u,0x0630u,0x76d7u,0x66f6u,0x5695u,0x46b4u,
        0xb75bu,0xa77au,0x9719u,0x8738u,0xf7dfu,0xe7feu,0xd79du,0xc7bcu,
        0x48c4u,0x58e5u,0x6886u,0x78a7u,0x0840u,0x1861u,0x2802u,0x3823u,
        0xc9ccu,0xd9edu,0xe98eu,0xf9afu,0x8948u,0x9969u,0xa90au,0xb92bu,
        0x5af5u,0x4ad4u,0x7ab7u,0x6a96u,0x1a71u,0x0a50u,0x3a33u,0x2a12u,
        0xdbfdu,0xcbdcu,0xfbbfu,0xeb9eu,0x9b79u,0x8b58u,0xbb3bu,0xab1au,
        0x6ca6u,0x7c87u,0x4ce4u,0x5cc5u,0x2c22u,0x3c03u,0x0c60u,0x1c41u,
        0xedaeu,0xfd8fu,0xcdecu,0xddcdu,0xad2au,0xbd0bu,0x8d68u,0x9d49u,
        0x7e97u,0x6eb6u,0x5ed5u,0x4ef4u,0x3e13u,0x2e32u,0x1e51u,0x0e70u,
        0xff9fu,0xefbeu,0xdfddu,0xcffcu,0xbf1bu,0xaf3au,0x9f59u,0x8f78u,
        0x9188u,0x81a9u,0xb1cau,0xa1ebu,0xd10cu,0xc12du,0xf14eu,0xe16fu,
        0x1080u,0x00a1u,0x30c2u,0x20e3u,0x5004u,0x4025u,0x7046u,0x6067u,
        0x83b9u,0x9398u,0xa3fbu,0xb3dau,0xc33du,0xd31cu,0xe37fu,0xf35eu,
        0x02b1u,0x1290u,0x22f3u,0x32d2u,0x4235u,0x5214u,0x6277u,0x7256u,
        0xb5eau,0xa5cbu,0x95a8u,0x8589u,0xf56eu,0xe54fu,0xd52cu,0xc50du,
        0x34e2u,0x24c3u,0x14a0u,0x0481u,0x7466u,0x6447u,0x5424u,0x4405u,
        0xa7dbu,0xb7fau,0x8799u,0x97b8u,0xe75fu,0xf77eu,0xc71du,0xd73cu,
        0x26d3u,0x36f2u,0x0691u,0x16b0u,0x6657u,0x7676u,0x4615u,0x5634u,
        0xd94cu,0xc96du,0xf90eu,0xe92fu,0x99c8u,0x89e9u,0xb98au,0xa9abu,
        0x5844u,0x4865u,0x7806u,0x6827u,0x18c0u,0x08e1u,0x3882u,0x28a3u,
        0xcb7du,0xdb5cu,0xeb3fu,0xfb1eu,0x8bf9u,0x9bd8u,0xabbbu,0xbb9au,
        0x4a75u,0x5a54u,0x6a37u,0x7a16u,0x0af1u,0x1ad0u,0x2ab3u,0x3a92u,
        0xfd2eu,0xed0fu,0xdd6cu,0xcd4du,0xbdaau,0xad8bu,0x9de8u,0x8dc9u,
        0x7c26u,0x6c07u,0x5c64u,0x4c45u,0x3ca2u,0x2c83u,0x1ce0u,0x0cc1u,
        0xef1fu,0xff3eu,0xcf5du,0xdf7cu,0xaf9bu,0xbfbau,0x8fd9u,0x9ff8u,
        0x6e17u,0x7e36u,0x4e55u,0x5e74u,0x2e93u,0x3eb2u,0x0ed1u,0x1ef0u,
    };
    size_t First=0,Last=Key.Size,Open=0;
    while(Open<Key.Size && Key.Data[Open]!='{')++Open;
    if(Open<Key.Size){size_t Close=Open+1;while(Close<Key.Size && Key.Data[Close]!='}')++Close;
        if(Close<Key.Size && Close>Open+1){First=Open+1;Last=Close;}}
    uint16 Crc=0;
    for(size_t N=First;N<Last;++N)Crc=(uint16)((Crc<<8)^Table[((Crc>>8)^Key.Data[N])&255u]);
    return (uint16)(Crc&16383u);
}
#endif
#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xredis/src/internal/sentinel_client.h */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_SENTINEL)
#ifndef XREDIS_SENTINEL_CLIENT_INTERNAL_H
#define XREDIS_SENTINEL_CLIENT_INTERNAL_H
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL)
bool __xredisSentinelClientProfile(xredisclient* Client,xnetengine* Engine);
#endif
#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xredis/src/internal/sentinel_config.h */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_SENTINEL)
#ifndef XREDIS_SENTINEL_CONFIG_INTERNAL_H
#define XREDIS_SENTINEL_CONFIG_INTERNAL_H
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL)
xredisadmission __xredisSentinelDiscoveryCreateUntil(xnetengine* Engine,const xredissentinelconfig* Config,double Deadline,xredissentineldiscovery** Output);
bool __xredisSentinelConfigValid(const xredissentinelconfig* Config,ptr Output,size_t Bytes);
#endif
#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xredis/src/internal/sentinel_pool_internal.h */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_POOL)
#ifndef XREDIS_SENTINEL_POOL_INTERNAL_H
#define XREDIS_SENTINEL_POOL_INTERNAL_H
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_POOL)
xredisadmission __xredisSentinelPoolCreate(xnetengine*,uint64,size_t,const xredispoolconfig*,const xredissentinelpoolconfig*,xredispool**);
bool __xredisSentinelPoolInfo(xredispool*,xredissentinelpoolinfo*);
#endif
#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xredis/src/internal/cluster_topology_internal.h */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_TOPOLOGY) || \
	defined(XREDIS_FEATURE_XREDIS_CLUSTER_ROUTE) || \
	defined(XREDIS_FEATURE_XREDIS_CLUSTER_REFRESH)
#ifndef XREDIS_INTERNAL_CLUSTER_TOPOLOGY_H
#define XREDIS_INTERNAL_CLUSTER_TOPOLOGY_H
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_TOPOLOGY)
bool __xredisClusterTopologyOutput(const xredisclustertopology* Topology,const void* Output,size_t Bytes);
#endif
#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xredis/src/internal/cluster_route_internal.h */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_ROUTE) || \
	defined(XREDIS_FEATURE_XREDIS_CLUSTER_CLIENT)
#ifndef XREDIS_CLUSTER_ROUTE_INTERNAL_H
#define XREDIS_CLUSTER_ROUTE_INTERNAL_H
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_ROUTE)
bool __xredisClusterRouteConfig(const xredisclusterrouteconfig* Config);
size_t __xredisClusterRouteBaseBytes(void);
xredisadmission __xredisClusterRouteCreate(const xredisclustertopology* Topology,
    xbytesview Origin,uint16 Slot,const xbytesview* Arguments,size_t Count,
    const xredisclusterrouteconfig* Config,xredisclusterroute** Route,double Entered,xredisclusterroutestatus* Failure);
/* Private client coallocation. Context is max_align_t aligned, zeroed and
 * becomes provider Data. Unit releases route duties without freeing its block. */
xredisadmission __xredisClusterRouteCreateContext(const xredisclustertopology* Topology,
    xbytesview Origin,uint16 Slot,const xbytesview* Arguments,size_t Count,
    const xredisclusterrouteconfig* Config,size_t ContextBytes,ptr* Context,
    xredisclusterroute** Route,double Entered);
void __xredisClusterRouteUnit(xredisclusterroute* Route);
xredisadmission __xredisClusterRouteRetryMissing(xredisclusterroute* Previous,
    const xredisclustertopology* Topology,xbytesview Origin,const xredisclusterrouteconfig* Config,xredisclusterroute** Route,xredisclusterroutestatus* Failure);
#endif
#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xredis/src/internal/cluster_refresh_internal.h */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_REFRESH) || \
	defined(XREDIS_FEATURE_XREDIS_CLUSTER_CLIENT)
#ifndef XREDIS_CLUSTER_REFRESH_INTERNAL_H
#define XREDIS_CLUSTER_REFRESH_INTERNAL_H
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_REFRESH)
bool __xredisClusterMapOutput(const xredisclustermap* Map,const void* Output,size_t Bytes);
#endif
#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xredis/src/internal/command_keys_internal.h */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_COMMAND_KEYS) || \
	defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND)
#ifndef XREDIS_INTERNAL_COMMAND_KEYS_H
#define XREDIS_INTERNAL_COMMAND_KEYS_H
#if defined(XREDIS_FEATURE_XREDIS_COMMAND_KEYS) && defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND)
bool __xredisCommandCatalogOutput(const xrediscommandcatalog* Catalogue,const void* Output,size_t Bytes);
#endif
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/command/command.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_COMMAND)
#include <string.h>

#if defined(XREDIS_FEATURE_XREDIS_COMMAND)

static bool xredisCommandFail(xerrkind Kind, xrediscommanderror Code,
	cstr Message)
{
	xrtSetErrorInfo(Kind, "xrt.redis", (int32)Code, Message);
	return false;
}

static size_t xredisCommandDigits(size_t Value)
{
	size_t Count = 1;
	while (Value >= 10u) { Value /= 10u; ++Count; }
	return Count;
}

static bool xredisCommandMeasure(const xbytesview* Arguments, size_t Count,
	size_t* Size)
{
	size_t Total;
	if (Count == 0 || Arguments == NULL)
		return xredisCommandFail(XERR_ARGUMENT, XREDIS_COMMAND_ARGUMENT,
			"a command requires a nonempty argument vector");
	if (Count > (uint64)INT64_MAX || Count > SIZE_MAX / sizeof(*Arguments) ||
		!xrtMemRangeValid(Arguments, Count * sizeof(*Arguments)))
		return xredisCommandFail(XERR_RANGE, XREDIS_COMMAND_OVERFLOW,
			"command argument count or descriptor span overflows");
	Total = 3u + xredisCommandDigits(Count); /* *Count CRLF */
	for (size_t Index = 0; Index < Count; ++Index) {
		size_t Bytes = Arguments[Index].Size;
		size_t Overhead;
		if (Arguments[Index].Data == NULL && Bytes != 0)
			return xredisCommandFail(XERR_ARGUMENT, XREDIS_COMMAND_ARGUMENT,
				"a nonempty command argument requires data");
		if (Bytes > (uint64)INT64_MAX ||
			!xrtMemRangeValid(Arguments[Index].Data, Bytes))
			return xredisCommandFail(XERR_RANGE, XREDIS_COMMAND_OVERFLOW,
				"command argument length or address span overflows");
		Overhead = 5u + xredisCommandDigits(Bytes); /* $Bytes CRLF data CRLF */
		if (Bytes > SIZE_MAX - Overhead || Total > SIZE_MAX - Bytes - Overhead)
			return xredisCommandFail(XERR_RANGE, XREDIS_COMMAND_OVERFLOW,
				"encoded command size overflows");
		Total += Bytes + Overhead;
	}
	*Size = Total;
	return true;
}

static bool xredisCommandOutputSafe(const void* Output, size_t Size,
	const xbytesview* Arguments, size_t Count)
{
	if (!xrtMemRangeValid(Output, Size))
		return xredisCommandFail(XERR_ARGUMENT, XREDIS_COMMAND_ARGUMENT,
			"command output address span is invalid");
	if (xrtMemRangesOverlap(Output, Size, Arguments,
		Count * sizeof(*Arguments)))
		return xredisCommandFail(XERR_ARGUMENT, XREDIS_COMMAND_OVERLAP,
			"command output overlaps argument descriptors");
	for (size_t Index = 0; Index < Count; ++Index) {
		if (xrtMemRangesOverlap(Output, Size,
			Arguments[Index].Data, Arguments[Index].Size))
			return xredisCommandFail(XERR_ARGUMENT, XREDIS_COMMAND_OVERLAP,
				"command output overlaps argument data");
	}
	return true;
}

static unsigned char* xredisCommandNumber(unsigned char* Output, size_t Value)
{
	unsigned char Digits[3u * sizeof(size_t)];
	size_t Count = 0;
	do { Digits[Count++] = (unsigned char)('0' + Value % 10u); Value /= 10u; }
	while (Value != 0);
	while (Count != 0) *Output++ = Digits[--Count];
	*Output++ = '\r'; *Output++ = '\n';
	return Output;
}

XREDIS_API bool xredisCommandSize(const xbytesview* Arguments, size_t Count,
	size_t* Required)
{
	size_t Size;
	if (Required == NULL)
		return xredisCommandFail(XERR_ARGUMENT, XREDIS_COMMAND_ARGUMENT,
			"command size requires an output");
	if (!xredisCommandMeasure(Arguments, Count, &Size) ||
		!xredisCommandOutputSafe(Required, sizeof(*Required), Arguments, Count))
		return false;
	*Required = Size;
	return true;
}

XREDIS_API bool xredisCommandWrite(ptr Output, size_t Capacity,
	const xbytesview* Arguments, size_t Count, size_t* Written)
{
	size_t Size;
	unsigned char* Cursor;
	if (!xredisCommandMeasure(Arguments, Count, &Size)) return false;
	if (Output == NULL)
		return xredisCommandFail(XERR_ARGUMENT, XREDIS_COMMAND_ARGUMENT,
			"command write requires an output buffer");
	if (Capacity < Size)
		return xredisCommandFail(XERR_RANGE, XREDIS_COMMAND_CAPACITY,
			"command output capacity is too small");
	if (!xredisCommandOutputSafe(Output, Size, Arguments, Count)) return false;
	if (Written != NULL) {
		if (!xredisCommandOutputSafe(Written, sizeof(*Written), Arguments, Count))
			return false;
		if (xrtMemRangesOverlap(Output, Size, Written, sizeof(*Written)))
			return xredisCommandFail(XERR_ARGUMENT, XREDIS_COMMAND_OVERLAP,
				"command size output overlaps wire bytes");
	}
	Cursor = Output;
	*Cursor++ = '*'; Cursor = xredisCommandNumber(Cursor, Count);
	for (size_t Index = 0; Index < Count; ++Index) {
		*Cursor++ = '$'; Cursor = xredisCommandNumber(Cursor, Arguments[Index].Size);
		if (Arguments[Index].Size != 0) {
			memcpy(Cursor, Arguments[Index].Data, Arguments[Index].Size);
			Cursor += Arguments[Index].Size;
		}
		*Cursor++ = '\r'; *Cursor++ = '\n';
	}
	if (Written != NULL) *Written = Size;
	return true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/reader/reader.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_READER)
#include <string.h>

#if defined(XREDIS_FEATURE_XREDIS_READER)

typedef enum xredisreaderstate {
	XREDIS_RS_TYPE, XREDIS_RS_BEGIN_LINE, XREDIS_RS_LINE, XREDIS_RS_LINE_LF,
	XREDIS_RS_HEADER, XREDIS_RS_HEADER_LF, XREDIS_RS_BEGIN_VALUE,
	XREDIS_RS_BULK, XREDIS_RS_BULK_CR, XREDIS_RS_BULK_LF, XREDIS_RS_CHUNK,
	XREDIS_RS_END, XREDIS_RS_FINISH_VALUE, XREDIS_RS_END_CONTAINER,
	XREDIS_RS_TERM_CR, XREDIS_RS_TERM_LF, XREDIS_RS_COMPLETE, XREDIS_RS_FAILED
} xredisreaderstate;

typedef enum xredisreaderfloatstate {
	XREDIS_RF_START, XREDIS_RF_SIGN, XREDIS_RF_INTEGER, XREDIS_RF_DOT,
	XREDIS_RF_FRACTION, XREDIS_RF_EXP, XREDIS_RF_EXP_SIGN, XREDIS_RF_EXP_DIGITS,
	XREDIS_RF_I, XREDIS_RF_IN, XREDIS_RF_INF,
	XREDIS_RF_N, XREDIS_RF_NA, XREDIS_RF_NAN, XREDIS_RF_NAN_PAYLOAD,
	XREDIS_RF_NAN_DONE
} xredisreaderfloatstate;

typedef struct xredisreaderlevel {
	uint64 Remaining;
	uint64 Seen;
	xredisreplytype Type;
	uint32 Flags;
	bool Attributes;
} xredisreaderlevel;

typedef struct xredisreaderparse {
	xredisreaderstate State;
	xredisreadevent Current;
	size_t Depth;
	uint64 StreamBytes, FrameBytes, FrameValues;
	uint64 LastFrameBytes, LastFrameValues, CompletedFrames;
	uint64 HeaderValue, HeaderBytes, HeaderDigits;
	uint64 LineBytes, Digits, Number, Remaining, PayloadBytes;
	xredisreaderfloatstate FloatState;
	bool Negative, Plus, HeaderNegative, HeaderUnknown, ChunkHeader;
	bool FrameActive, RootAttributes, Paused;
	xredisreadererror Error;
	xerrkind ErrorKind;
	cstr ErrorMessage;
	uint64 ErrorOffset;
} xredisreaderparse;

struct xredisreader {
	xredisreaderconfig Config;
	xredisreadproc Callback;
	ptr User;
	size_t StorageBytes;
	bool Active;
	xredisreaderparse Parse;
	xredisreaderlevel Stack[];
};

static bool xredisReaderReport(xerrkind Kind, xredisreadererror Code, cstr Message)
{
	xrtSetErrorInfo(Kind, "xrt.redis", (int32)Code, Message);
	return false;
}

static bool xredisReaderFail(xredisreader* Reader, xerrkind Kind,
	xredisreadererror Code, cstr Message)
{
	xredisreaderparse* P = &Reader->Parse;
	P->State = XREDIS_RS_FAILED; P->Error = Code; P->ErrorKind = Kind;
	P->ErrorMessage = Message; P->ErrorOffset = P->StreamBytes;
	return xredisReaderReport(Kind, Code, Message);
}

static bool xredisReaderIdle(xredisreader* Reader)
{
	if (Reader == NULL)
		return xredisReaderReport(XERR_ARGUMENT, XREDIS_READER_ARGUMENT, "reader is NULL");
	if (Reader->Active)
		return xredisReaderReport(XERR_STATE, XREDIS_READER_STATE, "reader callback cannot reenter mutation");
	return true;
}

XREDIS_API bool xredisReaderConfigInit(xredisreaderconfig* Config)
{
	if (Config == NULL || !xrtMemRangeValid(Config, sizeof(*Config)))
		return xredisReaderReport(XERR_ARGUMENT, XREDIS_READER_ARGUMENT, "invalid reader config output");
	*Config = (xredisreaderconfig){sizeof(*Config), XREDIS_RESP3, 0, 64,
		64u * 1024u * 1024u, 16u * 1024u * 1024u, 64u * 1024u,
		1000000u, 1000000u, 64u * 1024u};
	return true;
}

XREDIS_API bool xredisReaderCreate(const xredisreaderconfig* Config,
	xredisreadproc Callback, ptr User, xredisreader** Result)
{
	xredisreaderconfig Copy;
	xredisreader* Reader;
	size_t StorageBytes;
	if (Callback == NULL || Result == NULL || !xrtMemRangeValid(Result, sizeof(*Result)) ||
		(Config != NULL && (!xrtMemRangeValid(Config, sizeof(*Config)) ||
		 xrtMemRangesOverlap(Config, sizeof(*Config), Result, sizeof(*Result)))))
		return xredisReaderReport(XERR_ARGUMENT, XREDIS_READER_ARGUMENT, "invalid reader creation arguments");
	if (Config == NULL) xredisReaderConfigInit(&Copy);
	else Copy = *Config;
	if (Copy.Size != sizeof(Copy) || (Copy.Protocol != XREDIS_RESP2 && Copy.Protocol != XREDIS_RESP3) ||
		(Copy.Flags & ~XREDIS_READER_LEGACY_NAN) != 0 || Copy.MaxFrameBytes == 0 ||
		Copy.MaxValues == 0 || Copy.MaxFeedBytes == 0 || Copy.MaxFrameBytes > (uint64)INT64_MAX ||
		Copy.MaxBulkBytes > (uint64)INT64_MAX || Copy.MaxLineBytes > (uint64)INT64_MAX ||
		Copy.MaxContainerItems > (uint64)INT64_MAX || Copy.MaxValues > (uint64)INT64_MAX ||
		Copy.MaxDepth > (SIZE_MAX - sizeof(*Reader)) / sizeof(xredisreaderlevel))
		return xredisReaderReport(XERR_ARGUMENT, XREDIS_READER_ARGUMENT, "invalid reader config size, protocol or limits");
	StorageBytes = sizeof(*Reader) + Copy.MaxDepth * sizeof(xredisreaderlevel);
	Reader = (xredisreader*)xrtCalloc(1, StorageBytes);
	if (Reader == NULL) return false;
	Reader->Config = Copy; Reader->Callback = Callback; Reader->User = User;
	Reader->StorageBytes = StorageBytes; Reader->Parse.State = XREDIS_RS_TYPE;
	*Result = Reader;
	return true;
}

XREDIS_API bool xredisReaderDestroy(xredisreader* Reader)
{
	if (Reader == NULL) return true;
	if (!xredisReaderIdle(Reader)) return false;
	xrtFree(Reader);
	return true;
}

XREDIS_API bool xredisReaderReset(xredisreader* Reader)
{
	if (!xredisReaderIdle(Reader)) return false;
	memset(&Reader->Parse, 0, sizeof(Reader->Parse));
	Reader->Parse.State = XREDIS_RS_TYPE;
	return true;
}

XREDIS_API bool xredisReaderInfo(const xredisreader* Reader, xredisreaderinfo* Info)
{
	if (Reader == NULL || Info == NULL || !xrtMemRangeValid(Info, sizeof(*Info)) ||
		xrtMemRangesOverlap(Reader, Reader->StorageBytes, Info, sizeof(*Info)) || Info->Size != sizeof(*Info))
		return xredisReaderReport(XERR_ARGUMENT, XREDIS_READER_ARGUMENT, "invalid reader info output or size");
	const xredisreaderparse* P = &Reader->Parse;
	*Info = (xredisreaderinfo){sizeof(*Info), P->StreamBytes, P->FrameBytes, P->FrameValues,
		P->LastFrameBytes, P->LastFrameValues, P->CompletedFrames, P->Depth,
		P->Paused, P->Error, P->ErrorOffset};
	return true;
}

static bool xredisReaderAggregate(xredisreplytype Type)
{
	return Type == XREDIS_REPLY_ARRAY || Type == XREDIS_REPLY_MAP ||
		Type == XREDIS_REPLY_ATTRIBUTE || Type == XREDIS_REPLY_SET || Type == XREDIS_REPLY_PUSH;
}

static bool xredisReaderPairs(xredisreplytype Type)
{
	return Type == XREDIS_REPLY_MAP || Type == XREDIS_REPLY_ATTRIBUTE;
}

static bool xredisReaderRoom(xredisreader* Reader)
{
	if (Reader->Parse.FrameBytes >= Reader->Config.MaxFrameBytes || Reader->Parse.StreamBytes == UINT64_MAX)
		return xredisReaderFail(Reader, XERR_RANGE, XREDIS_READER_LIMIT, "reader wire byte limit exceeded");
	return true;
}

static void xredisReaderAdvance(xredisreader* Reader, size_t* Index, size_t Count)
{
	Reader->Parse.StreamBytes += Count; Reader->Parse.FrameBytes += Count;
	*Index += Count;
}

static bool xredisReaderEmit(xredisreader* Reader, xredisreadkind Kind, xbytesview Data)
{
	xredisreadevent Event = Reader->Parse.Current;
	Event.Kind = Kind; Event.Data = Data;
	xredisreadaction Action = Reader->Callback(&Event, Reader->User);
	if (Action == XREDIS_READ_NEXT) return true;
	if (Action == XREDIS_READ_PAUSE) { Reader->Parse.Paused = true; return false; }
	return xredisReaderFail(Reader, XERR_CANCELLED, XREDIS_READER_CALLBACK,
		Action == XREDIS_READ_FAIL ? "reader callback rejected the frame" : "invalid reader callback action");
}

static void xredisReaderHeaderStart(xredisreaderparse* P, bool Chunk)
{
	P->HeaderValue = 0; P->HeaderBytes = 0; P->HeaderDigits = 0;
	P->HeaderNegative = false; P->HeaderUnknown = false;
	P->ChunkHeader = Chunk; P->State = XREDIS_RS_HEADER;
}

static bool xredisReaderType(xredisreader* Reader, unsigned char Byte)
{
	xredisreaderparse* P = &Reader->Parse;
	xredisreplytype Type;
	switch (Byte) {
		case '+': Type = XREDIS_REPLY_SIMPLE_STRING; break;
		case '-': Type = XREDIS_REPLY_SIMPLE_ERROR; break;
		case ':': Type = XREDIS_REPLY_INTEGER; break;
		case '$': Type = XREDIS_REPLY_BULK_STRING; break;
		case '*': Type = XREDIS_REPLY_ARRAY; break;
		case '_': Type = XREDIS_REPLY_NULL; break;
		case '#': Type = XREDIS_REPLY_BOOLEAN; break;
		case ',': Type = XREDIS_REPLY_DOUBLE; break;
		case '(': Type = XREDIS_REPLY_BIG_NUMBER; break;
		case '!': Type = XREDIS_REPLY_BULK_ERROR; break;
		case '=': Type = XREDIS_REPLY_VERBATIM; break;
		case '%': Type = XREDIS_REPLY_MAP; break;
		case '|': Type = XREDIS_REPLY_ATTRIBUTE; break;
		case '~': Type = XREDIS_REPLY_SET; break;
		case '>': Type = XREDIS_REPLY_PUSH; break;
		default: return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "unknown RESP type prefix");
	}
	if (Reader->Config.Protocol == XREDIS_RESP2 && Type > XREDIS_REPLY_ARRAY)
		return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "RESP3 type in RESP2 mode");
	if (Type == XREDIS_REPLY_PUSH && P->Depth != 0)
		return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "push must be top-level");
	if (P->Depth != 0 && Type != XREDIS_REPLY_ATTRIBUTE) {
		xredisreaderlevel* Parent = &Reader->Stack[P->Depth - 1u];
		if (Parent->Type == XREDIS_REPLY_PUSH && Parent->Seen == 0 &&
			Type != XREDIS_REPLY_SIMPLE_STRING && Type != XREDIS_REPLY_BULK_STRING)
			return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "push kind must be a string");
		if ((Parent->Flags & XREDIS_REPLY_STREAMED) &&
			(xredisReaderPairs(Parent->Type) ? Parent->Seen / 2u : Parent->Seen) >= Reader->Config.MaxContainerItems)
			return xredisReaderFail(Reader, XERR_RANGE, XREDIS_READER_LIMIT, "streamed aggregate item limit exceeded");
	}
	if (P->FrameValues >= Reader->Config.MaxValues)
		return xredisReaderFail(Reader, XERR_RANGE, XREDIS_READER_LIMIT, "frame value limit exceeded");
	++P->FrameValues; P->FrameActive = true;
	P->Current = (xredisreadevent){0}; P->Current.Type = Type;
	P->Current.Depth = P->Depth; P->Current.Length = XREDIS_LENGTH_UNKNOWN;
	P->LineBytes = 0; P->Digits = 0; P->Number = 0; P->PayloadBytes = 0;
	P->Negative = false; P->Plus = false; P->FloatState = XREDIS_RF_START;
	if (Type == XREDIS_REPLY_NULL) { P->Current.Flags = XREDIS_REPLY_NULL_VALUE; P->Current.Length = 0; }
	if (Type == XREDIS_REPLY_BULK_STRING || Type == XREDIS_REPLY_BULK_ERROR ||
		Type == XREDIS_REPLY_VERBATIM || xredisReaderAggregate(Type)) xredisReaderHeaderStart(P, false);
	else P->State = XREDIS_RS_BEGIN_LINE;
	return true;
}

static bool xredisReaderHeaderByte(xredisreader* Reader, unsigned char Byte)
{
	xredisreaderparse* P = &Reader->Parse;
	if (P->HeaderBytes >= Reader->Config.MaxLineBytes)
		return xredisReaderFail(Reader, XERR_RANGE, XREDIS_READER_LIMIT, "RESP header line limit exceeded");
	if (P->HeaderBytes == 0 && Byte == '-') P->HeaderNegative = true;
	else if (P->HeaderBytes == 0 && Byte == '?') P->HeaderUnknown = true;
	else if (Byte >= '0' && Byte <= '9' && !P->HeaderUnknown) {
		uint64 Digit = (uint64)(Byte - '0');
		if (P->HeaderValue > ((uint64)INT64_MAX - Digit) / 10u)
			return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "RESP length exceeds int64");
		P->HeaderValue = P->HeaderValue * 10u + Digit; ++P->HeaderDigits;
	} else return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "invalid RESP length token");
	++P->HeaderBytes;
	return true;
}

static bool xredisReaderBulkRoom(xredisreader* Reader, uint64 Bytes)
{
	xredisreaderparse* P = &Reader->Parse;
	uint64 Room = Reader->Config.MaxFrameBytes - P->FrameBytes;
	if (Bytes > Reader->Config.MaxBulkBytes - P->PayloadBytes || Room < 2u || Bytes > Room - 2u)
		return xredisReaderFail(Reader, XERR_RANGE, XREDIS_READER_LIMIT, "bulk payload or frame limit exceeded");
	return true;
}

static bool xredisReaderHeaderEnd(xredisreader* Reader)
{
	xredisreaderparse* P = &Reader->Parse;
	xredisreplytype Type = P->Current.Type;
	uint64 Length = P->HeaderValue;
	if (P->HeaderUnknown) {
		if (P->HeaderBytes != 1u || P->ChunkHeader || Reader->Config.Protocol != XREDIS_RESP3 ||
			(Type != XREDIS_REPLY_BULK_STRING && Type != XREDIS_REPLY_ARRAY &&
			 Type != XREDIS_REPLY_MAP && Type != XREDIS_REPLY_SET))
			return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "streaming is invalid for this RESP type");
		P->Current.Flags = XREDIS_REPLY_STREAMED; P->Current.Length = XREDIS_LENGTH_UNKNOWN;
	} else {
		if (P->HeaderDigits == 0)
			return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "missing RESP length digits");
		if (P->HeaderNegative) {
			if (P->HeaderBytes != 2u || Length != 1u || P->ChunkHeader ||
				(Type != XREDIS_REPLY_BULK_STRING && Type != XREDIS_REPLY_ARRAY))
				return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "invalid negative RESP length");
			if (P->Depth != 0 && Reader->Stack[P->Depth - 1u].Type == XREDIS_REPLY_PUSH &&
				Reader->Stack[P->Depth - 1u].Seen == 0)
				return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "push kind cannot be null");
			P->Current.Flags = XREDIS_REPLY_NULL_VALUE; P->Current.Length = 0;
		} else if (P->ChunkHeader) {
			if (Length != 0 && !xredisReaderBulkRoom(Reader, Length)) return false;
			P->Remaining = Length; P->State = Length == 0 ? XREDIS_RS_END : XREDIS_RS_BULK;
			return true;
		} else {
			P->Current.Length = Length;
			if (xredisReaderAggregate(Type)) {
				uint64 Slots = xredisReaderPairs(Type) ? Length * 2u : Length;
				if (Length > Reader->Config.MaxContainerItems || Slots > Reader->Config.MaxValues - P->FrameValues)
					return xredisReaderFail(Reader, XERR_RANGE, XREDIS_READER_LIMIT, "declared aggregate exceeds item or value limit");
				if (Type == XREDIS_REPLY_PUSH && Length == 0)
					return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "push requires a kind string");
			} else if (!xredisReaderBulkRoom(Reader, Length)) return false;
			if (Type == XREDIS_REPLY_VERBATIM && Length < 4u)
				return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "verbatim payload requires a format prefix");
		}
	}
	if (xredisReaderAggregate(Type) && !(P->Current.Flags & XREDIS_REPLY_NULL_VALUE) && P->Depth >= Reader->Config.MaxDepth)
		return xredisReaderFail(Reader, XERR_RANGE, XREDIS_READER_LIMIT, "aggregate nesting limit exceeded");
	P->State = XREDIS_RS_BEGIN_VALUE;
	return true;
}

/* An incremental grammar, independent of locale/libc strtod and without a
 * token buffer. Conversion/rounding is intentionally left to a reply builder. */
static bool xredisReaderFloatByte(xredisreader* Reader, unsigned char Byte)
{
	xredisreaderparse* P = &Reader->Parse;
	bool Digit = Byte >= '0' && Byte <= '9';
	bool Legacy = (Reader->Config.Flags & XREDIS_READER_LEGACY_NAN) != 0;
	unsigned char Special = Legacy && Byte >= 'A' && Byte <= 'Z' ? (unsigned char)(Byte + ('a' - 'A')) : Byte;
	switch (P->FloatState) {
		case XREDIS_RF_START:
			if (Byte == '+' || Byte == '-') {
				P->Negative = Byte == '-'; P->Plus = Byte == '+'; P->FloatState = XREDIS_RF_SIGN; return true;
			}
			/* fall through */
		case XREDIS_RF_SIGN:
			if (Digit) P->FloatState = XREDIS_RF_INTEGER;
			else if (Byte == 'i' && !P->Plus) P->FloatState = XREDIS_RF_I;
			else if (Special == 'n' && (Legacy || (!P->Negative && !P->Plus))) P->FloatState = XREDIS_RF_N;
			else return false;
			return true;
		case XREDIS_RF_INTEGER:
			if (Digit) return true;
			if (Byte == '.') { P->FloatState = XREDIS_RF_DOT; return true; }
			if (Byte == 'e' || Byte == 'E') { P->FloatState = XREDIS_RF_EXP; return true; }
			return false;
		case XREDIS_RF_DOT:
			if (!Digit) return false;
			P->FloatState = XREDIS_RF_FRACTION; return true;
		case XREDIS_RF_FRACTION:
			if (Digit) return true;
			if (Byte == 'e' || Byte == 'E') { P->FloatState = XREDIS_RF_EXP; return true; }
			return false;
		case XREDIS_RF_EXP:
			if (Byte == '+' || Byte == '-') { P->FloatState = XREDIS_RF_EXP_SIGN; return true; }
			/* fall through */
		case XREDIS_RF_EXP_SIGN:
			if (!Digit) return false;
			P->FloatState = XREDIS_RF_EXP_DIGITS; return true;
		case XREDIS_RF_EXP_DIGITS: return Digit;
		case XREDIS_RF_I: if (Byte != 'n') return false; P->FloatState = XREDIS_RF_IN; return true;
		case XREDIS_RF_IN: if (Byte != 'f') return false; P->FloatState = XREDIS_RF_INF; return true;
		case XREDIS_RF_N: if (Special != 'a') return false; P->FloatState = XREDIS_RF_NA; return true;
		case XREDIS_RF_NA: if (Special != 'n') return false; P->FloatState = XREDIS_RF_NAN; return true;
		case XREDIS_RF_NAN:
			if (Byte != '(' || !Legacy) return false;
			P->FloatState = XREDIS_RF_NAN_PAYLOAD; return true;
		case XREDIS_RF_NAN_PAYLOAD:
			if (Byte == ')') { P->FloatState = XREDIS_RF_NAN_DONE; return true; }
			return Digit || (Special >= 'a' && Special <= 'z') || Byte == '_';
		default: return false;
	}
}

static bool xredisReaderScalarByte(xredisreader* Reader, unsigned char Byte)
{
	xredisreaderparse* P = &Reader->Parse;
	switch (P->Current.Type) {
		case XREDIS_REPLY_NULL: return false;
		case XREDIS_REPLY_BOOLEAN:
			if (P->LineBytes != 0 || (Byte != 't' && Byte != 'f')) return false;
			P->Current.Boolean = Byte == 't'; break;
		case XREDIS_REPLY_INTEGER:
		case XREDIS_REPLY_BIG_NUMBER:
			if (P->LineBytes == 0 && (Byte == '+' || Byte == '-')) P->Negative = Byte == '-';
			else {
				if (Byte < '0' || Byte > '9') return false;
				if (P->Current.Type == XREDIS_REPLY_INTEGER) {
					uint64 Limit = (uint64)INT64_MAX + (P->Negative ? 1u : 0u);
					uint64 Digit = (uint64)(Byte - '0');
					if (P->Number > (Limit - Digit) / 10u) return false;
					P->Number = P->Number * 10u + Digit;
				}
				++P->Digits;
			} break;
		case XREDIS_REPLY_DOUBLE: if (!xredisReaderFloatByte(Reader, Byte)) return false; break;
		default: break;
	}
	++P->LineBytes;
	return true;
}

static bool xredisReaderScalarEnd(xredisreader* Reader)
{
	xredisreaderparse* P = &Reader->Parse;
	xredisreplytype Type = P->Current.Type;
	bool Valid = true;
	if (Type == XREDIS_REPLY_INTEGER || Type == XREDIS_REPLY_BIG_NUMBER) Valid = P->Digits != 0;
	else if (Type == XREDIS_REPLY_BOOLEAN) Valid = P->LineBytes == 1u;
	else if (Type == XREDIS_REPLY_DOUBLE) Valid = P->FloatState == XREDIS_RF_INTEGER ||
		P->FloatState == XREDIS_RF_FRACTION || P->FloatState == XREDIS_RF_EXP_DIGITS ||
		P->FloatState == XREDIS_RF_INF || P->FloatState == XREDIS_RF_NAN || P->FloatState == XREDIS_RF_NAN_DONE;
	if (!Valid) return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "incomplete RESP scalar token");
	if (Type == XREDIS_REPLY_INTEGER)
		P->Current.Integer = P->Negative ? (P->Number == (uint64)INT64_MAX + 1u ? INT64_MIN : -(int64)P->Number) : (int64)P->Number;
	P->Current.Length = P->LineBytes; P->State = XREDIS_RS_END;
	return true;
}

static size_t xredisReaderAvailable(const xredisreader* Reader, size_t Available)
{
	uint64 Room = Reader->Config.MaxFrameBytes - Reader->Parse.FrameBytes;
	uint64 StreamRoom = UINT64_MAX - Reader->Parse.StreamBytes;
	if ((uint64)Available > Room) Available = (size_t)Room;
	if ((uint64)Available > StreamRoom) Available = (size_t)StreamRoom;
	return Available;
}

XREDIS_API xredisreadstatus xredisReaderFeed(xredisreader* Reader,
	xbytesview Input, size_t* Consumed)
{
	xredisreadstatus Result = XREDIS_READER_MORE;
	size_t Index = 0, Budget;
	xredisreaderparse* P;
	const xbytesview Empty = {NULL, 0};
	if (!xredisReaderIdle(Reader)) return XREDIS_READER_ERROR;
	if (Consumed == NULL || !xrtMemRangeValid(Consumed, sizeof(*Consumed)) ||
		(Input.Data == NULL && Input.Size != 0) || !xrtMemRangeValid(Input.Data, Input.Size) ||
		xrtMemRangesOverlap(Reader, Reader->StorageBytes, Input.Data, Input.Size) ||
		xrtMemRangesOverlap(Reader, Reader->StorageBytes, Consumed, sizeof(*Consumed)) ||
		xrtMemRangesOverlap(Input.Data, Input.Size, Consumed, sizeof(*Consumed))) {
		xredisReaderReport(XERR_ARGUMENT, XREDIS_READER_ARGUMENT, "invalid reader input, consumed output or overlap");
		return XREDIS_READER_ERROR;
	}
	P = &Reader->Parse;
	if (P->Error != XREDIS_READER_OK) {
		*Consumed = 0; xredisReaderReport(P->ErrorKind, P->Error, P->ErrorMessage); return XREDIS_READER_ERROR;
	}
	Budget = Input.Size < Reader->Config.MaxFeedBytes ? Input.Size : Reader->Config.MaxFeedBytes;
	Reader->Active = true; P->Paused = false;
	for (;;) {
		/* Pending events/transitions are committed before callbacks. Draining
		 * them costs O(depth), even for empty input or a one-byte work budget. */
		if (P->State == XREDIS_RS_BEGIN_LINE) {
			P->State = XREDIS_RS_LINE;
			if (!xredisReaderEmit(Reader, XREDIS_READ_BEGIN, Empty)) goto callback_stop;
			continue;
		}
		if (P->State == XREDIS_RS_BEGIN_VALUE) {
			if (P->Current.Flags & XREDIS_REPLY_NULL_VALUE) P->State = XREDIS_RS_END;
			else if (xredisReaderAggregate(P->Current.Type)) {
				xredisreaderlevel* Level = &Reader->Stack[P->Depth++];
				*Level = (xredisreaderlevel){0}; Level->Type = P->Current.Type; Level->Flags = P->Current.Flags;
				Level->Remaining = (Level->Flags & XREDIS_REPLY_STREAMED) ? 0 :
					(xredisReaderPairs(Level->Type) ? P->Current.Length * 2u : P->Current.Length);
				P->State = !(Level->Flags & XREDIS_REPLY_STREAMED) && Level->Remaining == 0 ? XREDIS_RS_END_CONTAINER : XREDIS_RS_TYPE;
			} else {
				P->Remaining = P->Current.Length;
				P->State = (P->Current.Flags & XREDIS_REPLY_STREAMED) ? XREDIS_RS_CHUNK :
					(P->Remaining == 0 ? XREDIS_RS_BULK_CR : XREDIS_RS_BULK);
			}
			if (!xredisReaderEmit(Reader, XREDIS_READ_BEGIN, Empty)) goto callback_stop;
			continue;
		}
		if (P->State == XREDIS_RS_END_CONTAINER) {
			xredisreaderlevel* Level = &Reader->Stack[--P->Depth];
			P->Current = (xredisreadevent){0}; P->Current.Type = Level->Type;
			P->Current.Flags = Level->Flags; P->Current.Depth = P->Depth;
			P->Current.Length = xredisReaderPairs(Level->Type) ? Level->Seen / 2u : Level->Seen;
			P->State = XREDIS_RS_END; continue;
		}
		if (P->State == XREDIS_RS_END) {
			if (P->Current.Type == XREDIS_REPLY_BULK_STRING || P->Current.Type == XREDIS_REPLY_BULK_ERROR ||
				P->Current.Type == XREDIS_REPLY_VERBATIM) P->Current.Length = P->PayloadBytes;
			P->State = XREDIS_RS_FINISH_VALUE;
			if (!xredisReaderEmit(Reader, XREDIS_READ_END, Empty)) goto callback_stop;
			continue;
		}
		if (P->State == XREDIS_RS_FINISH_VALUE) {
			if (P->Current.Type == XREDIS_REPLY_ATTRIBUTE) {
				if (P->Depth == 0) P->RootAttributes = true;
				else Reader->Stack[P->Depth - 1u].Attributes = true;
				P->State = XREDIS_RS_TYPE;
			} else if (P->Depth == 0) P->State = XREDIS_RS_COMPLETE;
			else {
				xredisreaderlevel* Parent = &Reader->Stack[P->Depth - 1u];
				++Parent->Seen; Parent->Attributes = false;
				if (!(Parent->Flags & XREDIS_REPLY_STREAMED) && --Parent->Remaining == 0) P->State = XREDIS_RS_END_CONTAINER;
				else P->State = XREDIS_RS_TYPE;
			}
			continue;
		}
		if (P->State == XREDIS_RS_COMPLETE) {
			if (P->CompletedFrames == UINT64_MAX) {
				xredisReaderFail(Reader, XERR_RANGE, XREDIS_READER_LIMIT, "completed frame counter overflows"); goto failed;
			}
			++P->CompletedFrames; P->LastFrameBytes = P->FrameBytes; P->LastFrameValues = P->FrameValues;
			P->FrameBytes = 0; P->FrameValues = 0; P->FrameActive = false; P->RootAttributes = false;
			P->State = XREDIS_RS_TYPE; Result = XREDIS_READER_READY; break;
		}
		if (Index == Budget) { Result = Index < Input.Size ? XREDIS_READER_YIELD : XREDIS_READER_MORE; break; }
		if (!xredisReaderRoom(Reader)) goto failed;
		unsigned char Byte = Input.Data[Index];
		switch (P->State) {
			case XREDIS_RS_TYPE:
				if (Byte == '.') {
					if (P->Depth == 0 || !(Reader->Stack[P->Depth - 1u].Flags & XREDIS_REPLY_STREAMED) ||
						Reader->Stack[P->Depth - 1u].Attributes ||
						(xredisReaderPairs(Reader->Stack[P->Depth - 1u].Type) && (Reader->Stack[P->Depth - 1u].Seen & 1u))) {
						xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "invalid streamed aggregate terminator or orphan attributes"); goto failed;
					}
					P->State = XREDIS_RS_TERM_CR;
				} else if (!xredisReaderType(Reader, Byte)) goto failed;
				xredisReaderAdvance(Reader, &Index, 1); break;
			case XREDIS_RS_HEADER:
				if (Byte == '\r') P->State = XREDIS_RS_HEADER_LF;
				else if (!xredisReaderHeaderByte(Reader, Byte)) goto failed;
				xredisReaderAdvance(Reader, &Index, 1); break;
			case XREDIS_RS_HEADER_LF:
				if (Byte != '\n') goto bad_terminator;
				xredisReaderAdvance(Reader, &Index, 1);
				if (!xredisReaderHeaderEnd(Reader)) goto failed;
				break;
			case XREDIS_RS_LINE: {
				size_t Count = xredisReaderAvailable(Reader, Budget - Index);
				const unsigned char* CR = (const unsigned char*)memchr(Input.Data + Index, '\r', Count);
				if (CR != NULL) Count = (size_t)(CR - Input.Data - Index);
				const unsigned char* LF = (const unsigned char*)memchr(Input.Data + Index, '\n', Count);
				if (LF != NULL) Count = (size_t)(LF - Input.Data - Index);
				if (Count == 0) {
					if (Byte != '\r') goto bad_terminator;
					xredisReaderAdvance(Reader, &Index, 1); P->State = XREDIS_RS_LINE_LF; break;
				}
				uint64 LineRoom = Reader->Config.MaxLineBytes - P->LineBytes;
				if (LineRoom == 0) { xredisReaderFail(Reader, XERR_RANGE, XREDIS_READER_LIMIT, "RESP scalar line limit exceeded"); goto failed; }
				if ((uint64)Count > LineRoom) Count = (size_t)LineRoom;
				if (P->Current.Type == XREDIS_REPLY_SIMPLE_STRING || P->Current.Type == XREDIS_REPLY_SIMPLE_ERROR) P->LineBytes += Count;
				else for (size_t Part = 0; Part < Count; ++Part) {
					if (!xredisReaderScalarByte(Reader, Input.Data[Index + Part])) {
						xredisReaderAdvance(Reader, &Index, Part);
						xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "invalid RESP scalar or integer overflow"); goto failed;
					}
				}
				xbytesview Data = {Input.Data + Index, Count}; xredisReaderAdvance(Reader, &Index, Count);
				if (!xredisReaderEmit(Reader, XREDIS_READ_DATA, Data)) goto callback_stop;
				break;
			}
			case XREDIS_RS_LINE_LF:
				if (Byte != '\n') goto bad_terminator;
				xredisReaderAdvance(Reader, &Index, 1);
				if (!xredisReaderScalarEnd(Reader)) goto failed;
				break;
			case XREDIS_RS_CHUNK:
				if (Byte != ';') { xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "missing streamed bulk chunk prefix"); goto failed; }
				xredisReaderAdvance(Reader, &Index, 1); xredisReaderHeaderStart(P, true); break;
			case XREDIS_RS_BULK: {
				size_t Count = xredisReaderAvailable(Reader, Budget - Index);
				if ((uint64)Count > P->Remaining) Count = (size_t)P->Remaining;
				if (P->Current.Type == XREDIS_REPLY_VERBATIM && P->PayloadBytes <= 3u &&
					(uint64)Count > 3u - P->PayloadBytes && Input.Data[Index + (size_t)(3u - P->PayloadBytes)] != ':') {
					xredisReaderAdvance(Reader, &Index, (size_t)(3u - P->PayloadBytes));
					xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "verbatim format requires a colon"); goto failed;
				}
				xbytesview Data = {Input.Data + Index, Count};
				P->Remaining -= Count; P->PayloadBytes += Count;
				xredisReaderAdvance(Reader, &Index, Count);
				if (P->Remaining == 0) P->State = XREDIS_RS_BULK_CR;
				if (!xredisReaderEmit(Reader, XREDIS_READ_DATA, Data)) goto callback_stop;
				break;
			}
			case XREDIS_RS_BULK_CR:
			case XREDIS_RS_TERM_CR:
				if (Byte != '\r') goto bad_terminator;
				xredisReaderAdvance(Reader, &Index, 1);
				P->State = P->State == XREDIS_RS_BULK_CR ? XREDIS_RS_BULK_LF : XREDIS_RS_TERM_LF; break;
			case XREDIS_RS_BULK_LF:
				if (Byte != '\n') goto bad_terminator;
				xredisReaderAdvance(Reader, &Index, 1);
				P->State = (P->Current.Flags & XREDIS_REPLY_STREAMED) ? XREDIS_RS_CHUNK : XREDIS_RS_END; break;
			case XREDIS_RS_TERM_LF:
				if (Byte != '\n') goto bad_terminator;
				xredisReaderAdvance(Reader, &Index, 1); P->State = XREDIS_RS_END_CONTAINER; break;
			default:
				xredisReaderFail(Reader, XERR_INTERNAL, XREDIS_READER_STATE, "invalid reader state"); goto failed;
		}
	}
	goto done;
bad_terminator:
	xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "missing RESP CRLF terminator");
failed:
	Result = XREDIS_READER_ERROR; goto done;
callback_stop:
	Result = P->Error != XREDIS_READER_OK ? XREDIS_READER_ERROR : XREDIS_READER_PAUSED;
done:
	Reader->Active = false; *Consumed = Index;
	return Result;
}

XREDIS_API bool xredisReaderFinish(xredisreader* Reader)
{
	if (!xredisReaderIdle(Reader)) return false;
	xredisreaderparse* P = &Reader->Parse;
	if (P->Error != XREDIS_READER_OK) return xredisReaderReport(P->ErrorKind, P->Error, P->ErrorMessage);
	if (P->Paused) return xredisReaderReport(XERR_STATE, XREDIS_READER_STATE, "resume paused reader before end-of-stream");
	if (P->State == XREDIS_RS_TYPE && !P->FrameActive && !P->RootAttributes && P->Depth == 0) return true;
	return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_TRUNCATED, "truncated RESP frame or orphan attributes at end-of-stream");
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/reply/reply.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_REPLY) || \
	defined(XREDIS_FEATURE_XREDIS_ROLE)
#include <string.h>

#if defined(XREDIS_FEATURE_XREDIS_REPLY)

typedef struct xredisreplynode {
	xredisreplytype Type;
	uint32 Flags;
	uint64 Length;
	int64 Integer;
	bool Boolean;
	size_t Offset, Bytes, ChildCount, FirstChild, Next, Attributes;
} xredisreplynode;

struct xredisreply {
	xatomic32 References;
	xredisreplynode* Nodes;
	unsigned char* Data;
	size_t Count, NodeCapacity, DataBytes, DataCapacity, Root, Reserved, Peak;
	xredisreplynode InlineNode[1];
	unsigned char InlineData[64];
};

typedef struct xredisreplylevel {
	size_t Open, LastChild, PendingFirst, PendingLast;
} xredisreplylevel;

struct xredisreplyreader {
	xredisreplyreaderconfig Config;
	xredisreader* Parser;
	xredisreply* Partial;
	size_t StorageBytes, Peak;
	xredisreplyerror Error;
	xerrkind ErrorKind;
	cstr ErrorMessage;
	xredisreplylevel Levels[];
};

static bool xredisReplyReport(xerrkind Kind, xredisreplyerror Code, cstr Message)
{
	xrtSetErrorInfo(Kind, "xrt.redis", (int32)Code, Message);
	return false;
}

static bool xredisReplyFail(xredisreplyreader* Reader, xerrkind Kind,
	xredisreplyerror Code, cstr Message)
{
	Reader->Error = Code; Reader->ErrorKind = Kind; Reader->ErrorMessage = Message;
	return false;
}

static bool xredisReplyOverlap(const xredisreply* Reply, const void* Data, size_t Bytes)
{
	return Reply != NULL && (xrtMemRangesOverlap(Reply, sizeof(*Reply), Data, Bytes) ||
		(Reply->Nodes != Reply->InlineNode && xrtMemRangesOverlap(Reply->Nodes,
			Reply->NodeCapacity * sizeof(*Reply->Nodes), Data, Bytes)) ||
		(Reply->Data != Reply->InlineData && xrtMemRangesOverlap(Reply->Data, Reply->DataCapacity, Data, Bytes)));
}

#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_TOPOLOGY) || defined(XREDIS_FEATURE_XREDIS_COMMAND_KEYS)
bool __xredisClusterReplyOutput(const xredisreply* Reply,const void* Output,size_t Bytes)
{return Reply && Output && xrtMemRangeValid(Output,Bytes) && !xredisReplyOverlap(Reply,Output,Bytes);}
#endif

static bool xredisReplyReaderOverlap(const xredisreplyreader* Reader, const void* Data, size_t Bytes)
{
	return xrtMemRangesOverlap(Reader, Reader->StorageBytes, Data, Bytes) ||
		xredisReplyOverlap(Reader->Partial, Data, Bytes);
}

XREDIS_API bool xredisReplyRetain(xredisreply* Reply)
{
	if (Reply == NULL) return xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "reply is NULL");
	uint32 Count = xrtAtomic32Load(&Reply->References, XMEMORY_RELAXED);
	for (;;) {
		if (Count == UINT32_MAX)
			return xredisReplyReport(XERR_RANGE, XREDIS_REPLY_LIMIT, "reply reference count overflows");
		if (xrtAtomic32CompareExchange(&Reply->References, &Count, Count + 1u, XMEMORY_RELAXED, XMEMORY_RELAXED)) return true;
	}
}

XREDIS_API void xredisReplyRelease(xredisreply* Reply)
{
	if (Reply == NULL) return;
	if (xrtAtomic32FetchSub(&Reply->References, 1, XMEMORY_ACQ_REL) != 1) return;
	if (Reply->Nodes != Reply->InlineNode) xrtFree(Reply->Nodes);
	if (Reply->Data != Reply->InlineData) xrtFree(Reply->Data);
	xrtFree(Reply);
}

XREDIS_API bool xredisReplyInfo(const xredisreply* Reply, xredisreplyinfo* Info)
{
	if (Reply == NULL || Info == NULL || !xrtMemRangeValid(Info, sizeof(*Info)) ||
		xredisReplyOverlap(Reply, Info, sizeof(*Info)) || Info->Size != sizeof(*Info))
		return xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "invalid reply info output or size");
	*Info = (xredisreplyinfo){sizeof(*Info), Reply->Root, Reply->Count, Reply->DataBytes, Reply->Reserved, Reply->Peak};
	return true;
}

XREDIS_API bool xredisReplyGet(const xredisreply* Reply, size_t Index, xredisreplyvalue* Value)
{
	if (Reply == NULL || Index >= Reply->Count || Value == NULL || !xrtMemRangeValid(Value, sizeof(*Value)) ||
		xredisReplyOverlap(Reply, Value, sizeof(*Value)))
		return xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "invalid reply node index or output");
	const xredisreplynode* Node = &Reply->Nodes[Index];
	*Value = (xredisreplyvalue){Node->Type, Node->Flags, Node->Length, Node->Integer, Node->Boolean,
		{Node->Bytes == 0 ? NULL : Reply->Data + Node->Offset, Node->Bytes},
		Node->ChildCount, Node->FirstChild, Node->Next, Node->Attributes};
	return true;
}

XREDIS_API bool xredisReplyDouble(const xredisreply* Reply, size_t Index, double* Value)
{
	if (Reply == NULL || Index >= Reply->Count || Reply->Nodes[Index].Type != XREDIS_REPLY_DOUBLE ||
		Value == NULL || !xrtMemRangeValid(Value, sizeof(*Value)) || xredisReplyOverlap(Reply, Value, sizeof(*Value)))
		return xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "invalid double node or output");
	const xredisreplynode* Node = &Reply->Nodes[Index];
	xstrview Text = {(cstr)Reply->Data + Node->Offset, Node->Bytes};
	size_t Start = Text.Data[0] == '-' || Text.Data[0] == '+' ? 1u : 0;
	if (Text.Data[Start] == 'n' || Text.Data[Start] == 'N')
		Text = Text.Data[0] == '-' ? (xstrview){"-nan", 4} : (xstrview){"nan", 3};
	return xrtNumParse(Text, XNUMBER_PARSE_SPECIAL, Value);
}

XREDIS_API bool xredisReplyReaderConfigInit(xredisreplyreaderconfig* Config)
{
	if (Config == NULL || !xrtMemRangeValid(Config, sizeof(*Config)))
		return xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "invalid reply reader config output");
	*Config = (xredisreplyreaderconfig){0}; Config->Size = sizeof(*Config);
	xredisReaderConfigInit(&Config->Reader); Config->MaxOwnedBytes = 64u * 1024u * 1024u;
	return true;
}

/* Budget includes the complete destination before the old buffer is freed.
 * Explicit copy/free counts only used bytes and keeps failure ownership clear. */
static size_t xredisReplyGrowth(xredisreplyreader* Reader, size_t Old, size_t Need, size_t Unit)
{
	xredisreply* Reply = Reader->Partial;
	size_t Available = (Reader->Config.MaxOwnedBytes - Reply->Reserved) / Unit;
	if (Need > Available) {
		xredisReplyFail(Reader, XERR_RANGE, XREDIS_REPLY_LIMIT, "owned reply peak byte limit exceeded"); return 0;
	}
	size_t Capacity = Old <= SIZE_MAX / 2u ? Old * 2u : SIZE_MAX;
	if (Capacity < Need) Capacity = Need;
	if (Capacity > Available) Capacity = Available;
	return Capacity;
}

static void xredisReplyPeak(xredisreplyreader* Reader, size_t Extra)
{
	xredisreply* Reply = Reader->Partial;
	if (Reply->Reserved + Extra > Reply->Peak) Reply->Peak = Reply->Reserved + Extra;
	Reader->Peak = Reply->Peak;
}

static bool xredisReplyNodes(xredisreplyreader* Reader)
{
	xredisreply* Reply = Reader->Partial;
	if (Reply->Count < Reply->NodeCapacity) return true;
	if (Reply->Count == SIZE_MAX)
		return xredisReplyFail(Reader, XERR_RANGE, XREDIS_REPLY_LIMIT, "owned node count overflows");
	size_t Capacity = xredisReplyGrowth(Reader, Reply->NodeCapacity, Reply->Count + 1u, sizeof(*Reply->Nodes));
	if (Capacity == 0) return false;
	size_t Bytes = Capacity * sizeof(*Reply->Nodes);
	xredisreplynode* Nodes = (xredisreplynode*)xrtMalloc(Bytes);
	if (Nodes == NULL) return xredisReplyFail(Reader, XERR_MEMORY, XREDIS_REPLY_MEMORY, "owned node allocation failed");
	xredisReplyPeak(Reader, Bytes);
	memcpy(Nodes, Reply->Nodes, Reply->Count * sizeof(*Nodes));
	if (Reply->Nodes != Reply->InlineNode) {
		xrtFree(Reply->Nodes); Reply->Reserved -= Reply->NodeCapacity * sizeof(*Nodes);
	}
	Reply->Nodes = Nodes; Reply->NodeCapacity = Capacity; Reply->Reserved += Bytes;
	return true;
}

static bool xredisReplyPayload(xredisreplyreader* Reader, size_t Additional)
{
	xredisreply* Reply = Reader->Partial;
	if (Additional > SIZE_MAX - Reply->DataBytes)
		return xredisReplyFail(Reader, XERR_RANGE, XREDIS_REPLY_LIMIT, "owned payload length overflows");
	size_t Need = Reply->DataBytes + Additional;
	if (Need <= Reply->DataCapacity) return true;
	size_t Capacity = xredisReplyGrowth(Reader, Reply->DataCapacity, Need, 1);
	if (Capacity == 0) return false;
	unsigned char* Data = (unsigned char*)xrtMalloc(Capacity);
	if (Data == NULL) return xredisReplyFail(Reader, XERR_MEMORY, XREDIS_REPLY_MEMORY, "owned payload allocation failed");
	xredisReplyPeak(Reader, Capacity);
	memcpy(Data, Reply->Data, Reply->DataBytes);
	if (Reply->Data != Reply->InlineData) { xrtFree(Reply->Data); Reply->Reserved -= Reply->DataCapacity; }
	Reply->Data = Data; Reply->DataCapacity = Capacity; Reply->Reserved += Capacity;
	return true;
}

static void xredisReplyLevels(xredisreplyreader* Reader)
{
	Reader->Levels[0] = (xredisreplylevel){XREDIS_REPLY_NONE, XREDIS_REPLY_NONE, XREDIS_REPLY_NONE, XREDIS_REPLY_NONE};
}

static xredisreadaction xredisReplyCollect(const xredisreadevent* Event, ptr User)
{
	xredisreplyreader* Reader = (xredisreplyreader*)User;
	xredisreplylevel* Level = &Reader->Levels[Event->Depth];
	if (Reader->Partial == NULL) {
		Reader->Peak = 0;
		if (sizeof(xredisreply) > Reader->Config.MaxOwnedBytes) {
			xredisReplyFail(Reader, XERR_RANGE, XREDIS_REPLY_LIMIT, "owned reply header exceeds byte limit"); return XREDIS_READ_FAIL;
		}
		xredisreply* Reply = (xredisreply*)xrtCalloc(1, sizeof(*Reply));
		if (Reply == NULL) {
			xredisReplyFail(Reader, XERR_MEMORY, XREDIS_REPLY_MEMORY, "owned reply allocation failed"); return XREDIS_READ_FAIL;
		}
		xrtAtomic32Init(&Reply->References, 1); Reply->Nodes = Reply->InlineNode; Reply->NodeCapacity = 1;
		Reply->Data = Reply->InlineData; Reply->DataCapacity = sizeof(Reply->InlineData); Reply->Root = XREDIS_REPLY_NONE;
		Reply->Reserved = sizeof(*Reply); Reply->Peak = sizeof(*Reply); Reader->Peak = Reply->Peak;
		Reader->Partial = Reply;
	}
	xredisreply* Reply = Reader->Partial;
	if (Event->Kind == XREDIS_READ_BEGIN) {
		/* A new parent's child level has no pending metadata. Initialize lazily
		 * so a scalar frame never clears the entire configured depth stack. */
		if (Event->Depth < Reader->Config.Reader.MaxDepth)
			Reader->Levels[Event->Depth + 1u] = (xredisreplylevel){XREDIS_REPLY_NONE,
				XREDIS_REPLY_NONE, XREDIS_REPLY_NONE, XREDIS_REPLY_NONE};
		if (!xredisReplyNodes(Reader)) return XREDIS_READ_FAIL;
		size_t Index = Reply->Count++;
		xredisreplynode* Node = &Reply->Nodes[Index];
		*Node = (xredisreplynode){0}; Node->Type = Event->Type; Node->Flags = Event->Flags;
		Node->Offset = Reply->DataBytes; Node->FirstChild = XREDIS_REPLY_NONE;
		Node->Next = XREDIS_REPLY_NONE; Node->Attributes = XREDIS_REPLY_NONE;
		Level->Open = Index; Level->LastChild = XREDIS_REPLY_NONE;
		if (Event->Type != XREDIS_REPLY_ATTRIBUTE) {
			Node->Attributes = Level->PendingFirst;
			Level->PendingFirst = XREDIS_REPLY_NONE; Level->PendingLast = XREDIS_REPLY_NONE;
			if (Event->Depth == 0) Reply->Root = Index;
			else {
				xredisreplylevel* ParentLevel = &Reader->Levels[Event->Depth - 1u];
				xredisreplynode* Parent = &Reply->Nodes[ParentLevel->Open];
				if (ParentLevel->LastChild == XREDIS_REPLY_NONE) Parent->FirstChild = Index;
				else Reply->Nodes[ParentLevel->LastChild].Next = Index;
				ParentLevel->LastChild = Index; ++Parent->ChildCount;
			}
		}
		if (!(Event->Flags & XREDIS_REPLY_NULL_VALUE) && Event->Length != XREDIS_LENGTH_UNKNOWN &&
			(Event->Type == XREDIS_REPLY_BULK_STRING || Event->Type == XREDIS_REPLY_BULK_ERROR || Event->Type == XREDIS_REPLY_VERBATIM)) {
			if (Event->Length > (uint64)SIZE_MAX) {
				xredisReplyFail(Reader, XERR_RANGE, XREDIS_REPLY_LIMIT, "owned bulk length overflows size_t"); return XREDIS_READ_FAIL;
			}
			if (!xredisReplyPayload(Reader, (size_t)Event->Length)) return XREDIS_READ_FAIL;
		}
	} else if (Event->Kind == XREDIS_READ_DATA) {
		if (!xredisReplyPayload(Reader, Event->Data.Size)) return XREDIS_READ_FAIL;
		xredisreplynode* Node = &Reply->Nodes[Level->Open];
		memcpy(Reply->Data + Reply->DataBytes, Event->Data.Data, Event->Data.Size);
		Reply->DataBytes += Event->Data.Size; Node->Bytes += Event->Data.Size;
	} else {
		xredisreplynode* Node = &Reply->Nodes[Level->Open];
		Node->Length = Event->Length; Node->Integer = Event->Integer; Node->Boolean = Event->Boolean;
		if (Event->Type == XREDIS_REPLY_ATTRIBUTE) {
			if (Level->PendingLast == XREDIS_REPLY_NONE) Level->PendingFirst = Level->Open;
			else Reply->Nodes[Level->PendingLast].Next = Level->Open;
			Level->PendingLast = Level->Open;
		}
		Level->Open = XREDIS_REPLY_NONE;
	}
	return XREDIS_READ_NEXT;
}

XREDIS_API bool xredisReplyReaderCreate(const xredisreplyreaderconfig* Config, xredisreplyreader** Result)
{
	xredisreplyreaderconfig Copy;
	if (Result == NULL || !xrtMemRangeValid(Result, sizeof(*Result)) ||
		(Config != NULL && (!xrtMemRangeValid(Config, sizeof(*Config)) ||
		 xrtMemRangesOverlap(Config, sizeof(*Config), Result, sizeof(*Result)))))
		return xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "invalid reply reader creation arguments");
	if (Config == NULL) xredisReplyReaderConfigInit(&Copy); else Copy = *Config;
	if (Copy.Size != sizeof(Copy) || Copy.MaxOwnedBytes == 0 ||
		Copy.Reader.MaxDepth >= (SIZE_MAX - sizeof(xredisreplyreader)) / sizeof(xredisreplylevel))
		return xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "invalid reply reader size or limits");
	size_t Bytes = sizeof(xredisreplyreader) + (Copy.Reader.MaxDepth + 1u) * sizeof(xredisreplylevel);
	xredisreplyreader* Reader = (xredisreplyreader*)xrtCalloc(1, Bytes);
	if (Reader == NULL) return false;
	Reader->Config = Copy; Reader->StorageBytes = Bytes;
	if (!xredisReaderCreate(&Copy.Reader, xredisReplyCollect, Reader, &Reader->Parser)) { xrtFree(Reader); return false; }
	xredisReplyLevels(Reader); *Result = Reader;
	return true;
}

XREDIS_API void xredisReplyReaderDestroy(xredisreplyreader* Reader)
{
	if (Reader == NULL) return;
	xredisReplyRelease(Reader->Partial); xredisReaderDestroy(Reader->Parser); xrtFree(Reader);
}

XREDIS_API bool xredisReplyReaderReset(xredisreplyreader* Reader)
{
	if (Reader == NULL) return xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "reply reader is NULL");
	xredisReplyRelease(Reader->Partial); Reader->Partial = NULL;
	Reader->Error = XREDIS_REPLY_OK; Reader->ErrorKind = XERR_NONE; Reader->ErrorMessage = NULL; Reader->Peak = 0;
	xredisReplyLevels(Reader);
	return xredisReaderReset(Reader->Parser);
}

XREDIS_API bool xredisReplyReaderInfo(const xredisreplyreader* Reader, xredisreplyreaderinfo* Info)
{
	if (Reader == NULL || Info == NULL || !xrtMemRangeValid(Info, sizeof(*Info)) ||
		xredisReplyReaderOverlap(Reader, Info, sizeof(*Info)) || Info->Size != sizeof(*Info))
		return xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "invalid reply reader info output or size");
	xredisreplyreaderinfo Copy = {0}; Copy.Size = sizeof(Copy); Copy.Reader.Size = sizeof(Copy.Reader);
	if (!xredisReaderInfo(Reader->Parser, &Copy.Reader)) return false;
	Copy.Error = Reader->Error; Copy.PeakOwnedBytes = Reader->Peak;
	if (Reader->Partial != NULL) {
		Copy.PartialNodes = Reader->Partial->Count; Copy.PartialPayloadBytes = Reader->Partial->DataBytes;
		Copy.PartialReservedBytes = Reader->Partial->Reserved;
	}
	*Info = Copy; return true;
}

static bool xredisReplyParserFailure(xredisreplyreader* Reader)
{
	if (Reader->Error == XREDIS_REPLY_OK) {
		xredisreaderinfo Info = {0}; Info.Size = sizeof(Info); xredisReaderInfo(Reader->Parser, &Info);
		switch (Info.Error) {
			case XREDIS_READER_LIMIT:
				xredisReplyFail(Reader, XERR_RANGE, XREDIS_REPLY_LIMIT, "RESP reader limit exceeded"); break;
			case XREDIS_READER_TRUNCATED:
				xredisReplyFail(Reader, XERR_PROTOCOL, XREDIS_REPLY_TRUNCATED, "incomplete RESP reply at EOF"); break;
			case XREDIS_READER_PROTOCOL:
				xredisReplyFail(Reader, XERR_PROTOCOL, XREDIS_REPLY_PROTOCOL, "invalid RESP reply"); break;
			default:
				xredisReplyFail(Reader, XERR_STATE, XREDIS_REPLY_STATE, "unexpected RESP reader state"); break;
		}
	}
	xredisReplyRelease(Reader->Partial); Reader->Partial = NULL;
	return xredisReplyReport(Reader->ErrorKind, Reader->Error, Reader->ErrorMessage);
}

XREDIS_API xredisreplystatus xredisReplyReaderFeed(xredisreplyreader* Reader,
	xbytesview Input, size_t* Consumed, xredisreply** Result)
{
	if (Reader == NULL || Consumed == NULL || Result == NULL ||
		!xrtMemRangeValid(Consumed, sizeof(*Consumed)) || !xrtMemRangeValid(Result, sizeof(*Result)) ||
		(Input.Data == NULL && Input.Size != 0) || !xrtMemRangeValid(Input.Data, Input.Size) ||
		xredisReplyReaderOverlap(Reader, Input.Data, Input.Size) ||
		xredisReplyReaderOverlap(Reader, Consumed, sizeof(*Consumed)) ||
		xredisReplyReaderOverlap(Reader, Result, sizeof(*Result)) ||
		xrtMemRangesOverlap(Input.Data, Input.Size, Consumed, sizeof(*Consumed)) ||
		xrtMemRangesOverlap(Input.Data, Input.Size, Result, sizeof(*Result)) ||
		xrtMemRangesOverlap(Consumed, sizeof(*Consumed), Result, sizeof(*Result))) {
		xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "invalid reply input, outputs or overlap"); return XREDIS_REPLY_ERROR;
	}
	if (Reader->Error != XREDIS_REPLY_OK) {
		*Consumed = 0; xredisReplyReport(Reader->ErrorKind, Reader->Error, Reader->ErrorMessage); return XREDIS_REPLY_ERROR;
	}
	xredisreadstatus Status = xredisReaderFeed(Reader->Parser, Input, Consumed);
	if (Status == XREDIS_READER_ERROR) { xredisReplyParserFailure(Reader); return XREDIS_REPLY_ERROR; }
	if (Status == XREDIS_READER_READY) {
		*Result = Reader->Partial; Reader->Partial = NULL; xredisReplyLevels(Reader); return XREDIS_REPLY_READY;
	}
	return Status == XREDIS_READER_YIELD ? XREDIS_REPLY_YIELD : XREDIS_REPLY_MORE;
}

XREDIS_API bool xredisReplyReaderFinish(xredisreplyreader* Reader)
{
	if (Reader == NULL) return xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "reply reader is NULL");
	if (Reader->Error != XREDIS_REPLY_OK)
		return xredisReplyReport(Reader->ErrorKind, Reader->Error, Reader->ErrorMessage);
	if (!xredisReaderFinish(Reader->Parser)) return xredisReplyParserFailure(Reader);
	return true;
}

#if defined(XREDIS_FEATURE_XREDIS_PUBSUB)
static bool xredisPubSubString(const xredisreplynode* N)
{return (N->Type==XREDIS_REPLY_BULK_STRING || N->Type==XREDIS_REPLY_SIMPLE_STRING) && !(N->Flags&XREDIS_REPLY_NULL_VALUE);}
static bool xredisPubSubWord(const xredisreplynode* N,const xredisreply* R,cstr Word,size_t Length)
{return xredisPubSubString(N) && N->Bytes==Length && !memcmp(R->Data+N->Offset,Word,Length);}
static xbytesview xredisPubSubView(const xredisreplynode* N,const xredisreply* R)
{return (xbytesview){R->Data+N->Offset,N->Bytes};}
static bool xredisPubSubNullName(const xredisreplynode* N)
{return N->Type==XREDIS_REPLY_NULL || (N->Type==XREDIS_REPLY_BULK_STRING && (N->Flags&XREDIS_REPLY_NULL_VALUE));}
static bool xredisPubSubOutput(const xredisreply* R,xredispubsubevent* Output)
{
    if(!R || !Output || !xrtMemRangeValid(Output,sizeof(*Output)) || xredisReplyOverlap(R,Output,sizeof(*Output))){
        xrtSetErrorInfo(XERR_ARGUMENT,"xrt.redis",XREDIS_PUBSUB_ARGUMENT,"invalid Pub/Sub event output");return false;
    }
    return true;
}
static xredispubsubstatus xredisPubSubRead(const xredisreply* R,xredispubsubevent* Output)
{
    const xredisreplynode* Root=R->Nodes+R->Root;
    if((Root->Type!=XREDIS_REPLY_ARRAY && Root->Type!=XREDIS_REPLY_PUSH) || !Root->ChildCount)return XREDIS_PUBSUB_OTHER;
    const xredisreplynode* Tag=R->Nodes+Root->FirstChild;
    static const xbytesview Tags[]={XRT_BYTES_LITERAL("message"),XRT_BYTES_LITERAL("pmessage"),XRT_BYTES_LITERAL("smessage"),
        XRT_BYTES_LITERAL("subscribe"),XRT_BYTES_LITERAL("psubscribe"),XRT_BYTES_LITERAL("ssubscribe"),
        XRT_BYTES_LITERAL("unsubscribe"),XRT_BYTES_LITERAL("punsubscribe"),XRT_BYTES_LITERAL("sunsubscribe"),XRT_BYTES_LITERAL("pong")};
    _Static_assert(sizeof(Tags)/sizeof(*Tags)==XREDIS_PUBSUB_PONG+1,"tag table follows public event kinds");
    size_t Kind;
    for(Kind=0;Kind<sizeof(Tags)/sizeof(*Tags);++Kind)
        if(xredisPubSubWord(Tag,R,(cstr)Tags[Kind].Data,Tags[Kind].Size))break;
    if(Kind==sizeof(Tags)/sizeof(*Tags))return XREDIS_PUBSUB_OTHER;
    size_t Arity=Kind==XREDIS_PUBSUB_PMESSAGE?4:Kind==XREDIS_PUBSUB_PONG?2:3;
    if(Root->ChildCount!=Arity)return XREDIS_PUBSUB_MALFORMED;
    xredispubsubevent E={0};E.Reply=R;E.Kind=(xredispubsubkind)Kind;E.Envelope=Root->Type;E.PayloadNode=XREDIS_REPLY_NONE;
    const xredisreplynode* Name=R->Nodes+Tag->Next;
    if(Kind>=XREDIS_PUBSUB_SUBSCRIBE && Kind<=XREDIS_PUBSUB_SUNSUBSCRIBE){
        const xredisreplynode* Count=R->Nodes+Name->Next;
        if(Count->Type!=XREDIS_REPLY_INTEGER || Count->Integer<0 || (Kind<=XREDIS_PUBSUB_SSUBSCRIBE && !Count->Integer))return XREDIS_PUBSUB_MALFORMED;
        if(xredisPubSubNullName(Name)){
            if(Kind<XREDIS_PUBSUB_UNSUBSCRIBE)return XREDIS_PUBSUB_MALFORMED;
            E.Flags=XREDIS_PUBSUB_NAME_ABSENT;
        }else {
            if(!xredisPubSubString(Name))return XREDIS_PUBSUB_MALFORMED;
            if(Kind==XREDIS_PUBSUB_PSUBSCRIBE || Kind==XREDIS_PUBSUB_PUNSUBSCRIBE)E.Pattern=xredisPubSubView(Name,R);
            else E.Channel=xredisPubSubView(Name,R);
        }
        E.Subscriptions=(uint64)Count->Integer;
    }else if(Kind==XREDIS_PUBSUB_PONG){
        if(!xredisPubSubString(Name))return XREDIS_PUBSUB_MALFORMED;
        E.Payload=xredisPubSubView(Name,R);E.PayloadNode=Tag->Next;
    }else{
        if(!xredisPubSubString(Name))return XREDIS_PUBSUB_MALFORMED;
        const xredisreplynode* Channel=Name;
        if(Kind==XREDIS_PUBSUB_PMESSAGE){E.Pattern=xredisPubSubView(Name,R);Channel=R->Nodes+Name->Next;if(!xredisPubSubString(Channel))return XREDIS_PUBSUB_MALFORMED;}
        E.Channel=xredisPubSubView(Channel,R);E.PayloadNode=Channel->Next;const xredisreplynode* Payload=R->Nodes+E.PayloadNode;
        if(xredisPubSubString(Payload))E.Payload=xredisPubSubView(Payload,R);
        else {
            if(Kind!=XREDIS_PUBSUB_MESSAGE || Root->Type!=XREDIS_REPLY_ARRAY || !xredisPubSubWord(Channel,R,"__redis__:invalidate",20))return XREDIS_PUBSUB_MALFORMED;
            bool Null=Payload->Type==XREDIS_REPLY_NULL || ((Payload->Type==XREDIS_REPLY_ARRAY || Payload->Type==XREDIS_REPLY_BULK_STRING) && (Payload->Flags&XREDIS_REPLY_NULL_VALUE));
            if(!Null){
                if(Payload->Type!=XREDIS_REPLY_ARRAY)return XREDIS_PUBSUB_MALFORMED;
                for(size_t N=Payload->FirstChild;N!=XREDIS_REPLY_NONE;N=R->Nodes[N].Next)
                    if(R->Nodes[N].Type!=XREDIS_REPLY_BULK_STRING || (R->Nodes[N].Flags&XREDIS_REPLY_NULL_VALUE))return XREDIS_PUBSUB_MALFORMED;
            }
            E.Flags=XREDIS_PUBSUB_STRUCTURED;
        }
    }
    *Output=E;return XREDIS_PUBSUB_EVENT;
}
XREDIS_API xredispubsubstatus xredisPubSubEventRead(const xredisreply* R,xredispubsubevent* Output)
{
    if(!xredisPubSubOutput(R,Output))return XREDIS_PUBSUB_ERROR;
    return xredisPubSubRead(R,Output);
}
XREDIS_API xredispubsubstatus xredisPubSubPongRead(const xredisreply* R,const xbytesview* Expected,xredispubsubevent* Output)
{
    if(!xredisPubSubOutput(R,Output))return XREDIS_PUBSUB_ERROR;
    if(Expected && (!xrtMemRangeValid(Expected,sizeof(*Expected)) || xrtMemRangesOverlap(Output,sizeof(*Output),Expected,sizeof(*Expected)) ||
        !xrtMemRangeValid(Expected->Data,Expected->Size) || xrtMemRangesOverlap(Output,sizeof(*Output),Expected->Data,Expected->Size))){
        xrtSetErrorInfo(XERR_ARGUMENT,"xrt.redis",XREDIS_PUBSUB_ARGUMENT,"invalid Pub/Sub expected PING payload");return XREDIS_PUBSUB_ERROR;
    }
    const xredisreplynode* Root=R->Nodes+R->Root;xredispubsubevent E;
    if(Root->Type==XREDIS_REPLY_ARRAY || Root->Type==XREDIS_REPLY_PUSH){
        xredispubsubstatus Status=xredisPubSubRead(R,&E);
        if(Status!=XREDIS_PUBSUB_EVENT)return Status;
        if(E.Kind!=XREDIS_PUBSUB_PONG)return XREDIS_PUBSUB_OTHER;
    }else{
        if(Root->Type==XREDIS_REPLY_SIMPLE_ERROR || Root->Type==XREDIS_REPLY_BULK_ERROR)return XREDIS_PUBSUB_OTHER;
        E=(xredispubsubevent){0};E.Reply=R;E.Kind=XREDIS_PUBSUB_PONG;E.Envelope=Root->Type;E.PayloadNode=R->Root;
        if(!Expected){if(!xredisPubSubWord(Root,R,"PONG",4) || Root->Type!=XREDIS_REPLY_SIMPLE_STRING)return XREDIS_PUBSUB_MALFORMED;}
        else {if(Root->Type!=XREDIS_REPLY_BULK_STRING || !xredisPubSubString(Root))return XREDIS_PUBSUB_MALFORMED;E.Payload=xredisPubSubView(Root,R);}
    }
    size_t Size=Expected?Expected->Size:0;
    if(E.Payload.Size!=Size || (Size && memcmp(E.Payload.Data,Expected->Data,Size)))return XREDIS_PUBSUB_MALFORMED;
    *Output=E;return XREDIS_PUBSUB_EVENT;
}
#endif

#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
static bool xredisTrackingWord(const xredisreplynode* N,const xredisreply* R,cstr Word,size_t Length)
{
    return (N->Type==XREDIS_REPLY_BULK_STRING || N->Type==XREDIS_REPLY_SIMPLE_STRING) && !(N->Flags&XREDIS_REPLY_NULL_VALUE) &&
        N->Bytes==Length && !memcmp(R->Data+N->Offset,Word,Length);
}
XREDIS_API xredistrackingstatus xredisTrackingEventRead(const xredisreply* R,xredistrackingevent* Output)
{
    if(!R || !Output || !xrtMemRangeValid(Output,sizeof(*Output)) || xredisReplyOverlap(R,Output,sizeof(*Output))){
        xredisReplyReport(XERR_ARGUMENT,XREDIS_REPLY_ARGUMENT,"invalid tracking event output");return XREDIS_TRACKING_ERROR;
    }
    const xredisreplynode* Root=R->Nodes+R->Root;
    if((Root->Type!=XREDIS_REPLY_PUSH && Root->Type!=XREDIS_REPLY_ARRAY) || !Root->ChildCount)return XREDIS_TRACKING_OTHER;
    size_t First=Root->FirstChild;const xredisreplynode* Tag=R->Nodes+First;
    bool Keys=Root->Type==XREDIS_REPLY_PUSH && xredisTrackingWord(Tag,R,"invalidate",10);
    bool Broken=Root->Type==XREDIS_REPLY_PUSH && xredisTrackingWord(Tag,R,"tracking-redir-broken",21);
    size_t Payload=Tag->Next;
    if(Root->Type==XREDIS_REPLY_ARRAY && xredisTrackingWord(Tag,R,"message",7) && Root->ChildCount>=2){
        const xredisreplynode* Channel=R->Nodes+Payload;
        if(xredisTrackingWord(Channel,R,"__redis__:invalidate",20)){
            if(Root->ChildCount!=3)return XREDIS_TRACKING_MALFORMED;
            Keys=true;Payload=Channel->Next;
        }
    }
    if(!Keys && !Broken)return XREDIS_TRACKING_OTHER;
    if(Root->Type==XREDIS_REPLY_PUSH && Root->ChildCount!=2)return XREDIS_TRACKING_MALFORMED;
    const xredisreplynode* Value=R->Nodes+Payload;
    xredistrackingevent E={R,XREDIS_TRACKING_KEYS,XREDIS_REPLY_NONE,0,0};
    if(Broken){
        if(Value->Type!=XREDIS_REPLY_INTEGER || Value->Integer<0)return XREDIS_TRACKING_MALFORMED;
        E.Kind=XREDIS_TRACKING_REDIRECT_BROKEN;E.Redirect=(uint64)Value->Integer;
    }else if(Value->Type==XREDIS_REPLY_NULL ||
        ((Value->Type==XREDIS_REPLY_ARRAY || Value->Type==XREDIS_REPLY_BULK_STRING) && (Value->Flags&XREDIS_REPLY_NULL_VALUE)))E.Kind=XREDIS_TRACKING_FLUSH;
    else {
        if(Value->Type!=XREDIS_REPLY_ARRAY)return XREDIS_TRACKING_MALFORMED;
        E.FirstKey=Value->FirstChild;E.Keys=Value->ChildCount;
        for(size_t N=E.FirstKey;N!=XREDIS_REPLY_NONE;N=R->Nodes[N].Next)
            if(R->Nodes[N].Type!=XREDIS_REPLY_BULK_STRING || (R->Nodes[N].Flags&XREDIS_REPLY_NULL_VALUE))return XREDIS_TRACKING_MALFORMED;
    }
    *Output=E;return XREDIS_TRACKING_EVENT;
}
#endif


#if defined(XREDIS_FEATURE_XREDIS_ROLE)
static bool xredisRoleArgument(cstr Text)
{return xredisReplyReport(XERR_ARGUMENT,XREDIS_REPLY_ARGUMENT,Text);}
static bool xredisRoleString(const xredisreplynode* N)
{return (N->Type==XREDIS_REPLY_BULK_STRING || N->Type==XREDIS_REPLY_SIMPLE_STRING) && !(N->Flags&XREDIS_REPLY_NULL_VALUE);}
static xbytesview xredisRoleView(const xredisreply* R,const xredisreplynode* N)
{return (xbytesview){N->Bytes?R->Data+N->Offset:NULL,N->Bytes};}
static bool xredisRoleWord(const xredisreply* R,const xredisreplynode* N,cstr Text)
{return xredisRoleString(N) && N->Bytes==strlen(Text) && !memcmp(R->Data+N->Offset,Text,N->Bytes);}
static bool xredisRoleArray(const xredisreplynode* N,size_t Count)
{return N->Type==XREDIS_REPLY_ARRAY && !(N->Flags&XREDIS_REPLY_NULL_VALUE) && N->ChildCount==Count;}
static bool xredisRoleUnsigned(const xredisreply* R,const xredisreplynode* V,int64* Output)
{
    if(V->Type==XREDIS_REPLY_INTEGER){if(V->Integer<0)return false;*Output=V->Integer;return true;}
    if(!xredisRoleString(V) || !V->Bytes)return false;
    uint64 N=0;for(size_t I=0;I<V->Bytes;++I){unsigned C=R->Data[V->Offset+I];if(C<'0' || C>'9' || N>((uint64)INT64_MAX-(C-'0'))/10)return false;N=N*10+C-'0';}*Output=(int64)N;return true;
}
static bool xredisRoleTuple(const xredisreply* R,size_t Node,xredisrolereplica* Output,size_t* Next)
{
    if(Node>=R->Count)return false;
    const xredisreplynode* T=R->Nodes+Node;if(!xredisRoleArray(T,3))return false;
    const xredisreplynode* H=R->Nodes+T->FirstChild;const xredisreplynode* P=R->Nodes+H->Next;const xredisreplynode* O=R->Nodes+P->Next;
    int64 Port,Offset;if(!xredisRoleString(H) || !xredisRoleUnsigned(R,P,&Port) || Port>65535 || !xredisRoleUnsigned(R,O,&Offset))return false;
    *Output=(xredisrolereplica){xredisRoleView(R,H),(uint16)Port,Offset};*Next=T->Next;return true;
}
XREDIS_API bool xredisRoleLimitsInit(xredisrolelimits* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K)))return xredisRoleArgument("invalid ROLE limits output");
    *K=(xredisrolelimits){sizeof(*K),4096,1024u*1024u};return true;
}
XREDIS_API xredisrolestatus xredisRoleRead(const xredisreply* R,const xredisrolelimits* Config,xredisroleinfo* Output)
{
    if(!R || !Output || !xrtMemRangeValid(Output,sizeof(*Output)) || xredisReplyOverlap(R,Output,sizeof(*Output)) ||
        (Config && (!xrtMemRangeValid(Config,sizeof(*Config)) || Config->Size!=sizeof(*Config) || xrtMemRangesOverlap(Config,sizeof(*Config),Output,sizeof(*Output))))){xredisRoleArgument("invalid ROLE arguments/limits");return XREDIS_ROLE_ERROR;}
    xredisrolelimits K=Config?*Config:(xredisrolelimits){sizeof(K),4096,1024u*1024u};
    const xredisreplynode* Root=R->Nodes+R->Root;
    if(Root->Type==XREDIS_REPLY_SIMPLE_ERROR || Root->Type==XREDIS_REPLY_BULK_ERROR)return XREDIS_ROLE_SERVER_ERROR;
    if(Root->Type!=XREDIS_REPLY_ARRAY || (Root->Flags&XREDIS_REPLY_NULL_VALUE) || !Root->ChildCount)return XREDIS_ROLE_OTHER;
    const xredisreplynode* Word=R->Nodes+Root->FirstChild;
    xredisrolekind Kind=xredisRoleWord(R,Word,"master")?XREDIS_ROLE_MASTER:xredisRoleWord(R,Word,"slave")?XREDIS_ROLE_REPLICA:xredisRoleWord(R,Word,"sentinel")?XREDIS_ROLE_SENTINEL:0;
    if(!Kind)return XREDIS_ROLE_OTHER;
    if(R->DataBytes>K.MaxPayloadBytes)return XREDIS_ROLE_LIMIT;
    if(!xredisRoleArray(Root,Kind==XREDIS_ROLE_MASTER?3:Kind==XREDIS_ROLE_REPLICA?5:2))return XREDIS_ROLE_MALFORMED;
    xredisroleinfo V={0};V.Kind=Kind;V.Replicas.Next=V.Monitored.Next=XREDIS_REPLY_NONE;
    const xredisreplynode* A=R->Nodes+Word->Next;
    if(Kind==XREDIS_ROLE_REPLICA){
        const xredisreplynode* B=R->Nodes+A->Next;const xredisreplynode* C=R->Nodes+B->Next;const xredisreplynode* D=R->Nodes+C->Next;
        if(!xredisRoleString(A) || B->Type!=XREDIS_REPLY_INTEGER || B->Integer<0 || B->Integer>65535 || !xredisRoleString(C) || D->Type!=XREDIS_REPLY_INTEGER || D->Integer< -1)return XREDIS_ROLE_MALFORMED;
        V.MasterHost=xredisRoleView(R,A);V.MasterPort=(uint16)B->Integer;V.Offset=D->Integer;V.LinkText=xredisRoleView(R,C);
        V.Link=xredisRoleWord(R,C,"connect")?XREDIS_ROLE_LINK_CONNECT:xredisRoleWord(R,C,"connecting")?XREDIS_ROLE_LINK_CONNECTING:xredisRoleWord(R,C,"sync")?XREDIS_ROLE_LINK_SYNC:xredisRoleWord(R,C,"connected")?XREDIS_ROLE_LINK_CONNECTED:XREDIS_ROLE_LINK_OTHER;
    }else{
        const xredisreplynode* B=A;
        if(Kind==XREDIS_ROLE_MASTER){if(A->Type!=XREDIS_REPLY_INTEGER || A->Integer<0)return XREDIS_ROLE_MALFORMED;V.Offset=A->Integer;B=R->Nodes+A->Next;}
        if(B->Type!=XREDIS_REPLY_ARRAY || (B->Flags&XREDIS_REPLY_NULL_VALUE))return XREDIS_ROLE_MALFORMED;
        if(B->ChildCount>K.MaxEntries)return XREDIS_ROLE_LIMIT;
        size_t Node=B->FirstChild;for(size_t N=0;N<B->ChildCount;++N){
            if(Kind==XREDIS_ROLE_MASTER){xredisrolereplica Peer;size_t Next;if(!xredisRoleTuple(R,Node,&Peer,&Next))return XREDIS_ROLE_MALFORMED;Node=Next;}
            else{const xredisreplynode* Name=R->Nodes+Node;if(!xredisRoleString(Name))return XREDIS_ROLE_MALFORMED;Node=Name->Next;}
        }
        xredisrolecursor Cursor={R,B->FirstChild,B->ChildCount,(uint32)Kind};if(Kind==XREDIS_ROLE_MASTER)V.Replicas=Cursor;else V.Monitored=Cursor;
    }
    *Output=V;return XREDIS_ROLE_VALUE;
}
static bool xredisRoleCursorValid(xredisrolecursor* C,ptr Output,size_t Bytes,xredisrolekind Kind)
{
    if(!C || !Output || !xrtMemRangeValid(C,sizeof(*C)) || !xrtMemRangeValid(Output,Bytes) || xrtMemRangesOverlap(C,sizeof(*C),Output,Bytes) || !C->Source || C->Kind!=(uint32)Kind ||
        xredisReplyOverlap(C->Source,C,sizeof(*C)) || xredisReplyOverlap(C->Source,Output,Bytes) || C->Remaining>C->Source->Count || (!C->Remaining && C->Next!=XREDIS_REPLY_NONE))return xredisRoleArgument("invalid ROLE cursor/output");
    return true;
}
XREDIS_API bool xredisRoleReplicaNext(xredisrolecursor* C,xredisrolereplica* Output)
{
    if(!xredisRoleCursorValid(C,Output,sizeof(*Output),XREDIS_ROLE_MASTER) || !C->Remaining)return false;
    xredisrolereplica V;size_t Next;if(!xredisRoleTuple(C->Source,C->Next,&V,&Next) || (C->Remaining==1)!=(Next==XREDIS_REPLY_NONE))return xredisRoleArgument("invalid ROLE replica cursor chain");
    *Output=V;C->Next=Next;--C->Remaining;return true;
}
XREDIS_API bool xredisRoleMasterNext(xredisrolecursor* C,xbytesview* Output)
{
    if(!xredisRoleCursorValid(C,Output,sizeof(*Output),XREDIS_ROLE_SENTINEL) || !C->Remaining)return false;
    if(C->Next>=C->Source->Count)return xredisRoleArgument("invalid ROLE monitor cursor index");
    const xredisreplynode* N=C->Source->Nodes+C->Next;if(!xredisRoleString(N) || (C->Remaining==1)!=(N->Next==XREDIS_REPLY_NONE))return xredisRoleArgument("invalid ROLE monitor cursor chain");
    *Output=xredisRoleView(C->Source,N);C->Next=N->Next;--C->Remaining;return true;
}
#endif

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/client/client.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_CLIENT) || \
	defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY) || \
	defined(XREDIS_FEATURE_XREDIS_ROLE_CLIENT) || \
	defined(XREDIS_FEATURE_XREDIS_SENTINEL) || \
	defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
#if defined(XREDIS_FEATURE_XREDIS_ROLE_CLIENT)
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS)
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
#endif
#include <math.h>
#include <string.h>

#if defined(XREDIS_FEATURE_XREDIS_CLIENT)

typedef struct xredisrequest xredisrequest;
typedef struct xredisinitwire xredisinitwire;
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
typedef struct xredisobservation {size_t Bytes;xatomic64 Sequence;uint64 Pushes[];} xredisobservation;
/* Observe tags 3/4 select a pair without enlarging ordinary requests. */
typedef struct xrediscachingstate {
    size_t MetadataBytes;
    xredisreply *Control,*Query;
    uint64 ControlSequence,QuerySequence;
    xredisrequeststatus ControlStatus,QueryStatus;
    unsigned Seen;
    bool Written;
} xrediscachingstate;
static const uint8 xredisCachingYes[]="*3\r\n$6\r\nCLIENT\r\n$7\r\nCACHING\r\n$3\r\nYES\r\n";
static const uint8 xredisCachingNo[]="*3\r\n$6\r\nCLIENT\r\n$7\r\nCACHING\r\n$2\r\nNO\r\n";
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRACKING) || defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
#define XREDIS_OBSERVE_PARAMETER ,unsigned Observe
#define XREDIS_OBSERVE_ORDINARY ,false
#else
#define XREDIS_OBSERVE_PARAMETER
#define XREDIS_OBSERVE_ORDINARY
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
enum { XREDIS_TX_MULTI, XREDIS_TX_COMMANDS, XREDIS_TX_EXEC, XREDIS_TX_DISCARD };
typedef struct xredistransactionstate {
	size_t Count, Seen, Errors, CommandsBytes, Written, MetadataBytes;
	xredisreply* Error;
	xredistransactionexecution Execution;
	unsigned Phase;
	bool Await;
} xredistransactionstate;
static const uint8 xredisTransactionMulti[] = "*1\r\n$5\r\nMULTI\r\n";
static const uint8 xredisTransactionExec[] = "*1\r\n$4\r\nEXEC\r\n";
static const uint8 xredisTransactionDiscard[] = "*1\r\n$7\r\nDISCARD\r\n";
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
typedef struct xredisaskingstate {
    size_t MetadataBytes;xredisreply *Control,*Query;
    xredisrequeststatus ControlStatus,QueryStatus;unsigned Seen;bool Written;
} xredisaskingstate;
static const uint8 xredisAskingWire[]="*1\r\n$6\r\nASKING\r\n";
#define XREDIS_CALL_MODE_PARAMETER unsigned CallMode
#else
#define XREDIS_CALL_MODE_PARAMETER bool Control
#endif
enum { XREDIS_R_SUBMITTED, XREDIS_R_PENDING, XREDIS_R_SENT, XREDIS_R_DONE };
enum { XREDIS_I_HELLO, XREDIS_I_AUTH, XREDIS_I_NAME, XREDIS_I_SELECT, XREDIS_I_END };
#if defined(XREDIS_FEATURE_XREDIS_ROLE_CLIENT)
/* END remains the four-slot wire table/idle tag; ROLE is a fixed tail slice. */
enum { XREDIS_I_ROLE = XREDIS_I_END + 1, XREDIS_I_ROLE_DONE };
#define XREDIS_INIT_ROLES (XREDIS_INIT_REQUIRE_MASTER|XREDIS_INIT_REQUIRE_REPLICA|XREDIS_INIT_REQUIRE_SENTINEL)
static const unsigned char xredisRoleWire[] = "*1\r\n$4\r\nROLE\r\n";
#endif


struct xredisinitwire {
	xatomic32 References;
	xredisclient* Client;
	size_t Size;
	unsigned char Data[];
};

#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
typedef struct xredissubentry {
    struct xredissubentry *BucketNext,*Next,*Previous;
    uint64 Hash;size_t Bytes,Reservations;unsigned Family;bool Confirmed;
    unsigned char Name[];
} xredissubentry;
typedef struct xredissubscriber {
    xredissubscriberconfig Config;xredissubscriberinfo Info;
    size_t StorageBytes,Buckets,Entries,NameBytes,EntryBytes,Counts[3];
    xredissubentry* Head;
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY)
    xpromise* ReadablePromise;
    xfuture* ReadableFuture;
    bool ReadablePreparing;
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS)
    ptr ReplayOwner;
#endif
    xredissubentry* Table[];
} xredissubscriber;
typedef struct xredissubcall {
    size_t MetadataBytes,Count,Cursor,Acknowledged,Expected,Reserved;
    xredispubsubcommand Command;
    bool Prepared,Written,Finished,AllInitialized,Absent;
    xredisrequeststatus Status;xredisreply* Final;
    xredissubentry* Entries[];
} xredissubcall;
static void xredisSubscriberRetire(xredisrequest* Request);
static void xredisSubscriberRelease(xredisclient* Client);
static bool xredisSubscriberCloseStep(xredisclient* Client);
static bool xredisSubscriberPrepare(xredisrequest* Request);
static void xredisSubscriberWritten(xredisrequest* Request);
static void xredisSubscriberFrame(xredisclient* Client,xredisreply* Reply,const xredisreplyvalue* Root,size_t Bytes);
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY)
static void xredisSubscriberNotify(xpromise* Promise);
#endif
#define XREDIS_SUBSCRIBER_RETIRE(R) do{if((R)->Observe>=5)xredisSubscriberRetire(R);}while(0)
#else
#define XREDIS_SUBSCRIBER_RETIRE(R) ((void)0)
#endif

struct xredisrequest {
	xatomic32 References, Cancelled;
	xredisclient* Client;
	xpromise* Promise;
	xcancelwatch* Watch;
	xredisrequestresult* Result;
	xredisrequest *Next, *Previous, *SubmitNext, *CancelNext;
	size_t WireBytes;
	double Deadline;
	uint64 Timer;
	unsigned Phase;
	bool CancelQueued;
#if defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
    /* Bit0 retains wire wiping; bit7 selects optional ASKING pair state.
     * Same byte width/alignment and Wire offset as the former bool. */
    uint8 Sensitive;
#else
    bool Sensitive;
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRACKING) || defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    uint8 Observe;
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
	bool Transaction;
#endif
	unsigned char Wire[];
};

struct xredisclient {
	xatomic32 References;
	xmutex Gate;
	xnetengine* Engine;
	xnetworker* Worker;
	xnetstream* Stream;
	xnetaddr Remote;
	xredisclientconfig Config;
	xredisreplyreader* Reader;
	xpromise* ClosePromise;
	xfuture* CloseFuture;
	xpromise* ReadyPromise;
	xfuture* ReadyFuture;
	xredisreadyresult* ReadyResult;
	xredisreply* Hello;
	xredisinitwire* InitWire;
	xredisinitmode InitMode;
	uint32 InitFlags;
	size_t InitOffset[XREDIS_I_END], InitLength[XREDIS_I_END], InitNext, InitAwait;
	double InitDeadline;
	uint64 InitTimer;
	xnetpost Wake;
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
	xredisexclusiveowner* Owner;
#endif
	xredisrequest *SubmitHead, *SubmitTail, *CancelHead, *CancelTail;
	xredisrequest *PendingHead, *PendingTail, *SentHead, *SentTail;
	xredisclientinfo Info;
	xerror* CloseCause;
	size_t StorageBytes, SentCount, PushHead;
	bool WakeQueued, CloseDone, EnginePinned;
	/* Only the owning worker accesses these fields. */
	bool Started, TransportClosed, ReadEnded, Reading, ReadYielded, ReadPaused;
	bool Managed, TransportOpen;
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
	const xredisconnectplan* Plan;
	bool PlanOwned, PlanPending, PlanCancelled;
#endif
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	xtlsstream* TLS;
	xredisrequest* TlsWrite;
	size_t TlsWritten, TlsWriteBytes, TlsTurnBytes, TlsTurnCalls, InitWritten;
	bool InitSending;
#endif
	#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    xatomicptr Observation;uint64 ObservationRejected;bool ObservationPreparing;
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    xatomicptr Subscriber;
#endif
    xredisreply* PushQueue[];
};

#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
/* The tag occupies existing tail padding. Only transactions compute their
 * aligned state address; ordinary request allocation size stays unchanged. */
static xredistransactionstate* xredisTransactionState(xredisrequest* Request)
{
    size_t Alignment = _Alignof(xredistransactionstate);
    size_t Padding = (Alignment - (offsetof(xredisrequest, Wire) + Request->WireBytes) % Alignment) % Alignment;
    return (xredistransactionstate*)(Request->Wire + Request->WireBytes + Padding);
}
#endif

#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
static bool xredisCachingPair(const xredisrequest* R) { return R->Observe==3 || R->Observe==4; }
static xrediscachingstate* xredisCachingState(xredisrequest* R)
{
    size_t A=_Alignof(xrediscachingstate);
    size_t Padding=(A-(offsetof(xredisrequest,Wire)+R->WireBytes)%A)%A;
    return (xrediscachingstate*)(R->Wire+R->WireBytes+Padding);
}
static bool xredisTrackingOutput(xredisclient* C,const void* Output,size_t Bytes);
static void xredisCachingWritten(xredisrequest* R);
static void xredisCachingFrame(xredisrequest* R,xredisreply* Reply,const xredisreplyvalue* Root,uint64 Sequence);
#endif
static void xredisClientWork(xnetworker* Worker, ptr Data);
static void xredisClientRead(xnetstream* Stream, xnetbuf* Buffer, ptr Data);
static void xredisClientReadBuffer(xredisclient* Client, const xnetbuf* Buffer);
static void xredisClientTimer(xnetworker* Worker, uint64 Id, xnetresult Result, ptr Data);
static void xredisClientRef(xredisclient* Client);
static bool xredisClientWakeLocked(xredisclient* Client);
static void xredisClientWake(xredisclient* Client);
static void xredisClientFail(xredisclient* Client, xredisrequeststatus Status, const xerror* Cause);
static void xredisClientReadyFinish(xredisclient* Client, xredisreadystatus Status, const xerror* Cause);
static void xredisClientInitPump(xredisclient* Client);
static void xredisClientInitFrame(xredisclient* Client, xredisreply* Reply, const xredisreplyvalue* Root);
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
static void xredisTransactionPump(xredisrequest* Request);
static void xredisTransactionFrame(xredisrequest* Request, xredisreply* Reply, const xredisreplyvalue* Root);
#endif

#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
static xredissubcall* xredisSubscriberState(xredisrequest* R)
{
    size_t A=_Alignof(xredissubcall),Padding=(A-(offsetof(xredisrequest,Wire)+R->WireBytes)%A)%A;
    return (xredissubcall*)(R->Wire+R->WireBytes+Padding);
}
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
_Static_assert(sizeof(bool)==sizeof(uint8),"ASKING mode preserves private byte layout");
_Static_assert(offsetof(xredisaskingresult,Query)==0,"native result prefix");
static bool xredisAskingPair(const xredisrequest* R){return (R->Sensitive&128u)!=0;}
static xredisaskingstate* xredisAskingState(xredisrequest* R)
{
    size_t A=_Alignof(xredisaskingstate),P=(A-(offsetof(xredisrequest,Wire)+R->WireBytes)%A)%A;
    return (xredisaskingstate*)(R->Wire+R->WireBytes+P);
}
#endif
static void xredisClientTransportAbort(xredisclient* Client)
{
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Client->TLS != NULL) { (void)xrtTlsStreamAbort(Client->TLS); return; }
#endif
	if (Client->Stream != NULL) xrtNetStreamAbort(Client->Stream); else Client->TransportClosed = true;
}

static bool xredisClientTransportOpen(xredisclient* Client)
{
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Client->TLS != NULL) return xrtTlsStreamState(Client->TLS) == XTLS_STREAM_OPEN;
#endif
	return Client->Stream != NULL && xrtNetStreamState(Client->Stream) == XNET_STREAM_OPEN;
}

static const xnetbuf* xredisClientTransportBuffer(xredisclient* Client)
{
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Client->TLS != NULL) return Client->TransportOpen ? xrtTlsStreamBuffer(Client->TLS) : NULL;
#endif
	return Client->Stream == NULL ? NULL : xrtNetStreamBuffer(Client->Stream);
}

static bool xredisClientReport(xerrkind Kind, xredisclienterror Code, cstr Message)
{
	xrtSetErrorInfo(Kind, "xrt.redis", (int32)Code, Message); return false;
}

static double xredisClientDeadline(double Now, int64 Timeout)
{
	if (Timeout == XRT_WAIT_FOREVER) return INFINITY;
	if (Timeout == 0) return Now;
	return nextafter(Now + (double)(Timeout / 1000) + (double)(Timeout % 1000) * 0.001, INFINITY);
}

static void xredisInitWireRelease(xredisinitwire* Wire)
{
	if (Wire == NULL || xrtAtomic32FetchSub(&Wire->References, 1, XMEMORY_ACQ_REL) != 1) return;
	xredisclient* Client = Wire->Client;
	xrtSecureZero(Wire->Data, Wire->Size); xrtFree(Wire);
	if (Client != NULL) {
		xrtMutexLock(&Client->Gate); Client->Info.InitBytes = 0;
		if (Client->Info.State >= XREDIS_CLIENT_CLOSING) (void)xredisClientWakeLocked(Client);
		xrtMutexUnlock(&Client->Gate); xredisClientRelease(Client);
	}
}

static void xredisInitWireDrop(ptr Data, cbytes Bytes, size_t Size)
{ (void)Bytes; (void)Size; xredisInitWireRelease((xredisinitwire*)Data); }

static void xredisClientReadyDrop(ptr Data, ptr Context)
{
	(void)Context; xredisreadyresult* Result = (xredisreadyresult*)Data;
	xredisReplyRelease(Result->Hello); xrtErrorFree((xerror*)Result->Cause); xrtFree(Result);
}

XREDIS_API bool xredisClientInitConfigInit(xredisclientinitconfig* Init)
{
	if (Init == NULL || !xrtMemRangeValid(Init, sizeof(*Init)))
		return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid initialization config output");
	*Init = (xredisclientinitconfig){0}; Init->Size = sizeof(*Init); Init->Mode = XREDIS_INIT_RESP3;
	Init->Timeout = 5000; Init->MaxWireBytes = 64u * 1024u; return true;
}

static bool xredisClientInitValid(const xredisclientinitconfig* Init, const xredisclientconfig* Config)
{
	uint32 Known = XREDIS_INIT_AUTH | XREDIS_INIT_USERNAME | XREDIS_INIT_SETNAME | XREDIS_INIT_SELECT;
#if defined(XREDIS_FEATURE_XREDIS_ROLE_CLIENT)
    Known |= XREDIS_INIT_ROLES;
    uint32 Roles = Init->Flags & XREDIS_INIT_ROLES;
    if (Roles && (Roles & (Roles - 1u))) return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "choose only one initialization role");
#endif
	if (Init->Size != sizeof(*Init) || (Init->Mode != XREDIS_INIT_AUTO && Init->Mode != XREDIS_INIT_RESP2 && Init->Mode != XREDIS_INIT_RESP3) ||
		(Init->Flags & ~Known) != 0 || Init->Timeout < XRT_WAIT_FOREVER || Init->MaxWireBytes == 0 ||
		((Init->Flags & XREDIS_INIT_USERNAME) && !(Init->Flags & XREDIS_INIT_AUTH)) ||
		(!(Init->Flags & XREDIS_INIT_AUTH) && Init->Password.Size != 0) ||
		(!(Init->Flags & XREDIS_INIT_USERNAME) && Init->Username.Size != 0) ||
		(!(Init->Flags & XREDIS_INIT_SETNAME) && Init->Name.Size != 0) ||
		(!(Init->Flags & XREDIS_INIT_SELECT) && Init->Database != 0) ||
		(Init->Mode != XREDIS_INIT_RESP2 && Config->Replies.Reader.Protocol != XREDIS_RESP3))
		return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid initialization mode, flags, limits or reader");
	const xbytesview Views[] = {Init->Username, Init->Password, Init->Name};
	for (size_t Index = 0; Index < sizeof(Views) / sizeof(Views[0]); ++Index)
		if ((Views[Index].Data == NULL && Views[Index].Size != 0) || !xrtMemRangeValid(Views[Index].Data, Views[Index].Size))
			return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid initialization input span");
	return true;
}

static bool xredisClientInitBuild(xredisclient* Client, const xredisclientinitconfig* Init)
{
	xbytesview Args[XREDIS_I_END][7]; size_t Counts[XREDIS_I_END] = {0};
	char Database[32]; size_t DatabaseBytes = 0;
	if (Init->Flags & XREDIS_INIT_SELECT) {
		if (!xrtUIntWrite(Init->Database, 10, Database, sizeof(Database), &DatabaseBytes, 0)) return false;
	}
	if (Init->Mode != XREDIS_INIT_RESP2) {
		size_t N = 0; Args[XREDIS_I_HELLO][N++] = (xbytesview)XRT_BYTES_LITERAL("HELLO");
		Args[XREDIS_I_HELLO][N++] = (xbytesview)XRT_BYTES_LITERAL("3");
		if (Init->Flags & XREDIS_INIT_AUTH) {
			Args[XREDIS_I_HELLO][N++] = (xbytesview)XRT_BYTES_LITERAL("AUTH");
			Args[XREDIS_I_HELLO][N++] = (Init->Flags & XREDIS_INIT_USERNAME) ? Init->Username : (xbytesview)XRT_BYTES_LITERAL("default");
			Args[XREDIS_I_HELLO][N++] = Init->Password;
		}
		if (Init->Flags & XREDIS_INIT_SETNAME) {
			Args[XREDIS_I_HELLO][N++] = (xbytesview)XRT_BYTES_LITERAL("SETNAME"); Args[XREDIS_I_HELLO][N++] = Init->Name;
		}
		Counts[XREDIS_I_HELLO] = N;
	}
	if (Init->Mode != XREDIS_INIT_RESP3) {
		if (Init->Flags & XREDIS_INIT_AUTH) {
			size_t N = 0; Args[XREDIS_I_AUTH][N++] = (xbytesview)XRT_BYTES_LITERAL("AUTH");
			if (Init->Flags & XREDIS_INIT_USERNAME) Args[XREDIS_I_AUTH][N++] = Init->Username;
			Args[XREDIS_I_AUTH][N++] = Init->Password; Counts[XREDIS_I_AUTH] = N;
		}
		if (Init->Flags & XREDIS_INIT_SETNAME) {
			Args[XREDIS_I_NAME][0] = (xbytesview)XRT_BYTES_LITERAL("CLIENT"); Args[XREDIS_I_NAME][1] = (xbytesview)XRT_BYTES_LITERAL("SETNAME");
			Args[XREDIS_I_NAME][2] = Init->Name; Counts[XREDIS_I_NAME] = 3;
		}
	}
	if (Init->Flags & XREDIS_INIT_SELECT) {
		Args[XREDIS_I_SELECT][0] = (xbytesview)XRT_BYTES_LITERAL("SELECT");
		Args[XREDIS_I_SELECT][1] = (xbytesview){(cbytes)Database, DatabaseBytes}; Counts[XREDIS_I_SELECT] = 2;
	}
	size_t Total = 0;
	for (size_t Index = 0; Index < XREDIS_I_END; ++Index) if (Counts[Index] != 0) {
		size_t Bytes = 0;
		if (!xredisCommandSize(Args[Index], Counts[Index], &Bytes)) return false;
		if (Bytes > Client->Config.Stream.WriteLimit || Bytes > Init->MaxWireBytes - Total ||
			Bytes > SIZE_MAX - sizeof(xredisinitwire) - Total)
			return xredisClientReport(XERR_RANGE, XREDIS_CLIENT_ARGUMENT, "initialization encoded wire exceeds configured limits");
		Client->InitOffset[Index] = Total; Client->InitLength[Index] = Bytes; Total += Bytes;
	}
#if defined(XREDIS_FEATURE_XREDIS_ROLE_CLIENT)
    if (Init->Flags & XREDIS_INIT_ROLES) {
        size_t Bytes = sizeof(xredisRoleWire) - 1u;
        if (Bytes > Client->Config.Stream.WriteLimit || Bytes > Init->MaxWireBytes - Total || Bytes > SIZE_MAX - sizeof(xredisinitwire) - Total)
            return xredisClientReport(XERR_RANGE, XREDIS_CLIENT_ARGUMENT, "initialization ROLE wire exceeds configured limits");
        Total += Bytes;
    }
#endif
	Client->InitNext = Init->Mode == XREDIS_INIT_RESP2 ? XREDIS_I_AUTH : XREDIS_I_HELLO;
	Client->InitAwait = XREDIS_I_END;
	if (Total == 0) return true;
	xredisinitwire* Wire = (xredisinitwire*)xrtMalloc(sizeof(*Wire) + Total);
	if (Wire == NULL) return false;
	xrtAtomic32Init(&Wire->References, 1); Wire->Client = NULL; Wire->Size = Total;
	for (size_t Index = 0; Index < XREDIS_I_END; ++Index) if (Counts[Index] != 0 &&
		!xredisCommandWrite(Wire->Data + Client->InitOffset[Index], Client->InitLength[Index], Args[Index], Counts[Index], NULL)) {
		xredisInitWireRelease(Wire); return false;
	}
#if defined(XREDIS_FEATURE_XREDIS_ROLE_CLIENT)
    if (Init->Flags & XREDIS_INIT_ROLES) memcpy(Wire->Data + Total - (sizeof(xredisRoleWire) - 1u), xredisRoleWire, sizeof(xredisRoleWire) - 1u);
#endif
	Wire->Client = Client; xredisClientRef(Client); Client->InitWire = Wire; Client->Info.InitBytes = Total;
	return true;
}

static void xredisClientRef(xredisclient* Client)
{ (void)xrtAtomic32FetchAdd(&Client->References, 1, XMEMORY_RELAXED); }

XREDIS_API bool xredisClientRetain(xredisclient* Client)
{
	if (Client == NULL) return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "client is NULL");
	uint32 Count = xrtAtomic32Load(&Client->References, XMEMORY_RELAXED);
	for (;;) {
		/* Reserve the upper half for bounded private request/control references. */
		if (Count >= UINT32_MAX / 2u) return xredisClientReport(XERR_RANGE, XREDIS_CLIENT_STATE, "client reference count limit");
		if (xrtAtomic32CompareExchange(&Client->References, &Count, Count + 1u, XMEMORY_RELAXED, XMEMORY_RELAXED)) return true;
	}
}

XREDIS_API void xredisClientRelease(xredisclient* Client)
{
	if (Client == NULL || xrtAtomic32FetchSub(&Client->References, 1, XMEMORY_ACQ_REL) != 1) return;
	for (size_t Index = 0; Index < Client->Config.MaxPushes; ++Index) xredisReplyRelease(Client->PushQueue[Index]);
	xredisReplyReaderDestroy(Client->Reader); xrtNetStreamDestroy(Client->Stream);
	xrtPromiseDestroy(Client->ClosePromise); xrtFutureDestroy(Client->CloseFuture); xrtErrorFree(Client->CloseCause);
	xrtPromiseDestroy(Client->ReadyPromise); xrtFutureDestroy(Client->ReadyFuture);
	xrtFree(Client->ReadyResult); xredisReplyRelease(Client->Hello);
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
	if (Client->PlanOwned) Client->Plan->Drop(Client->Plan->Data);
#endif
	if (Client->EnginePinned) xrtNetEngineUnpin(Client->Engine);
	#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    xrtFree(xrtAtomicPtrLoad(&Client->Observation,XMEMORY_RELAXED));
#endif
    #if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    xredisSubscriberRelease(Client);
#endif
	xrtMutexUnit(&Client->Gate); xrtFree(Client);
}

/* Gate is held. One reference protects each accepted embedded-post callback. */
static bool xredisClientWakeLocked(xredisclient* Client)
{
	if (Client->CloseDone || Client->WakeQueued) return true;
	Client->WakeQueued = true; xredisClientRef(Client);
	if (xrtNetPost(Client->Worker, &Client->Wake, xredisClientWork, Client)) return true;
	Client->WakeQueued = false; xredisClientRelease(Client); return false;
}

static void xredisClientWake(xredisclient* Client)
{
	xrtMutexLock(&Client->Gate); (void)xredisClientWakeLocked(Client); xrtMutexUnlock(&Client->Gate);
}

static bool xredisClientClosing(xredisclient* Client)
{
	xrtMutexLock(&Client->Gate); bool Closing = Client->Info.State >= XREDIS_CLIENT_CLOSING;
	xrtMutexUnlock(&Client->Gate); return Closing;
}

/* A connection retired by ANOTHER request's drain grace must not report that
 * this request's own deadline expired. CloseStatus keeps the connection cause. */
static xredisrequeststatus xredisClientFailureStatus(xredisclient* Client)
{ return Client->Info.CloseStatus == XREDIS_REQUEST_TIMEOUT ? XREDIS_REQUEST_DISCONNECTED : Client->Info.CloseStatus; }

static void xredisRequestRef(xredisrequest* Request)
{ (void)xrtAtomic32FetchAdd(&Request->References, 1, XMEMORY_RELAXED); }

#if defined(XREDIS_FEATURE_XREDIS_POOL)
static bool xredisClientQuotaReserve(xredisexclusiveowner* Owner, size_t Bytes)
{
    xredisownerquota* Quota = Owner == NULL ? NULL : Owner->Quota;
    if (Quota == NULL) return true;
    uint64 Requests = xrtAtomic64Load(&Quota->Requests, XMEMORY_RELAXED);
    for (;;) {
        if (Requests >= Quota->MaxRequests) return false;
        if (xrtAtomic64CompareExchange(&Quota->Requests, &Requests, Requests + 1u, XMEMORY_RELAXED, XMEMORY_RELAXED)) break;
    }
    uint64 Current = xrtAtomic64Load(&Quota->Bytes, XMEMORY_RELAXED);
    for (;;) {
        if ((uint64)Bytes > Quota->MaxBytes - Current) {
            xrtAtomic64FetchSub(&Quota->Requests, 1, XMEMORY_RELAXED); return false;
        }
        if (xrtAtomic64CompareExchange(&Quota->Bytes, &Current, Current + (uint64)Bytes, XMEMORY_RELAXED, XMEMORY_RELAXED)) return true;
    }
}
#endif

/* Drop the real reservation only after its storage/constructor has retired.
 * An idle transition wakes the exclusive session without calling it under Gate. */
static void xredisClientReservationDrop(xredisclient* Client, size_t Bytes)
{
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
    xredisexclusiveowner* Owner = NULL;
#endif
    xrtMutexLock(&Client->Gate);
    --Client->Info.Requests; Client->Info.CommandBytes -= Bytes;
#if defined(XREDIS_FEATURE_XREDIS_POOL)
    if (Client->Owner != NULL && Client->Owner->Quota != NULL) {
        xrtAtomic64FetchSub(&Client->Owner->Quota->Requests, 1, XMEMORY_RELAXED);
        xrtAtomic64FetchSub(&Client->Owner->Quota->Bytes, (uint64)Bytes, XMEMORY_RELAXED);
    }
#endif
    if (Client->Info.State >= XREDIS_CLIENT_CLOSING) (void)xredisClientWakeLocked(Client);
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
    if (Client->Info.Requests == 0 && Client->Owner != NULL) {
        Owner = Client->Owner; Owner->Retain(Owner->Data);
    }
#endif
    xrtMutexUnlock(&Client->Gate);
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
    if (Owner != NULL) { Owner->Wake(Owner->Data); Owner->Release(Owner->Data); }
#endif
    xredisClientRelease(Client);
}

static void xredisRequestRelease(xredisrequest* Request)
{
	if (xrtAtomic32FetchSub(&Request->References, 1, XMEMORY_ACQ_REL) != 1) return;
	xredisclient* Client = Request->Client;
	size_t WireBytes = Request->WireBytes;
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    if(Request->Observe>=5){xredissubcall* S=xredisSubscriberState(Request);WireBytes+=S->MetadataBytes;xredisReplyRelease(S->Final);}
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    if(xredisCachingPair(Request)){
        xrediscachingstate* S=xredisCachingState(Request);WireBytes+=S->MetadataBytes;
        xredisReplyRelease(S->Control);xredisReplyRelease(S->Query);
    }
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
	if (Request->Transaction) {
		WireBytes += xredisTransactionState(Request)->MetadataBytes;
		xredisReplyRelease(xredisTransactionState(Request)->Error);
	}
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
    if(xredisAskingPair(Request)){xredisaskingstate* S=xredisAskingState(Request);WireBytes+=S->MetadataBytes;xredisReplyRelease(S->Control);xredisReplyRelease(S->Query);}
#endif
	/* The reservation protects actual storage, including the free tail. Keep
	 * the independent Client reference while dropping it AFTER the buffer. */
	#if defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
    if(Request->Sensitive&1u)xrtSecureZero(Request->Wire,Request->WireBytes);
#else
    if(Request->Sensitive)xrtSecureZero(Request->Wire,Request->WireBytes);
#endif
	xrtFree(Request);
	xredisClientReservationDrop(Client, WireBytes);
}

static void xredisClientWireDrop(ptr Data, cbytes Bytes, size_t Size)
{ (void)Bytes; (void)Size; xredisRequestRelease((xredisrequest*)Data); }

static void xredisClientResultDrop(ptr Value, ptr Data)
{
	(void)Data; xredisrequestresult* Result = (xredisrequestresult*)Value;
	xredisReplyRelease(Result->Reply); xrtErrorFree((xerror*)Result->Cause); xrtFree(Result);
}

#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
static void xredisCachingResultDrop(ptr Value,ptr Data)
{
    xrediscachingresult* R=Value;
    xredisReplyRelease(R->Control.Request.Reply);xrtErrorFree((xerror*)R->Control.Request.Cause);
    xredisClientResultDrop(&R->Query.Request,Data);
}
#endif

#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
static void xredisTransactionResultDrop(ptr Value, ptr Data)
{
	xredistransactionresult* Result = (xredistransactionresult*)Value;
	xredisReplyRelease(Result->QueueError); xredisClientResultDrop(&Result->Request, Data);
}
#endif

static void xredisClientCancel(ptr Data)
{
	xredisrequest* Request = (xredisrequest*)Data; xredisclient* Client = Request->Client;
	xrtAtomic32Store(&Request->Cancelled, 1, XMEMORY_RELEASE);
	xrtMutexLock(&Client->Gate);
	if (!Request->CancelQueued && !Client->CloseDone) {
		Request->CancelQueued = true; Request->CancelNext = NULL; xredisRequestRef(Request);
		if (Client->CancelTail != NULL) Client->CancelTail->CancelNext = Request; else Client->CancelHead = Request;
		Client->CancelTail = Request; (void)xredisClientWakeLocked(Client);
	}
	xrtMutexUnlock(&Client->Gate);
}

#if defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
static void xredisAskingResultDrop(ptr Value,ptr Data)
{
    xredisaskingresult* R=Value;xredisReplyRelease(R->Control.Reply);xrtErrorFree((xerror*)R->Control.Cause);
    xredisClientResultDrop(&R->Query,Data);
}
#endif
static void xredisClientComplete(xredisrequest* Request, xredisrequeststatus Status,
	xredisdelivery Delivery, xredisreply* Reply, const xerror* Cause)
{
	if (Request->Promise == NULL) { xredisReplyRelease(Reply); return; }
	/* No Gate is held. Our cancellation callback only queues/wakes and never
	 * waits for this worker, so physical Unwatch retirement cannot self-deadlock. */
	xrtCancelUnwatch(Request->Watch); Request->Watch = NULL;
	xpromise* Promise = Request->Promise; Request->Promise = NULL;
	xredisrequestresult* Result = Request->Result; Request->Result = NULL;
	xfuturefreeproc Drop = xredisClientResultDrop;
#if defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
    if(xredisAskingPair(Request)){
        xredisaskingstate* S=xredisAskingState(Request);xredisaskingresult* R=(xredisaskingresult*)Result;
        R->ControlReceived=S->Seen!=0;R->Control=(xredisrequestresult){S->Seen?S->ControlStatus:Status,
            S->Seen?XREDIS_REPLIED:Delivery,S->Control,S->Seen?NULL:xrtErrorRef(Cause)};
        S->Control=NULL;Drop=xredisAskingResultDrop;if(S->Seen==2)Delivery=XREDIS_REPLIED;
        if(S->Query && !Reply){xredisReplyRelease(S->Query);S->Query=NULL;xrtMutexLock(&Request->Client->Gate);++Request->Client->Info.DiscardedReplies;xrtMutexUnlock(&Request->Client->Gate);}
    }
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    if(xredisCachingPair(Request)){
        xrediscachingstate* S=xredisCachingState(Request);xrediscachingresult* R=(xrediscachingresult*)Result;
        R->ControlReceived=S->Seen!=0;
        R->Control=(xredistrackingresult){
            {S->Seen?S->ControlStatus:Status,S->Seen?XREDIS_REPLIED:Delivery,S->Control,
             S->Seen?NULL:xrtErrorRef(Cause)},S->Seen?S->ControlSequence:0};
        S->Control=NULL;R->Query.Sequence=Reply?S->QuerySequence:0;Drop=xredisCachingResultDrop;
        if(S->Seen==2)Delivery=XREDIS_REPLIED;
        if(S->Query && !Reply){
            xredisReplyRelease(S->Query);S->Query=NULL;
            xrtMutexLock(&Request->Client->Gate);++Request->Client->Info.DiscardedReplies;xrtMutexUnlock(&Request->Client->Gate);
        }
    }
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
	if (Request->Transaction) {
		xredistransactionstate* State = xredisTransactionState(Request);
		xredistransactionresult* Transaction = (xredistransactionresult*)Result;
		Transaction->Execution = State->Execution; Transaction->Commands = State->Count;
		Transaction->Queued = State->Seen - State->Errors; Transaction->QueueErrors = State->Errors;
		Transaction->QueueError = State->Error; State->Error = NULL; Drop = xredisTransactionResultDrop;
		if (Delivery != XREDIS_REPLIED) Delivery = State->Execution == XREDIS_TRANSACTION_NOT_EXECUTED ? XREDIS_NOT_SENT : XREDIS_MAY_HAVE_EXECUTED;
	}
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    if(Request->Observe>=5){
        xredissubcall* S=xredisSubscriberState(Request);xredissubscriber* M=xrtAtomicPtrLoad(&Request->Client->Subscriber,XMEMORY_ACQUIRE);
        xredissubscriberresult* R=(xredissubscriberresult*)Result;
        R->Acknowledged=S->Acknowledged;R->Subscriptions=M->Counts[0]+M->Counts[1];R->ShardSubscriptions=M->Counts[2];
        if(S->Finished)Delivery=XREDIS_REPLIED;
    }
#endif
	*Result = (xredisrequestresult){Status, Delivery, Reply, xrtErrorRef(Cause)};
	xrtMutexLock(&Request->Client->Gate); ++Request->Client->Info.Completed; xrtMutexUnlock(&Request->Client->Gate);
	if (!xrtPromiseResolveOwned(Promise, Result, Drop, NULL)) Drop(Result, NULL);
	xrtPromiseDestroy(Promise);
}

static void xredisClientCancelTimer(xredisrequest* Request)
{
	uint64 Timer = Request->Timer;
	if (Timer != 0) (void)xrtNetEngineTimerCancelCurrent(Request->Client->Engine, Timer);
}

static bool xredisClientArm(xredisrequest* Request, double Deadline)
{
	if (Deadline == INFINITY) return true;
	double Milliseconds = ceil((Deadline - xrtTimer()) * 1000.0);
	int64 Timeout = Milliseconds <= 0 ? 0 : Milliseconds >= 0x1p63 ? INT64_MAX : (int64)Milliseconds;
	xredisRequestRef(Request);
	Request->Timer = xrtNetEngineAfter(Request->Client->Engine, xrtNetWorkerIndex(Request->Client->Worker),
		Timeout, xredisClientTimer, Request);
	if (Request->Timer != 0) return true;
	xredisRequestRelease(Request); return false;
}

static void xredisClientUnlink(xredisrequest* Request)
{
	xredisclient* Client = Request->Client;
	xredisrequest** Head = Request->Phase == XREDIS_R_SENT ? &Client->SentHead : &Client->PendingHead;
	xredisrequest** Tail = Request->Phase == XREDIS_R_SENT ? &Client->SentTail : &Client->PendingTail;
	if (Request->Previous != NULL) Request->Previous->Next = Request->Next; else *Head = Request->Next;
	if (Request->Next != NULL) Request->Next->Previous = Request->Previous; else *Tail = Request->Previous;
	if (Request->Phase == XREDIS_R_SENT) --Client->SentCount;
	Request->Previous = NULL; Request->Next = NULL;
}

static void xredisClientAppend(xredisclient* Client, xredisrequest* Request, bool Sent)
{
	xredisrequest** Head = Sent ? &Client->SentHead : &Client->PendingHead;
	xredisrequest** Tail = Sent ? &Client->SentTail : &Client->PendingTail;
	Request->Previous = *Tail; Request->Next = NULL;
	if (*Tail != NULL) (*Tail)->Next = Request; else *Head = Request;
	*Tail = Request; Request->Phase = Sent ? XREDIS_R_SENT : XREDIS_R_PENDING;
	if (Sent) ++Client->SentCount;
}

static void xredisClientFail(xredisclient* Client, xredisrequeststatus Status, const xerror* Cause)
{
	xrtMutexLock(&Client->Gate);
	if (Client->Info.State < XREDIS_CLIENT_CLOSING) {
		Client->Info.State = XREDIS_CLIENT_CLOSING; Client->Info.CloseStatus = Status;
		Client->CloseCause = xrtErrorRef(Cause);
	}
	xrtMutexUnlock(&Client->Gate);
	xredisClientTransportAbort(Client);
	xredisClientWake(Client);
}

static xredisreadystatus xredisClientReadyFailure(xredisrequeststatus Status)
{
	switch (Status) {
		case XREDIS_REQUEST_CLOSED: return XREDIS_READY_CLOSED;
		case XREDIS_REQUEST_TIMEOUT: return XREDIS_READY_TIMEOUT;
		case XREDIS_REQUEST_PROTOCOL: return XREDIS_READY_PROTOCOL;
		case XREDIS_REQUEST_LIMIT: return XREDIS_READY_LIMIT;
		case XREDIS_REQUEST_MEMORY: return XREDIS_READY_MEMORY;
		case XREDIS_REQUEST_INITIALIZATION: return XREDIS_READY_SERVER_ERROR;
		default: return XREDIS_READY_DISCONNECTED;
	}
}

static void xredisClientReadyFinish(xredisclient* Client, xredisreadystatus Status, const xerror* Cause)
{
	if (Client->ReadyPromise == NULL) return;
	xpromise* Promise = Client->ReadyPromise; Client->ReadyPromise = NULL;
	xredisreadyresult* Result = Client->ReadyResult; Client->ReadyResult = NULL;
	xrtMutexLock(&Client->Gate);
	if (Status == XREDIS_READY_OK && Client->Info.State >= XREDIS_CLIENT_CLOSING)
		Status = xredisClientReadyFailure(Client->Info.CloseStatus);
	if (Status == XREDIS_READY_OK) {
		Client->Info.Ready = true; Client->Info.State = XREDIS_CLIENT_OPEN; Client->Info.InitStep = XREDIS_INIT_DONE;
	}
	*Result = (xredisreadyresult){Status, Client->Info.InitStep, Client->Info.Protocol,
		Client->Info.Negotiated, Client->Info.Downgraded, NULL, xrtErrorRef(Cause)};
	xrtMutexUnlock(&Client->Gate);
	if (Status == XREDIS_READY_OK) Result->Hello = Client->Hello;
	else xredisReplyRelease(Client->Hello);
	Client->Hello = NULL;
	if (Client->InitTimer != 0) (void)xrtNetEngineTimerCancelCurrent(Client->Engine, Client->InitTimer);
	xredisinitwire* Wire = Client->InitWire; Client->InitWire = NULL; xredisInitWireRelease(Wire);
	if (!xrtPromiseResolveOwned(Promise, Result, xredisClientReadyDrop, NULL)) xredisClientReadyDrop(Result, NULL);
	xrtPromiseDestroy(Promise);
}

static void xredisClientInitReject(xredisclient* Client, xredisreadystatus Status)
{
	xerrkind Kind = Status == XREDIS_READY_AUTH_FAILED ? XERR_PERMISSION :
		Status == XREDIS_READY_UNSUPPORTED ? XERR_UNSUPPORTED : Status == XREDIS_READY_TIMEOUT ? XERR_TIMEOUT :
		Status == XREDIS_READY_PROTOCOL ? XERR_PROTOCOL : XERR_VALUE;
	/* Deliberately resident/redacted diagnostics: an unknown-command response
	 * can echo HELLO AUTH arguments. Never wrap/copy that server text. */
	xerror* Cause = xrtErrorCreate(Kind, "xrt.redis", XREDIS_CLIENT_INITIALIZATION_FAILED, "connection initialization failed");
	xredisClientFail(Client, XREDIS_REQUEST_INITIALIZATION, Cause);
	xredisClientReadyFinish(Client, Status, Cause); xrtErrorFree(Cause);
}

static void xredisClientInitTimer(xnetworker* Worker, uint64 Id, xnetresult Result, ptr Data)
{
	(void)Worker; xredisclient* Client = (xredisclient*)Data;
	if (Client->InitTimer == Id) Client->InitTimer = 0;
	if (Client->ReadyPromise != NULL) {
		if (Result == XNET_RESULT_OK) xredisClientInitReject(Client, XREDIS_READY_TIMEOUT);
		else if (Result != XNET_RESULT_CANCELLED) xredisClientFail(Client, XREDIS_REQUEST_CLOSED, NULL);
	}
	xredisClientWake(Client); xredisClientRelease(Client);
}

static bool xredisClientInitArm(xredisclient* Client)
{
	if (!Client->Managed || Client->InitDeadline == INFINITY) return true;
	double Remaining = ceil((Client->InitDeadline - xrtTimer()) * 1000.0);
	int64 Timeout = Remaining <= 0 ? 0 : Remaining >= 0x1p63 ? INT64_MAX : (int64)Remaining;
	xredisClientRef(Client);
	Client->InitTimer = xrtNetEngineAfter(Client->Engine, xrtNetWorkerIndex(Client->Worker), Timeout, xredisClientInitTimer, Client);
	if (Client->InitTimer != 0) return true;
	xredisClientRelease(Client); return false;
}

static bool xredisClientInitSlice(xredisclient* Client, size_t* Index, size_t* Offset, size_t* Length)
{
    while (Client->InitNext < XREDIS_I_END && Client->InitLength[Client->InitNext] == 0) ++Client->InitNext;
    xredisinitstep Step;
#if defined(XREDIS_FEATURE_XREDIS_ROLE_CLIENT)
    if (Client->InitNext == XREDIS_I_END && (Client->InitFlags & XREDIS_INIT_ROLES)) Client->InitNext = XREDIS_I_ROLE;
    if (Client->InitNext == XREDIS_I_ROLE) {
        *Index = XREDIS_I_ROLE; *Length = sizeof(xredisRoleWire) - 1u; *Offset = Client->InitWire->Size - *Length; Step = XREDIS_INIT_ROLE;
    } else
#endif
    {
        if (Client->InitNext >= XREDIS_I_END) { xredisClientReadyFinish(Client, XREDIS_READY_OK, NULL); return false; }
        const xredisinitstep Steps[] = {XREDIS_INIT_HELLO, XREDIS_INIT_AUTHENTICATE, XREDIS_INIT_NAME, XREDIS_INIT_DATABASE};
        *Index = Client->InitNext; *Offset = Client->InitOffset[*Index]; *Length = Client->InitLength[*Index]; Step = Steps[*Index];
    }
    xrtMutexLock(&Client->Gate); Client->Info.InitStep = Step; xrtMutexUnlock(&Client->Gate); return true;
}

#if defined(XREDIS_FEATURE_XREDIS_TLS)
/* TLS copies accepted plaintext into authenticated record storage. Nothing
 * borrows input after Send returns. A short prefix must never be replayed. */
static xtlsresult xredisClientTlsSend(xredisclient* Client, cbytes Data, size_t Size, size_t* Written)
{
	*Written = 0;
	if (Client->TlsTurnBytes == 0 || Client->TlsTurnCalls == 0) { xredisClientWake(Client); return XTLS_AGAIN; }
	if (Size > Client->TlsTurnBytes) Size = Client->TlsTurnBytes;
	--Client->TlsTurnCalls; xtlsresult Result = xrtTlsStreamSend(Client->TLS, Data, Size, Written);
	Client->TlsTurnBytes -= *Written; return Result;
}

static void xredisClientTlsInitPump(xredisclient* Client)
{
	if (Client->InitAwait != XREDIS_I_END && !Client->InitSending) return;
    size_t Index, Offset, Length; if (!xredisClientInitSlice(Client, &Index, &Offset, &Length)) return;
	Client->InitAwait = Index; Client->InitSending = true;
	while (Client->InitWritten < Length && !xredisClientClosing(Client)) {
		xredisinitwire* Wire = Client->InitWire; (void)xrtAtomic32FetchAdd(&Wire->References, 1, XMEMORY_RELAXED);
		size_t Written = 0; xtlsresult Result = xredisClientTlsSend(Client, Wire->Data + Offset + Client->InitWritten,
			Length - Client->InitWritten, &Written);
		Client->InitWritten += Written; xredisInitWireRelease(Wire);
		if (Result == XTLS_ERROR || Result == XTLS_CLOSED) {
			xredisClientFail(Client, Result == XTLS_ERROR && xrtErrorKind(xrtGetError()) == XERR_MEMORY ? XREDIS_REQUEST_MEMORY : XREDIS_REQUEST_DISCONNECTED,
				Result == XTLS_ERROR ? xrtGetError() : NULL); return;
		}
		if (Written == 0) return;
	}
	if (Client->InitWritten == Length) { Client->InitWritten = 0; Client->InitSending = false; }
}
#endif

static void xredisClientInitPump(xredisclient* Client)
{
	if (Client->ReadyPromise == NULL || !Client->TransportOpen) return;
	if (xrtTimer() >= Client->InitDeadline) { xredisClientInitReject(Client, XREDIS_READY_TIMEOUT); return; }
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Client->TLS != NULL) { xredisClientTlsInitPump(Client); return; }
#endif
	if (Client->InitAwait != XREDIS_I_END) return;
    size_t Index, Offset, Length; if (!xredisClientInitSlice(Client, &Index, &Offset, &Length)) return;
	xredisinitwire* Wire = Client->InitWire;
	Client->InitAwait = Index; (void)xrtAtomic32FetchAdd(&Wire->References, 1, XMEMORY_RELAXED);
	xnetresult Result = xrtNetStreamSendRef(Client->Stream, Wire->Data + Offset, Length, xredisInitWireDrop, Wire);
	if (Result == XNET_RESULT_OK) return;
	Client->InitAwait = XREDIS_I_END; xredisInitWireRelease(Wire);
	if (Result == XNET_RESULT_AGAIN) return;
	xredisClientFail(Client, Result == XNET_RESULT_ERROR && xrtErrorKind(xrtGetError()) == XERR_MEMORY ?
		XREDIS_REQUEST_MEMORY : XREDIS_REQUEST_DISCONNECTED, Result == XNET_RESULT_ERROR ? xrtGetError() : NULL);
}

static bool xredisClientInitLiteral(xbytesview Bytes, cstr Text)
{ size_t Length = strlen(Text); return Bytes.Size == Length && (Length == 0 || memcmp(Bytes.Data, Text, Length) == 0); }

static bool xredisClientInitCode(xbytesview Bytes, cstr Code)
{
	size_t Length = strlen(Code);
	return Bytes.Size >= Length && memcmp(Bytes.Data, Code, Length) == 0 && (Bytes.Size == Length || Bytes.Data[Length] == ' ');
}

static bool xredisClientInitUnknown(xbytesview Bytes, cstr Prefix)
{
	size_t Length = strlen(Prefix);
	if (Bytes.Size < Length || (Bytes.Size > Length && Bytes.Data[Length] != ',' && Bytes.Data[Length] != ' ')) return false;
	for (size_t Index = 0; Index < Length; ++Index) {
		unsigned char A = Bytes.Data[Index], B = (unsigned char)Prefix[Index];
		if (A >= 'a' && A <= 'z') A -= 'a' - 'A';
		if (B >= 'a' && B <= 'z') B -= 'a' - 'A';
		if (A != B) return false;
	}
	return true;
}

static bool xredisClientInitUnsupported(xbytesview Bytes)
{ return xredisClientInitCode(Bytes, "NOPROTO") || xredisClientInitUnknown(Bytes, "ERR unknown command 'HELLO'"); }

static bool xredisClientInitHello(xredisreply* Reply, const xredisreplyvalue* Root)
{
	if (Root->Type != XREDIS_REPLY_MAP || (Root->Flags & XREDIS_REPLY_NULL_VALUE)) return false;
	uint32 Seen = 0;
	for (size_t Index = Root->FirstChild; Index != XREDIS_REPLY_NONE;) {
		xredisreplyvalue Key, Value;
		if (!xredisReplyGet(Reply, Index, &Key) || Key.Next == XREDIS_REPLY_NONE || !xredisReplyGet(Reply, Key.Next, &Value)) return false;
		if ((Key.Type == XREDIS_REPLY_SIMPLE_STRING || Key.Type == XREDIS_REPLY_BULK_STRING) && !(Key.Flags & XREDIS_REPLY_NULL_VALUE)) {
			uint32 Bit = xredisClientInitLiteral(Key.Data, "proto") ? 1u : xredisClientInitLiteral(Key.Data, "server") ? 2u :
				xredisClientInitLiteral(Key.Data, "version") ? 4u : 0;
			if (Bit != 0) {
				if ((Seen & Bit) || (Bit == 1u ? Value.Type != XREDIS_REPLY_INTEGER || Value.Integer != 3 :
					(Value.Type != XREDIS_REPLY_SIMPLE_STRING && Value.Type != XREDIS_REPLY_BULK_STRING) || (Value.Flags & XREDIS_REPLY_NULL_VALUE))) return false;
				Seen |= Bit;
			}
		}
		Index = Value.Next;
	}
	return Seen == 7u;
}

static void xredisClientInitFrame(xredisclient* Client, xredisreply* Reply, const xredisreplyvalue* Root)
{
	if (xrtTimer() >= Client->InitDeadline) { xredisReplyRelease(Reply); xredisClientInitReject(Client, XREDIS_READY_TIMEOUT); return; }
	size_t Index = Client->InitAwait;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Client->InitSending) { xredisReplyRelease(Reply); xredisClientInitReject(Client, XREDIS_READY_PROTOCOL); return; }
#endif
	if (Index >= XREDIS_I_END
#if defined(XREDIS_FEATURE_XREDIS_ROLE_CLIENT)
        && Index != XREDIS_I_ROLE
#endif
    ) { xredisReplyRelease(Reply); xredisClientInitReject(Client, XREDIS_READY_PROTOCOL); return; }
	if (Root->Type == XREDIS_REPLY_SIMPLE_ERROR || Root->Type == XREDIS_REPLY_BULK_ERROR) {
		bool Unsupported = Index == XREDIS_I_HELLO && xredisClientInitUnsupported(Root->Data);
#if defined(XREDIS_FEATURE_XREDIS_ROLE_CLIENT)
        Unsupported = Unsupported || (Index == XREDIS_I_ROLE && xredisClientInitUnknown(Root->Data, "ERR unknown command 'ROLE'"));
#endif
		bool Authentication = xredisClientInitCode(Root->Data, "WRONGPASS") ||
			xredisClientInitCode(Root->Data, "NOAUTH") ||
			((Index == XREDIS_I_HELLO || Index == XREDIS_I_AUTH) && (Client->InitFlags & XREDIS_INIT_AUTH) &&
			 xredisClientInitLiteral(Root->Data, "ERR invalid password"));
		xredisReplyRelease(Reply);
		if (Unsupported && Index == XREDIS_I_HELLO && Client->InitMode == XREDIS_INIT_AUTO) {
			xrtMutexLock(&Client->Gate); Client->Info.Downgraded = true; Client->Info.Protocol = XREDIS_RESP2; xrtMutexUnlock(&Client->Gate);
			Client->InitNext = XREDIS_I_AUTH; Client->InitAwait = XREDIS_I_END; xredisClientWake(Client); return;
		}
		xredisClientInitReject(Client, Unsupported ? XREDIS_READY_UNSUPPORTED : Authentication ? XREDIS_READY_AUTH_FAILED : XREDIS_READY_SERVER_ERROR);
		return;
	}
#if defined(XREDIS_FEATURE_XREDIS_ROLE_CLIENT)
    if (Index == XREDIS_I_ROLE) {
        const xredisreaderconfig* Reader = &Client->Config.Replies.Reader;
        xredisrolelimits Limits = {sizeof(Limits), Reader->MaxValues > SIZE_MAX ? SIZE_MAX : (size_t)Reader->MaxValues,
            Reader->MaxFrameBytes > SIZE_MAX ? SIZE_MAX : (size_t)Reader->MaxFrameBytes};
        xredisroleinfo Role; xredisrolestatus Status = xredisRoleRead(Reply, &Limits, &Role); xredisReplyRelease(Reply);
        if (xrtTimer() >= Client->InitDeadline) { xredisClientInitReject(Client, XREDIS_READY_TIMEOUT); return; }
        if (Status != XREDIS_ROLE_VALUE) { xredisClientInitReject(Client, Status == XREDIS_ROLE_LIMIT ? XREDIS_READY_LIMIT : XREDIS_READY_PROTOCOL); return; }
        xredisrolekind Expected = (Client->InitFlags & XREDIS_INIT_REQUIRE_MASTER) ? XREDIS_ROLE_MASTER :
            (Client->InitFlags & XREDIS_INIT_REQUIRE_REPLICA) ? XREDIS_ROLE_REPLICA : XREDIS_ROLE_SENTINEL;
        if (Role.Kind != Expected) { xredisClientInitReject(Client, XREDIS_READY_ROLE_MISMATCH); return; }
        Client->InitNext = XREDIS_I_ROLE_DONE; Client->InitAwait = XREDIS_I_END; xredisClientWake(Client); return;
    }
#endif
	bool Valid = Index == XREDIS_I_HELLO ? xredisClientInitHello(Reply, Root) :
		Root->Type == XREDIS_REPLY_SIMPLE_STRING && xredisClientInitLiteral(Root->Data, "OK");
	if (!Valid) { xredisReplyRelease(Reply); xredisClientInitReject(Client, XREDIS_READY_PROTOCOL); return; }
	if (Index == XREDIS_I_HELLO) {
		Client->Hello = Reply;
		xrtMutexLock(&Client->Gate); Client->Info.Protocol = XREDIS_RESP3; Client->Info.Negotiated = true; xrtMutexUnlock(&Client->Gate);
		Client->InitNext = XREDIS_I_SELECT;
	} else { xredisReplyRelease(Reply); Client->InitNext = Index + 1u; }
	Client->InitAwait = XREDIS_I_END; xredisClientWake(Client);
}

static void xredisClientAbandon(xredisrequest* Request, xredisrequeststatus Status)
{
	xredisclient* Client = Request->Client;
	if (Request->Promise == NULL) return;
	xredisClientComplete(Request, Status, Request->Phase == XREDIS_R_SENT ? XREDIS_MAY_HAVE_EXECUTED : XREDIS_NOT_SENT, NULL, NULL);
	xredisClientCancelTimer(Request);
	if (Request->Phase == XREDIS_R_SENT) {
		if (!xredisClientArm(Request, xredisClientDeadline(xrtTimer(), Client->Config.DrainTimeout)))
			xredisClientFail(Client, XREDIS_REQUEST_MEMORY, xrtGetError());
	} else if (Request->Phase == XREDIS_R_PENDING) {
		xredisClientUnlink(Request); Request->Phase = XREDIS_R_DONE; XREDIS_SUBSCRIBER_RETIRE(Request); xredisRequestRelease(Request);
	}
}

#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
static void xredisCachingFinish(xredisrequest* R)
{
    xrediscachingstate* S=xredisCachingState(R);xredisclient* C=R->Client;
    if(!S->Written || S->Seen!=2)return;
    if(R->Promise){
        xredisrequeststatus Status=S->QueryStatus;
        if(xrtAtomic32Load(&R->Cancelled,XMEMORY_ACQUIRE))Status=XREDIS_REQUEST_CANCELLED;
        else if(xrtTimer()>=R->Deadline)Status=XREDIS_REQUEST_TIMEOUT;
        xredisreply* Reply=S->Query;S->Query=NULL;
        if(Status==XREDIS_REQUEST_CANCELLED || Status==XREDIS_REQUEST_TIMEOUT){
            xredisReplyRelease(Reply);Reply=NULL;
            xrtMutexLock(&C->Gate);++C->Info.DiscardedReplies;xrtMutexUnlock(&C->Gate);
        }
        xredisClientComplete(R,Status,XREDIS_REPLIED,Reply,NULL);
    }
    xredisClientUnlink(R);R->Phase=XREDIS_R_DONE;
    xredisClientCancelTimer(R);xredisRequestRelease(R);xredisClientWake(C);
}
static void xredisCachingWritten(xredisrequest* R)
{
    if(!xredisCachingPair(R))return;
    xredisCachingState(R)->Written=true;xredisCachingFinish(R);
}
static void xredisCachingFrame(xredisrequest* R,xredisreply* Reply,const xredisreplyvalue* Root,uint64 Sequence)
{
    xrediscachingstate* S=xredisCachingState(R);xredisclient* C=R->Client;
    if(S->Seen>=2){xredisReplyRelease(Reply);xredisClientFail(C,XREDIS_REQUEST_PROTOCOL,NULL);return;}
    bool Error=Root->Type==XREDIS_REPLY_SIMPLE_ERROR || Root->Type==XREDIS_REPLY_BULK_ERROR;
    if(S->Seen++==0){
        S->ControlStatus=Error?XREDIS_REQUEST_SERVER_ERROR:XREDIS_REQUEST_OK;
        if(!Error && !(Root->Type==XREDIS_REPLY_SIMPLE_STRING && xredisClientInitLiteral(Root->Data,"OK"))){
            S->ControlStatus=XREDIS_REQUEST_PROTOCOL;xredisReplyRelease(Reply);
            xredisClientFail(C,XREDIS_REQUEST_PROTOCOL,NULL);return;
        }
        if(R->Promise){S->Control=Reply;S->ControlSequence=Sequence;}
        else {xredisReplyRelease(Reply);xrtMutexLock(&C->Gate);++C->Info.DiscardedReplies;xrtMutexUnlock(&C->Gate);}
    }else{
        S->QueryStatus=Error?XREDIS_REQUEST_SERVER_ERROR:XREDIS_REQUEST_OK;
        if(R->Promise){S->Query=Reply;S->QuerySequence=Sequence;}
        else {xredisReplyRelease(Reply);xrtMutexLock(&C->Gate);++C->Info.DiscardedReplies;xrtMutexUnlock(&C->Gate);}
        xredisCachingFinish(R);
    }
}
#endif

#if defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
static void xredisAskingFinish(xredisrequest* R)
{
    xredisaskingstate* S=xredisAskingState(R);xredisclient* C=R->Client;
    if(!S->Written || S->Seen!=2)return;
    if(R->Promise){xredisrequeststatus Status=S->QueryStatus;
        if(xrtAtomic32Load(&R->Cancelled,XMEMORY_ACQUIRE))Status=XREDIS_REQUEST_CANCELLED;
        else if(xrtTimer()>=R->Deadline)Status=XREDIS_REQUEST_TIMEOUT;
        xredisreply* Reply=S->Query;S->Query=NULL;
        if(Status==XREDIS_REQUEST_CANCELLED || Status==XREDIS_REQUEST_TIMEOUT){xredisReplyRelease(Reply);Reply=NULL;xrtMutexLock(&C->Gate);++C->Info.DiscardedReplies;xrtMutexUnlock(&C->Gate);}
        xredisClientComplete(R,Status,XREDIS_REPLIED,Reply,NULL);
    }
    xredisClientUnlink(R);R->Phase=XREDIS_R_DONE;xredisClientCancelTimer(R);xredisRequestRelease(R);xredisClientWake(C);
}
static void xredisAskingWritten(xredisrequest* R)
{xredisAskingState(R)->Written=true;xredisAskingFinish(R);}
static void xredisAskingFrame(xredisrequest* R,xredisreply* Reply,const xredisreplyvalue* Root)
{
    xredisaskingstate* S=xredisAskingState(R);xredisclient* C=R->Client;
    if(S->Seen>=2){xredisReplyRelease(Reply);xredisClientFail(C,XREDIS_REQUEST_PROTOCOL,NULL);return;}
    bool Error=Root->Type==XREDIS_REPLY_SIMPLE_ERROR || Root->Type==XREDIS_REPLY_BULK_ERROR;
    bool Control=S->Seen++==0;
    if(Control){
        S->ControlStatus=Error?XREDIS_REQUEST_SERVER_ERROR:XREDIS_REQUEST_OK;
        if(!Error && !(Root->Type==XREDIS_REPLY_SIMPLE_STRING && xredisClientInitLiteral(Root->Data,"OK")))S->ControlStatus=XREDIS_REQUEST_PROTOCOL;
        if(R->Promise)S->Control=Reply;else{xredisReplyRelease(Reply);xrtMutexLock(&C->Gate);++C->Info.DiscardedReplies;xrtMutexUnlock(&C->Gate);}
        if(S->ControlStatus==XREDIS_REQUEST_PROTOCOL){xredisClientFail(C,XREDIS_REQUEST_PROTOCOL,NULL);return;}
    }else{
        S->QueryStatus=Error?XREDIS_REQUEST_SERVER_ERROR:XREDIS_REQUEST_OK;
        if(R->Promise)S->Query=Reply;else{xredisReplyRelease(Reply);xrtMutexLock(&C->Gate);++C->Info.DiscardedReplies;xrtMutexUnlock(&C->Gate);}
        xredisAskingFinish(R);
    }
}
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
static void xredisTransactionObserve(xredisrequest* Request)
{
	if (Request->Promise != NULL) {
		if (xrtAtomic32Load(&Request->Cancelled, XMEMORY_ACQUIRE)) xredisClientAbandon(Request, XREDIS_REQUEST_CANCELLED);
		else if (xrtTimer() >= Request->Deadline) xredisClientAbandon(Request, XREDIS_REQUEST_TIMEOUT);
	}
}

static void xredisTransactionNext(xredisrequest* Request, unsigned Phase)
{
	xredistransactionstate* State = xredisTransactionState(Request);
	State->Phase = Phase; State->Written = 0; State->Await = false; xredisClientWake(Request->Client);
}

static void xredisTransactionFinish(xredisrequest* Request, xredisrequeststatus Status, xredisreply* Reply)
{
	xredisclient* Client = Request->Client;
	if (Request->Promise == NULL) {
		xredisReplyRelease(Reply);
		xrtMutexLock(&Client->Gate); ++Client->Info.DiscardedReplies; xrtMutexUnlock(&Client->Gate);
	} else {
		if (xrtAtomic32Load(&Request->Cancelled, XMEMORY_ACQUIRE)) Status = XREDIS_REQUEST_CANCELLED;
		else if (xrtTimer() >= Request->Deadline) Status = XREDIS_REQUEST_TIMEOUT;
		if (Status == XREDIS_REQUEST_CANCELLED || Status == XREDIS_REQUEST_TIMEOUT) { xredisReplyRelease(Reply); Reply = NULL; }
		xredisClientComplete(Request, Status, XREDIS_REPLIED, Reply, NULL);
	}
	xredisClientUnlink(Request); Request->Phase = XREDIS_R_DONE; XREDIS_SUBSCRIBER_RETIRE(Request);
	xredisClientCancelTimer(Request); xredisRequestRelease(Request); xredisClientWake(Client);
}

static void xredisTransactionPump(xredisrequest* Request)
{
	xredisclient* Client = Request->Client; xredistransactionstate* State = xredisTransactionState(Request);
	xredisTransactionObserve(Request);
	if (xredisClientClosing(Client)) return;
	if (State->Await) return;
	if (Request->Promise == NULL && State->Written == 0) {
		if (State->Phase == XREDIS_TX_MULTI) {
			xredisTransactionFinish(Request, XREDIS_REQUEST_CANCELLED, NULL); return;
		}
		if (State->Phase == XREDIS_TX_COMMANDS || State->Phase == XREDIS_TX_EXEC)
			State->Phase = XREDIS_TX_DISCARD;
	}
	size_t Offset = 0, Length = sizeof(xredisTransactionMulti) - 1u;
	if (State->Phase == XREDIS_TX_COMMANDS) { Offset = Length; Length = State->CommandsBytes; }
	else if (State->Phase == XREDIS_TX_EXEC) { Offset = Length + State->CommandsBytes; Length = sizeof(xredisTransactionExec) - 1u; }
	else if (State->Phase == XREDIS_TX_DISCARD) {
		Offset = Length + State->CommandsBytes + sizeof(xredisTransactionExec) - 1u; Length = sizeof(xredisTransactionDiscard) - 1u;
	}
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Client->TLS != NULL) {
		while (State->Written < Length && !xredisClientClosing(Client)) {
			if (Client->TlsTurnBytes == 0 || Client->TlsTurnCalls == 0) { xredisClientWake(Client); return; }
			if (Client->TlsWrite == NULL) { Client->TlsWrite = Request; xredisRequestRef(Request); }
			size_t Written = 0;
			xtlsresult Result = xredisClientTlsSend(Client, Request->Wire + Offset + State->Written, Length - State->Written, &Written);
			State->Written += Written;
			if (State->Phase == XREDIS_TX_EXEC && Written != 0) State->Execution = XREDIS_TRANSACTION_MAY_HAVE_EXECUTED;
			if (Result == XTLS_ERROR || Result == XTLS_CLOSED) {
				xredisClientFail(Client, Result == XTLS_ERROR && xrtErrorKind(xrtGetError()) == XERR_MEMORY ?
					XREDIS_REQUEST_MEMORY : XREDIS_REQUEST_DISCONNECTED, Result == XTLS_ERROR ? xrtGetError() : NULL); return;
			}
			if (Written == 0) return;
		}
		if (State->Written != Length) return; /* Concurrent Close owns the remaining write reference. */
		Client->TlsWrite = NULL; Client->TlsWritten = 0; xredisRequestRelease(Request);
	} else
#endif
	{
		xredisRequestRef(Request);
		xnetresult Result = xrtNetStreamSendRef(Client->Stream, Request->Wire + Offset, Length, xredisClientWireDrop, Request);
		if (Result != XNET_RESULT_OK) {
			xredisRequestRelease(Request);
			if (Result != XNET_RESULT_AGAIN) xredisClientFail(Client,
				xrtErrorKind(xrtGetError()) == XERR_MEMORY && Result == XNET_RESULT_ERROR ? XREDIS_REQUEST_MEMORY : XREDIS_REQUEST_DISCONNECTED,
				Result == XNET_RESULT_ERROR ? xrtGetError() : NULL);
			return;
		}
		State->Written = Length;
		if (State->Phase == XREDIS_TX_EXEC) State->Execution = XREDIS_TRANSACTION_MAY_HAVE_EXECUTED;
	}
	State->Await = true;
	/* QUEUED replies may arrive while a fragmented TLS command batch is still
	 * being sent. Only cross this boundary after BOTH the full send and N replies. */
	if (State->Phase == XREDIS_TX_COMMANDS && State->Seen == State->Count) {
		xredisTransactionObserve(Request);
		xredisTransactionNext(Request, State->Errors != 0 || Request->Promise == NULL ? XREDIS_TX_DISCARD : XREDIS_TX_EXEC);
	}
}

static void xredisTransactionFrame(xredisrequest* Request, xredisreply* Reply, const xredisreplyvalue* Root)
{
	xredisclient* Client = Request->Client; xredistransactionstate* State = xredisTransactionState(Request);
	bool Error = Root->Type == XREDIS_REPLY_SIMPLE_ERROR || Root->Type == XREDIS_REPLY_BULK_ERROR;
	if (State->Phase == XREDIS_TX_COMMANDS && State->Seen < State->Count) {
		xredisTransactionObserve(Request);
		if (!Error && !(Root->Type == XREDIS_REPLY_SIMPLE_STRING && xredisClientInitLiteral(Root->Data, "QUEUED"))) goto protocol;
		++State->Seen;
		if (Error) {
			++State->Errors;
			if (Request->Promise != NULL && State->Error == NULL) { State->Error = Reply; Reply = NULL; }
		}
		xredisReplyRelease(Reply);
		if (State->Await && State->Seen == State->Count)
			xredisTransactionNext(Request, State->Errors != 0 || Request->Promise == NULL ? XREDIS_TX_DISCARD : XREDIS_TX_EXEC);
		return;
	}
	if (!State->Await) goto protocol;
	if (State->Phase == XREDIS_TX_MULTI) {
		if (Error) { xredisTransactionFinish(Request, XREDIS_REQUEST_SERVER_ERROR, Reply); return; }
		if (Root->Type != XREDIS_REPLY_SIMPLE_STRING || !xredisClientInitLiteral(Root->Data, "OK")) goto protocol;
		xredisTransactionObserve(Request);
		xredisReplyRelease(Reply);
		xredisTransactionNext(Request, Request->Promise == NULL ? XREDIS_TX_DISCARD : State->Count ? XREDIS_TX_COMMANDS : XREDIS_TX_EXEC); return;
	}
	if (State->Phase == XREDIS_TX_EXEC) {
		if (Root->Type == XREDIS_REPLY_ARRAY && !(Root->Flags & XREDIS_REPLY_NULL_VALUE) && Root->ChildCount == State->Count)
			State->Execution = XREDIS_TRANSACTION_EXECUTED;
		else if (Root->Type == XREDIS_REPLY_NULL || (Root->Type == XREDIS_REPLY_ARRAY && (Root->Flags & XREDIS_REPLY_NULL_VALUE)))
			State->Execution = XREDIS_TRANSACTION_ABORTED;
		else if (Error) {
			if (xredisClientInitCode(Root->Data, "EXECABORT")) State->Execution = XREDIS_TRANSACTION_ABORTED;
			else {
				/* An unknown EXEC error does not certify that the connection left
				 * MULTI. Do not expose possibly dirty state to following calls. */
				xredisClientFail(Client, XREDIS_REQUEST_DISCONNECTED, NULL);
			}
			xredisTransactionFinish(Request, XREDIS_REQUEST_SERVER_ERROR, Reply); return;
		} else goto protocol;
		xredisTransactionFinish(Request, XREDIS_REQUEST_OK, Reply); return;
	}
	if (State->Phase == XREDIS_TX_DISCARD) {
		if (Error) {
			xredisClientFail(Client, XREDIS_REQUEST_DISCONNECTED, NULL);
			xredisTransactionFinish(Request, XREDIS_REQUEST_SERVER_ERROR, Reply); return;
		}
		if (Root->Type != XREDIS_REPLY_SIMPLE_STRING || !xredisClientInitLiteral(Root->Data, "OK")) goto protocol;
		xredisReplyRelease(Reply); xredisTransactionFinish(Request, State->Errors ? XREDIS_REQUEST_SERVER_ERROR : XREDIS_REQUEST_CANCELLED, NULL); return;
	}
protocol:
	xredisReplyRelease(Reply); xredisClientFail(Client, XREDIS_REQUEST_PROTOCOL, NULL);
}
#endif

static void xredisClientTimer(xnetworker* Worker, uint64 Id, xnetresult Result, ptr Data)
{
	(void)Worker; xredisrequest* Request = (xredisrequest*)Data; xredisclient* Client = Request->Client;
	if (Request->Timer == Id) Request->Timer = 0;
	if (Result == XNET_RESULT_OK) {
		if (Request->Promise != NULL) xredisClientAbandon(Request, xrtAtomic32Load(&Request->Cancelled, XMEMORY_ACQUIRE) ?
			XREDIS_REQUEST_CANCELLED : XREDIS_REQUEST_TIMEOUT);
		else if (Request->Phase == XREDIS_R_SENT) xredisClientFail(Client, XREDIS_REQUEST_TIMEOUT, NULL);
	} else if (Result != XNET_RESULT_CANCELLED && Request->Phase != XREDIS_R_DONE)
		xredisClientFail(Client, XREDIS_REQUEST_CLOSED, NULL);
	xredisClientWake(Client); xredisRequestRelease(Request);
}

static void xredisClientOnOpen(xnetstream* Stream, ptr Data)
{
	(void)Stream; xredisclient* Client = (xredisclient*)Data;
	Client->TransportOpen = true;
	if (!Client->Managed) xredisClientReadyFinish(Client, XREDIS_READY_OK, NULL);
	else if (Client->InitMode == XREDIS_INIT_RESP2) {
		xrtMutexLock(&Client->Gate); Client->Info.Protocol = XREDIS_RESP2; xrtMutexUnlock(&Client->Gate);
	}
	xredisClientWake(Client);
}

static void xredisClientOnEnd(xnetstream* Stream, ptr Data)
{
	(void)Stream; xredisclient* Client = (xredisclient*)Data; Client->ReadEnded = true; xredisClientWake(Client);
}

static void xredisClientOnWritable(xnetstream* Stream, ptr Data)
{ (void)Stream; xredisClientWake((xredisclient*)Data); }

static void xredisClientOnLowWater(xnetstream* Stream, size_t Queued, ptr Data)
{ (void)Queued; xredisClientOnWritable(Stream, Data); }

static void xredisClientOnClose(xnetstream* Stream, xnetresult Result, const xerror* Error, ptr Data)
{
	(void)Stream; (void)Result; xredisclient* Client = (xredisclient*)Data;
	Client->TransportClosed = true;
	xrtMutexLock(&Client->Gate);
	if (Client->Info.State < XREDIS_CLIENT_CLOSING) {
		Client->Info.State = XREDIS_CLIENT_CLOSING;
		Client->Info.CloseStatus = xrtErrorKind(Error) == XERR_MEMORY ? XREDIS_REQUEST_MEMORY : XREDIS_REQUEST_DISCONNECTED;
		Client->CloseCause = xrtErrorRef(Error);
	}
	xrtMutexUnlock(&Client->Gate); xredisClientWake(Client);
}

static const xnetstreamevents xredisClientEvents = {
	xredisClientOnOpen, xredisClientRead, xredisClientOnEnd, NULL,
	xredisClientOnLowWater, xredisClientOnWritable, xredisClientOnClose
};

#if defined(XREDIS_FEATURE_XREDIS_DIAL)
static xredisrequeststatus xredisClientConnectFailure(const xerror* Cause)
{
	xerrkind Kind = xrtErrorKind(Cause);
	return Kind == XERR_MEMORY ? XREDIS_REQUEST_MEMORY :
		(Kind == XERR_AGAIN || Kind == XERR_RANGE) ? XREDIS_REQUEST_LIMIT : XREDIS_REQUEST_DISCONNECTED;
}

void __xredisClientConnectDone(xredisclient* Client, xnetresult Result,
	xnetstream* Stream, const xerror* Cause)
{
	if (Stream != NULL) {
		Client->Stream = Stream;
		/* A close requested during DNS may have observed no transport. A late
		 * winning reference still has to reach its ACTUAL TCP close barrier. */
		Client->TransportClosed = xrtNetStreamState(Stream) == XNET_STREAM_CLOSED;
		if (xredisClientClosing(Client)) xrtNetStreamAbort(Stream);
	} else {
		/* A cancelled pending Dial never created a stream. Its terminal callback
		 * is the physical transport barrier even when public Close won first. */
		Client->TransportClosed = true;
		if (!xredisClientClosing(Client)) {
		xredisrequeststatus Status = Result == XNET_RESULT_TIMEOUT || Result == XNET_RESULT_CANCELLED ?
			XREDIS_REQUEST_DISCONNECTED : xredisClientConnectFailure(Cause);
		xredisClientFail(Client, Status, Cause);
		xredisreadystatus Ready = Result == XNET_RESULT_TIMEOUT ? XREDIS_READY_TIMEOUT :
			Result == XNET_RESULT_CANCELLED ? XREDIS_READY_CLOSED : xredisClientReadyFailure(Status);
		xredisClientReadyFinish(Client, Ready, Cause);
		}
	}
	xredisClientWake(Client);
}

void __xredisClientConnectRetired(xredisclient* Client)
{ Client->PlanPending = false; xredisClientWake(Client); }
#endif

#if defined(XREDIS_FEATURE_XREDIS_TLS)
static void xredisClientTlsOpen(xtlsstream* Stream, ptr Data)
{ (void)Stream; xredisClientOnOpen(NULL, Data); }
static void xredisClientTlsRead(xtlsstream* Stream, const xnetbuf* Buffer, ptr Data)
{ (void)Stream; xredisClientReadBuffer((xredisclient*)Data, Buffer); }
static void xredisClientTlsEnd(xtlsstream* Stream, ptr Data)
{ (void)Stream; xredisClientOnEnd(NULL, Data); }
static void xredisClientTlsWritable(xtlsstream* Stream, ptr Data)
{ (void)Stream; xredisClientWake((xredisclient*)Data); }
static void xredisClientTlsClose(xtlsstream* Stream, xnetresult Result, const xerror* Error, ptr Data)
{
	(void)Stream; xredisclient* Client = (xredisclient*)Data; xredisClientOnClose(NULL, Result, Error, Data);
	if (Client->ReadyPromise != NULL && xrtErrorKind(Error) == XERR_TIMEOUT) xredisClientReadyFinish(Client, XREDIS_READY_TIMEOUT, Error);
}
static const xtlsstreamevents xredisClientTlsEvents = {
	xredisClientTlsOpen, xredisClientTlsRead, xredisClientTlsEnd, xredisClientTlsWritable, xredisClientTlsWritable, xredisClientTlsClose, NULL
};
const xnetstreamevents* __xredisClientTlsPendingEvents(void)
{
	static const xnetstreamevents Events = {NULL, NULL, NULL, NULL, NULL, NULL, xredisClientOnClose}; return &Events;
}
bool __xredisClientConnectTls(xredisclient* Client, xnetstream* Transport, const xredistlsplan* Plan)
{
	if (xredisClientClosing(Client) || xrtNetStreamState(Transport) != XNET_STREAM_OPEN) {
		__xredisClientConnectDone(Client, XNET_RESULT_OK, Transport, NULL);
		if (!xredisClientClosing(Client)) xredisClientFail(Client, XREDIS_REQUEST_DISCONNECTED, NULL);
		return false;
	}
	Client->TransportClosed = false; Client->TlsWriteBytes = Plan->WriteBytes;
	if (xrtTlsStreamAttach(Transport, Plan->Session, &Plan->Config, &xredisClientTlsEvents, Client, &Client->TLS)) return true;
	xerror* Cause = xrtErrorRef(xrtGetError()); __xredisClientConnectDone(Client, XNET_RESULT_ERROR, Transport, Cause);
	xredisClientFail(Client, xredisClientConnectFailure(Cause), Cause); xrtErrorFree(Cause); return false;
}
#endif

static void xredisClientFrame(xredisclient* Client, xredisreply* Reply)
{
	xredisreplyinfo Info = {0}; Info.Size = sizeof(Info); xredisReplyInfo(Reply, &Info);
	xredisreplyvalue Root; xredisReplyGet(Reply, Info.Root, &Root);
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    if(xrtAtomicPtrLoad(&Client->Subscriber,XMEMORY_ACQUIRE)){xredisSubscriberFrame(Client,Reply,&Root,Info.ReservedBytes);return;}
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    xredisobservation* Observation=xrtAtomicPtrLoad(&Client->Observation,XMEMORY_ACQUIRE);uint64 Sequence=0;
    if(Observation){
        /* One IO worker writes; external Info readers only load this atomic. */
        Sequence=xrtAtomic64Load(&Observation->Sequence,XMEMORY_RELAXED);
        if(Sequence==UINT64_MAX){xredisReplyRelease(Reply);xredisClientFail(Client,XREDIS_REQUEST_LIMIT,NULL);return;}
        ++Sequence;xrtAtomic64Store(&Observation->Sequence,Sequence,XMEMORY_RELEASE);
    }
#endif
    if (Root.Type == XREDIS_REPLY_PUSH) {
		xrtMutexLock(&Client->Gate);
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
        /* Adoption can race the initial mode load, but cannot publish between
         * this recheck and ordinary push enqueue under the same Gate. */
        if(xrtAtomicPtrLoad(&Client->Subscriber,XMEMORY_ACQUIRE)){
            xrtMutexUnlock(&Client->Gate);xredisSubscriberFrame(Client,Reply,&Root,Info.ReservedBytes);return;
        }
#endif
		bool Fits = Client->Info.Pushes < Client->Config.MaxPushes &&
			Info.ReservedBytes <= Client->Config.MaxPushBytes - Client->Info.PushBytes;
		if (Fits) {
			size_t Index = (Client->PushHead + Client->Info.Pushes) % Client->Config.MaxPushes;
			Client->PushQueue[Index] = Reply; ++Client->Info.Pushes; Client->Info.PushBytes += Info.ReservedBytes;
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
            if(Observation)Observation->Pushes[Index]=Sequence;
#endif
		}
		xrtMutexUnlock(&Client->Gate);
		if (!Fits) { xredisReplyRelease(Reply); xredisClientFail(Client, XREDIS_REQUEST_LIMIT, NULL); }
		return;
	}
	if (Client->ReadyPromise != NULL) { xredisClientInitFrame(Client, Reply, &Root); return; }
	xredisrequest* Request = Client->SentHead;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Request != NULL && Request == Client->TlsWrite
#if defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
        && !xredisAskingPair(Request)
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
        && !xredisCachingPair(Request)
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
		&& !Request->Transaction
#endif
	) { xredisReplyRelease(Reply); xredisClientFail(Client, XREDIS_REQUEST_PROTOCOL, NULL); return; }
#endif
	if (Request == NULL) { xredisReplyRelease(Reply); xredisClientFail(Client, XREDIS_REQUEST_PROTOCOL, NULL); return; }
#if defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
    if(xredisAskingPair(Request)){xredisAskingFrame(Request,Reply,&Root);return;}
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
	if (Request->Transaction) { xredisTransactionFrame(Request, Reply, &Root); return; }
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    if(xredisCachingPair(Request)){xredisCachingFrame(Request,Reply,&Root,Sequence);return;}
    if(Request->Observe==2 && Root.Type!=XREDIS_REPLY_SIMPLE_ERROR && Root.Type!=XREDIS_REPLY_BULK_ERROR &&
        !(Root.Type==XREDIS_REPLY_SIMPLE_STRING && Root.Data.Size==2 && !memcmp(Root.Data.Data,"OK",2))){
        xredisReplyRelease(Reply);xredisClientFail(Client,XREDIS_REQUEST_PROTOCOL,NULL);return;
    }
#endif
	if (Request->Promise != NULL) {
		xredisrequeststatus Status = Root.Type == XREDIS_REPLY_SIMPLE_ERROR || Root.Type == XREDIS_REPLY_BULK_ERROR ?
			XREDIS_REQUEST_SERVER_ERROR : XREDIS_REQUEST_OK;
		if (xrtAtomic32Load(&Request->Cancelled, XMEMORY_ACQUIRE)) Status = XREDIS_REQUEST_CANCELLED;
		else if (xrtTimer() >= Request->Deadline) Status = XREDIS_REQUEST_TIMEOUT;
		if (Status == XREDIS_REQUEST_CANCELLED || Status == XREDIS_REQUEST_TIMEOUT) {
			xredisReplyRelease(Reply); Reply = NULL;
			xrtMutexLock(&Client->Gate); ++Client->Info.DiscardedReplies; xrtMutexUnlock(&Client->Gate);
		}
		#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
        if(Request->Observe)((xredistrackingresult*)Request->Result)->Sequence=Reply?Sequence:0;
#endif
        xredisClientComplete(Request, Status, XREDIS_REPLIED, Reply, NULL);
	} else {
		xredisReplyRelease(Reply); xrtMutexLock(&Client->Gate); ++Client->Info.DiscardedReplies; xrtMutexUnlock(&Client->Gate);
	}
	xredisClientUnlink(Request); Request->Phase = XREDIS_R_DONE; XREDIS_SUBSCRIBER_RETIRE(Request);
	xredisClientCancelTimer(Request); xredisRequestRelease(Request); xredisClientWake(Client);
}

static void xredisClientReadBuffer(xredisclient* Client, const xnetbuf* Buffer)
{
	if (Client->Reading || xredisClientClosing(Client)) return;
	Client->Reading = true; size_t Bytes = 0, Frames = 0;
	while (Bytes < Client->Config.ReadBytes && Frames < Client->Config.ReadFrames && !xredisClientClosing(Client)) {
		xnetspan Span; if (!xrtNetBufFront(Buffer, &Span)) break;
		size_t Available = Client->Config.ReadBytes - Bytes;
		if (Span.Size > Available) Span.Size = Available;
		xredisreply* Reply = NULL; size_t Consumed = 0;
		xredisreplystatus Status = xredisReplyReaderFeed(Client->Reader, (xbytesview){Span.Data, Span.Size}, &Consumed, &Reply);
#if defined(XREDIS_FEATURE_XREDIS_TLS)
		if (Client->TLS != NULL) (void)xrtTlsStreamConsume(Client->TLS, Consumed); else
#endif
		(void)xrtNetBufConsume((xnetbuf*)Buffer, Consumed);
		Bytes += Consumed;
		if (Status == XREDIS_REPLY_ERROR) {
			xredisreplyreaderinfo State = {0}; State.Size = sizeof(State); xredisReplyReaderInfo(Client->Reader, &State);
			xredisrequeststatus Error = State.Error == XREDIS_REPLY_MEMORY ? XREDIS_REQUEST_MEMORY :
				State.Error == XREDIS_REPLY_LIMIT ? XREDIS_REQUEST_LIMIT : XREDIS_REQUEST_PROTOCOL;
			xredisClientFail(Client, Error, xrtGetError()); break;
		}
		if (Status == XREDIS_REPLY_READY) { ++Frames; xredisClientFrame(Client, Reply); }
		else if (Consumed == 0) break;
	}
	Client->Reading = false;
	if (!xredisClientClosing(Client) && (Bytes == Client->Config.ReadBytes || Frames == Client->Config.ReadFrames)) {
		Client->ReadYielded = true; Client->ReadPaused = true;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
		if (Client->TLS == NULL)
#endif
		xrtNetStreamPause(Client->Stream);
		xredisClientWake(Client);
	}
}

static void xredisClientRead(xnetstream* Stream, xnetbuf* Buffer, ptr Data)
{ (void)Stream; xredisClientReadBuffer((xredisclient*)Data, Buffer); }

static void xredisClientIngest(xredisclient* Client)
{
	for (size_t Work = 0; Work < Client->Config.WorkRequests; ++Work) {
		xrtMutexLock(&Client->Gate); xredisrequest* Request = Client->SubmitHead;
		if (Request != NULL) {
			Client->SubmitHead = Request->SubmitNext;
			if (Client->SubmitHead == NULL) Client->SubmitTail = NULL;
		}
		xrtMutexUnlock(&Client->Gate);
		if (Request == NULL) break;
		if (Request->Promise == NULL) { Request->Phase = XREDIS_R_DONE; XREDIS_SUBSCRIBER_RETIRE(Request); xredisRequestRelease(Request); continue; }
		if (xredisClientClosing(Client)) {
			xredisClientComplete(Request, xredisClientFailureStatus(Client), XREDIS_NOT_SENT, NULL, Client->CloseCause);
			Request->Phase = XREDIS_R_DONE; XREDIS_SUBSCRIBER_RETIRE(Request); xredisRequestRelease(Request); continue;
		}
		xredisClientAppend(Client, Request, false);
		if (xrtAtomic32Load(&Request->Cancelled, XMEMORY_ACQUIRE)) xredisClientAbandon(Request, XREDIS_REQUEST_CANCELLED);
		else if (xrtTimer() >= Request->Deadline) xredisClientAbandon(Request, XREDIS_REQUEST_TIMEOUT);
		else if (!xredisClientArm(Request, Request->Deadline)) {
			xredisClientComplete(Request, XREDIS_REQUEST_MEMORY, XREDIS_NOT_SENT, NULL, xrtGetError());
			xredisClientUnlink(Request); Request->Phase = XREDIS_R_DONE; XREDIS_SUBSCRIBER_RETIRE(Request); xredisRequestRelease(Request);
		}
	}
}

#if defined(XREDIS_FEATURE_XREDIS_TLS)
static void xredisClientTlsPump(xredisclient* Client)
{
	while (!xredisClientClosing(Client) && xredisClientTransportOpen(Client)) {
		if (Client->TlsTurnBytes == 0 || Client->TlsTurnCalls == 0) {
			if (Client->TlsWrite != NULL || (Client->PendingHead != NULL && Client->SentCount < Client->Config.MaxInFlight)) xredisClientWake(Client);
			return;
		}
		xredisrequest* Request = Client->TlsWrite;
		if (Request == NULL) {
			if (Client->PendingHead == NULL || Client->SentCount >= Client->Config.MaxInFlight) return;
			Request = Client->PendingHead;
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
			if (Request->Transaction) {
				if (Client->SentCount != 0) return;
				xredisClientUnlink(Request); xredisClientAppend(Client, Request, true); xredisTransactionPump(Request); return;
			}
#endif
			if (xrtAtomic32Load(&Request->Cancelled, XMEMORY_ACQUIRE) || xrtTimer() >= Request->Deadline) {
				--Client->TlsTurnCalls;
				xredisClientAbandon(Request, xrtAtomic32Load(&Request->Cancelled, XMEMORY_ACQUIRE) ? XREDIS_REQUEST_CANCELLED : XREDIS_REQUEST_TIMEOUT); continue;
			}

#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
        if(Request->Observe>=5 && !xredisSubscriberPrepare(Request))continue;
#endif
        xredisClientUnlink(Request); xredisClientAppend(Client, Request, true); xredisRequestRef(Request);
			Client->TlsWrite = Request; Client->TlsWritten = 0;
		}
		size_t Written = 0; xtlsresult Result = xredisClientTlsSend(Client, Request->Wire + Client->TlsWritten, Request->WireBytes - Client->TlsWritten, &Written);
		Client->TlsWritten += Written;
		if (Result == XTLS_ERROR || Result == XTLS_CLOSED) {
			/* Zero accepted bytes still means NOT_SENT. A partial command is a
			 * sent FIFO obligation even after the caller abandons its Future. */
			if (Client->TlsWritten == 0) { xredisClientUnlink(Request); xredisClientAppend(Client, Request, false); }
			xredisClientFail(Client, Result == XTLS_ERROR && xrtErrorKind(xrtGetError()) == XERR_MEMORY ? XREDIS_REQUEST_MEMORY : XREDIS_REQUEST_DISCONNECTED,
				Result == XTLS_ERROR ? xrtGetError() : NULL); return;
		}
		if (Client->TlsWritten == Request->WireBytes) {
			Client->TlsWrite = NULL; Client->TlsWritten = 0; xredisRequestRelease(Request);
#if defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
            if(xredisAskingPair(Request)){xredisAskingWritten(Request);continue;}
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
            xredisCachingWritten(Request);
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
            if(Request->Observe>=5)xredisSubscriberWritten(Request);
#endif
			continue;
		}
		if (Written == 0) {
			if (Client->TlsWritten == 0 && Client->TlsWrite != NULL) {
				/* No byte was accepted: keep original FIFO position and report
				 * NOT_SENT if the caller now expires/cancels while blocked. */
				xredisClientUnlink(Request); Request->Phase = XREDIS_R_PENDING;
				Request->Next = Client->PendingHead;
				if (Client->PendingHead != NULL) Client->PendingHead->Previous = Request; else Client->PendingTail = Request;
				Client->PendingHead = Request; Client->TlsWrite = NULL; xredisRequestRelease(Request);
			}
			return;
		}
	}
}
#endif

static void xredisClientPump(xredisclient* Client)
{
	if (Client->ReadyPromise != NULL) return;
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
	if (Client->SentHead != NULL && Client->SentHead->Transaction) { xredisTransactionPump(Client->SentHead); return; }
#endif
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Client->TLS != NULL) { xredisClientTlsPump(Client); return; }
#endif
	for (size_t Work = 0; Work < Client->Config.WorkRequests && Client->PendingHead != NULL &&
		Client->SentCount < Client->Config.MaxInFlight && !xredisClientClosing(Client) &&
		xredisClientTransportOpen(Client); ++Work) {
		xredisrequest* Request = Client->PendingHead;
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
		if (Request->Transaction) {
			if (Client->SentCount != 0) return;
			xredisClientUnlink(Request); xredisClientAppend(Client, Request, true); xredisTransactionPump(Request); return;
		}
#endif
		if (xrtAtomic32Load(&Request->Cancelled, XMEMORY_ACQUIRE) || xrtTimer() >= Request->Deadline) {
			xredisClientAbandon(Request, xrtAtomic32Load(&Request->Cancelled, XMEMORY_ACQUIRE) ?
				XREDIS_REQUEST_CANCELLED : XREDIS_REQUEST_TIMEOUT); continue;
		}

#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
        if(Request->Observe>=5 && !xredisSubscriberPrepare(Request))continue;
#endif
        xredisClientUnlink(Request); xredisClientAppend(Client, Request, true); xredisRequestRef(Request);
		xnetresult Result = xrtNetStreamSendRef(Client->Stream, Request->Wire, Request->WireBytes, xredisClientWireDrop, Request);
		if (Result == XNET_RESULT_OK) {
#if defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
            if(xredisAskingPair(Request)){xredisAskingWritten(Request);continue;}
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
            xredisCachingWritten(Request);
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
            if(Request->Observe>=5)xredisSubscriberWritten(Request);
#endif
            continue;
        }
		xredisRequestRelease(Request); xredisClientUnlink(Request);
		Request->Phase = XREDIS_R_PENDING;
		if (Result == XNET_RESULT_AGAIN) {
			Request->Next = Client->PendingHead;
			if (Client->PendingHead != NULL) Client->PendingHead->Previous = Request; else Client->PendingTail = Request;
			Client->PendingHead = Request; break;
		}
		xredisClientComplete(Request, Result == XNET_RESULT_CLOSED ? XREDIS_REQUEST_DISCONNECTED :
			xrtErrorKind(xrtGetError()) == XERR_MEMORY ? XREDIS_REQUEST_MEMORY : XREDIS_REQUEST_DISCONNECTED,
			XREDIS_NOT_SENT, NULL, Result == XNET_RESULT_ERROR ? xrtGetError() : NULL);
		Request->Phase = XREDIS_R_DONE; XREDIS_SUBSCRIBER_RETIRE(Request); xredisClientCancelTimer(Request); xredisRequestRelease(Request);
	}
}

static void xredisClientRetire(xredisclient* Client)
{
	for (size_t Work = 0; Work < Client->Config.WorkRequests &&
		(Client->PendingHead != NULL || Client->SentHead != NULL); ++Work) {
		xredisrequest* Request = Client->PendingHead != NULL ? Client->PendingHead : Client->SentHead;
		xredisdelivery Delivery = Request->Phase == XREDIS_R_SENT ? XREDIS_MAY_HAVE_EXECUTED : XREDIS_NOT_SENT;
		xredisClientUnlink(Request); Request->Phase = XREDIS_R_DONE; XREDIS_SUBSCRIBER_RETIRE(Request);
		xredisClientComplete(Request, xredisClientFailureStatus(Client), Delivery, NULL, Client->CloseCause);
		xredisClientCancelTimer(Request); xredisRequestRelease(Request);
	}
}

static void xredisClientWork(xnetworker* Worker, ptr Data)
{
	(void)Worker; xredisclient* Client = (xredisclient*)Data;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	Client->TlsTurnBytes = Client->TlsWriteBytes; Client->TlsTurnCalls = Client->Config.WorkRequests;
#endif
	xrtMutexLock(&Client->Gate); Client->WakeQueued = false;
	xredisrequest* Cancel = Client->CancelHead;
	xredisrequest* Last = Cancel;
	for (size_t Count = 1; Last != NULL && Count < Client->Config.WorkRequests && Last->CancelNext != NULL; ++Count)
		Last = Last->CancelNext;
	if (Last != NULL) { Client->CancelHead = Last->CancelNext; Last->CancelNext = NULL; }
	if (Client->CancelHead == NULL) Client->CancelTail = NULL;
	xrtMutexUnlock(&Client->Gate);
	Client->ReadYielded = false;
	while (Cancel != NULL) {
		xredisrequest* Next = Cancel->CancelNext;
		if (Cancel->Phase != XREDIS_R_DONE) xredisClientAbandon(Cancel, XREDIS_REQUEST_CANCELLED);
		xredisRequestRelease(Cancel); Cancel = Next;
	}
	if (!Client->Started) {
		Client->Started = true;
		if (!xredisClientClosing(Client) && Client->Managed) {
			if (xrtTimer() >= Client->InitDeadline) xredisClientInitReject(Client, XREDIS_READY_TIMEOUT);
			else if (!xredisClientInitArm(Client)) xredisClientFail(Client, XREDIS_REQUEST_MEMORY, xrtGetError());
		}
		if (!xredisClientClosing(Client)) {
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
			if (Client->Plan != NULL) {
				Client->PlanPending = true;
				if (!Client->Plan->Start(Client->Plan->Data, Client, Client->Engine,
					xrtNetWorkerIndex(Client->Worker), &Client->Config.Stream, &xredisClientEvents)) {
					Client->PlanPending = false;
					xredisClientFail(Client, xredisClientConnectFailure(xrtGetError()), xrtGetError());
				}
			} else
#endif
			{
			Client->Stream = xrtNetStreamConnect(Client->Engine, &Client->Remote, xrtNetWorkerIndex(Client->Worker),
				&Client->Config.Stream, &xredisClientEvents, Client);
			if (Client->Stream == NULL) xredisClientFail(Client, xrtErrorKind(xrtGetError()) == XERR_MEMORY ?
				XREDIS_REQUEST_MEMORY : XREDIS_REQUEST_DISCONNECTED, xrtGetError());
			}
		} else Client->TransportClosed = true;
	}
	xredisClientIngest(Client);
	if (!xredisClientClosing(Client)) {
		const xnetbuf* Buffer = xredisClientTransportBuffer(Client);
		if (Buffer != NULL && xrtNetBufSize(Buffer) != 0) xredisClientReadBuffer(Client, Buffer);
		if (!xredisClientClosing(Client) && !Client->ReadEnded && !Client->ReadYielded && Client->ReadPaused) {
			Client->ReadPaused = false;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
			if (Client->TLS == NULL && Client->Stream != NULL)
#endif
			xrtNetStreamResume(Client->Stream);
		}
		if (Client->ReadEnded && !Client->ReadYielded && !xredisClientClosing(Client)) {
			bool Complete = xredisReplyReaderFinish(Client->Reader);
			xredisClientFail(Client, Complete ? XREDIS_REQUEST_DISCONNECTED : XREDIS_REQUEST_PROTOCOL,
				Complete ? NULL : xrtGetError());
		}
		if (!xredisClientClosing(Client) && xredisClientTransportOpen(Client)) {
			if (Client->Managed && Client->ReadyPromise != NULL) xredisClientInitPump(Client);
			else xredisClientPump(Client);
		}
	}
	if (xredisClientClosing(Client)) {
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
		if (Client->PlanPending && !Client->PlanCancelled) {
			Client->PlanCancelled = true; Client->Plan->Cancel(Client->Plan->Data);
		}
#endif
		if (Client->ReadyPromise != NULL)
			xredisClientReadyFinish(Client, xredisClientReadyFailure(Client->Info.CloseStatus), Client->CloseCause);
		if (!Client->TransportClosed) xredisClientTransportAbort(Client);
#if defined(XREDIS_FEATURE_XREDIS_TLS)
		if (Client->TlsWrite != NULL) {
			xredisrequest* Write = Client->TlsWrite; Client->TlsWrite = NULL; Client->TlsWritten = 0; xredisRequestRelease(Write);
		}
#endif
		xredisClientRetire(Client);
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
        (void)xredisSubscriberCloseStep(Client);
#endif
	}
	xrtMutexLock(&Client->Gate);
	bool Again = Client->SubmitHead != NULL || Client->CancelHead != NULL ||
		(Client->Info.State >= XREDIS_CLIENT_CLOSING && (Client->PendingHead != NULL || Client->SentHead != NULL));
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    xredissubscriber* Subscriber=xrtAtomicPtrLoad(&Client->Subscriber,XMEMORY_ACQUIRE);
    if(Client->Info.State>=XREDIS_CLIENT_CLOSING && !Client->Info.Requests && Subscriber && Subscriber->Entries)Again=true;
#endif
	if (Again) (void)xredisClientWakeLocked(Client);
	bool Done = Client->TransportClosed && Client->Info.Requests == 0 && Client->Info.InitBytes == 0 &&
		Client->InitTimer == 0 && !Client->WakeQueued && !Client->CloseDone;
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    Done=Done && (!Subscriber || !Subscriber->Entries);
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    Done=Done && !Client->ObservationPreparing;
#endif
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
	Done = Done && !Client->PlanPending;
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY)
    Done=Done && (!Subscriber || !Subscriber->ReadablePreparing);
    xpromise* ReadableSignal=NULL;
    if(Subscriber && Subscriber->ReadablePromise && (Client->Info.Pushes || Done)){
        ReadableSignal=Subscriber->ReadablePromise;Subscriber->ReadablePromise=NULL;
    }
#endif
	if (Done) { Client->CloseDone = true; Client->Info.State = XREDIS_CLIENT_CLOSED; Client->EnginePinned = false; }
	xrtMutexUnlock(&Client->Gate);
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY)
    xredisSubscriberNotify(ReadableSignal);
#endif
	if (Done) {
		xpromise* Promise = Client->ClosePromise; Client->ClosePromise = NULL;
		xrtNetStreamDestroy(Client->Stream); Client->Stream = NULL;
		xredisReplyReaderDestroy(Client->Reader); Client->Reader = NULL;
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
		if (Client->PlanOwned) {
			const xredisconnectplan* Plan = Client->Plan; Client->Plan = NULL; Client->PlanOwned = false;
			Plan->Drop(Plan->Data);
		}
#endif
#if defined(XREDIS_FEATURE_XREDIS_TLS)
		xrtTlsStreamDestroy(Client->TLS); Client->TLS = NULL;
#endif
		xrtNetEngineUnpin(Client->Engine); xrtPromiseResolve(Promise, NULL); xrtPromiseDestroy(Promise);
		xredisClientRelease(Client); /* Runtime activation. */
	}
	xredisClientRelease(Client); /* This embedded post; last access. */
}

XREDIS_API bool xredisClientConfigInit(xredisclientconfig* Config)
{
	if (Config == NULL || !xrtMemRangeValid(Config, sizeof(*Config)))
		return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid client config output");
	*Config = (xredisclientconfig){0}; Config->Size = sizeof(*Config);
	xrtNetStreamConfigInit(&Config->Stream); xredisReplyReaderConfigInit(&Config->Replies);
	Config->MaxRequests = 1024; Config->MaxCommandBytes = 1024u * 1024u;
	if (Config->MaxCommandBytes > Config->Stream.WriteLimit) Config->MaxCommandBytes = Config->Stream.WriteLimit;
	Config->MaxCommandBytesTotal = 8u * 1024u * 1024u; Config->MaxInFlight = 128;
	Config->MaxPushes = 128; Config->MaxPushBytes = 16u * 1024u * 1024u;
	Config->WorkRequests = 128; Config->ReadBytes = 64u * 1024u; Config->ReadFrames = 128; Config->DrainTimeout = 5000;
	return true;
}

static bool xredisClientCreate(xnetengine* Engine, const xnetaddr* Remote,
	uint64 Affinity, const xredisclientconfig* Config, const xredisclientinitconfig* Init,
	double Entered, const xredisconnectplan* Plan, xredisclient** Result)
{
	xredisclientconfig Copy;
	if (Engine == NULL || (Remote == NULL && Plan == NULL) || Result == NULL ||
		(Remote != NULL && (!xrtMemRangeValid(Remote, sizeof(*Remote)) || xrtMemRangesOverlap(Remote, sizeof(*Remote), Result, sizeof(*Result)))) ||
		!xrtMemRangeValid(Result, sizeof(*Result)) ||
		(Config != NULL && (!xrtMemRangeValid(Config, sizeof(*Config)) ||
		 xrtMemRangesOverlap(Config, sizeof(*Config), Result, sizeof(*Result)))))
		return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid client creation arguments");
	if (Config == NULL) xredisClientConfigInit(&Copy); else Copy = *Config;
	xredisclientinitconfig InitCopy = {0};
	if (Init != NULL) {
		if (!xrtMemRangeValid(Init, sizeof(*Init)) || xrtMemRangesOverlap(Init, sizeof(*Init), Result, sizeof(*Result)))
			return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid initialization descriptor or output overlap");
		InitCopy = *Init;
		if (!xredisClientInitValid(&InitCopy, &Copy)) return false;
		const xbytesview Views[] = {InitCopy.Username, InitCopy.Password, InitCopy.Name};
		for (size_t Index = 0; Index < sizeof(Views) / sizeof(Views[0]); ++Index)
			if (xrtMemRangesOverlap(Views[Index].Data, Views[Index].Size, Result, sizeof(*Result)))
				return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "initialization output overlaps input payload");
	}
	if (Copy.Size != sizeof(Copy) || Copy.MaxRequests == 0 || Copy.MaxRequests > 1000000u ||
		Copy.MaxInFlight == 0 || Copy.MaxInFlight > Copy.MaxRequests || Copy.MaxCommandBytes == 0 ||
		Copy.MaxCommandBytes > Copy.MaxCommandBytesTotal || Copy.MaxCommandBytes > Copy.Stream.WriteLimit ||
		Copy.MaxCommandBytes > SIZE_MAX - sizeof(xredisrequest) || Copy.WorkRequests == 0 ||
		Copy.ReadBytes == 0 || Copy.ReadFrames == 0 || Copy.DrainTimeout <= 0 ||
		Copy.MaxPushes > (SIZE_MAX - sizeof(xredisclient)) / sizeof(xredisreply*) ||
		(Remote != NULL && ((Remote->Family != XNET_FAMILY_IPV4 && Remote->Family != XNET_FAMILY_IPV6) || Remote->Port == 0)) ||
		!xrtNetStreamConfigValid(&Copy.Stream))
		return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid client config or endpoint");
	if (!xrtNetEnginePin(Engine)) return false;
	size_t Bytes = sizeof(xredisclient) + Copy.MaxPushes * sizeof(xredisreply*);
	xredisclient* Client = (xredisclient*)xrtCalloc(1, Bytes);
	if (Client == NULL) { xrtNetEngineUnpin(Engine); return false; }
	Client->Engine = Engine; Client->EnginePinned = true; Client->Config = Copy;
	if (Remote != NULL) Client->Remote = *Remote;
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
	Client->Plan = Plan;
#endif
	Client->StorageBytes = Bytes; Client->Info.Size = sizeof(Client->Info); Client->Info.CloseStatus = XREDIS_REQUEST_CLOSED;
	Client->Managed = Init != NULL; Client->InitAwait = XREDIS_I_END;
	if (Client->Managed) {
		Client->InitMode = InitCopy.Mode; Client->InitFlags = InitCopy.Flags;
		Client->InitDeadline = xredisClientDeadline(Entered, InitCopy.Timeout);
	}
	Client->Worker = xrtNetEngineWorker(Engine, (uint32)(Affinity % xrtNetEngineWorkerCount(Engine)));
	xrtAtomic32Init(&Client->References, 1);
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    xrtAtomicPtrInit(&Client->Subscriber,NULL);
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    xrtAtomicPtrInit(&Client->Observation,NULL);
#endif
	if (!xrtMutexInit(&Client->Gate)) { xrtNetEngineUnpin(Engine); xrtFree(Client); return false; }
	if (!xrtNetPostInit(&Client->Wake) || !xredisReplyReaderCreate(&Copy.Replies, &Client->Reader) ||
		(Client->ClosePromise = xrtPromiseCreate(&Client->CloseFuture, NULL)) == NULL ||
		(Client->ReadyPromise = xrtPromiseCreate(&Client->ReadyFuture, NULL)) == NULL ||
		(Client->ReadyResult = (xredisreadyresult*)xrtMalloc(sizeof(*Client->ReadyResult))) == NULL ||
		(Client->Managed && !xredisClientInitBuild(Client, &InitCopy))) { xredisClientRelease(Client); return false; }
	xredisClientRef(Client); /* Active connection, even before Stream is created. */
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
	Client->PlanOwned = Plan != NULL;
#endif
	xrtMutexLock(&Client->Gate); bool Posted = xredisClientWakeLocked(Client); xrtMutexUnlock(&Client->Gate);
	if (!Posted) {
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
		Client->PlanOwned = false;
#endif
		xredisinitwire* Wire = Client->InitWire; Client->InitWire = NULL; xredisInitWireRelease(Wire);
		xredisClientRelease(Client); xredisClientRelease(Client); return false;
	}
	*Result = Client; return true;
}

XREDIS_API bool xredisClientConnect(xnetengine* Engine, const xnetaddr* Remote,
	uint64 Affinity, const xredisclientconfig* Config, xredisclient** Result)
{ return xredisClientCreate(Engine, Remote, Affinity, Config, NULL, xrtTimer(), NULL, Result); }

XREDIS_API bool xredisClientConnectInit(xnetengine* Engine, const xnetaddr* Remote,
	uint64 Affinity, const xredisclientconfig* Config, const xredisclientinitconfig* Init, xredisclient** Result)
{
	double Entered = xrtTimer(); xredisclientinitconfig Defaults;
	if (Init == NULL) { xredisClientInitConfigInit(&Defaults); Init = &Defaults; }
	return xredisClientCreate(Engine, Remote, Affinity, Config, Init, Entered, NULL, Result);
}

#if defined(XREDIS_FEATURE_XREDIS_DIAL)
bool __xredisClientCreatePlanned(xnetengine* Engine, uint64 Affinity,
	const xredisclientconfig* Config, const xredisclientinitconfig* Init,
	double Entered, const xredisconnectplan* Plan, xredisclient** Result)
{ return xredisClientCreate(Engine, NULL, Affinity, Config, Init, Entered, Plan, Result); }
#endif

XREDIS_API xfuture* xredisClientReady(xredisclient* Client)
{
	if (Client == NULL) { xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "client is NULL"); return NULL; }
	return xrtFutureRef(Client->ReadyFuture);
}

static bool xredisClientWord(xbytesview Word, cstr Text)
{
	size_t Size = strlen(Text); if (Word.Size != Size) return false;
	for (size_t Index = 0; Index < Size; ++Index) {
		unsigned char Byte = Word.Data[Index]; if (Byte >= 'a' && Byte <= 'z') Byte -= 'a' - 'A';
		if (Byte != (unsigned char)Text[Index]) return false;
	}
	return true;
}

static bool xredisClientCommandMode(const xbytesview* Args, size_t Count)
{
	const cstr Modes[] = {"SUBSCRIBE", "PSUBSCRIBE", "SSUBSCRIBE", "UNSUBSCRIBE", "PUNSUBSCRIBE",
		"SUNSUBSCRIBE", "MONITOR", "SYNC", "PSYNC"};
	for (size_t Index = 0; Index < sizeof(Modes) / sizeof(Modes[0]); ++Index)
		if (xredisClientWord(Args[0], Modes[Index])) return false;
	return !(Count > 1 && xredisClientWord(Args[0], "CLIENT") && xredisClientWord(Args[1], "REPLY"));
}

static bool xredisClientManagedControl(const xbytesview* Arguments, size_t Count)
{
	return xredisClientWord(Arguments[0], "HELLO") || xredisClientWord(Arguments[0], "AUTH") ||
		xredisClientWord(Arguments[0], "SELECT") || xredisClientWord(Arguments[0], "RESET") ||
		xredisClientWord(Arguments[0], "MULTI") || xredisClientWord(Arguments[0], "EXEC") ||
		xredisClientWord(Arguments[0], "DISCARD") || xredisClientWord(Arguments[0], "WATCH") ||
		xredisClientWord(Arguments[0], "UNWATCH") ||
		(Count > 1 && xredisClientWord(Arguments[0], "CLIENT") && xredisClientWord(Arguments[1], "SETNAME"));
}

/* Gate held. Ordinary calls cannot enter a claimed connection. Owned calls
 * must recheck at publication, since a concurrent commit/abort may seal it
 * while the caller is allocating/encoding outside Gate. */
static xredisadmission xredisClientOwnerAdmission(xredisclient* Client, xredisexclusiveowner* Owner, bool Control)
{
    if (Client->Info.State >= XREDIS_CLIENT_CLOSING) return XREDIS_ADMISSION_CLOSED;
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    if(xrtAtomicPtrLoad(&Client->Subscriber,XMEMORY_ACQUIRE))return XREDIS_ADMISSION_OVERLOADED;
#endif
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
    if (Client->Owner != Owner) return Owner == NULL ? XREDIS_ADMISSION_OVERLOADED : XREDIS_ADMISSION_CLOSED;
    if (Owner != NULL) {
#if defined(XREDIS_FEATURE_XREDIS_POOL)
        if (Owner->Quota != NULL && xrtAtomic32Load(&Owner->Quota->Closing, XMEMORY_ACQUIRE)) return XREDIS_ADMISSION_CLOSED;
#endif
        uint32 State = xrtAtomic32Load(&Owner->State, XMEMORY_ACQUIRE);
        if ((!Control && State != XREDIS_OWNER_ACTIVE) || State == XREDIS_OWNER_CLOSED) return XREDIS_ADMISSION_CLOSED;
    }
#else
    (void)Owner; (void)Control;
#endif
    return XREDIS_ADMISSION_ACCEPTED;
}

#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
static bool xredisClientOwnerSwap(xredisexclusiveowner* Owner, uint32 From, uint32 To)
{ return xrtAtomic32CompareExchange(&Owner->State, &From, To, XMEMORY_ACQ_REL, XMEMORY_ACQUIRE); }
#endif

xredisadmission __xredisClientClaim(xredisclient* Client, xredisexclusiveowner* Owner,
    xnetengine** Engine, xnetworker** Worker, ptr Output)
{
    if (Client == NULL || Owner == NULL || Engine == NULL || Worker == NULL ||
        xrtMemRangesOverlap(Client, Client ? Client->StorageBytes : 0, Output, sizeof(xfuture*))) {
        xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid exclusive owner"); return XREDIS_ADMISSION_ERROR;
    }
    xrtMutexLock(&Client->Gate);
    xredisadmission Admission = Client->Info.State >= XREDIS_CLIENT_CLOSING ? XREDIS_ADMISSION_CLOSED :
        !Client->Managed ? XREDIS_ADMISSION_ERROR :
        Client->Info.State != XREDIS_CLIENT_OPEN || !Client->Info.Ready || Client->Info.Requests || Client->Owner
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
        || xrtAtomicPtrLoad(&Client->Subscriber,XMEMORY_ACQUIRE)
#endif
        ?
        XREDIS_ADMISSION_OVERLOADED : XREDIS_ADMISSION_ACCEPTED;
    if (Admission == XREDIS_ADMISSION_ACCEPTED) {
        if (!xrtNetEnginePin(Client->Engine)) Admission = XREDIS_ADMISSION_ERROR;
        else { Client->Owner = Owner; xredisClientRef(Client); *Engine = Client->Engine; *Worker = Client->Worker; }
    }
    if (Admission != XREDIS_ADMISSION_ACCEPTED) ++Client->Info.Rejected;
    xrtMutexUnlock(&Client->Gate);
    if (Admission == XREDIS_ADMISSION_ERROR) xredisClientReport(XERR_STATE, XREDIS_CLIENT_STATE, "exclusive sessions require a live managed connection");
    return Admission;
}

#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
xredisadmission __xredisClientClaimStarting(xredisclient* Client, xredisexclusiveowner* Owner, xnetengine* Engine)
{
    xrtMutexLock(&Client->Gate);
    xredisadmission Admission = Client->Info.State >= XREDIS_CLIENT_CLOSING ? XREDIS_ADMISSION_CLOSED :
        !Client->Managed || Client->Engine != Engine ? XREDIS_ADMISSION_ERROR :
        Client->Info.Requests || Client->Owner
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
        || xrtAtomicPtrLoad(&Client->Subscriber,XMEMORY_ACQUIRE)
#endif
        ? XREDIS_ADMISSION_OVERLOADED : XREDIS_ADMISSION_ACCEPTED;
    if (Admission == XREDIS_ADMISSION_ACCEPTED) {
        if (!xrtNetEnginePin(Engine)) Admission = XREDIS_ADMISSION_ERROR;
        else { Client->Owner = Owner; xredisClientRef(Client); }
    }
    xrtMutexUnlock(&Client->Gate); return Admission;
}
#endif
static bool xredisClientUnclaim(xredisclient* Client, xredisexclusiveowner* Owner, bool Unused)
{
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
    xredisexclusiveowner* Parent = NULL;
#else
    (void)Unused;
#endif
    xrtMutexLock(&Client->Gate);
    bool Idle = Client->Owner == Owner && Client->Info.Requests == 0;
    if (Idle) {
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
        Parent = Owner->Parent;
        if (Parent != NULL) {
            if (Unused) {
                Parent->Retain(Parent->Data);
                xrtAtomic32Store(&Parent->Nested, 0, XMEMORY_RELEASE);
                if (!xrtAtomic32Load(&Parent->Quota->Closing, XMEMORY_ACQUIRE))
                    (void)xredisClientOwnerSwap(Parent, XREDIS_OWNER_SEALED, XREDIS_OWNER_ACTIVE);
            }
        }
        Client->Owner = Parent;
#else
        Client->Owner = NULL;
#endif
    }
    xrtMutexUnlock(&Client->Gate);
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
    if (Parent != NULL && Unused) { Parent->Wake(Parent->Data); Parent->Release(Parent->Data); }
#endif
    return Idle;
}
bool __xredisClientUnclaim(xredisclient* Client, xredisexclusiveowner* Owner)
{ return xredisClientUnclaim(Client, Owner, false); }

#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
bool __xredisClientUnclaimUnused(xredisclient* Client, xredisexclusiveowner* Owner)
{ return xredisClientUnclaim(Client, Owner, true); }

xredisadmission __xredisClientClaimNested(xredisclient* Client, xredisexclusiveowner* Parent,
    xredisexclusiveowner* Owner, xnetengine** Engine, xnetworker** Worker, ptr Output)
{
    if (xrtMemRangesOverlap(Client, Client->StorageBytes, Output, sizeof(xfuture*))) {
        xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "nested output overlaps client"); return XREDIS_ADMISSION_ERROR;
    }
    xrtMutexLock(&Client->Gate);
    xredisadmission Admission = Client->Info.State >= XREDIS_CLIENT_CLOSING || Client->Owner != Parent ||
        xrtAtomic32Load(&Parent->Quota->Closing, XMEMORY_ACQUIRE) ||
        xrtAtomic32Load(&Parent->State, XMEMORY_ACQUIRE) != XREDIS_OWNER_ACTIVE ? XREDIS_ADMISSION_CLOSED :
        Client->Info.Requests ? XREDIS_ADMISSION_OVERLOADED : XREDIS_ADMISSION_ACCEPTED;
    if (Admission == XREDIS_ADMISSION_ACCEPTED) {
        if (!xredisClientOwnerSwap(Parent, XREDIS_OWNER_ACTIVE, XREDIS_OWNER_SEALED)) Admission = XREDIS_ADMISSION_CLOSED;
        else if (!xrtNetEnginePin(Client->Engine)) {
            (void)xredisClientOwnerSwap(Parent, XREDIS_OWNER_SEALED, XREDIS_OWNER_ACTIVE); Admission = XREDIS_ADMISSION_ERROR;
        } else {
            Owner->Parent = Parent; Owner->Quota = Parent->Quota;
            xrtAtomic32Store(&Parent->Nested, 1, XMEMORY_RELEASE);
            if (Owner->Deadline > Parent->Deadline) Owner->Deadline = Parent->Deadline;
            Client->Owner = Owner; xredisClientRef(Client); *Engine = Client->Engine; *Worker = Client->Worker;
        }
    }
    if (Admission != XREDIS_ADMISSION_ACCEPTED) ++Client->Info.Rejected;
    xrtMutexUnlock(&Client->Gate); return Admission;
}

/* Caller owns the parent/client lifetime. Retain the child under the native
 * Gate, then request abandonment outside it. A temporary zero-request WATCH
 * interval is not permission to return the parent slot. */
bool __xredisClientOwnerReady(xredisclient* Client, xredisexclusiveowner* Parent, bool Abandon)
{
    xredisexclusiveowner* Child = NULL;
    xrtMutexLock(&Client->Gate); bool Ready = Client->Owner == Parent && !xrtAtomic32Load(&Parent->Nested, XMEMORY_ACQUIRE);
    if (!Ready && Abandon && Client->Owner != NULL && Client->Owner->Parent == Parent) {
        Child = Client->Owner; Child->Retain(Child->Data);
    }
    xrtMutexUnlock(&Client->Gate);
    if (Child != NULL) { Child->Abandon(Child->Data); Child->Release(Child->Data); }
    return Ready;
}
void __xredisClientChildDone(xredisexclusiveowner* Owner)
{
    xredisexclusiveowner* Parent = Owner->Parent;
    if (Parent == NULL) return;
    Parent->Retain(Parent->Data);
    xrtAtomic32Store(&Parent->Nested, 0, XMEMORY_RELEASE);
    Parent->Wake(Parent->Data); Parent->Release(Parent->Data);
}
#endif

bool __xredisClientIsWorker(xredisclient* Client)
{
    xrtMutexLock(&Client->Gate);
    bool Worker = !Client->CloseDone && xrtNetEngineCurrent(Client->Engine) != NULL;
    xrtMutexUnlock(&Client->Gate); return Worker;
}
#endif

#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
static xredisadmission xredisClientTransactionAdmit(xredisclient* Client,
	const xredistransactioncommand* Commands, size_t Count, const xredistransactionconfig* Config,
	int64 Timeout, double Entered, xfuture** Future, xredisexclusiveowner* Owner)
{
	xredistransactionconfig Options;
	if (Client == NULL || Future == NULL || !xrtMemRangeValid(Future, sizeof(*Future)) || Timeout < XRT_WAIT_FOREVER ||
		xrtMemRangesOverlap(Client, Client == NULL ? 0 : Client->StorageBytes, Future, sizeof(*Future)) ||
		(Config != NULL && (!xrtMemRangeValid(Config, sizeof(*Config)) || xrtMemRangesOverlap(Config, sizeof(*Config), Future, sizeof(*Future))))) {
		xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid transaction arguments/output"); return XREDIS_ADMISSION_ERROR;
	}
	if (Config == NULL) xredisClientTransactionConfigInit(&Options); else Options = *Config;
	if (Options.Size != sizeof(Options) || Options.MaxCommands == 0 || Options.MaxCommands > XREDIS_TRANSACTION_COMMANDS_MAX ||
		Count > Options.MaxCommands || (Count != 0 && Commands == NULL) || !xrtMemRangeValid(Commands, Count * sizeof(*Commands)) ||
		xrtMemRangesOverlap(Commands, Count * sizeof(*Commands), Future, sizeof(*Future))) {
		xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid or excessive transaction descriptors"); return XREDIS_ADMISSION_ERROR;
	}
	if (!Client->Managed) {
		xredisClientReport(XERR_STATE, XREDIS_CLIENT_STATE, "transactions require a managed connection"); return XREDIS_ADMISSION_ERROR;
	}
	size_t CommandsBytes = 0;
	for (size_t Index = 0; Index < Count; ++Index) {
		const xredistransactioncommand* Command = &Commands[Index]; size_t Bytes = 0;
		if (!xredisCommandSize(Command->Arguments, Command->Count, &Bytes)) return XREDIS_ADMISSION_ERROR;
		if (xrtMemRangesOverlap(Command->Arguments, Command->Count * sizeof(*Command->Arguments), Future, sizeof(*Future))) {
			xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "transaction output overlaps arguments"); return XREDIS_ADMISSION_ERROR;
		}
		for (size_t Argument = 0; Argument < Command->Count; ++Argument)
			if (xrtMemRangesOverlap(Command->Arguments[Argument].Data, Command->Arguments[Argument].Size, Future, sizeof(*Future))) {
				xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "transaction output overlaps payload"); return XREDIS_ADMISSION_ERROR;
			}
		if (!xredisClientCommandMode(Command->Arguments, Command->Count) || xredisClientManagedControl(Command->Arguments, Command->Count) ||
			xredisClientWord(Command->Arguments[0], "QUIT") || xredisClientWord(Command->Arguments[0], "READONLY") ||
			xredisClientWord(Command->Arguments[0], "READWRITE") || xredisClientWord(Command->Arguments[0], "ASKING") ||
			(Command->Count > 1 && xredisClientWord(Command->Arguments[0], "CLIENT") &&
			 (xredisClientWord(Command->Arguments[1], "TRACKING") || xredisClientWord(Command->Arguments[1], "CACHING")))) {
			xredisClientReport(XERR_UNSUPPORTED, XREDIS_CLIENT_COMMAND_MODE, "transaction payload changes connection/control mode"); return XREDIS_ADMISSION_ERROR;
		}
		if (Bytes > SIZE_MAX - CommandsBytes) {
			xredisClientReport(XERR_RANGE, XREDIS_CLIENT_ARGUMENT, "transaction wire size overflow"); return XREDIS_ADMISSION_ERROR;
		}
		CommandsBytes += Bytes;
	}
	size_t ControlBytes = sizeof(xredisTransactionMulti) + sizeof(xredisTransactionExec) + sizeof(xredisTransactionDiscard) - 3u;
	if (CommandsBytes > SIZE_MAX - ControlBytes - sizeof(xredisrequest) - sizeof(xredistransactionstate) - _Alignof(xredistransactionstate)) {
		xredisClientReport(XERR_RANGE, XREDIS_CLIENT_ARGUMENT, "transaction allocation size overflow"); return XREDIS_ADMISSION_ERROR;
	}
	size_t Bytes = CommandsBytes + ControlBytes;
	size_t Padding = (_Alignof(xredistransactionstate) - (offsetof(xredisrequest, Wire) + Bytes) % _Alignof(xredistransactionstate)) % _Alignof(xredistransactionstate);
	size_t MetadataBytes = Padding + sizeof(xredistransactionstate), Charged = Bytes + MetadataBytes;
	xrtMutexLock(&Client->Gate);
	xredisadmission Admission = xredisClientOwnerAdmission(Client, Owner, false);
	if (Admission == XREDIS_ADMISSION_ACCEPTED) Admission = Client->Info.Requests >= Client->Config.MaxRequests || Charged > Client->Config.MaxCommandBytes ||
		Charged > Client->Config.MaxCommandBytesTotal - Client->Info.CommandBytes ? XREDIS_ADMISSION_OVERLOADED : XREDIS_ADMISSION_ACCEPTED;
#if defined(XREDIS_FEATURE_XREDIS_POOL)
    if (Admission == XREDIS_ADMISSION_ACCEPTED && !xredisClientQuotaReserve(Owner, Charged)) Admission = XREDIS_ADMISSION_OVERLOADED;
#endif
	if (Admission != XREDIS_ADMISSION_ACCEPTED) { ++Client->Info.Rejected; xrtMutexUnlock(&Client->Gate); return Admission; }
	++Client->Info.Requests; Client->Info.CommandBytes += Charged; xredisClientRef(Client); xrtMutexUnlock(&Client->Gate);
	xredisrequest* Request = (xredisrequest*)xrtMalloc(sizeof(*Request) + Charged);
	if (Request == NULL) {
		xredisClientReservationDrop(Client, Charged); return XREDIS_ADMISSION_ERROR;
	}
	memset(Request, 0, offsetof(xredisrequest, Wire));
	xrtAtomic32Init(&Request->References, 1); xrtAtomic32Init(&Request->Cancelled, 0);
	Request->Client = Client; Request->WireBytes = Bytes; Request->Deadline = xredisClientDeadline(Entered, Timeout);
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
    if (Owner != NULL && Request->Deadline > Owner->Deadline) Request->Deadline = Owner->Deadline;
#endif
	Request->Transaction = true;
	xredistransactionstate* State = xredisTransactionState(Request);
	*State = (xredistransactionstate){0}; State->Count = Count;
	State->CommandsBytes = CommandsBytes; State->MetadataBytes = MetadataBytes;
	Request->Result = (xredisrequestresult*)xrtMalloc(sizeof(xredistransactionresult));
	xfuture* Observer = NULL;
	if (Request->Result != NULL) Request->Promise = xrtPromiseCreate(&Observer, NULL);
	if (Request->Promise != NULL) {
		xcancel* Token = xrtFutureCancelToken(Observer);
		if (Token != NULL) Request->Watch = xrtCancelWatch(Token, xredisClientCancel, Request);
		xrtCancelDestroy(Token);
	}
	bool Encoded = Request->Watch != NULL; size_t Offset = sizeof(xredisTransactionMulti) - 1u;
	if (Encoded) memcpy(Request->Wire, xredisTransactionMulti, Offset);
	for (size_t Index = 0; Encoded && Index < Count; ++Index) {
		size_t Written = 0;
		Encoded = xredisCommandWrite(Request->Wire + Offset, CommandsBytes - (Offset - (sizeof(xredisTransactionMulti) - 1u)),
			Commands[Index].Arguments, Commands[Index].Count, &Written);
		Offset += Written;
	}
	if (Encoded) {
		memcpy(Request->Wire + Offset, xredisTransactionExec, sizeof(xredisTransactionExec) - 1u); Offset += sizeof(xredisTransactionExec) - 1u;
		memcpy(Request->Wire + Offset, xredisTransactionDiscard, sizeof(xredisTransactionDiscard) - 1u);
		xrtMutexLock(&Client->Gate);
        Admission = xredisClientOwnerAdmission(Client, Owner, false);
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
        if (Admission == XREDIS_ADMISSION_ACCEPTED && Owner != NULL &&
            !xredisClientOwnerSwap(Owner, XREDIS_OWNER_ACTIVE, XREDIS_OWNER_SEALED)) Admission = XREDIS_ADMISSION_CLOSED;
#endif
		if (Admission == XREDIS_ADMISSION_ACCEPTED) {
			if (Client->SubmitTail != NULL) Client->SubmitTail->SubmitNext = Request; else Client->SubmitHead = Request;
			Client->SubmitTail = Request;
			if (xredisClientWakeLocked(Client)) {
				++Client->Info.Accepted; *Future = Observer; xrtMutexUnlock(&Client->Gate); return XREDIS_ADMISSION_ACCEPTED;
			}
			xredisrequest* Previous = NULL;
			for (xredisrequest* Item = Client->SubmitHead; Item != Request; Item = Item->SubmitNext) Previous = Item;
			if (Previous != NULL) Previous->SubmitNext = NULL; else Client->SubmitHead = NULL;
			Client->SubmitTail = Previous;
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
            if (Owner != NULL) (void)xredisClientOwnerSwap(Owner, XREDIS_OWNER_SEALED, XREDIS_OWNER_ACTIVE);
#endif
		} else ++Client->Info.Rejected;
		xrtMutexUnlock(&Client->Gate);
	}
	xrtCancelUnwatch(Request->Watch); xrtPromiseDestroy(Request->Promise); xrtFutureDestroy(Observer);
	xrtFree(Request->Result); xredisRequestRelease(Request);
	return Admission == XREDIS_ADMISSION_ACCEPTED ? XREDIS_ADMISSION_ERROR : Admission;
}

xredisadmission __xredisClientTransactionAsync(xredisclient* Client,
    const xredistransactioncommand* Commands, size_t Count, const xredistransactionconfig* Config,
    int64 Timeout, double Entered, xfuture** Future)
{ return xredisClientTransactionAdmit(Client, Commands, Count, Config, Timeout, Entered, Future, NULL); }
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
xredisadmission __xredisClientOwnedTransaction(xredisclient* Client, xredisexclusiveowner* Owner,
    const xredistransactioncommand* Commands, size_t Count, const xredistransactionconfig* Config,
    int64 Timeout, double Entered, xfuture** Future)
{ return xredisClientTransactionAdmit(Client, Commands, Count, Config, Timeout, Entered, Future, Owner); }
#endif

xredisadmission __xredisClientTransaction(xredisclient* Client,
	const xredistransactioncommand* Commands, size_t Count, const xredistransactionconfig* Config,
	int64 Timeout, double Entered, xfuture** Future)
{
	if (Client == NULL) {
		xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "client is NULL"); return XREDIS_ADMISSION_ERROR;
	}
	xrtMutexLock(&Client->Gate); bool Worker = !Client->CloseDone && xrtNetEngineCurrent(Client->Engine) != NULL; xrtMutexUnlock(&Client->Gate);
	if (Worker) { xredisClientReport(XERR_STATE, XREDIS_CLIENT_STATE, "sync transaction cannot block an engine worker"); return XREDIS_ADMISSION_ERROR; }
	xredisadmission Admission = __xredisClientTransactionAsync(Client, Commands, Count, Config, Timeout, Entered, Future);
	if (Admission == XREDIS_ADMISSION_ACCEPTED) (void)xrtFutureWait(*Future);
	return Admission;
}
#endif

static xredisadmission xredisClientCallAdmit(xredisclient* Client,
	const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future,
    double Entered, ptr Scope, XREDIS_CALL_MODE_PARAMETER XREDIS_OBSERVE_PARAMETER)
{
#if defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
    bool Asking=CallMode==2,Control=CallMode==1;
#endif
	size_t Bytes = 0;
	if (Client == NULL || Future == NULL || !xrtMemRangeValid(Future, sizeof(*Future)) || Timeout < XRT_WAIT_FOREVER ||
		xrtMemRangesOverlap(Client, Client == NULL ? 0 : Client->StorageBytes, Future, sizeof(*Future))) {
		xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid client call arguments"); return XREDIS_ADMISSION_ERROR;
	}
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    if(Observe && Observe<5 && !xredisTrackingOutput(Client,Future,sizeof(*Future))){
        xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"tracking output overlaps owned storage");return XREDIS_ADMISSION_ERROR;
    }
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    bool Subscription=Observe>=5;
    bool Measured=Subscription?xredisPubSubCommandSize((xredispubsubcommand)(Observe-5),Arguments,Count,&Bytes):xredisCommandSize(Arguments,Count,&Bytes);
#else
    bool Measured=xredisCommandSize(Arguments,Count,&Bytes);
#endif
    if(!Measured)return XREDIS_ADMISSION_ERROR;
    xredisexclusiveowner* Owner=Scope;
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    if(Subscription)Owner=NULL;
#endif
	if (xrtMemRangesOverlap(Arguments, Count * sizeof(*Arguments), Future, sizeof(*Future))) {
		xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "call output overlaps arguments"); return XREDIS_ADMISSION_ERROR;
	}
	for (size_t Index = 0; Index < Count; ++Index) if (xrtMemRangesOverlap(Arguments[Index].Data, Arguments[Index].Size, Future, sizeof(*Future))) {
		xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "call output overlaps argument payload"); return XREDIS_ADMISSION_ERROR;
	}
	if (
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
        !Subscription &&
#endif
        !xredisClientCommandMode(Arguments, Count)) {
		xredisClientReport(XERR_UNSUPPORTED, XREDIS_CLIENT_COMMAND_MODE, "command changes response cardinality/mode"); return XREDIS_ADMISSION_ERROR;
	}
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
    if (Owner != NULL && !Control && (xredisClientWord(Arguments[0], "QUIT") ||
        xredisClientWord(Arguments[0], "READONLY") || xredisClientWord(Arguments[0], "READWRITE") ||
        xredisClientWord(Arguments[0], "ASKING") ||
        (Count > 1 && xredisClientWord(Arguments[0], "CLIENT") &&
         (xredisClientWord(Arguments[1], "TRACKING") || xredisClientWord(Arguments[1], "CACHING"))))) {
        xredisClientReport(XERR_UNSUPPORTED, XREDIS_CLIENT_COMMAND_MODE, "exclusive payload changes connection/control mode"); return XREDIS_ADMISSION_ERROR;
    }
#endif
	if (!Control && Client->Managed && xredisClientManagedControl(Arguments, Count)) {
		xredisClientReport(XERR_UNSUPPORTED, XREDIS_CLIENT_COMMAND_MODE, "managed initialization/transaction controls require their dedicated API");
		return XREDIS_ADMISSION_ERROR;
	}
	size_t Charged=Bytes;
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    size_t SubscriberMetadata=0;
    if(Subscription){size_t A=_Alignof(xredissubcall),Padding=(A-(offsetof(xredisrequest,Wire)+Bytes)%A)%A;
        SubscriberMetadata=Padding+sizeof(xredissubcall)+(Observe<=7?Count*sizeof(xredissubentry*):0);
        if(Bytes>SIZE_MAX-sizeof(xredisrequest)-SubscriberMetadata){xredisClientReport(XERR_RANGE,XREDIS_CLIENT_ARGUMENT,"subscriber request size overflow");return XREDIS_ADMISSION_ERROR;}
        Charged+=SubscriberMetadata;
    }
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    size_t PrefixBytes=0,MetadataBytes=0,QueryBytes=Bytes;
    if((Observe==3 || Observe==4)){
        if(xredisClientWord(Arguments[0],"CLIENT") || xredisClientWord(Arguments[0],"QUIT") ||
           xredisClientWord(Arguments[0],"READONLY") || xredisClientWord(Arguments[0],"READWRITE") ||
           xredisClientWord(Arguments[0],"ASKING")){
            xredisClientReport(XERR_UNSUPPORTED,XREDIS_CLIENT_COMMAND_MODE,"caching query changes connection/control mode");return XREDIS_ADMISSION_ERROR;
        }
        PrefixBytes=Observe==3?sizeof(xredisCachingYes)-1:sizeof(xredisCachingNo)-1;
        if(Bytes>SIZE_MAX-sizeof(xredisrequest)-sizeof(xrediscachingstate)-_Alignof(xrediscachingstate)-PrefixBytes){
            xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"caching group size overflow");return XREDIS_ADMISSION_ERROR;
        }
        Bytes+=PrefixBytes;
        size_t A=_Alignof(xrediscachingstate),Padding=(A-(offsetof(xredisrequest,Wire)+Bytes)%A)%A;
        MetadataBytes=Padding+sizeof(xrediscachingstate);Charged=Bytes+MetadataBytes;
    }
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
    size_t AskMetadata=0,AskQueryBytes=Bytes;
    if(Asking){
        if(!Owner || xredisClientWord(Arguments[0],"CLIENT")){
            xredisClientReport(XERR_UNSUPPORTED,XREDIS_CLIENT_COMMAND_MODE,"ASKING query changes connection/control mode");return XREDIS_ADMISSION_ERROR;}
        size_t Extra=sizeof(xredisaskingstate)+_Alignof(xredisaskingstate)+sizeof(xredisAskingWire)-1;
        if(Bytes>SIZE_MAX-sizeof(xredisrequest)-Extra){xredisClientReport(XERR_RANGE,XREDIS_CLIENT_ARGUMENT,"ASKING group size overflow");return XREDIS_ADMISSION_ERROR;}
        Bytes+=sizeof(xredisAskingWire)-1;size_t A=_Alignof(xredisaskingstate),P=(A-(offsetof(xredisrequest,Wire)+Bytes)%A)%A;
        AskMetadata=P+sizeof(xredisaskingstate);Charged=Bytes+AskMetadata;
    }
#endif
	xrtMutexLock(&Client->Gate);
	xredisadmission Admission =
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
        Subscription ? (Client->Info.State>=XREDIS_CLIENT_CLOSING?XREDIS_ADMISSION_CLOSED:
            xrtAtomicPtrLoad(&Client->Subscriber,XMEMORY_ACQUIRE)?
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS)
            (((xredissubscriber*)xrtAtomicPtrLoad(&Client->Subscriber,XMEMORY_RELAXED))->ReplayOwner==Scope?XREDIS_ADMISSION_ACCEPTED:XREDIS_ADMISSION_OVERLOADED):
#else
            XREDIS_ADMISSION_ACCEPTED:
#endif
            XREDIS_ADMISSION_ERROR) :
#endif
        xredisClientOwnerAdmission(Client, Owner, Control);
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    if(Subscription && Admission==XREDIS_ADMISSION_ERROR){xrtMutexUnlock(&Client->Gate);xredisClientReport(XERR_STATE,XREDIS_CLIENT_STATE,"subscriber mode has not been adopted");return XREDIS_ADMISSION_ERROR;}
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    if(Observe && Observe<5 && Admission==XREDIS_ADMISSION_ACCEPTED && !xrtAtomicPtrLoad(&Client->Observation,XMEMORY_ACQUIRE)){
        xrtMutexUnlock(&Client->Gate);xredisClientReport(XERR_STATE,XREDIS_CLIENT_STATE,"tracking observation is not enabled");return XREDIS_ADMISSION_ERROR;
    }
    if((Observe==3 || Observe==4) && Admission==XREDIS_ADMISSION_ACCEPTED && !Client->Managed){
        xrtMutexUnlock(&Client->Gate);xredisClientReport(XERR_STATE,XREDIS_CLIENT_STATE,"caching groups require a managed client");return XREDIS_ADMISSION_ERROR;
    }
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
    if(Asking && Admission==XREDIS_ADMISSION_ACCEPTED && !Client->Managed){xrtMutexUnlock(&Client->Gate);xredisClientReport(XERR_STATE,XREDIS_CLIENT_STATE,"ASKING requires managed pooled readiness");return XREDIS_ADMISSION_ERROR;}
#endif
	if (Admission == XREDIS_ADMISSION_ACCEPTED) Admission = Client->Info.Requests >= Client->Config.MaxRequests || Charged > Client->Config.MaxCommandBytes ||
		Charged > Client->Config.MaxCommandBytesTotal - Client->Info.CommandBytes ? XREDIS_ADMISSION_OVERLOADED : XREDIS_ADMISSION_ACCEPTED;
#if defined(XREDIS_FEATURE_XREDIS_POOL)
    if (Admission == XREDIS_ADMISSION_ACCEPTED && !xredisClientQuotaReserve(Owner, Charged)) Admission = XREDIS_ADMISSION_OVERLOADED;
#endif
	if (Admission != XREDIS_ADMISSION_ACCEPTED) { ++Client->Info.Rejected; xrtMutexUnlock(&Client->Gate); return Admission; }
	++Client->Info.Requests; Client->Info.CommandBytes += Charged; xredisClientRef(Client); xrtMutexUnlock(&Client->Gate);
	xredisrequest* Request = (xredisrequest*)xrtMalloc(sizeof(*Request) + Charged);
	if (Request == NULL) {
		xredisClientReservationDrop(Client, Charged); return XREDIS_ADMISSION_ERROR;
	}
	/* Initialize metadata only: opaque wire bytes are written by the encoder,
	 * without a redundant full-payload zero pass. */
	memset(Request, 0, offsetof(xredisrequest, Wire));
	xrtAtomic32Init(&Request->References, 1); xrtAtomic32Init(&Request->Cancelled, 0);
	Request->Client = Client; Request->WireBytes = Bytes; Request->Deadline = xredisClientDeadline(Entered, Timeout);
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
    if (Owner != NULL && (!Control || xredisClientWord(Arguments[0], "WATCH")) && Request->Deadline > Owner->Deadline) Request->Deadline = Owner->Deadline;
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    Request->Sensitive=!Subscription && (xredisClientWord(Arguments[0],"AUTH") || xredisClientWord(Arguments[0],"HELLO"));
#else
    Request->Sensitive=xredisClientWord(Arguments[0],"AUTH") || xredisClientWord(Arguments[0],"HELLO");
#endif
    size_t ResultBytes=sizeof(*Request->Result);
#if defined(XREDIS_FEATURE_XREDIS_TRACKING) || defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    Request->Observe=(uint8)Observe;
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    if(Observe==3 || Observe==4){xrediscachingstate* S=xredisCachingState(Request);memset(S,0,sizeof(*S));S->MetadataBytes=MetadataBytes;ResultBytes=sizeof(xrediscachingresult);}
    else if(Observe==1 || Observe==2)ResultBytes=sizeof(xredistrackingresult);
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    if(Subscription){
        xredissubcall* S=xredisSubscriberState(Request);memset(S,0,SubscriberMetadata-(size_t)((unsigned char*)S-(Request->Wire+Bytes)));
        S->MetadataBytes=SubscriberMetadata;S->Count=Count;S->Command=(xredispubsubcommand)(Observe-5);ResultBytes=sizeof(xredissubscriberresult);
    }
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
    if(Asking){Request->Sensitive|=128u;xredisaskingstate* S=xredisAskingState(Request);memset(S,0,sizeof(*S));S->MetadataBytes=AskMetadata;ResultBytes=sizeof(xredisaskingresult);}
#endif
    Request->Result=xrtMalloc(ResultBytes);
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    if(Observe && Observe<5 && Request->Result)memset(Request->Result,0,ResultBytes);
#endif
    xfuture* Observer = NULL;
	if (Request->Result != NULL) Request->Promise = xrtPromiseCreate(&Observer, NULL);
	if (Request->Promise != NULL) {
		xcancel* Token = xrtFutureCancelToken(Observer);
		if (Token != NULL) Request->Watch = xrtCancelWatch(Token, xredisClientCancel, Request);
		xrtCancelDestroy(Token); /* FutureCancelToken returns a retained reference. */
	}
    bool Encoded=false;
    if(Request->Watch!=NULL){
#if defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
        if(Asking){memcpy(Request->Wire,xredisAskingWire,sizeof(xredisAskingWire)-1);Encoded=xredisCommandWrite(Request->Wire+sizeof(xredisAskingWire)-1,AskQueryBytes,Arguments,Count,NULL);}else
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
        if(PrefixBytes){
            memcpy(Request->Wire,Observe==3?xredisCachingYes:xredisCachingNo,PrefixBytes);
            Encoded=xredisCommandWrite(Request->Wire+PrefixBytes,QueryBytes,Arguments,Count,NULL);
        }else
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
        if(Subscription)Encoded=xredisPubSubCommandWrite(Request->Wire,Bytes,(xredispubsubcommand)(Observe-5),Arguments,Count,NULL);else
#endif
        Encoded=xredisCommandWrite(Request->Wire,Bytes,Arguments,Count,NULL);
    }
	if (Encoded) {
		xrtMutexLock(&Client->Gate);
        Admission =
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
        Subscription ? (Client->Info.State>=XREDIS_CLIENT_CLOSING?XREDIS_ADMISSION_CLOSED:
            xrtAtomicPtrLoad(&Client->Subscriber,XMEMORY_ACQUIRE)?
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS)
            (((xredissubscriber*)xrtAtomicPtrLoad(&Client->Subscriber,XMEMORY_RELAXED))->ReplayOwner==Scope?XREDIS_ADMISSION_ACCEPTED:XREDIS_ADMISSION_OVERLOADED):
#else
            XREDIS_ADMISSION_ACCEPTED:
#endif
            XREDIS_ADMISSION_ERROR) :
#endif
        xredisClientOwnerAdmission(Client, Owner, Control);
		if (Admission == XREDIS_ADMISSION_ACCEPTED) {
			Request->SubmitNext = NULL;
			if (Client->SubmitTail != NULL) Client->SubmitTail->SubmitNext = Request; else Client->SubmitHead = Request;
			Client->SubmitTail = Request;
			/* A live pinned worker accepts this embedded post without allocation. */
			if (xredisClientWakeLocked(Client)) {
				++Client->Info.Accepted; *Future = Observer; xrtMutexUnlock(&Client->Gate); return XREDIS_ADMISSION_ACCEPTED;
			}
			/* The failed wake accepted no callback. Remove this exact tail. */
			xredisrequest* Previous = NULL;
			for (xredisrequest* Item = Client->SubmitHead; Item != Request; Item = Item->SubmitNext) Previous = Item;
			if (Previous != NULL) Previous->SubmitNext = NULL; else Client->SubmitHead = NULL;
			Client->SubmitTail = Previous;
		} else ++Client->Info.Rejected;
		xrtMutexUnlock(&Client->Gate);
	}
	xrtCancelUnwatch(Request->Watch); xrtPromiseDestroy(Request->Promise); xrtFutureDestroy(Observer);
	xrtFree(Request->Result); xredisRequestRelease(Request);
	return Admission == XREDIS_ADMISSION_ACCEPTED ? XREDIS_ADMISSION_ERROR : Admission;
}

#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
/* Registry mutation belongs to the IO worker. Info is a Gate-protected snapshot;
 * the installed mode/bucket allocation lives through the last Client reference. */
static void xredisSubscriberPublish(xredisclient* C,xredissubscriber* M)
{
    xrtMutexLock(&C->Gate);M->Info.Entries=M->Entries;M->Info.NameBytes=M->NameBytes;
    M->Info.RegistryBytes=M->StorageBytes+M->EntryBytes;
    M->Info.Subscriptions=M->Counts[0]+M->Counts[1];M->Info.ShardSubscriptions=M->Counts[2];xrtMutexUnlock(&C->Gate);
}
static xredissubentry* xredisSubscriberFind(xredissubscriber* M,unsigned Family,xbytesview Name,uint64 Hash)
{
    for(xredissubentry* E=M->Table[Hash&(M->Buckets-1)];E;E=E->BucketNext)
        if(E->Hash==Hash && E->Family==Family && E->Bytes==Name.Size && (!Name.Size || !memcmp(E->Name,Name.Data,Name.Size)))return E;
    return NULL;
}
static void xredisSubscriberDelete(xredissubscriber* M,xredissubentry* E,bool Closing)
{
    if(!Closing){xredissubentry** Link=&M->Table[E->Hash&(M->Buckets-1)];while(*Link!=E)Link=&(*Link)->BucketNext;*Link=E->BucketNext;}
    if(E->Previous)E->Previous->Next=E->Next;else M->Head=E->Next;
    if(E->Next)E->Next->Previous=E->Previous;
    --M->Entries;M->NameBytes-=E->Bytes;M->EntryBytes-=sizeof(*E)+E->Bytes;xrtFree(E);
}
static bool xredisSubscriberName(xredisrequest* R,size_t* Cursor,xbytesview* Name)
{
    /* Only traverses bytes produced by our own bounded RESP encoder. */
    size_t N=*Cursor,Bytes=0;if(N>=R->WireBytes || R->Wire[N++]!='$')return false;
    while(N<R->WireBytes && R->Wire[N]>='0' && R->Wire[N]<='9')Bytes=Bytes*10+(R->Wire[N++]-'0');
    if(N+2>R->WireBytes || R->Wire[N]!='\r' || R->Wire[N+1]!='\n')return false;
    N+=2;
    if(Bytes>R->WireBytes-N || R->WireBytes-N-Bytes<2)return false;
    *Name=(xbytesview){R->Wire+N,Bytes};*Cursor=N+Bytes+2;return true;
}
static size_t xredisSubscriberFirstName(xredisrequest* R)
{
    size_t N=0;while(N<R->WireBytes && R->Wire[N]!='\n')++N;++N;
    xbytesview Word;if(!xredisSubscriberName(R,&N,&Word))return R->WireBytes;return N;
}
static void xredisSubscriberRetire(xredisrequest* R)
{
    xredissubcall* S=xredisSubscriberState(R);xredissubscriber* M=xrtAtomicPtrLoad(&R->Client->Subscriber,XMEMORY_ACQUIRE);
    for(size_t N=0;N<S->Reserved;++N){xredissubentry* E=S->Entries[N];--E->Reservations;if(!E->Reservations && !E->Confirmed)xredisSubscriberDelete(M,E,false);}
    S->Reserved=0;if(M)xredisSubscriberPublish(R->Client,M);
}
static bool xredisSubscriberCloseStep(xredisclient* C)
{
    xredissubscriber* M=xrtAtomicPtrLoad(&C->Subscriber,XMEMORY_ACQUIRE);if(!M)return true;
    xrtMutexLock(&C->Gate);bool Idle=!C->Info.Requests;xrtMutexUnlock(&C->Gate);if(!Idle)return false;
    for(size_t N=0;N<C->Config.WorkRequests && M->Head;++N){xredissubentry* E=M->Head;if(E->Confirmed)--M->Counts[E->Family];xredisSubscriberDelete(M,E,true);}
    xredisSubscriberPublish(C,M);return !M->Head;
}
static void xredisSubscriberRelease(xredisclient* C)
{
    xredissubscriber* M=xrtAtomicPtrLoad(&C->Subscriber,XMEMORY_RELAXED);if(!M)return;
    while(M->Head)xredisSubscriberDelete(M,M->Head,true);
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY)
    xrtPromiseDestroy(M->ReadablePromise);xrtFutureDestroy(M->ReadableFuture);
#endif
    xrtFree(M);
}
static bool xredisSubscriberPrepare(xredisrequest* R)
{
    if(R->Observe<5)return true;
    xredissubcall* S=xredisSubscriberState(R);if(S->Prepared)return true;
    xredisclient* C=R->Client;xredissubscriber* M=xrtAtomicPtrLoad(&C->Subscriber,XMEMORY_ACQUIRE);
    size_t Cursor=xredisSubscriberFirstName(R);S->Cursor=Cursor;S->Expected=S->Count?S->Count:1;
    xredisrequeststatus Status=XREDIS_REQUEST_OK;
    if(S->Command<=XREDIS_PUBSUB_CMD_SSUBSCRIBE)for(size_t N=0;N<S->Count;++N){
        xbytesview Name;if(!xredisSubscriberName(R,&Cursor,&Name)){Status=XREDIS_REQUEST_PROTOCOL;break;}
        uint64 Hash=xrtHash64(Name.Data,Name.Size);xredissubentry* E=xredisSubscriberFind(M,(unsigned)S->Command,Name,Hash);
        if(!E){
            if(M->Entries==M->Config.MaxSubscriptions || Name.Size>M->Config.MaxNameBytes-M->NameBytes){Status=XREDIS_REQUEST_LIMIT;break;}
            E=xrtMalloc(sizeof(*E)+Name.Size);if(!E){Status=XREDIS_REQUEST_MEMORY;break;}
            *E=(xredissubentry){0};E->Hash=Hash;E->Bytes=Name.Size;E->Family=(unsigned)S->Command;
            if(Name.Size)memcpy(E->Name,Name.Data,Name.Size);
            size_t Bucket=Hash&(M->Buckets-1);E->BucketNext=M->Table[Bucket];M->Table[Bucket]=E;
            E->Next=M->Head;if(M->Head)M->Head->Previous=E;M->Head=E;
            ++M->Entries;M->NameBytes+=Name.Size;M->EntryBytes+=sizeof(*E)+Name.Size;
        }
        ++E->Reservations;S->Entries[S->Reserved++]=E;
    }
    xredisSubscriberPublish(C,M);
    if(Status!=XREDIS_REQUEST_OK){
        xredisClientComplete(R,Status,XREDIS_NOT_SENT,NULL,Status==XREDIS_REQUEST_MEMORY?xrtGetError():NULL);
        xredisClientUnlink(R);R->Phase=XREDIS_R_DONE;xredisSubscriberRetire(R);xredisClientCancelTimer(R);xredisRequestRelease(R);return false;
    }
    S->Prepared=true;
    if(xrtAtomic32Load(&R->Cancelled,XMEMORY_ACQUIRE) || xrtTimer()>=R->Deadline){
        xredisClientAbandon(R,xrtAtomic32Load(&R->Cancelled,XMEMORY_ACQUIRE)?XREDIS_REQUEST_CANCELLED:XREDIS_REQUEST_TIMEOUT);return false;
    }
    return true;
}
static void xredisSubscriberFinish(xredisrequest* R)
{
    xredissubcall* S=xredisSubscriberState(R);if(!S->Finished || !S->Written)return;
    xredisclient* C=R->Client;xredisreply* Reply=S->Final;S->Final=NULL;
    if(R->Promise){
        xredisrequeststatus Status=S->Status;
        if(xrtAtomic32Load(&R->Cancelled,XMEMORY_ACQUIRE))Status=XREDIS_REQUEST_CANCELLED;
        else if(xrtTimer()>=R->Deadline)Status=XREDIS_REQUEST_TIMEOUT;
        if(Status==XREDIS_REQUEST_CANCELLED || Status==XREDIS_REQUEST_TIMEOUT){xredisReplyRelease(Reply);Reply=NULL;}
        xredisClientComplete(R,Status,XREDIS_REPLIED,Reply,NULL);
    }else{xredisReplyRelease(Reply);xrtMutexLock(&C->Gate);++C->Info.DiscardedReplies;xrtMutexUnlock(&C->Gate);}
    xredisClientUnlink(R);R->Phase=XREDIS_R_DONE;xredisSubscriberRetire(R);
    xredisClientCancelTimer(R);xredisRequestRelease(R);xredisClientWake(C);
}
static void xredisSubscriberWritten(xredisrequest* R)
{if(R->Observe>=5){xredisSubscriberState(R)->Written=true;xredisSubscriberFinish(R);}}
static bool xredisSubscriberQueue(xredisclient* C,xredissubscriber* M,xredisreply* Reply,size_t Bytes,bool Message)
{
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY)
    xpromise* Signal=NULL;
#endif
    xrtMutexLock(&C->Gate);bool Fits=C->Info.Pushes<C->Config.MaxPushes && Bytes<=C->Config.MaxPushBytes-C->Info.PushBytes;
    if(Fits){size_t N=(C->PushHead+C->Info.Pushes)%C->Config.MaxPushes;C->PushQueue[N]=Reply;++C->Info.Pushes;C->Info.PushBytes+=Bytes;
        if(Message)++M->Info.Messages;else ++M->Info.Sidebands;
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY)
        Signal=M->ReadablePromise;if(Signal)M->ReadablePromise=NULL;
#endif
    }
    xrtMutexUnlock(&C->Gate);
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY)
    xredisSubscriberNotify(Signal);
#endif
    return Fits;
}
static void xredisSubscriberFrame(xredisclient* C,xredisreply* Reply,const xredisreplyvalue* Root,size_t Bytes)
{
    xredissubscriber* M=xrtAtomicPtrLoad(&C->Subscriber,XMEMORY_ACQUIRE);xredispubsubevent E;
    xredispubsubstatus Decoded=xredisPubSubEventRead(Reply,&E);
    if(Decoded==XREDIS_PUBSUB_EVENT && E.Kind<=XREDIS_PUBSUB_SMESSAGE){
        unsigned Family=E.Kind==XREDIS_PUBSUB_PMESSAGE?1:E.Kind==XREDIS_PUBSUB_SMESSAGE?2:0;
        xbytesview Name=Family==1?E.Pattern:E.Channel;
        xredissubentry* Entry=xredisSubscriberFind(M,Family,Name,xrtHash64(Name.Data,Name.Size));
        if(!Entry || !Entry->Confirmed)goto Protocol;
        if(!xredisSubscriberQueue(C,M,Reply,Bytes,true)){xredisReplyRelease(Reply);xredisClientFail(C,XREDIS_REQUEST_LIMIT,NULL);}return;
    }
    if(Decoded==XREDIS_PUBSUB_OTHER && Root->Type==XREDIS_REPLY_PUSH){
        if(!xredisSubscriberQueue(C,M,Reply,Bytes,false)){xredisReplyRelease(Reply);xredisClientFail(C,XREDIS_REQUEST_LIMIT,NULL);}return;
    }
    if(Decoded==XREDIS_PUBSUB_MALFORMED || Decoded==XREDIS_PUBSUB_ERROR)goto Protocol;
    xredisrequest* R=C->SentHead;if(!R || R->Observe<5)goto Protocol;
    xredissubcall* S=xredisSubscriberState(R);if(S->Finished)goto Protocol;
    if(Root->Type==XREDIS_REPLY_SIMPLE_ERROR || Root->Type==XREDIS_REPLY_BULK_ERROR){
        if(S->Acknowledged)goto Protocol;
        S->Status=XREDIS_REQUEST_SERVER_ERROR;S->Finished=true;S->Final=Reply;xredisSubscriberFinish(R);return;
    }
    if(S->Command==XREDIS_PUBSUB_CMD_PING){
        xbytesview Expected;size_t Cursor=S->Cursor;
        if(S->Count && !xredisSubscriberName(R,&Cursor,&Expected))goto Protocol;
        if(xredisPubSubPongRead(Reply,S->Count?&Expected:NULL,&E)!=XREDIS_PUBSUB_EVENT)goto Protocol;
        S->Status=XREDIS_REQUEST_OK;S->Finished=true;S->Final=Reply;xredisSubscriberFinish(R);return;
    }
    if(Decoded!=XREDIS_PUBSUB_EVENT || E.Kind!=(xredispubsubkind)(S->Command+XREDIS_PUBSUB_SUBSCRIBE))goto Protocol;
    unsigned Family=(unsigned)S->Command%3;bool Subscribe=S->Command<=XREDIS_PUBSUB_CMD_SSUBSCRIBE;
    xbytesview Name=Family==1?E.Pattern:E.Channel;xredissubentry* Entry=NULL;
    if(S->Count){
        xbytesview Expected;size_t Cursor=S->Cursor;
        if(E.Flags&XREDIS_PUBSUB_NAME_ABSENT || !xredisSubscriberName(R,&Cursor,&Expected) || Name.Size!=Expected.Size || (Name.Size && memcmp(Name.Data,Expected.Data,Name.Size)))goto Protocol;
        S->Cursor=Cursor;Entry=Subscribe?S->Entries[S->Acknowledged]:xredisSubscriberFind(M,Family,Name,xrtHash64(Name.Data,Name.Size));
    }else{
        if(!S->AllInitialized){S->Expected=M->Counts[Family]?M->Counts[Family]:1;S->Absent=!M->Counts[Family];S->AllInitialized=true;}
        if(S->Absent){if(!(E.Flags&XREDIS_PUBSUB_NAME_ABSENT))goto Protocol;}
        else{
            if(E.Flags&XREDIS_PUBSUB_NAME_ABSENT)goto Protocol;
            Entry=xredisSubscriberFind(M,Family,Name,xrtHash64(Name.Data,Name.Size));if(!Entry || !Entry->Confirmed)goto Protocol;
        }
    }
    size_t Before=Family==2?M->Counts[2]:M->Counts[0]+M->Counts[1];
    bool Changes=Subscribe?Entry && !Entry->Confirmed:Entry && Entry->Confirmed;
    size_t After=Changes?(Subscribe?Before+1:Before-1):Before;if(E.Subscriptions!=After)goto Protocol;
    if(Changes){Entry->Confirmed=Subscribe;if(Subscribe)++M->Counts[Family];else --M->Counts[Family];}
    if(Entry && !Entry->Confirmed && !Entry->Reservations)xredisSubscriberDelete(M,Entry,false);
    ++S->Acknowledged;xrtMutexLock(&C->Gate);++M->Info.Acknowledgements;xrtMutexUnlock(&C->Gate);xredisSubscriberPublish(C,M);
    if(S->Acknowledged==S->Expected){S->Status=XREDIS_REQUEST_OK;S->Finished=true;S->Final=Reply;xredisSubscriberFinish(R);}else xredisReplyRelease(Reply);
    return;
Protocol:
    xredisReplyRelease(Reply);xredisClientFail(C,XREDIS_REQUEST_PROTOCOL,NULL);
}
static xredisadmission xredisSubscriberCanStart(xredisclient* C)
{
    return C->Info.State>=XREDIS_CLIENT_CLOSING?XREDIS_ADMISSION_CLOSED:
        C->Info.State!=XREDIS_CLIENT_OPEN || !C->Info.Ready || C->Info.Requests || C->Info.Pushes || xrtAtomicPtrLoad(&C->Subscriber,XMEMORY_ACQUIRE)
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
        || C->Owner
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
        || C->ObservationPreparing || xrtAtomicPtrLoad(&C->Observation,XMEMORY_ACQUIRE)
#endif
        ?XREDIS_ADMISSION_OVERLOADED:XREDIS_ADMISSION_ACCEPTED;
}
static xredisadmission xredisSubscriberStartScope(xredisclient* C,const xredissubscriberconfig* Config,ptr Scope)
{
    xredissubscriberconfig K;if(!C || (Config && (!xrtMemRangeValid(Config,sizeof(*Config)) || xrtMemRangesOverlap(C,C->StorageBytes,Config,sizeof(*Config))))){xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"invalid subscriber adoption arguments");return XREDIS_ADMISSION_ERROR;}
    if(Config)K=*Config;else xredisSubscriberConfigInit(&K);
    if(K.Size!=sizeof(K) || !K.MaxSubscriptions || K.MaxSubscriptions>100000 || !K.MaxNameBytes ||
       K.MaxNameBytes>SIZE_MAX-sizeof(xredissubscriber)-K.MaxSubscriptions*(sizeof(xredissubentry)+4*sizeof(xredissubentry*))){xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"invalid subscriber registry bounds");return XREDIS_ADMISSION_ERROR;}
    xrtMutexLock(&C->Gate);xredisadmission A=xredisSubscriberCanStart(C);if(A!=XREDIS_ADMISSION_ACCEPTED)++C->Info.Rejected;xrtMutexUnlock(&C->Gate);
    if(A!=XREDIS_ADMISSION_ACCEPTED)return A;
    size_t Buckets=1;while(Buckets<2*K.MaxSubscriptions)Buckets*=2;
    size_t Bytes=sizeof(xredissubscriber)+Buckets*sizeof(xredissubentry*);xredissubscriber* M=xrtCalloc(1,Bytes);if(!M)return XREDIS_ADMISSION_ERROR;
    M->Config=K;M->Buckets=Buckets;M->StorageBytes=Bytes;M->Info.Size=sizeof(M->Info);M->Info.RegistryBytes=Bytes;
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS)
    M->ReplayOwner=Scope;
#else
    (void)Scope;
#endif
    xrtMutexLock(&C->Gate);
    A=xredisSubscriberCanStart(C);
    if(A==XREDIS_ADMISSION_ACCEPTED)xrtAtomicPtrStore(&C->Subscriber,M,XMEMORY_RELEASE);else ++C->Info.Rejected;
    xrtMutexUnlock(&C->Gate);if(A!=XREDIS_ADMISSION_ACCEPTED)xrtFree(M);return A;
}
XREDIS_API xredisadmission xredisSubscriberStart(xredisclient* C,const xredissubscriberconfig* Config)
{return xredisSubscriberStartScope(C,Config,NULL);}
static xredisadmission xredisSubscriberAdmit(xredisclient* C,xredispubsubcommand Command,const xbytesview* Names,size_t Count,int64 Timeout,xfuture** Future,double Entered)
{
    if((unsigned)Command>XREDIS_PUBSUB_CMD_PING){xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"invalid subscriber command");return XREDIS_ADMISSION_ERROR;}
    return xredisClientCallAdmit(C,Names,Count,Timeout,Future,Entered,NULL,true,(unsigned)Command+5);
}
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS)
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION)
bool __xredisSubscriptionRegistryBounds(const xredissubscriberconfig* K)
{return K->MaxNameBytes<=SIZE_MAX-sizeof(xredissubscriber)-K->MaxSubscriptions*(sizeof(xredissubentry)+4*sizeof(xredissubentry*));}
bool __xredisSubscriptionClientProfile(xredisclient* C,xnetengine* Engine)
{
    xrtMutexLock(&C->Gate);bool OK=C->Managed && (C->CloseDone || C->Engine==Engine) && !xrtAtomicPtrLoad(&C->Subscriber,XMEMORY_ACQUIRE);
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
    OK=OK && !C->Owner;
#endif
    xrtMutexUnlock(&C->Gate);return OK;
}
#endif
xredisadmission __xredisSubscriberReplayStart(xredisclient* C,const xredissubscriberconfig* Config,ptr Owner)
{return xredisSubscriberStartScope(C,Config,Owner);}
void __xredisSubscriberReplayRelease(xredisclient* C,ptr Owner)
{
    xrtMutexLock(&C->Gate);xredissubscriber* M=xrtAtomicPtrLoad(&C->Subscriber,XMEMORY_ACQUIRE);
    if(M && M->ReplayOwner==Owner)M->ReplayOwner=NULL;
    xrtMutexUnlock(&C->Gate);
}
xredisadmission __xredisSubscriberCommandBudget(xredisclient* C,xredispubsubcommand Command,
    const xbytesview* Names,size_t Count,int64 Timeout,double Entered,ptr Owner,xfuture** Future)
{return xredisClientCallAdmit(C,Names,Count,Timeout,Future,Entered,Owner,true,(unsigned)Command+5);}
#endif
XREDIS_API xredisadmission xredisSubscriberCommandAsync(xredisclient* C,xredispubsubcommand Command,const xbytesview* Names,size_t Count,int64 Timeout,xfuture** Future)
{return xredisSubscriberAdmit(C,Command,Names,Count,Timeout,Future,xrtTimer());}
XREDIS_API xredisadmission xredisSubscriberCommand(xredisclient* C,xredispubsubcommand Command,const xbytesview* Names,size_t Count,int64 Timeout,xfuture** Future)
{
    double Entered=xrtTimer();
    if(!C){xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"subscriber client is NULL");return XREDIS_ADMISSION_ERROR;}
    xrtMutexLock(&C->Gate);bool Worker=!C->CloseDone && xrtNetEngineCurrent(C->Engine)!=NULL;xrtMutexUnlock(&C->Gate);
    if(Worker){xredisClientReport(XERR_STATE,XREDIS_CLIENT_STATE,"sync subscriber cannot block an Engine worker");return XREDIS_ADMISSION_ERROR;}
    xredisadmission A=xredisSubscriberAdmit(C,Command,Names,Count,Timeout,Future,Entered);if(A==XREDIS_ADMISSION_ACCEPTED)(void)xrtFutureWait(*Future);return A;
}
XREDIS_API bool xredisSubscriberInfo(xredisclient* C,xredissubscriberinfo* Info)
{
    if(!C || !Info || !xrtMemRangeValid(Info,sizeof(*Info)) || xrtMemRangesOverlap(C,C?C->StorageBytes:0,Info,sizeof(*Info)) || Info->Size!=sizeof(*Info))return xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"invalid subscriber info output");
    xredissubscriber* M=xrtAtomicPtrLoad(&C->Subscriber,XMEMORY_ACQUIRE);
    if(!M || xrtMemRangesOverlap(M,M->StorageBytes,Info,sizeof(*Info)))return xredisClientReport(XERR_STATE,XREDIS_CLIENT_STATE,"subscriber mode is not active or output overlaps registry");
    xrtMutexLock(&C->Gate);*Info=M->Info;xrtMutexUnlock(&C->Gate);return true;
}
#endif

#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
static bool xredisTrackingOutput(xredisclient* C,const void* Output,size_t Bytes)
{
    if(!C || !Output || !xrtMemRangeValid(Output,Bytes) || xrtMemRangesOverlap(C,C->StorageBytes,Output,Bytes))return false;
    xredisobservation* S=xrtAtomicPtrLoad(&C->Observation,XMEMORY_ACQUIRE);
    return !S || !xrtMemRangesOverlap(S,S->Bytes,Output,Bytes);
}
XREDIS_API xredisadmission xredisClientTrackingObserve(xredisclient* C,size_t MaxBytes)
{
    if(!C){xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"invalid tracking client");return XREDIS_ADMISSION_ERROR;}
    if(!MaxBytes)MaxBytes=XREDIS_TRACKING_OBSERVATION_BYTES_DEFAULT;
    xrtMutexLock(&C->Gate);xredisobservation* S=xrtAtomicPtrLoad(&C->Observation,XMEMORY_ACQUIRE);
    size_t Bytes=sizeof(xredisobservation);
    xredisadmission A=C->Info.State>=XREDIS_CLIENT_CLOSING?XREDIS_ADMISSION_CLOSED:XREDIS_ADMISSION_ACCEPTED;
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    if(A==XREDIS_ADMISSION_ACCEPTED && xrtAtomicPtrLoad(&C->Subscriber,XMEMORY_ACQUIRE))A=XREDIS_ADMISSION_OVERLOADED;
#endif
    if(A==XREDIS_ADMISSION_ACCEPTED && (C->Config.MaxPushes>(SIZE_MAX-Bytes)/sizeof(uint64) ||
        (Bytes+=C->Config.MaxPushes*sizeof(uint64))>MaxBytes || C->ObservationPreparing))A=XREDIS_ADMISSION_OVERLOADED;
    if(A!=XREDIS_ADMISSION_ACCEPTED || S){
        if(A==XREDIS_ADMISSION_OVERLOADED && C->ObservationRejected!=UINT64_MAX)++C->ObservationRejected;
        xrtMutexUnlock(&C->Gate);return A;
    }
    C->ObservationPreparing=true;xrtMutexUnlock(&C->Gate);
    S=xrtCalloc(1,Bytes);
    if(S){S->Bytes=Bytes;xrtAtomic64Init(&S->Sequence,0);}
    xrtMutexLock(&C->Gate);
    A=!S?XREDIS_ADMISSION_ERROR:C->Info.State>=XREDIS_CLIENT_CLOSING?XREDIS_ADMISSION_CLOSED:XREDIS_ADMISSION_ACCEPTED;
    if(A==XREDIS_ADMISSION_ACCEPTED)xrtAtomicPtrStore(&C->Observation,S,XMEMORY_RELEASE);
    else {xrtFree(S);if(C->ObservationRejected!=UINT64_MAX)++C->ObservationRejected;}
    /* Refund unpublished storage before physical close can observe quiescence. */
    C->ObservationPreparing=false;
    if(C->Info.State>=XREDIS_CLIENT_CLOSING)(void)xredisClientWakeLocked(C);
    xrtMutexUnlock(&C->Gate);return A;
}
XREDIS_API bool xredisClientTrackingInfo(xredisclient* C,xredistrackinginfo* Info)
{
    if(!xredisTrackingOutput(C,Info,sizeof(*Info)) || Info->Size!=sizeof(*Info))return xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"invalid tracking info output");
    xrtMutexLock(&C->Gate);xredisobservation* S=xrtAtomicPtrLoad(&C->Observation,XMEMORY_ACQUIRE);
    *Info=(xredistrackinginfo){sizeof(*Info),S?S->Bytes:0,C->ObservationPreparing?sizeof(xredisobservation)+C->Config.MaxPushes*sizeof(uint64):0,S?xrtAtomic64Load(&S->Sequence,XMEMORY_ACQUIRE):0,C->ObservationRejected,S!=NULL,C->ObservationPreparing};
    xrtMutexUnlock(&C->Gate);return true;
}
XREDIS_API xredisadmission xredisClientTrackingCallAsync(xredisclient* C,const xbytesview* Args,size_t Count,int64 Timeout,xfuture** Future)
{return xredisClientCallAdmit(C,Args,Count,Timeout,Future,xrtTimer(),NULL,false,true);}
XREDIS_API xredisadmission xredisClientCachingCallAsync(xredisclient* C,bool Caching,const xbytesview* Args,size_t Count,int64 Timeout,xfuture** Future)
{return xredisClientCallAdmit(C,Args,Count,Timeout,Future,xrtTimer(),NULL,false,Caching?3:4);}
XREDIS_API xredisadmission xredisClientCachingCall(xredisclient* C,bool Caching,const xbytesview* Args,size_t Count,int64 Timeout,xfuture** Future)
{
    double Entered=xrtTimer();
    if(!C){xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"invalid caching client");return XREDIS_ADMISSION_ERROR;}
    xrtMutexLock(&C->Gate);bool Worker=!C->CloseDone && xrtNetEngineCurrent(C->Engine)!=NULL;xrtMutexUnlock(&C->Gate);
    if(Worker){xredisClientReport(XERR_STATE,XREDIS_CLIENT_STATE,"sync caching call cannot block an engine worker");return XREDIS_ADMISSION_ERROR;}
    xredisadmission A=xredisClientCallAdmit(C,Args,Count,Timeout,Future,Entered,NULL,false,Caching?3:4);
    if(A==XREDIS_ADMISSION_ACCEPTED)(void)xrtFutureWait(*Future);
    return A;
}
XREDIS_API xredisadmission xredisClientTrackingCall(xredisclient* C,const xbytesview* Args,size_t Count,int64 Timeout,xfuture** Future)
{
    double Entered=xrtTimer();
    if(!C){xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"invalid tracking client");return XREDIS_ADMISSION_ERROR;}
    xrtMutexLock(&C->Gate);bool Worker=!C->CloseDone && xrtNetEngineCurrent(C->Engine)!=NULL;xrtMutexUnlock(&C->Gate);
    if(Worker){xredisClientReport(XERR_STATE,XREDIS_CLIENT_STATE,"sync tracking call cannot block an engine worker");return XREDIS_ADMISSION_ERROR;}
    xredisadmission A=xredisClientCallAdmit(C,Args,Count,Timeout,Future,Entered,NULL,false,true);
    if(A==XREDIS_ADMISSION_ACCEPTED)(void)xrtFutureWait(*Future);
    return A;
}
XREDIS_API bool xredisClientTrackingTake(xredisclient* C,xredistrackingpush* Push)
{
    if(!xredisTrackingOutput(C,Push,sizeof(*Push)))return xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"invalid tracking push output");
    xrtMutexLock(&C->Gate);xredisobservation* S=xrtAtomicPtrLoad(&C->Observation,XMEMORY_ACQUIRE);
    if(!S){xrtMutexUnlock(&C->Gate);return xredisClientReport(XERR_STATE,XREDIS_CLIENT_STATE,"tracking observation is not enabled");}
    if(!C->Info.Pushes){xrtMutexUnlock(&C->Gate);return false;}
    size_t Index=C->PushHead;xredisreply* Value=C->PushQueue[Index];C->PushQueue[Index]=NULL;
    *Push=(xredistrackingpush){Value,S->Pushes[Index]};S->Pushes[Index]=0;
    C->PushHead=(Index+1)%C->Config.MaxPushes;--C->Info.Pushes;
    xredisreplyinfo I={0};I.Size=sizeof(I);xredisReplyInfo(Value,&I);C->Info.PushBytes-=I.ReservedBytes;
    xrtMutexUnlock(&C->Gate);return true;
}
XREDIS_API xredisadmission xredisClientTrackingConfigureAsync(xredisclient* C,const xredistrackingconfig* K,int64 Timeout,xfuture** Future)
{
    double Entered=xrtTimer();
    if(!K || !xrtMemRangeValid(K,sizeof(*K)) || K->Size!=sizeof(*K) || K->Flags&~31u || K->Redirect<0 ||
        K->PrefixCount>XREDIS_TRACKING_PREFIXES_MAX || !xrtMemRangeValid(K->Prefixes,K->PrefixCount*sizeof(*K->Prefixes)) ||
        ((K->Flags&XREDIS_TRACKING_OPTIN) && (K->Flags&XREDIS_TRACKING_OPTOUT)) ||
        ((K->Flags&XREDIS_TRACKING_BCAST) && (K->Flags&(XREDIS_TRACKING_OPTIN|XREDIS_TRACKING_OPTOUT))) ||
        (K->PrefixCount && !(K->Flags&XREDIS_TRACKING_BCAST)) || (K->Redirect && !(K->Flags&XREDIS_TRACKING_REDIRECT)) ||
        (!K->Enabled && (K->Flags || K->Redirect || K->PrefixCount)) || !Future || !xrtMemRangeValid(Future,sizeof(*Future)) ||
        xrtMemRangesOverlap(K,sizeof(*K),Future,sizeof(*Future)) || xrtMemRangesOverlap(K->Prefixes,K->PrefixCount*sizeof(*K->Prefixes),Future,sizeof(*Future))){
        xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"invalid tracking configuration");return XREDIS_ADMISSION_ERROR;
    }
    if(C && K->Enabled && (!(K->Flags&XREDIS_TRACKING_REDIRECT) || !K->Redirect)){
        xrtMutexLock(&C->Gate);
        bool Unknown=C->Info.State<XREDIS_CLIENT_CLOSING && !(C->Managed &&
            (C->InitMode==XREDIS_INIT_RESP3 || (C->Info.Ready && C->Info.Protocol==XREDIS_RESP3)));
        xrtMutexUnlock(&C->Gate);
        if(Unknown){xredisClientReport(XERR_STATE,XREDIS_CLIENT_STATE,"direct invalidations require managed RESP3 selection");return XREDIS_ADMISSION_ERROR;}
    }
    xbytesview Args[3+2+2*XREDIS_TRACKING_PREFIXES_MAX+4];size_t Count=0;
    Args[Count++]=(xbytesview)XRT_BYTES_LITERAL("CLIENT");Args[Count++]=(xbytesview)XRT_BYTES_LITERAL("TRACKING");
    Args[Count++]=K->Enabled?(xbytesview)XRT_BYTES_LITERAL("ON"):(xbytesview)XRT_BYTES_LITERAL("OFF");
    unsigned char Number[20];size_t Start=sizeof(Number);uint64 Id=(uint64)K->Redirect;
    if(K->Flags&XREDIS_TRACKING_REDIRECT){do {Number[--Start]=(unsigned char)('0'+Id%10u);Id/=10u;}while(Id);
        Args[Count++]=(xbytesview)XRT_BYTES_LITERAL("REDIRECT");Args[Count++]=(xbytesview){Number+Start,sizeof(Number)-Start};}
    for(size_t I=0;I<K->PrefixCount;++I){Args[Count++]=(xbytesview)XRT_BYTES_LITERAL("PREFIX");Args[Count++]=K->Prefixes[I];}
    const xbytesview Flags[]={XRT_BYTES_LITERAL("BCAST"),XRT_BYTES_LITERAL("OPTIN"),XRT_BYTES_LITERAL("OPTOUT"),XRT_BYTES_LITERAL("NOLOOP")};
    for(unsigned I=0;I<4;++I)if(K->Flags&(1u<<I))Args[Count++]=Flags[I];
    return xredisClientCallAdmit(C,Args,Count,Timeout,Future,Entered,NULL,false,2);
}
#endif

#if defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
xredisadmission __xredisClientOwnedAsking(xredisclient* C,xredisexclusiveowner* O,const xbytesview* A,size_t N,int64 T,double Entered,xfuture** F)
{return xredisClientCallAdmit(C,A,N,T,F,Entered,O,2 XREDIS_OBSERVE_ORDINARY);}
#endif

XREDIS_API xredisadmission xredisClientCallAsync(xredisclient* Client,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future)
{ return xredisClientCallAdmit(Client, Arguments, Count, Timeout, Future, xrtTimer(), NULL, false XREDIS_OBSERVE_ORDINARY); }
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
xredisadmission __xredisClientOwnedCall(xredisclient* Client, xredisexclusiveowner* Owner,
    const xbytesview* Arguments, size_t Count, int64 Timeout, double Entered, bool Control, xfuture** Future)
{ return xredisClientCallAdmit(Client, Arguments, Count, Timeout, Future, Entered, Owner, Control XREDIS_OBSERVE_ORDINARY); }
#endif

XREDIS_API xredisadmission xredisClientCall(xredisclient* Client,
	const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future)
{
	if (Client == NULL) {
		xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "client is NULL"); return XREDIS_ADMISSION_ERROR;
	}
	xrtMutexLock(&Client->Gate);
	bool Worker = !Client->CloseDone && xrtNetEngineCurrent(Client->Engine) != NULL;
	xrtMutexUnlock(&Client->Gate);
	if (Worker) { xredisClientReport(XERR_STATE, XREDIS_CLIENT_STATE, "sync client call cannot block an engine worker"); return XREDIS_ADMISSION_ERROR; }
	xredisadmission Admission = xredisClientCallAsync(Client, Arguments, Count, Timeout, Future);
	if (Admission == XREDIS_ADMISSION_ACCEPTED) (void)xrtFutureWait(*Future);
	return Admission;
}

XREDIS_API bool xredisClientTakePush(xredisclient* Client, xredisreply** Reply)
{
	if (Client == NULL || Reply == NULL || !xrtMemRangeValid(Reply, sizeof(*Reply)) ||
		xrtMemRangesOverlap(Client, Client == NULL ? 0 : Client->StorageBytes, Reply, sizeof(*Reply)))
		return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid push output");
	xrtMutexLock(&Client->Gate);
	if (Client->Info.Pushes == 0) { xrtMutexUnlock(&Client->Gate); return false; }
	xredisreply* Value = Client->PushQueue[Client->PushHead]; Client->PushQueue[Client->PushHead] = NULL;
	Client->PushHead = (Client->PushHead + 1u) % Client->Config.MaxPushes; --Client->Info.Pushes;
	xredisreplyinfo Info = {0}; Info.Size = sizeof(Info); xredisReplyInfo(Value, &Info); Client->Info.PushBytes -= Info.ReservedBytes;
	*Reply = Value; xrtMutexUnlock(&Client->Gate); return true;
}

XREDIS_API bool xredisClientInfo(xredisclient* Client, xredisclientinfo* Info)
{
	if (Client == NULL || Info == NULL || !xrtMemRangeValid(Info, sizeof(*Info)) ||
		xrtMemRangesOverlap(Client, Client == NULL ? 0 : Client->StorageBytes, Info, sizeof(*Info)) || Info->Size != sizeof(*Info))
		return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid client info output");
	xrtMutexLock(&Client->Gate); *Info = Client->Info; xrtMutexUnlock(&Client->Gate); return true;
}

XREDIS_API bool xredisClientClose(xredisclient* Client)
{
	if (Client == NULL) return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "client is NULL");
	xrtMutexLock(&Client->Gate);
	if (Client->Info.State < XREDIS_CLIENT_CLOSING) { Client->Info.State = XREDIS_CLIENT_CLOSING; Client->Info.CloseStatus = XREDIS_REQUEST_CLOSED; }
	bool Result = xredisClientWakeLocked(Client); xrtMutexUnlock(&Client->Gate); return Result;
}

XREDIS_API xfuture* xredisClientClosed(xredisclient* Client)
{
	if (Client == NULL) { xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "client is NULL"); return NULL; }
	return xrtFutureRef(Client->CloseFuture);
}


#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY)
static void xredisSubscriberNotify(xpromise* Promise)
{if(Promise){(void)xrtPromiseResolve(Promise,NULL);xrtPromiseDestroy(Promise);}}
static bool xredisSubscriberOutput(xredisclient* C,void* Output,size_t Bytes,xredissubscriber** Mode)
{
    if(!C || !Output || !xrtMemRangeValid(Output,Bytes) || xrtMemRangesOverlap(C,C?C->StorageBytes:0,Output,Bytes))return false;
    *Mode=xrtAtomicPtrLoad(&C->Subscriber,XMEMORY_ACQUIRE);
    return !*Mode || !xrtMemRangesOverlap(*Mode,(*Mode)->StorageBytes,Output,Bytes);
}
/* Preparing protects the owning Engine activation until unexposed Future
 * resources are destroyed and the replacement epoch is fully published. */
static void xredisSubscriberPrepared(xredisclient* C,xredissubscriber* M)
{
    xrtMutexLock(&C->Gate);M->ReadablePreparing=false;
    if(C->Info.State>=XREDIS_CLIENT_CLOSING && !C->CloseDone)(void)xredisClientWakeLocked(C);
    xrtMutexUnlock(&C->Gate);
}
XREDIS_API xredisadmission xredisSubscriberReadable(xredisclient* C,xfuture** Output)
{
    xredissubscriber* M=NULL;
    if(!xredisSubscriberOutput(C,Output,sizeof(*Output),&M)){
        xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"invalid subscriber readiness output");return XREDIS_ADMISSION_ERROR;
    }
    if(!M){xredisClientReport(XERR_STATE,XREDIS_CLIENT_STATE,"subscriber mode is not active");return XREDIS_ADMISSION_ERROR;}
    xrtMutexLock(&C->Gate);
    if(C->CloseDone && !C->Info.Pushes){xrtMutexUnlock(&C->Gate);return XREDIS_ADMISSION_CLOSED;}
    if(M->ReadableFuture && (M->ReadablePromise || C->Info.Pushes)){
        xfuture* F=xrtFutureRef(M->ReadableFuture);if(F)*Output=F;
        xrtMutexUnlock(&C->Gate);return F?XREDIS_ADMISSION_ACCEPTED:XREDIS_ADMISSION_ERROR;
    }
    if(M->ReadablePreparing){xrtMutexUnlock(&C->Gate);return XREDIS_ADMISSION_OVERLOADED;}
    M->ReadablePreparing=true;xrtMutexUnlock(&C->Gate);
    xfuture* F=NULL;xfuture* Returned=NULL;xpromise* P=xrtPromiseCreate(&F,NULL);
    if(P)Returned=xrtFutureRef(F);
    if(!P || !Returned){xrtPromiseDestroy(P);xrtFutureDestroy(F);xredisSubscriberPrepared(C,M);return XREDIS_ADMISSION_ERROR;}
    xrtMutexLock(&C->Gate);
    if(C->CloseDone && !C->Info.Pushes){
        xrtMutexUnlock(&C->Gate);xrtPromiseDestroy(P);xrtFutureDestroy(F);xrtFutureDestroy(Returned);
        xredisSubscriberPrepared(C,M);return XREDIS_ADMISSION_CLOSED;
    }
    if(C->Info.Pushes || C->CloseDone){
        xrtMutexUnlock(&C->Gate);xredisSubscriberNotify(P);P=NULL;xrtMutexLock(&C->Gate);
    }
    xfuture* Old=M->ReadableFuture;M->ReadableFuture=F;M->ReadablePromise=P;
    xrtMutexUnlock(&C->Gate);xrtFutureDestroy(Old);
    /* No fallible operation follows publication. Keep Preparing until the old
     * reference is dropped; a closing worker must observe the final epoch. */
    xrtMutexLock(&C->Gate);M->ReadablePreparing=false;*Output=Returned;
    if(C->Info.State>=XREDIS_CLIENT_CLOSING && !C->CloseDone)(void)xredisClientWakeLocked(C);
    xrtMutexUnlock(&C->Gate);return XREDIS_ADMISSION_ACCEPTED;
}
XREDIS_API bool xredisSubscriberTakeBatch(xredisclient* C,xredisreply** Replies,size_t Capacity,size_t* Taken)
{
    xredissubscriber *M=NULL,*Other=NULL;
    if(!Capacity || Capacity>SIZE_MAX/sizeof(*Replies) ||
        !xredisSubscriberOutput(C,Replies,Capacity*sizeof(*Replies),&M) ||
        !xredisSubscriberOutput(C,Taken,sizeof(*Taken),&Other) ||
        xrtMemRangesOverlap(Replies,Capacity*sizeof(*Replies),Taken,sizeof(*Taken)))
        return xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"invalid subscriber batch outputs");
    if(!M)return xredisClientReport(XERR_STATE,XREDIS_CLIENT_STATE,"subscriber mode is not active");
    xrtMutexLock(&C->Gate);size_t Count=C->Info.Pushes<Capacity?C->Info.Pushes:Capacity;
    if(!Count){xrtMutexUnlock(&C->Gate);return false;}
    size_t Bytes=0,Head=C->PushHead,Limit=C->Config.MaxPushes;
    for(size_t N=0;N<Count;++N){
        xredisreply* R=C->PushQueue[Head];C->PushQueue[Head]=NULL;
        if(++Head==Limit)Head=0;
        xredisreplyinfo I={0};I.Size=sizeof(I);(void)xredisReplyInfo(R,&I);Bytes+=I.ReservedBytes;Replies[N]=R;
    }
    C->PushHead=Head;C->Info.Pushes-=Count;C->Info.PushBytes-=Bytes;*Taken=Count;xrtMutexUnlock(&C->Gate);return true;
}
#endif

#if defined(XREDIS_FEATURE_XREDIS_SENTINEL)
bool __xredisSentinelClientProfile(xredisclient* C,xnetengine* Engine)
{
    xrtMutexLock(&C->Gate);
    bool OK=(C->CloseDone || C->Engine==Engine) && C->Managed && !C->Info.Requests;
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
    OK=OK && !C->Owner;
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    OK=OK && !xrtAtomicPtrLoad(&C->Subscriber,XMEMORY_ACQUIRE);
#endif
    xrtMutexUnlock(&C->Gate);return OK;
}
#endif

#endif

#undef XREDIS_OBSERVE_PARAMETER
#undef XREDIS_OBSERVE_ORDINARY
#undef XREDIS_SUBSCRIBER_RETIRE
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/pool/pool.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_POOL) || \
	defined(XREDIS_FEATURE_XREDIS_SENTINEL_POOL) || \
	defined(XREDIS_FEATURE_XREDIS_POOL_ASKING) || \
	defined(XREDIS_FEATURE_XREDIS_POOL_SHARED) || \
	defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_CONNECT) || \
	defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_ASKING)
#include <math.h>
#include <stdlib.h>
#include <string.h>

#if defined(XREDIS_FEATURE_XREDIS_POOL)
typedef struct xredispoolslot xredispoolslot;
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
typedef struct xredispoolconnectstate xredispoolconnectstate;
#endif
enum { XREDIS_L_WAITING, XREDIS_L_GRANTED, XREDIS_L_DONE };
struct xredislease {
    xatomic32 References, Cancelled, Reason;
    xmutex Gate;
    xredispool* Pool;
    xredispoolslot* Slot;
    xredislease *Next, *Previous, *EventNext;
    xpromise *Promise, *ClosePromise;
    xfuture *Future, *CloseFuture;
    xredisleaseresult* Result;
    xredisleaseclosedresult* CloseResult;
    xcancelwatch* Cancel;
    double Deadline, HoldDeadline, ReturnBy, TimerDeadline;
    int64 ReturnTimeout;
    uint64 Timer;
    size_t APIs;
    unsigned Phase;
    bool EventQueued, Returning, Abort;
};
struct xredispoolslot {
    xredispool* Pool;
    xredisclient* Client;
    xredislease* Lease;
    xredisexclusiveowner Owner;
    xfuture* Closed;
    xfuturewatch Watch;
    xredispoolslot *NextIdle, *PreviousIdle, *EventNext;
    bool Claimed, Registered, Idle, EventQueued, Dead, Retired;
};
struct xredispool {
    xatomic32 References;
    xmutex Gate;
    xnetengine* Engine;
    xnetworker* Worker;
    xnetpost Work;
    xredispoolconfig Config;
    xredispoolinfo Info;
    xredisownerquota Quota;
    xpromise* ClosePromise;
    xfuture* CloseFuture;
    xredislease *WaitHead, *WaitTail, *LeaseEvents, *LeaseEventTail;
    xredispoolslot *IdleHead, *IdleTail, *SlotEvents, *SlotEventTail;
    size_t Count, StorageBytes, InitializeNext, CloseNext, Retired;
    bool Initialized, Queued, Finished;
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
    xredispoolconnectstate* Connector;
#endif
    xredispoolslot Slots[];
};

static void xredisPoolWork(xnetworker* Worker, ptr Data);
static void xredisPoolLeaseNotify(xredislease* Lease);
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
static bool xredisPoolConnectProcess(xredispool* Pool, xredispoolslot* Slot);
static void xredisPoolConnectRetired(xredispool* Pool, xredispoolslot* Slot, xredisrequeststatus Status);
static void xredisPoolConnectDrive(xredispool* Pool);
static bool xredisPoolConnectMore(xredispool* Pool);
static void xredisPoolConnectSchedule(xredispool* Pool);
static bool xredisPoolConnectDispose(xredispool* Pool);
#endif
static bool xredisPoolCanWait(xredispool* Pool)
{
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
    if (Pool->Connector) return true;
#endif
    return Pool->Info.Live != 0;
}
static void xredisPoolRef(xredispool* Pool)
{ xrtAtomic32FetchAdd(&Pool->References, 1, XMEMORY_RELAXED); }
static bool xredisPoolReport(xerrkind Kind, cstr Message)
{ xrtSetErrorInfo(Kind, "xrt.redis", Kind == XERR_STATE ? XREDIS_CLIENT_STATE : XREDIS_CLIENT_ARGUMENT, Message); return false; }
static double xredisPoolDeadline(double Entered, int64 Timeout)
{ return Timeout == XRT_WAIT_FOREVER ? INFINITY : Timeout == 0 ? Entered : nextafter(Entered + (double)Timeout * 0.001, INFINITY); }
static void xredisLeaseRef(xredislease* Lease)
{ xrtAtomic32FetchAdd(&Lease->References, 1, XMEMORY_RELAXED); }

XREDIS_API bool xredisPoolRetain(xredispool* Pool)
{ if (!Pool) return xredisPoolReport(XERR_ARGUMENT, "pool is NULL"); xredisPoolRef(Pool); return true; }
XREDIS_API void xredisPoolRelease(xredispool* Pool)
{
    if (!Pool || xrtAtomic32FetchSub(&Pool->References, 1, XMEMORY_ACQ_REL) != 1) return;
    xrtPromiseDestroy(Pool->ClosePromise); xrtFutureDestroy(Pool->CloseFuture);
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
    xrtFree(Pool->Connector);
#endif
    xrtMutexUnit(&Pool->Gate); xrtFree(Pool);
}
XREDIS_API bool xredisLeaseRetain(xredislease* Lease)
{ if (!Lease) return xredisPoolReport(XERR_ARGUMENT, "lease is NULL"); xredisLeaseRef(Lease); return true; }
XREDIS_API void xredisLeaseRelease(xredislease* Lease)
{
    if (!Lease || xrtAtomic32FetchSub(&Lease->References, 1, XMEMORY_ACQ_REL) != 1) return;
    xrtCancelUnwatch(Lease->Cancel);
    xrtPromiseDestroy(Lease->Promise); xrtFutureDestroy(Lease->Future);
    xrtPromiseDestroy(Lease->ClosePromise); xrtFutureDestroy(Lease->CloseFuture);
    xrtFree(Lease->Result); xrtFree(Lease->CloseResult);
    xrtMutexUnit(&Lease->Gate); xrtFree(Lease);
}

/* Pool Gate held. The runtime Pin and one reference per post protect Worker. */
static bool xredisPoolWakeLocked(xredispool* Pool)
{
    if (!Pool->Initialized || Pool->Finished || Pool->Queued) return true;
    Pool->Queued = true; xredisPoolRef(Pool);
    if (xrtNetPost(Pool->Worker, &Pool->Work, xredisPoolWork, Pool)) return true;
    Pool->Queued = false; xredisPoolRelease(Pool); return false;
}
static void xredisPoolSlotRef(ptr Data) { xredisPoolRef(((xredispoolslot*)Data)->Pool); }
static void xredisPoolSlotDrop(ptr Data) { xredisPoolRelease(((xredispoolslot*)Data)->Pool); }
static void xredisPoolSlotNotify(ptr Data)
{
    xredispoolslot* Slot = Data; xredispool* Pool = Slot->Pool;
    xrtMutexLock(&Pool->Gate);
    if (Pool->Initialized && !Pool->Finished && !Slot->EventQueued) {
        Slot->EventQueued = true; Slot->EventNext = NULL;
        if (Pool->SlotEventTail) Pool->SlotEventTail->EventNext = Slot; else Pool->SlotEvents = Slot;
        Pool->SlotEventTail = Slot; xredisPoolWakeLocked(Pool);
    }
    xrtMutexUnlock(&Pool->Gate);
}
static void xredisPoolQueueLease(xredispool* Pool, xredislease* Lease)
{
    xrtMutexLock(&Pool->Gate);
    if (!Pool->Finished && !Lease->EventQueued) {
        Lease->EventQueued = true; Lease->EventNext = NULL; xredisLeaseRef(Lease);
        if (Pool->LeaseEventTail) Pool->LeaseEventTail->EventNext = Lease; else Pool->LeaseEvents = Lease;
        Pool->LeaseEventTail = Lease; xredisPoolWakeLocked(Pool);
    }
    xrtMutexUnlock(&Pool->Gate);
}
static void xredisPoolLeaseNotify(xredislease* Lease)
{
    xrtMutexLock(&Lease->Gate); xredispool* Pool = Lease->Pool;
    if (Pool) xredisPoolRef(Pool);
    xrtMutexUnlock(&Lease->Gate);
    if (Pool) { xredisPoolQueueLease(Pool, Lease); xredisPoolRelease(Pool); }
}
static void xredisLeaseReason(xredislease* Lease, xredisrequeststatus Status)
{
    uint32 Expected = 0;
    xrtAtomic32CompareExchange(&Lease->Reason, &Expected, (uint32)Status + 1u, XMEMORY_ACQ_REL, XMEMORY_ACQUIRE);
}
static void xredisLeaseCancel(ptr Data)
{ xredislease* Lease = Data; xrtAtomic32Store(&Lease->Cancelled, 1, XMEMORY_RELEASE); xredisPoolLeaseNotify(Lease); }
static void xredisLeaseTimer(xnetworker* Worker, uint64 Id, xnetresult Result, ptr Data)
{
    (void)Worker; xredislease* Lease = Data;
    if (Lease->Timer == Id) Lease->Timer = 0;
    if (Result == XNET_RESULT_OK) xredisLeaseReason(Lease, XREDIS_REQUEST_TIMEOUT);
    else if (Result != XNET_RESULT_CANCELLED) xredisLeaseReason(Lease, XREDIS_REQUEST_CLOSED);
    xredisPoolLeaseNotify(Lease); xredisLeaseRelease(Lease);
}
/* Owning pool worker only; cancellation callbacks only queue events. */
static bool xredisLeaseArm(xredispool* Pool, xredislease* Lease, double Deadline)
{
    if (Lease->Timer && Lease->TimerDeadline == Deadline) return true;
    if (Lease->Timer) xrtNetEngineTimerCancelCurrent(Pool->Engine, Lease->Timer);
    if (Lease->Timer) return false;
    if (Deadline == INFINITY) return true;
    Lease->TimerDeadline = Deadline; xredisLeaseRef(Lease);
    Lease->Timer = __xrtNetEngineSchedule(Pool->Engine, xrtNetWorkerIndex(Pool->Worker), Deadline, xredisLeaseTimer, Lease);
    if (Lease->Timer) return true;
    xredisLeaseRelease(Lease); xredisLeaseReason(Lease, XREDIS_REQUEST_MEMORY); return false;
}
static bool xredisLeaseDisarm(xredispool* Pool, xredislease* Lease)
{
    if (Lease->Timer) xrtNetEngineTimerCancelCurrent(Pool->Engine, Lease->Timer);
    return Lease->Timer == 0;
}

static void xredisPoolIdleRemove(xredispool* Pool, xredispoolslot* Slot)
{
    /* Pool Gate held, including shared callers rotating ready members. */
    if (!Slot->Idle) return;
    if (Slot->PreviousIdle) Slot->PreviousIdle->NextIdle = Slot->NextIdle; else Pool->IdleHead = Slot->NextIdle;
    if (Slot->NextIdle) Slot->NextIdle->PreviousIdle = Slot->PreviousIdle; else Pool->IdleTail = Slot->PreviousIdle;
    Slot->NextIdle = Slot->PreviousIdle = NULL; Slot->Idle = false; --Pool->Info.Idle;
}
static void xredisPoolIdleAppend(xredispool* Pool, xredispoolslot* Slot)
{
    if (Slot->Idle || Slot->Dead || Slot->Lease || Pool->Info.State != XREDIS_POOL_OPEN) return;
    Slot->Idle = true; Slot->PreviousIdle = Pool->IdleTail; Slot->NextIdle = NULL;
    if (Pool->IdleTail) Pool->IdleTail->NextIdle = Slot; else Pool->IdleHead = Slot;
    Pool->IdleTail = Slot; ++Pool->Info.Idle;
}
static void xredisPoolWaitRemove(xredispool* Pool, xredislease* Lease)
{
    if (Lease->Previous) Lease->Previous->Next = Lease->Next; else Pool->WaitHead = Lease->Next;
    if (Lease->Next) Lease->Next->Previous = Lease->Previous; else Pool->WaitTail = Lease->Previous;
    Lease->Next = Lease->Previous = NULL; --Pool->Info.Pending;
}
static void xredisPoolAcquireResultDrop(ptr Value, ptr Data)
{ (void)Data; xredisleaseresult* Result = Value; xredisLeaseRelease(Result->Lease); xrtFree(Result); }
static void xredisPoolClosedResultDrop(ptr Value, ptr Data) { (void)Data; xrtFree(Value); }
static void xredisPoolPublishAcquire(xredislease* Lease, xredisrequeststatus Status)
{
    xpromise* Promise = Lease->Promise; Lease->Promise = NULL;
    xfuture* Future = Lease->Future; Lease->Future = NULL;
    xredisleaseresult* Result = Lease->Result; Lease->Result = NULL;
    *Result = (xredisleaseresult){Status, NULL};
    if (Status == XREDIS_REQUEST_OK) { xredisLeaseRef(Lease); Result->Lease = Lease; }
    if (!xrtPromiseResolveOwned(Promise, Result, xredisPoolAcquireResultDrop, NULL)) xredisPoolAcquireResultDrop(Result, NULL);
    xrtPromiseDestroy(Promise); xrtFutureDestroy(Future);
}
static void xredisPoolPublishReturn(xredislease* Lease, xredisrequeststatus Status, bool Reusable)
{
    xpromise* Promise = Lease->ClosePromise; Lease->ClosePromise = NULL;
    xredisleaseclosedresult* Result = Lease->CloseResult; Lease->CloseResult = NULL;
    *Result = (xredisleaseclosedresult){Status, Reusable};
    if (!xrtPromiseResolveOwned(Promise, Result, xredisPoolClosedResultDrop, NULL)) xrtFree(Result);
    xrtPromiseDestroy(Promise);
}
static void xredisPoolFailAcquire(xredispool* Pool, xredislease* Lease, xredisrequeststatus Status)
{
    xrtCancelUnwatch(Lease->Cancel); Lease->Cancel = NULL;
    if (!xredisLeaseDisarm(Pool, Lease)) return;
    xrtMutexLock(&Pool->Gate); xredisPoolWaitRemove(Pool, Lease); xrtMutexUnlock(&Pool->Gate);
    xrtMutexLock(&Lease->Gate); Lease->Phase = XREDIS_L_DONE; Lease->Pool = NULL; xrtMutexUnlock(&Lease->Gate);
    xredisPoolRelease(Pool); /* Acquisition's runtime reference. */
    xredisPoolPublishAcquire(Lease, Status); xredisPoolPublishReturn(Lease, Status, false); xredisLeaseRelease(Lease);
}

static void xredisPoolProcessLease(xredispool* Pool, xredislease* Lease)
{
    xrtMutexLock(&Lease->Gate);
    unsigned Phase = Lease->Phase; xredispoolslot* Slot = Lease->Slot;
    bool Returning = Lease->Returning, Abort = Lease->Abort; size_t APIs = Lease->APIs;
    double ReturnBy = Lease->ReturnBy;
    xrtMutexUnlock(&Lease->Gate);
    if (Phase == XREDIS_L_DONE) return;
    xrtMutexLock(&Pool->Gate); bool Closing = Pool->Info.State != XREDIS_POOL_OPEN;
    bool Live = xredisPoolCanWait(Pool), Immediate = Pool->WaitHead == Lease && Pool->IdleHead != NULL;
    xrtMutexUnlock(&Pool->Gate);
    if (Closing) xredisLeaseReason(Lease, XREDIS_REQUEST_CLOSED);
    uint32 Reason = xrtAtomic32Load(&Lease->Reason, XMEMORY_ACQUIRE);
    if (Phase == XREDIS_L_WAITING) {
        if (xrtAtomic32Load(&Lease->Cancelled, XMEMORY_ACQUIRE)) xredisLeaseReason(Lease, XREDIS_REQUEST_CANCELLED);
        if (xrtTimer() >= Lease->Deadline) xredisLeaseReason(Lease, XREDIS_REQUEST_TIMEOUT);
        if (!Live) xredisLeaseReason(Lease, XREDIS_REQUEST_DISCONNECTED);
        Reason = xrtAtomic32Load(&Lease->Reason, XMEMORY_ACQUIRE);
        if (Reason) { xredisPoolFailAcquire(Pool, Lease, (xredisrequeststatus)(Reason - 1u)); return; }
        if (!Immediate && !xredisLeaseArm(Pool, Lease, Lease->Deadline) && !Lease->Timer) xredisPoolLeaseNotify(Lease);
        return;
    }
    xredisclientinfo Info = {0}; Info.Size = sizeof(Info); xredisClientInfo(Slot->Client, &Info);
    if (Info.State >= XREDIS_CLIENT_CLOSING) xredisLeaseReason(Lease, Info.CloseStatus);
    if (xrtTimer() >= Lease->HoldDeadline || (Returning && xrtTimer() >= ReturnBy)) xredisLeaseReason(Lease, XREDIS_REQUEST_TIMEOUT);
    Reason = xrtAtomic32Load(&Lease->Reason, XMEMORY_ACQUIRE);
    if (!Returning && !Reason) return;
    xrtAtomic32Store(&Slot->Owner.State, XREDIS_OWNER_CLOSING, XMEMORY_RELEASE);
    if (Abort || Closing || (Reason && Info.Requests)) xredisClientClose(Slot->Client);
    if (APIs) return; /* Leave always queues another event. */
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
    if (!__xredisClientOwnerReady(Slot->Client, &Slot->Owner, true)) {
        if (Reason || Abort || Closing) xredisClientClose(Slot->Client);
        else {
            double Deadline = ReturnBy < Lease->HoldDeadline ? ReturnBy : Lease->HoldDeadline;
            if (!xredisLeaseArm(Pool, Lease, Deadline) && !Lease->Timer) xredisClientClose(Slot->Client);
        }
        return;
    }
#endif
    xredisClientInfo(Slot->Client, &Info);
    if (Info.Requests) {
        if (!Reason && !Abort && !Closing) {
            double Deadline = ReturnBy < Lease->HoldDeadline ? ReturnBy : Lease->HoldDeadline;
            if (!xredisLeaseArm(Pool, Lease, Deadline) && !Lease->Timer) { xredisClientClose(Slot->Client); xredisPoolLeaseNotify(Lease); }
        }
        return;
    }
    bool Reusable = Info.State == XREDIS_CLIENT_OPEN && !Abort && !Closing;
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_POOL)
    Reusable = Reusable && xrtAtomic32Load(&Pool->Quota.Closing, XMEMORY_ACQUIRE) != 2;
#endif
    if (!Reusable && xrtFutureState(Slot->Closed) == XFUTURE_PENDING) { xredisClientClose(Slot->Client); return; }
    if (!xredisLeaseDisarm(Pool, Lease)) return;
    xrtMutexLock(&Lease->Gate);
    if (Lease->APIs) { xrtMutexUnlock(&Lease->Gate); return; }
    if (Lease->Abort && Reusable) { xrtMutexUnlock(&Lease->Gate); xredisClientClose(Slot->Client); return; }
    Lease->Phase = XREDIS_L_DONE; Lease->Pool = NULL; Lease->Slot = NULL;
    xrtMutexUnlock(&Lease->Gate);
    Slot->Lease = NULL;
    xrtMutexLock(&Pool->Gate); --Pool->Info.Leased; ++Pool->Info.Returned;
    if (Reusable) xredisPoolIdleAppend(Pool, Slot);
    xrtMutexUnlock(&Pool->Gate);
    xredisPoolPublishReturn(Lease, Reason ? (xredisrequeststatus)(Reason - 1u) : XREDIS_REQUEST_OK, Reusable);
    xredisPoolRelease(Pool); xredisLeaseRelease(Lease);
    if (!Reusable) xredisPoolSlotNotify(Slot);
}

static bool xredisPoolDropMember(xredispool* Pool, xredispoolslot* Slot)
{
    if (Slot->Registered) { xrtFutureWatchRemove(Slot->Closed, &Slot->Watch); Slot->Registered = false; }
    if (Slot->Claimed) {
        if (!__xredisClientUnclaim(Slot->Client, &Slot->Owner)) return false;
        Slot->Claimed = false; xrtNetEngineUnpin(Pool->Engine);
    }
    xrtAtomic32Store(&Slot->Owner.State, XREDIS_OWNER_CLOSED, XMEMORY_RELEASE);
    xredisClientRelease(Slot->Client); Slot->Client = NULL;
    xrtFutureDestroy(Slot->Closed); Slot->Closed = NULL;
    xrtMutexLock(&Pool->Gate); ++Pool->Info.Retired; xrtMutexUnlock(&Pool->Gate); return true;
}
static void xredisPoolProcessSlot(xredispool* Pool, xredispoolslot* Slot)
{
    if (Slot->Retired) return;
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
    if (Pool->Connector && xredisPoolConnectProcess(Pool, Slot)) return;
#endif
    xredisclientinfo Info = {0}; Info.Size = sizeof(Info); xredisClientInfo(Slot->Client, &Info);
    if (Info.State >= XREDIS_CLIENT_CLOSING) {
        xrtMutexLock(&Pool->Gate);
        if (!Slot->Dead) { Slot->Dead = true; --Pool->Info.Live; xredisPoolIdleRemove(Pool, Slot); }
        xrtMutexUnlock(&Pool->Gate);
    }
    if (Slot->Lease) { xredisPoolQueueLease(Pool, Slot->Lease); return; }
    if (Slot->Dead) {
        if (Info.Requests || xrtFutureState(Slot->Closed) == XFUTURE_PENDING) return;
        if (!xredisPoolDropMember(Pool, Slot)) return;
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
        if (Pool->Connector) { xredisPoolConnectRetired(Pool, Slot, Info.CloseStatus); return; }
#endif
        Slot->Retired = true;
        xrtMutexLock(&Pool->Gate); ++Pool->Retired; xrtMutexUnlock(&Pool->Gate);
    } else if (!Info.Requests) {
        xrtMutexLock(&Pool->Gate); xredisPoolIdleAppend(Pool, Slot); xrtMutexUnlock(&Pool->Gate);
    }
}

static void xredisPoolGrant(xredispool* Pool, xredislease* Lease, xredispoolslot* Slot)
{
    /* No application can use this lease yet. Retire the acquisition cancel
     * callback before the final grant check; future cancellation afterwards
     * cannot revoke an already granted lease. */
    if (!xredisLeaseDisarm(Pool, Lease)) { xredisPoolLeaseNotify(Lease); return; }
    xrtCancelUnwatch(Lease->Cancel); Lease->Cancel = NULL;
    if (xrtAtomic32Load(&Lease->Cancelled, XMEMORY_ACQUIRE)) xredisLeaseReason(Lease, XREDIS_REQUEST_CANCELLED);
    if (xrtTimer() >= Lease->Deadline) xredisLeaseReason(Lease, XREDIS_REQUEST_TIMEOUT);
    uint32 Reason = xrtAtomic32Load(&Lease->Reason, XMEMORY_ACQUIRE);
    if (Reason) { xredisPoolFailAcquire(Pool, Lease, (xredisrequeststatus)(Reason - 1u)); return; }
    xredisclientinfo Info = {0}; Info.Size = sizeof(Info); xredisClientInfo(Slot->Client, &Info);
    if (Info.State != XREDIS_CLIENT_OPEN || Info.Requests) { xredisPoolSlotNotify(Slot); return; }
    Lease->HoldDeadline = xredisPoolDeadline(xrtTimer(), Pool->Config.LeaseTimeout); Lease->ReturnTimeout = Pool->Config.ReturnTimeout;
    if (!xredisLeaseArm(Pool, Lease, Lease->HoldDeadline)) {
        xredisPoolFailAcquire(Pool, Lease, XREDIS_REQUEST_MEMORY); return;
    }
    xrtMutexLock(&Pool->Gate);
    if (Pool->Info.State != XREDIS_POOL_OPEN) {
        xrtMutexUnlock(&Pool->Gate); xredisPoolFailAcquire(Pool, Lease, XREDIS_REQUEST_CLOSED); return;
    }
    xredisPoolWaitRemove(Pool, Lease); ++Pool->Info.Leased; ++Pool->Info.Acquired;
    xrtMutexUnlock(&Pool->Gate);
    xrtMutexLock(&Lease->Gate); Lease->Phase = XREDIS_L_GRANTED; Lease->Slot = Slot; xrtMutexUnlock(&Lease->Gate);
    Slot->Lease = Lease; Slot->Owner.Deadline = Lease->HoldDeadline;
    xrtAtomic32Store(&Slot->Owner.State, XREDIS_OWNER_ACTIVE, XMEMORY_RELEASE);
    xredisPoolPublishAcquire(Lease, XREDIS_REQUEST_OK);
}

static void xredisPoolFinish(xredispool* Pool)
{
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
    if (Pool->Connector && !xredisPoolConnectDispose(Pool)) return;
#endif
    xrtMutexLock(&Pool->Gate);
    if (Pool->Queued || Pool->Info.Pending || Pool->Info.Leased || Pool->Retired != Pool->Count || Pool->LeaseEvents || Pool->SlotEvents) {
        xrtMutexUnlock(&Pool->Gate); return;
    }
    Pool->Finished = true; Pool->Info.State = XREDIS_POOL_CLOSED;
    xnetengine* Engine = Pool->Engine; Pool->Engine = NULL; Pool->Worker = NULL;
    xrtMutexUnlock(&Pool->Gate);
    xrtNetEngineUnpin(Engine);
    xpromise* Promise = Pool->ClosePromise; Pool->ClosePromise = NULL;
    xrtPromiseResolve(Promise, NULL); xrtPromiseDestroy(Promise); xredisPoolRelease(Pool); /* Runtime. */
}
static void xredisPoolWork(xnetworker* Worker, ptr Data)
{
    (void)Worker; xredispool* Pool = Data;
    xrtMutexLock(&Pool->Gate); Pool->Queued = false; bool Closing = Pool->Info.State != XREDIS_POOL_OPEN; xrtMutexUnlock(&Pool->Gate);
    size_t Budget = Pool->Config.WorkItems;
    if (Closing) for (size_t Index = 0; Index < Budget && Pool->CloseNext < Pool->Count; ++Index) {
        xredispoolslot* Slot = &Pool->Slots[Pool->CloseNext++];
        if (!Slot->Retired) { if (Slot->Client) xredisClientClose(Slot->Client); xredisPoolProcessSlot(Pool, Slot); }
    }
    else for (size_t Index = 0; Index < Budget && Pool->InitializeNext < Pool->Count; ++Index)
        xredisPoolProcessSlot(Pool, &Pool->Slots[Pool->InitializeNext++]);
    for (size_t Index = 0; Index < Budget; ++Index) {
        xrtMutexLock(&Pool->Gate); xredispoolslot* Slot = Pool->SlotEvents;
        if (Slot) { Pool->SlotEvents = Slot->EventNext; if (!Pool->SlotEvents) Pool->SlotEventTail = NULL; Slot->EventQueued = false; }
        xrtMutexUnlock(&Pool->Gate); if (!Slot) break; xredisPoolProcessSlot(Pool, Slot);
    }
    for (size_t Index = 0; Index < Budget; ++Index) {
        xrtMutexLock(&Pool->Gate); xredislease* Lease = Pool->LeaseEvents;
        if (Lease) { Pool->LeaseEvents = Lease->EventNext; if (!Pool->LeaseEvents) Pool->LeaseEventTail = NULL; Lease->EventQueued = false; }
        xrtMutexUnlock(&Pool->Gate); if (!Lease) break; xredisPoolProcessLease(Pool, Lease); xredisLeaseRelease(Lease);
    }
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
    if (Pool->Connector) xredisPoolConnectDrive(Pool);
#endif
    for (size_t Index = 0; Index < Budget; ++Index) {
        xrtMutexLock(&Pool->Gate); xredislease* Lease = Pool->WaitHead;
        bool Paused = false;
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_POOL)
        Paused = xrtAtomic32Load(&Pool->Quota.Closing, XMEMORY_ACQUIRE) == 2;
#endif
        xredispoolslot* Slot = Pool->Info.State == XREDIS_POOL_OPEN && !Paused ? Pool->IdleHead : NULL;
        bool NoLive = !xredisPoolCanWait(Pool);
        if (Slot && Lease) xredisPoolIdleRemove(Pool, Slot);
        xrtMutexUnlock(&Pool->Gate);
        if (!Lease) break;
        if (Closing || NoLive) { xredisLeaseReason(Lease, Closing ? XREDIS_REQUEST_CLOSED : XREDIS_REQUEST_DISCONNECTED); xredisPoolProcessLease(Pool, Lease); }
        else if (Slot) {
            xredisPoolGrant(Pool, Lease, Slot);
            if (!Slot->Lease && !Slot->Dead) { xrtMutexLock(&Pool->Gate); xredisPoolIdleAppend(Pool, Slot); xrtMutexUnlock(&Pool->Gate); }
        } else {
            if (!Lease->Timer && !xrtAtomic32Load(&Lease->Reason, XMEMORY_ACQUIRE)) xredisPoolLeaseNotify(Lease);
            break;
        }
    }
    xrtMutexLock(&Pool->Gate);
    Closing = Pool->Info.State != XREDIS_POOL_OPEN;
    bool More = Pool->SlotEvents || Pool->LeaseEvents || (Pool->WaitHead && (Pool->IdleHead || Closing || !xredisPoolCanWait(Pool))) ||
        (Closing ? Pool->CloseNext < Pool->Count : Pool->InitializeNext < Pool->Count);
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
    if (Pool->Connector) More = More || xredisPoolConnectMore(Pool);
#endif
    if (More) xredisPoolWakeLocked(Pool);
    xrtMutexUnlock(&Pool->Gate);
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
    if (Pool->Connector) xredisPoolConnectSchedule(Pool);
#endif
    if (Closing) xredisPoolFinish(Pool);
    xredisPoolRelease(Pool);
}

XREDIS_API bool xredisPoolConfigInit(xredispoolconfig* Config)
{
    if (!Config || !xrtMemRangeValid(Config, sizeof(*Config))) return xredisPoolReport(XERR_ARGUMENT, "invalid pool config output");
    *Config = (xredispoolconfig){sizeof(*Config), 256, 4096, 64u * 1024u * 1024u, 30000, 5000, 64}; return true;
}
static int xredisPoolCompareClient(const void* Left, const void* Right)
{
    uintptr_t A = (uintptr_t)*(xredisclient* const*)Left, B = (uintptr_t)*(xredisclient* const*)Right;
    return (A > B) - (A < B);
}
static bool xredisPoolBounds(const xredispoolconfig* Options)
{
    return Options->Size == sizeof(*Options) && Options->MaxPending && Options->MaxPending <= 65536 && Options->MaxRequests && Options->MaxCommandBytes &&
        Options->LeaseTimeout > 0 && Options->LeaseTimeout <= 86400000 && Options->ReturnTimeout > 0 && Options->ReturnTimeout <= 60000 &&
        Options->WorkItems && Options->WorkItems <= 65536;
}
static xredispool* xredisPoolAllocate(xnetengine* Engine, uint64 Affinity, size_t Count, const xredispoolconfig* Options)
{
    if (!xrtNetEnginePin(Engine)) return NULL;
    size_t Bytes = sizeof(xredispool) + Count * sizeof(xredispoolslot);
    xredispool* Pool = xrtCalloc(1, Bytes);
    if (!Pool || !xrtMutexInit(&Pool->Gate)) { xrtFree(Pool); xrtNetEngineUnpin(Engine); return NULL; }
    xrtAtomic32Init(&Pool->References, 1); Pool->Engine = Engine; Pool->Count = Count; Pool->StorageBytes = Bytes; Pool->Config = *Options;
    Pool->Worker = xrtNetEngineWorker(Engine, (uint32)(Affinity % xrtNetEngineWorkerCount(Engine)));
    Pool->Info.Size = sizeof(Pool->Info); Pool->Info.Members = Count;
    xrtAtomic64Init(&Pool->Quota.Requests, 0); xrtAtomic64Init(&Pool->Quota.Bytes, 0); xrtAtomic32Init(&Pool->Quota.Closing, 0);
    Pool->Quota.MaxRequests = (uint64)Options->MaxRequests; Pool->Quota.MaxBytes = (uint64)Options->MaxCommandBytes;
    if (!xrtNetPostInit(&Pool->Work) || !(Pool->ClosePromise = xrtPromiseCreate(&Pool->CloseFuture, NULL))) {
        xrtNetEngineUnpin(Engine); xredisPoolRelease(Pool); return NULL;
    }
    return Pool;
}
static void xredisPoolSlotInit(xredispool* Pool, xredispoolslot* Slot)
{
    Slot->Pool = Pool; Slot->Owner.Data = Slot;
    Slot->Owner.Retain = xredisPoolSlotRef; Slot->Owner.Release = xredisPoolSlotDrop; Slot->Owner.Wake = xredisPoolSlotNotify;
    Slot->Owner.Quota = &Pool->Quota; Slot->Owner.Deadline = INFINITY; xrtAtomic32Init(&Slot->Owner.State, XREDIS_OWNER_CLOSING);
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
    xrtAtomic32Init(&Slot->Owner.Nested, 0);
#endif
}
static xredisadmission xredisPoolCreateAdmit(xnetengine* Engine, uint64 Affinity,
    xredisclient* const* Clients, size_t Count, const xredispoolconfig* Config, xredispool** Result, bool Shared)
{
    xredispoolconfig Options;
    if (!Engine || !Result || !xrtMemRangeValid(Result, sizeof(*Result)) || !Count || Count > 65536 || !Clients ||
        !xrtMemRangeValid(Clients, Count * sizeof(*Clients)) || xrtMemRangesOverlap(Clients, Count * sizeof(*Clients), Result, sizeof(*Result)) ||
        (Config && (!xrtMemRangeValid(Config, sizeof(*Config)) || xrtMemRangesOverlap(Config, sizeof(*Config), Result, sizeof(*Result))))) {
        xredisPoolReport(XERR_ARGUMENT, "invalid pool members/config/output"); return XREDIS_ADMISSION_ERROR;
    }
    if (Config) Options = *Config; else xredisPoolConfigInit(&Options);
    if (!xredisPoolBounds(&Options)) {
        xredisPoolReport(XERR_ARGUMENT, "invalid pool bounds/deadlines"); return XREDIS_ADMISSION_ERROR;
    }
    /* Private mode tag. Public exclusive configurations require a finite hold. */
    if (Shared) Options.LeaseTimeout = 0;
    xredisclient** Sorted = xrtMalloc(Count * sizeof(*Sorted)); if (!Sorted) return XREDIS_ADMISSION_ERROR;
    memcpy(Sorted, Clients, Count * sizeof(*Sorted)); qsort(Sorted, Count, sizeof(*Sorted), xredisPoolCompareClient);
    bool Valid = Sorted[0] != NULL;
    for (size_t Index = 1; Valid && Index < Count; ++Index) Valid = Sorted[Index] != Sorted[Index - 1];
    xrtFree(Sorted);
    if (!Valid) { xredisPoolReport(XERR_ARGUMENT, "NULL or duplicate pool member"); return XREDIS_ADMISSION_ERROR; }
    xredispool* Pool = xredisPoolAllocate(Engine, Affinity, Count, &Options);
    if (!Pool) return XREDIS_ADMISSION_ERROR;
    Pool->Info.Live = Count;
    xredisadmission Admission = XREDIS_ADMISSION_ACCEPTED;
    xredisprotocol Protocol = XREDIS_PROTOCOL_UNKNOWN;
    for (size_t Index = 0; Admission == XREDIS_ADMISSION_ACCEPTED && Index < Count; ++Index) {
        xredispoolslot* Slot = &Pool->Slots[Index]; xredisPoolSlotInit(Pool, Slot);
        xnetengine* MemberEngine = NULL; xnetworker* MemberWorker = NULL;
        Admission = __xredisClientClaim(Clients[Index], &Slot->Owner, &MemberEngine, &MemberWorker, Result);
        (void)MemberWorker;
        if (Admission != XREDIS_ADMISSION_ACCEPTED) break;
        Slot->Claimed = true; Slot->Client = Clients[Index];
        if (MemberEngine != Engine) {
            __xredisClientUnclaim(Slot->Client, &Slot->Owner); xredisClientRelease(Slot->Client); Slot->Client = NULL; Slot->Claimed = false;
            xrtNetEngineUnpin(MemberEngine); xredisPoolReport(XERR_ARGUMENT, "pool members belong to different engines"); Admission = XREDIS_ADMISSION_ERROR; break;
        }
        xredisclientinfo MemberInfo = {0}; MemberInfo.Size = sizeof(MemberInfo); xredisClientInfo(Slot->Client, &MemberInfo);
        if (Index == 0) Protocol = MemberInfo.Protocol;
        else if (Protocol != MemberInfo.Protocol) {
            xredisPoolReport(XERR_ARGUMENT, "pool members use different RESP protocols"); Admission = XREDIS_ADMISSION_ERROR; break;
        }
        Slot->Closed = xredisClientClosed(Slot->Client);
        if (!xrtFutureWatchInit(&Slot->Watch, xredisPoolSlotNotify, xredisPoolSlotDrop, Slot)) { Admission = XREDIS_ADMISSION_ERROR; break; }
        xredisPoolRef(Pool); xfuturewatchresult Watched = xrtFutureWatchAdd(Slot->Closed, &Slot->Watch);
        Slot->Registered = Watched == XFUTURE_WATCH_PENDING;
        if (!Slot->Registered) xredisPoolRelease(Pool);
        if (Watched == XFUTURE_WATCH_ERROR) Admission = XREDIS_ADMISSION_ERROR;
    }
    if (Admission != XREDIS_ADMISSION_ACCEPTED) {
        for (size_t Index = 0; Index < Count; ++Index) {
            xredispoolslot* Slot = &Pool->Slots[Index];
            if (Slot->Registered) xrtFutureWatchRemove(Slot->Closed, &Slot->Watch);
            if (Slot->Claimed) { __xredisClientUnclaim(Slot->Client, &Slot->Owner); xredisClientRelease(Slot->Client); xrtNetEngineUnpin(Engine); }
            xrtFutureDestroy(Slot->Closed);
        }
        xrtNetEngineUnpin(Engine); xredisPoolRelease(Pool); return Admission;
    }
    xredisPoolRef(Pool); /* Active lifetime, independent of caller. */
    xrtMutexLock(&Pool->Gate);
    if (Shared) for (size_t Index = 0; Index < Count; ++Index) {
        xredispoolslot* Slot = &Pool->Slots[Index];
        xrtAtomic32Store(&Slot->Owner.State, XREDIS_OWNER_ACTIVE, XMEMORY_RELEASE);
        xredisPoolIdleAppend(Pool, Slot);
    }
    Pool->Initialized = true; *Result = Pool; xredisPoolWakeLocked(Pool); xrtMutexUnlock(&Pool->Gate);
    return XREDIS_ADMISSION_ACCEPTED;
}
XREDIS_API xredisadmission xredisPoolCreate(xnetengine* Engine, uint64 Affinity,
    xredisclient* const* Clients, size_t Count, const xredispoolconfig* Config, xredispool** Result)
{ return xredisPoolCreateAdmit(Engine, Affinity, Clients, Count, Config, Result, false); }

#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED)
xredisadmission __xredisSharedPoolCreate(xnetengine* Engine, uint64 Affinity,
    xredisclient* const* Clients, size_t Count, const xredispoolconfig* Config, xredispool** Result)
{ return xredisPoolCreateAdmit(Engine, Affinity, Clients, Count, Config, Result, true); }

static xredisadmission xredisSharedPoolCallAdmit(xredispool* Pool, const xbytesview* Arguments, size_t Count,
    int64 Timeout, xfuture** Future, double Entered, bool Sync, bool Asking)
{
#if !defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_ASKING)
    (void)Asking;
#endif
    if (!Pool || !Future || !xrtMemRangeValid(Future, sizeof(*Future)) || Timeout < XRT_WAIT_FOREVER ||
        xrtMemRangesOverlap(Pool, Pool ? Pool->StorageBytes : 0, Future, sizeof(*Future))) {
        xredisPoolReport(XERR_ARGUMENT, "invalid shared pool call/output"); return XREDIS_ADMISSION_ERROR;
    }
    xrtMutexLock(&Pool->Gate);
    if (Pool->Config.LeaseTimeout != 0 || (Sync && !Pool->Finished && xrtNetEngineCurrent(Pool->Engine))) {
        xrtMutexUnlock(&Pool->Gate);
        xredisPoolReport(XERR_STATE, "requires shared pool; sync call cannot block an engine worker"); return XREDIS_ADMISSION_ERROR;
    }
    size_t Budget = Pool->Info.Idle < Pool->Config.WorkItems ? Pool->Info.Idle : Pool->Config.WorkItems;
    xrtMutexUnlock(&Pool->Gate);
    xredisadmission Admission = XREDIS_ADMISSION_OVERLOADED;
    for (size_t Index = 0; Index < Budget; ++Index) {
        xrtMutexLock(&Pool->Gate);
        xredispoolslot* Slot = Pool->Info.State == XREDIS_POOL_OPEN ? Pool->IdleHead : NULL;
        xredisclient* Client = Slot ? Slot->Client : NULL;
        if (Client) {
            xredisClientRetain(Client);
            xredisPoolIdleRemove(Pool, Slot); xredisPoolIdleAppend(Pool, Slot);
        }
        xrtMutexUnlock(&Pool->Gate);
        if (!Client) break;
        /* Native owner admission rechecks state/claim under the client gate.
         * Only a rejected constructor may try another member; never replay. */
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_ASKING)
        if (Asking) Admission = __xredisClientOwnedAsking(Client, &Slot->Owner, Arguments, Count, Timeout, Entered, Future);
        else
#endif
        Admission = __xredisClientOwnedCall(Client, &Slot->Owner, Arguments, Count, Timeout, Entered, false, Future);
        xredisClientRelease(Client);
        if (Admission == XREDIS_ADMISSION_ACCEPTED) {
            if (Sync) xrtFutureWait(*Future);
            return Admission;
        }
        if (Admission != XREDIS_ADMISSION_CLOSED && Admission != XREDIS_ADMISSION_OVERLOADED) return Admission;
    }
    xrtMutexLock(&Pool->Gate);
    Admission = Pool->Info.State == XREDIS_POOL_OPEN ? XREDIS_ADMISSION_OVERLOADED : XREDIS_ADMISSION_CLOSED;
    ++Pool->Info.Rejected; xrtMutexUnlock(&Pool->Gate); return Admission;
}
xredisadmission __xredisSharedPoolCall(xredispool* Pool, const xbytesview* Arguments, size_t Count,
    int64 Timeout, xfuture** Future, double Entered, bool Sync)
{ return xredisSharedPoolCallAdmit(Pool, Arguments, Count, Timeout, Future, Entered, Sync, false); }
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_ASKING)
xredisadmission __xredisSharedPoolAsking(xredispool* Pool, const xbytesview* Arguments, size_t Count,
    int64 Timeout, xfuture** Future, double Entered, bool Sync)
{ return xredisSharedPoolCallAdmit(Pool, Arguments, Count, Timeout, Future, Entered, Sync, true); }
#endif
#endif

static xredisadmission xredisPoolAcquireAdmit(xredispool* Pool, int64 Timeout, xfuture** Future, double Entered, bool Sync)
{
    if (!Pool || !Future || !xrtMemRangeValid(Future, sizeof(*Future)) || Timeout < XRT_WAIT_FOREVER ||
        xrtMemRangesOverlap(Pool, Pool ? Pool->StorageBytes : 0, Future, sizeof(*Future))) {
        xredisPoolReport(XERR_ARGUMENT, "invalid pool acquisition/output"); return XREDIS_ADMISSION_ERROR;
    }
    xrtMutexLock(&Pool->Gate);
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED)
    if (!Pool->Config.LeaseTimeout) {
        xrtMutexUnlock(&Pool->Gate); xredisPoolReport(XERR_STATE, "shared pools do not grant exclusive leases"); return XREDIS_ADMISSION_ERROR;
    }
#endif
    if (Sync && !Pool->Finished && xrtNetEngineCurrent(Pool->Engine)) {
        xrtMutexUnlock(&Pool->Gate); xredisPoolReport(XERR_STATE, "sync acquire cannot block an engine worker"); return XREDIS_ADMISSION_ERROR;
    }
    xredisadmission Admission = Pool->Info.State != XREDIS_POOL_OPEN ? XREDIS_ADMISSION_CLOSED :
        Pool->Info.Pending >= Pool->Config.MaxPending ? XREDIS_ADMISSION_OVERLOADED : XREDIS_ADMISSION_ACCEPTED;
    if (Admission != XREDIS_ADMISSION_ACCEPTED) { ++Pool->Info.Rejected; xrtMutexUnlock(&Pool->Gate); return Admission; }
    ++Pool->Info.Pending; xredisPoolRef(Pool); xrtMutexUnlock(&Pool->Gate);
    xredislease* Lease = xrtCalloc(1, sizeof(*Lease));
    if (!Lease || !xrtMutexInit(&Lease->Gate)) {
        xrtFree(Lease); xrtMutexLock(&Pool->Gate); --Pool->Info.Pending; xredisPoolWakeLocked(Pool); xrtMutexUnlock(&Pool->Gate);
        xredisPoolRelease(Pool); return XREDIS_ADMISSION_ERROR;
    }
    xrtAtomic32Init(&Lease->References, 1); xrtAtomic32Init(&Lease->Cancelled, 0); xrtAtomic32Init(&Lease->Reason, 0);
    Lease->Pool = Pool; Lease->Deadline = xredisPoolDeadline(Entered, Timeout);
    Lease->Result = xrtMalloc(sizeof(*Lease->Result)); Lease->CloseResult = xrtMalloc(sizeof(*Lease->CloseResult));
    bool Ready = Lease->Result && Lease->CloseResult && (Lease->Promise = xrtPromiseCreate(&Lease->Future, NULL)) &&
        (Lease->ClosePromise = xrtPromiseCreate(&Lease->CloseFuture, NULL));
    if (Ready) {
        xcancel* Token = xrtFutureCancelToken(Lease->Future); if (Token) Lease->Cancel = xrtCancelWatch(Token, xredisLeaseCancel, Lease);
        xrtCancelDestroy(Token); Ready = Lease->Cancel != NULL;
    }
    xrtMutexLock(&Pool->Gate);
    if (Ready && Pool->Info.State == XREDIS_POOL_OPEN) {
        Lease->Previous = Pool->WaitTail; if (Pool->WaitTail) Pool->WaitTail->Next = Lease; else Pool->WaitHead = Lease; Pool->WaitTail = Lease;
        xfuture* Observer = xrtFutureRef(Lease->Future); xredisLeaseRef(Lease); xredisPoolRef(Pool); /* Admission tail. */
        *Future = Observer; xrtMutexUnlock(&Pool->Gate);
        xredisPoolQueueLease(Pool, Lease); xredisLeaseRelease(Lease); xredisPoolRelease(Pool);
        if (Sync) xrtFutureWait(Observer);
        return XREDIS_ADMISSION_ACCEPTED;
    }
    Admission = Pool->Info.State == XREDIS_POOL_OPEN ? XREDIS_ADMISSION_ERROR : XREDIS_ADMISSION_CLOSED;
    --Pool->Info.Pending; if (Admission == XREDIS_ADMISSION_CLOSED) ++Pool->Info.Rejected;
    xredisPoolWakeLocked(Pool); xrtMutexUnlock(&Pool->Gate);
    xredisLeaseRelease(Lease); xredisPoolRelease(Pool); return Admission;
}
XREDIS_API xredisadmission xredisPoolAcquireAsync(xredispool* Pool, int64 Timeout, xfuture** Future)
{ return xredisPoolAcquireAdmit(Pool, Timeout, Future, xrtTimer(), false); }
XREDIS_API xredisadmission xredisPoolAcquire(xredispool* Pool, int64 Timeout, xfuture** Future)
{ return xredisPoolAcquireAdmit(Pool, Timeout, Future, xrtTimer(), true); }
XREDIS_API bool xredisPoolInfo(xredispool* Pool, xredispoolinfo* Info)
{
    if (!Pool || !Info || !xrtMemRangeValid(Info, sizeof(*Info)) || Info->Size != sizeof(*Info) ||
        xrtMemRangesOverlap(Pool, Pool ? Pool->StorageBytes : 0, Info, sizeof(*Info))) return xredisPoolReport(XERR_ARGUMENT, "invalid pool snapshot output");
    xrtMutexLock(&Pool->Gate); *Info = Pool->Info;
    Info->Requests = xrtAtomic64Load(&Pool->Quota.Requests, XMEMORY_RELAXED); Info->CommandBytes = xrtAtomic64Load(&Pool->Quota.Bytes, XMEMORY_RELAXED);
    xrtMutexUnlock(&Pool->Gate); return true;
}
XREDIS_API bool xredisPoolClose(xredispool* Pool)
{
    if (!Pool) return xredisPoolReport(XERR_ARGUMENT, "pool is NULL");
    xrtMutexLock(&Pool->Gate);
    if (Pool->Info.State == XREDIS_POOL_OPEN) Pool->Info.State = XREDIS_POOL_CLOSING;
    xrtAtomic32Store(&Pool->Quota.Closing, 1, XMEMORY_RELEASE);
    xredisPoolWakeLocked(Pool); xrtMutexUnlock(&Pool->Gate); return true;
}
XREDIS_API xfuture* xredisPoolClosed(xredispool* Pool)
{ if (!Pool) { xredisPoolReport(XERR_ARGUMENT, "pool is NULL"); return NULL; } return xrtFutureRef(Pool->CloseFuture); }

static xredisadmission xredisLeaseEnter(xredislease* Lease, bool Sync, xfuture** Future,
    xredispool** ResultPool, xredispoolslot** ResultSlot)
{
    if (!Lease || !Future || !xrtMemRangeValid(Future, sizeof(*Future)) || xrtMemRangesOverlap(Lease, Lease ? sizeof(*Lease) : 0, Future, sizeof(*Future))) {
        xredisPoolReport(XERR_ARGUMENT, "invalid leased call/output"); return XREDIS_ADMISSION_ERROR;
    }
    xrtMutexLock(&Lease->Gate);
    if (Lease->Phase != XREDIS_L_GRANTED || Lease->Returning || xrtAtomic32Load(&Lease->Reason, XMEMORY_ACQUIRE)) {
        xrtMutexUnlock(&Lease->Gate); return XREDIS_ADMISSION_CLOSED;
    }
    xredispoolslot* Slot = Lease->Slot; xredispool* Pool = Lease->Pool; ++Lease->APIs;
    xrtMutexUnlock(&Lease->Gate);
    if (Sync && __xredisClientIsWorker(Slot->Client)) {
        xrtMutexLock(&Lease->Gate); --Lease->APIs; xredisPoolRef(Pool); xrtMutexUnlock(&Lease->Gate);
        xredisPoolQueueLease(Pool, Lease); xredisPoolRelease(Pool);
        xredisPoolReport(XERR_STATE, "sync leased operation cannot block an engine worker"); return XREDIS_ADMISSION_ERROR;
    }
    *ResultPool = Pool; *ResultSlot = Slot; return XREDIS_ADMISSION_ACCEPTED;
}
static void xredisLeaseLeave(xredislease* Lease, xredispool* Pool)
{
    xrtMutexLock(&Lease->Gate); --Lease->APIs;
    bool Wake = Lease->Returning || xrtAtomic32Load(&Lease->Reason, XMEMORY_ACQUIRE) ||
        xrtAtomic32Load(&Pool->Quota.Closing, XMEMORY_ACQUIRE);
    if (Wake) xredisPoolRef(Pool);
    xrtMutexUnlock(&Lease->Gate);
    if (Wake) { xredisPoolQueueLease(Pool, Lease); xredisPoolRelease(Pool); }
}
static xredisadmission xredisLeaseCallAdmit(xredislease* Lease, const xbytesview* Arguments, size_t Count,
    int64 Timeout, xfuture** Future, double Entered, bool Sync)
{
    xredispool* Pool = NULL; xredispoolslot* Slot = NULL;
    xredisadmission Admission = xredisLeaseEnter(Lease, Sync, Future, &Pool, &Slot);
    if (Admission != XREDIS_ADMISSION_ACCEPTED) return Admission;
    Admission = __xredisClientOwnedCall(Slot->Client, &Slot->Owner, Arguments, Count, Timeout, Entered, false, Future);
    xredisLeaseLeave(Lease, Pool);
    if (Sync && Admission == XREDIS_ADMISSION_ACCEPTED) xrtFutureWait(*Future);
    return Admission;
}
XREDIS_API xredisadmission xredisLeaseCallAsync(xredislease* Lease, const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future)
{ return xredisLeaseCallAdmit(Lease, Arguments, Count, Timeout, Future, xrtTimer(), false); }
XREDIS_API xredisadmission xredisLeaseCall(xredislease* Lease, const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future)
{ return xredisLeaseCallAdmit(Lease, Arguments, Count, Timeout, Future, xrtTimer(), true); }
#if defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
xredisadmission __xredisLeaseAsking(xredislease* Lease,const xbytesview* Arguments,size_t Count,
    int64 Timeout,xfuture** Future,double Entered,bool Sync)
{
    xredispool* Pool=NULL;xredispoolslot* Slot=NULL;
    xredisadmission A=xredisLeaseEnter(Lease,Sync,Future,&Pool,&Slot);
    if(A!=XREDIS_ADMISSION_ACCEPTED)return A;
    A=__xredisClientOwnedAsking(Slot->Client,&Slot->Owner,Arguments,Count,Timeout,Entered,Future);
    xredisLeaseLeave(Lease,Pool);
    if(Sync && A==XREDIS_ADMISSION_ACCEPTED)xrtFutureWait(*Future);
    return A;
}
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_TRANSACTION)
xredisadmission __xredisLeaseTransaction(xredislease* Lease, const xredistransactioncommand* Commands,
    size_t Count, const xredistransactionconfig* Config, int64 Timeout, xfuture** Future, double Entered, bool Sync)
{
    xredispool* Pool = NULL; xredispoolslot* Slot = NULL;
    xredisadmission Admission = xredisLeaseEnter(Lease, Sync, Future, &Pool, &Slot);
    if (Admission != XREDIS_ADMISSION_ACCEPTED) return Admission;
    Admission = __xredisClientOwnedTransaction(Slot->Client, &Slot->Owner, Commands, Count, Config, Timeout, Entered, Future);
    xredisLeaseLeave(Lease, Pool);
    if (Sync && Admission == XREDIS_ADMISSION_ACCEPTED) xrtFutureWait(*Future);
    return Admission;
}
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
xredisadmission __xredisLeaseWatch(xredislease* Lease, const xbytesview* Keys, size_t Count,
    const xrediswatchconfig* Config, xfuture** Future, double Entered, bool Sync)
{
    xredispool* Pool = NULL; xredispoolslot* Slot = NULL;
    xredisadmission Admission = xredisLeaseEnter(Lease, Sync, Future, &Pool, &Slot);
    if (Admission != XREDIS_ADMISSION_ACCEPTED) return Admission;
    Admission = __xredisWatchBeginOwned(Slot->Client, &Slot->Owner, Keys, Count, Config, Future, Entered);
    xredisLeaseLeave(Lease, Pool);
    if (Sync && Admission == XREDIS_ADMISSION_ACCEPTED) xrtFutureWait(*Future);
    return Admission;
}
#endif
static bool xredisLeaseSeal(xredislease* Lease, bool Abort)
{
    if (!Lease) return xredisPoolReport(XERR_ARGUMENT, "lease is NULL");
    xrtMutexLock(&Lease->Gate);
    if (Lease->Phase == XREDIS_L_GRANTED) {
        if (!Lease->Returning) { Lease->Returning = true; Lease->ReturnBy = xredisPoolDeadline(xrtTimer(), Lease->ReturnTimeout); }
        if (Abort) { Lease->Abort = true; xredisLeaseReason(Lease, XREDIS_REQUEST_CANCELLED); }
        xrtAtomic32Store(&Lease->Slot->Owner.State, XREDIS_OWNER_CLOSING, XMEMORY_RELEASE);
    }
    xrtMutexUnlock(&Lease->Gate); xredisPoolLeaseNotify(Lease); return true;
}
XREDIS_API bool xredisLeaseReturn(xredislease* Lease) { return xredisLeaseSeal(Lease, false); }
XREDIS_API bool xredisLeaseAbort(xredislease* Lease) { return xredisLeaseSeal(Lease, true); }
XREDIS_API xfuture* xredisLeaseClosed(xredislease* Lease)
{ if (!Lease) { xredisPoolReport(XERR_ARGUMENT, "lease is NULL"); return NULL; } return xrtFutureRef(Lease->CloseFuture); }
/* Optional cold maintenance: no transport dispatch, adapter Future or command
 * replay on hot calls. All mutable maintenance state belongs to this worker. */
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
enum { XREDIS_PC_WAIT, XREDIS_PC_CONNECTING, XREDIS_PC_DRAIN, XREDIS_PC_ONLINE, XREDIS_PC_DONE };
typedef struct xredispoolconnectentry {
    xfuture* Ready;
    xfuturewatch Watch;
    size_t Heap, Previous, Next;
    double Due;
    uint64 Attempt;
    unsigned Phase, Failures;
    bool Registered, Queued, InFlight;
} xredispoolconnectentry;
struct xredispoolconnectstate {
    xredispoolconnectconfig Config;
    xredispoolconnectinfo Info;
    xredispoolconnectentry* Entries;
    size_t* Heap;
    size_t HeapCount, Head, Tail;
    uint64 Random, Timer;
    double TimerDeadline;
    bool Dropped;
};
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_POOL)
enum { XREDIS_PC_DISCOVERING = 5 };
typedef struct xredispoolsentinelentry {
    xredissentineldiscovery* Discovery;xfuture* Future;xfuturewatch Watch;
    double Deadline;uint64 Generation;bool Active,FencePending,FenceCancel,Cancelled;
} xredispoolsentinelentry;
typedef struct xredispoolsentinelstate {
    xredispoolconnectstate Base;xredissentinelpoolconfig Config;xredissentinelpoolinfo Info;
    xredispoolsentinelentry* Entries;unsigned char *Host,*Service;size_t HostBytes,FenceNext,FencePending,Keeper;
    bool Fencing,Notify;
} xredispoolsentinelstate;
static bool xredisPoolSpMode(xredispool* P){return P->Connector->Config.Size==0;}
static xredispoolsentinelstate* xredisPoolSpState(xredispool* P){return (xredispoolsentinelstate*)P->Connector;}
static bool xredisPoolSpProcess(xredispool*,xredispoolslot*);
static void xredisPoolSpStart(xredispool*,size_t);
static bool xredisPoolSpRetired(xredispool*,size_t);
static void xredisPoolSpDrive(xredispool*);
static bool xredisPoolSpAllowed(xredispool* P){return !xredisPoolSpMode(P) || !xredisPoolSpState(P)->Fencing;}
static bool xredisPoolSpMore(xredispool* P){return xredisPoolSpMode(P) && xredisPoolSpState(P)->Fencing && xredisPoolSpState(P)->FenceNext<P->Count;}
static void xredisPoolSpDispose(xredispool*);
static void xredisPoolSpFailure(xredispool*,xredissentinelstatus);
#endif
static void xredisPoolConnectIncrement(uint64* Value) { if (*Value != UINT64_MAX) ++*Value; }
static bool xredisPoolConnectOpen(xredispool* Pool)
{ return xrtAtomic32Load(&Pool->Quota.Closing, XMEMORY_ACQUIRE) != 1; }
static void xredisPoolConnectHeapSwap(xredispoolconnectstate* State, size_t A, size_t B)
{
    size_t Item = State->Heap[A]; State->Heap[A] = State->Heap[B]; State->Heap[B] = Item;
    State->Entries[State->Heap[A]].Heap = A; State->Entries[State->Heap[B]].Heap = B;
}
static bool xredisPoolConnectBefore(xredispoolconnectstate* State, size_t A, size_t B)
{
    double Left = State->Entries[State->Heap[A]].Due, Right = State->Entries[State->Heap[B]].Due;
    return Left < Right || (Left == Right && State->Heap[A] < State->Heap[B]);
}
static void xredisPoolConnectHeapFix(xredispoolconnectstate* State, size_t Index)
{
    while (Index && xredisPoolConnectBefore(State, Index, (Index - 1u) / 2u)) {
        size_t Parent = (Index - 1u) / 2u; xredisPoolConnectHeapSwap(State, Index, Parent); Index = Parent;
    }
    for (;;) {
        size_t Left = Index * 2u + 1u; if (Left >= State->HeapCount) break;
        size_t Best = Left;
        if (Left + 1u < State->HeapCount && xredisPoolConnectBefore(State, Left + 1u, Left)) Best = Left + 1u;
        if (!xredisPoolConnectBefore(State, Best, Index)) break;
        xredisPoolConnectHeapSwap(State, Index, Best); Index = Best;
    }
}
static void xredisPoolConnectHeapRemove(xredispoolconnectstate* State, size_t Item)
{
    size_t Index = State->Entries[Item].Heap; if (Index == SIZE_MAX) return;
    size_t Last = --State->HeapCount; State->Entries[Item].Heap = SIZE_MAX;
    if (Index == Last) return;
    State->Heap[Index] = State->Heap[Last]; State->Entries[State->Heap[Index]].Heap = Index;
    xredisPoolConnectHeapFix(State, Index);
}
static void xredisPoolConnectHeapPut(xredispoolconnectstate* State, size_t Item, double Due)
{
    xredisPoolConnectHeapRemove(State, Item); State->Entries[Item].Due = Due;
    size_t Index = State->HeapCount++; State->Heap[Index] = Item; State->Entries[Item].Heap = Index;
    xredisPoolConnectHeapFix(State, Index);
}
static void xredisPoolConnectQueueRemove(xredispoolconnectstate* State, size_t Item)
{
    xredispoolconnectentry* Entry = &State->Entries[Item]; if (!Entry->Queued) return;
    if (Entry->Previous != SIZE_MAX) State->Entries[Entry->Previous].Next = Entry->Next; else State->Head = Entry->Next;
    if (Entry->Next != SIZE_MAX) State->Entries[Entry->Next].Previous = Entry->Previous; else State->Tail = Entry->Previous;
    Entry->Previous = Entry->Next = SIZE_MAX; Entry->Queued = false;
}
static void xredisPoolConnectQueue(xredispoolconnectstate* State, size_t Item)
{
    xredispoolconnectentry* Entry = &State->Entries[Item]; if (Entry->Queued) return;
    Entry->Queued = true; Entry->Next = SIZE_MAX; Entry->Previous = State->Tail;
    if (State->Tail != SIZE_MAX) State->Entries[State->Tail].Next = Item; else State->Head = Item;
    State->Tail = Item;
}
static void xredisPoolConnectReadyDrop(xredispool* Pool, size_t Item)
{
    xredispoolconnectentry* Entry = &Pool->Connector->Entries[Item];
    if (Entry->Registered) { xrtFutureWatchRemove(Entry->Ready, &Entry->Watch); Entry->Registered = false; }
    xrtFutureDestroy(Entry->Ready); Entry->Ready = NULL;
}
static xredisrequeststatus xredisPoolConnectReadyStatus(xredisreadystatus Status)
{
    switch (Status) {
    case XREDIS_READY_TIMEOUT: return XREDIS_REQUEST_TIMEOUT;
    case XREDIS_READY_CLOSED: return XREDIS_REQUEST_CLOSED;
    case XREDIS_READY_DISCONNECTED: return XREDIS_REQUEST_DISCONNECTED;
    case XREDIS_READY_PROTOCOL: return XREDIS_REQUEST_PROTOCOL;
    case XREDIS_READY_LIMIT: return XREDIS_REQUEST_LIMIT;
    case XREDIS_READY_MEMORY: return XREDIS_REQUEST_MEMORY;
    default: return XREDIS_REQUEST_INITIALIZATION;
    }
}
static void xredisPoolConnectFailed(xredispool* Pool, size_t Item, xredisrequeststatus Status)
{
    xredispoolconnectstate* State = Pool->Connector; xredispoolconnectentry* Entry = &State->Entries[Item];
    if (Entry->Failures < 31) ++Entry->Failures;
    xrtMutexLock(&Pool->Gate); xredisPoolConnectIncrement(&State->Info.Failures); State->Info.LastFailure = Status; xrtMutexUnlock(&Pool->Gate);
}
static double xredisPoolConnectRetry(xredispool* Pool, size_t Item)
{
    xredispoolconnectstate* State = Pool->Connector; xredispoolconnectentry* Entry = &State->Entries[Item];
    uint64 Cap = (uint64)State->Config.RetryMin << Entry->Failures;
    if (Cap > (uint64)State->Config.RetryMax) Cap = (uint64)State->Config.RetryMax;
    /* Jitter is for retry dephasing, never for cryptographic purposes. */
    uint64 Value = State->Random; Value ^= Value << 13; Value ^= Value >> 7; Value ^= Value << 17; State->Random = Value;
    uint64 Delay = (uint64)State->Config.RetryMin + Value % (Cap - (uint64)State->Config.RetryMin + 1u);
    return xredisPoolDeadline(xrtTimer(), (int64)Delay);
}
static void xredisPoolConnectRetired(xredispool* Pool, xredispoolslot* Slot, xredisrequeststatus Status)
{
    xredispoolconnectstate* State = Pool->Connector; size_t Item = (size_t)(Slot - Pool->Slots);
    xredispoolconnectentry* Entry = &State->Entries[Item];
    xredisPoolConnectHeapRemove(State, Item); xredisPoolConnectQueueRemove(State, Item);
    xredisPoolConnectReadyDrop(Pool, Item);
    bool CountFailure = true;
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_POOL)
    if (xredisPoolSpMode(Pool)) CountFailure = xredisPoolSpRetired(Pool, Item);
#endif
    if (CountFailure && Entry->Phase == XREDIS_PC_ONLINE && xredisPoolConnectOpen(Pool)) xredisPoolConnectFailed(Pool, Item, Status);
    xrtMutexLock(&Pool->Gate);
    if (Entry->InFlight) { Entry->InFlight = false; --State->Info.Connecting; }
    bool Open = Pool->Info.State == XREDIS_POOL_OPEN;
    if (Open) { Entry->Phase = XREDIS_PC_WAIT; ++State->Info.Waiting; }
    else { Entry->Phase = XREDIS_PC_DONE; Slot->Retired = true; ++Pool->Retired; }
    xrtMutexUnlock(&Pool->Gate);
    if (Open) xredisPoolConnectHeapPut(State, Item, CountFailure ? xredisPoolConnectRetry(Pool, Item) : xrtTimer());
}
static void xredisPoolConnectDrain(xredispool* Pool, size_t Item, xredisrequeststatus Status)
{
    xredispoolconnectstate* State = Pool->Connector; xredispoolconnectentry* Entry = &State->Entries[Item];
    if (Entry->Phase != XREDIS_PC_DRAIN) {
        bool CountFailure=true;
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_POOL)
        if(xredisPoolSpMode(Pool) && xredisPoolSpState(Pool)->Entries[Item].FenceCancel)CountFailure=false;
#endif
        if (CountFailure && xredisPoolConnectOpen(Pool)) xredisPoolConnectFailed(Pool, Item, Status);
        Entry->Phase = XREDIS_PC_DRAIN; xredisPoolConnectHeapRemove(State, Item); xredisPoolConnectReadyDrop(Pool, Item);
    }
    xredisClientClose(Pool->Slots[Item].Client); xredisPoolSlotNotify(&Pool->Slots[Item]);
}
static bool xredisPoolConnectProcess(xredispool* Pool, xredispoolslot* Slot)
{
    xredispoolconnectstate* State = Pool->Connector; size_t Item = (size_t)(Slot - Pool->Slots);
    xredispoolconnectentry* Entry = &State->Entries[Item];
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_POOL)
    if (xredisPoolSpMode(Pool) && Entry->Phase == XREDIS_PC_DISCOVERING) return xredisPoolSpProcess(Pool, Slot);
#endif
    if (Entry->Phase == XREDIS_PC_ONLINE) return false;
    if (Entry->Phase == XREDIS_PC_WAIT) {
        if (!xredisPoolConnectOpen(Pool)) {
            xredisPoolConnectHeapRemove(State, Item); xredisPoolConnectQueueRemove(State, Item);
            Entry->Phase = XREDIS_PC_DONE; Slot->Retired = true;
            xrtMutexLock(&Pool->Gate); --State->Info.Waiting; ++Pool->Retired; xrtMutexUnlock(&Pool->Gate);
        }
        return true;
    }
    if (Entry->Phase == XREDIS_PC_DONE) return true;
    xredisclientinfo Info = {0}; Info.Size = sizeof(Info); xredisClientInfo(Slot->Client, &Info);
    if (Entry->Phase == XREDIS_PC_CONNECTING) {
        if (!xredisPoolConnectOpen(Pool)) xredisPoolConnectDrain(Pool, Item, XREDIS_REQUEST_CLOSED);
        else if (xrtTimer() >= Entry->Due) xredisPoolConnectDrain(Pool, Item, XREDIS_REQUEST_TIMEOUT);
        else if (xrtFutureState(Entry->Ready) != XFUTURE_PENDING) {
            const xredisreadyresult* Ready = xrtFutureValue(Entry->Ready);
            if (Ready && Ready->Status == XREDIS_READY_OK && Ready->Protocol == State->Config.Protocol &&
                Slot->Claimed && Info.State == XREDIS_CLIENT_OPEN && !Info.Requests) {
                xredisPoolConnectReadyDrop(Pool, Item); xredisPoolConnectHeapRemove(State, Item);
                Entry->Phase = XREDIS_PC_ONLINE; Entry->Failures = 0; Entry->InFlight = false; Slot->Dead = false;
                xrtMutexLock(&Pool->Gate);
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED)
                /* Activate only after native readiness/initialization storage
                 * retire, before ProcessSlot publishes the ready-list entry. */
                if (!Pool->Config.LeaseTimeout && Pool->Info.State == XREDIS_POOL_OPEN)
                    xrtAtomic32Store(&Slot->Owner.State, XREDIS_OWNER_ACTIVE, XMEMORY_RELEASE);
#endif
                --State->Info.Connecting; ++Pool->Info.Live; xredisPoolConnectIncrement(&State->Info.Connected); xrtMutexUnlock(&Pool->Gate);
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_POOL)
                if(xredisPoolSpMode(Pool))xredisPoolSpState(Pool)->Notify=true;
#endif
                return false;
            }
            xredisPoolConnectDrain(Pool, Item, Ready ? xredisPoolConnectReadyStatus(Ready->Status) : XREDIS_REQUEST_INITIALIZATION);
        } else if (Info.State >= XREDIS_CLIENT_CLOSING) xredisPoolConnectDrain(Pool, Item, Info.CloseStatus);
    }
    if (Entry->Phase == XREDIS_PC_DRAIN) {
        if (!Info.Requests && xrtFutureState(Slot->Closed) != XFUTURE_PENDING && xredisPoolDropMember(Pool, Slot))
            xredisPoolConnectRetired(Pool, Slot, Info.CloseStatus);
        else if (!Slot->Registered) xredisPoolConnectHeapPut(State, Item, xredisPoolDeadline(xrtTimer(), 10));
    }
    return true;
}
static bool xredisPoolConnectWatch(xredispool* Pool, xredispoolslot* Slot, xfuture* Future, xfuturewatch* Watch, bool* Registered)
{
    if (!xrtFutureWatchInit(Watch, xredisPoolSlotNotify, xredisPoolSlotDrop, Slot)) return false;
    xredisPoolRef(Pool); xfuturewatchresult Watched = xrtFutureWatchAdd(Future, Watch);
    *Registered = Watched == XFUTURE_WATCH_PENDING;
    if (!*Registered) xredisPoolRelease(Pool);
    return Watched != XFUTURE_WATCH_ERROR;
}
static void xredisPoolConnectAdopt(xredispool* Pool,size_t Item,xredisclient* Client)
{
    xredispoolconnectstate* State=Pool->Connector;xredispoolconnectentry* Entry=&State->Entries[Item];xredispoolslot* Slot=&Pool->Slots[Item];
    Slot->Client = Client; Slot->Dead = true;
    xrtAtomic32Store(&Slot->Owner.State, XREDIS_OWNER_CLOSING, XMEMORY_RELEASE); Slot->Owner.Deadline = INFINITY;
    if (__xredisClientClaimStarting(Client, &Slot->Owner, Pool->Engine) == XREDIS_ADMISSION_ACCEPTED) {
        Slot->Claimed = true; xredisClientRelease(Client); /* Keep the native claim reference. */
    }
    Slot->Closed = xredisClientClosed(Client); Entry->Ready = xredisClientReady(Client);
    bool ClosedWatch = xredisPoolConnectWatch(Pool, Slot, Slot->Closed, &Slot->Watch, &Slot->Registered);
    bool ReadyWatch = xredisPoolConnectWatch(Pool, Slot, Entry->Ready, &Entry->Watch, &Entry->Registered);
    if (!Slot->Claimed || !ClosedWatch || !ReadyWatch) xredisPoolConnectDrain(Pool, Item, XREDIS_REQUEST_INITIALIZATION);
    else xredisPoolConnectHeapPut(State, Item, Entry->Due);
    xredisPoolSlotNotify(Slot);
}

static void xredisPoolConnectStart(xredispool* Pool, size_t Item)
{
    xredispoolconnectstate* State = Pool->Connector; xredispoolconnectentry* Entry = &State->Entries[Item]; xredispoolslot* Slot = &Pool->Slots[Item];
    xrtMutexLock(&Pool->Gate);
    if (Pool->Info.State != XREDIS_POOL_OPEN) { xrtMutexUnlock(&Pool->Gate); return; }
    --State->Info.Waiting; ++State->Info.Connecting; xredisPoolConnectIncrement(&State->Info.Attempts);
    Entry->InFlight = true; Entry->Phase = XREDIS_PC_CONNECTING; xredisPoolConnectIncrement(&Entry->Attempt);
    xrtMutexUnlock(&Pool->Gate);
    Entry->Due = xredisPoolDeadline(xrtTimer(), State->Config.AttemptTimeout);
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_POOL)
    if(xredisPoolSpMode(Pool)){xredisPoolSpStart(Pool,Item);return;}
#endif
    xredisclient* Client = NULL;
    bool Started = State->Config.Connect(Pool->Engine, Item, Entry->Attempt, State->Config.AttemptTimeout, State->Config.Data, &Client);
    if (!Started || !Client) {
        if (xredisPoolConnectOpen(Pool)) xredisPoolConnectFailed(Pool, Item, XREDIS_REQUEST_INITIALIZATION);
        xredisPoolConnectRetired(Pool, Slot, XREDIS_REQUEST_INITIALIZATION); return;
    }
    xredisPoolConnectAdopt(Pool,Item,Client);
}

#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_POOL)
static xredisrequeststatus xredisPoolSpNative(xredissentinelstatus S)
{
    switch(S){case XREDIS_SENTINEL_TIMEOUT:return XREDIS_REQUEST_TIMEOUT;
    case XREDIS_SENTINEL_CANCELLED:return XREDIS_REQUEST_CANCELLED;
    case XREDIS_SENTINEL_UNREACHABLE:return XREDIS_REQUEST_DISCONNECTED;
    case XREDIS_SENTINEL_PROTOCOL:return XREDIS_REQUEST_PROTOCOL;
    case XREDIS_SENTINEL_LIMIT:return XREDIS_REQUEST_LIMIT;
    case XREDIS_SENTINEL_MEMORY:return XREDIS_REQUEST_MEMORY;default:return XREDIS_REQUEST_INITIALIZATION;}
}
static bool xredisPoolSpTransient(xredissentinelstatus S)
{return S==XREDIS_SENTINEL_TIMEOUT || S==XREDIS_SENTINEL_UNREACHABLE || S==XREDIS_SENTINEL_STALE_ROLE;}
static void xredisPoolSpFailure(xredispool* P,xredissentinelstatus Status)
{xredispoolsentinelstate* S=xredisPoolSpState(P);xrtMutexLock(&P->Gate);S->Info.LastStatus=Status;xrtMutexUnlock(&P->Gate);S->Notify=true;}
static void xredisPoolSpNotify(xredispool* P)
{
    xredispoolsentinelstate* S=xredisPoolSpState(P);if(!S->Notify)return;S->Notify=false;
    if(S->Config.Notify){xredissentinelpoolinfo I={0};I.Size=sizeof(I);(void)__xredisSentinelPoolInfo(P,&I);S->Config.Notify(P,&I,S->Config.Discovery.Data);}
}
static void xredisPoolSpWatchDrop(ptr Data)
{
    xredispoolslot* Slot=Data;xredispool* P=Slot->Pool;xredispoolsentinelentry* H=&xredisPoolSpState(P)->Entries[Slot-P->Slots];
    xrtMutexLock(&P->Gate);H->Active=false;xrtMutexUnlock(&P->Gate);
    xredisPoolSlotNotify(Slot);xredisPoolRelease(P); /* No more Watch access. */
}
static bool xredisPoolSpWatchStop(xredispool* P,size_t Item)
{
    xredispoolsentinelentry* H=&xredisPoolSpState(P)->Entries[Item];if(!H->Future)return true;
    xrtMutexLock(&P->Gate);bool Active=H->Active;xrtMutexUnlock(&P->Gate);
    if(Active)(void)xrtFutureWatchDetach(H->Future,&H->Watch);
    xrtMutexLock(&P->Gate);Active=H->Active;xrtMutexUnlock(&P->Gate);if(Active)return false;
    xrtFutureDestroy(H->Future);H->Future=NULL;return true;
}
static bool xredisPoolSpWatchStart(xredispool* P,size_t Item,xfuture* F)
{
    xredispoolsentinelentry* H=&xredisPoolSpState(P)->Entries[Item];H->Future=F;
    if(!xrtFutureWatchInit(&H->Watch,xredisPoolSlotNotify,xredisPoolSpWatchDrop,&P->Slots[Item]))return false;
    xredisPoolRef(P);xrtMutexLock(&P->Gate);H->Active=true;xrtMutexUnlock(&P->Gate);
    xfuturewatchresult A=xrtFutureWatchAdd(F,&H->Watch);
    if(A!=XFUTURE_WATCH_PENDING){xrtMutexLock(&P->Gate);H->Active=false;xrtMutexUnlock(&P->Gate);xredisPoolRelease(P);}
    if(A==XFUTURE_WATCH_READY)xredisPoolSlotNotify(&P->Slots[Item]);
    return A!=XFUTURE_WATCH_ERROR;
}
static bool xredisPoolSpRetired(xredispool* P,size_t Item)
{
    xredispoolsentinelstate* S=xredisPoolSpState(P);xredispoolsentinelentry* H=&S->Entries[Item];
    bool Intentional=H->FencePending || H->FenceCancel;
    if(H->FencePending){H->FencePending=false;--S->FencePending;}
    H->FenceCancel=false;
    if(!Intentional && S->Base.Entries[Item].Phase==XREDIS_PC_ONLINE && xredisPoolConnectOpen(P))xredisPoolSpFailure(P,XREDIS_SENTINEL_UNREACHABLE);
    return !Intentional;
}
static void xredisPoolSpStart(xredispool* P,size_t Item)
{
    xredispoolsentinelstate* S=xredisPoolSpState(P);xredispoolconnectentry* E=&S->Base.Entries[Item];xredispoolsentinelentry* H=&S->Entries[Item];
    E->Phase=XREDIS_PC_DISCOVERING;H->Deadline=E->Due;H->Generation=S->Info.Generation;H->FenceCancel=false;H->Cancelled=false;
    xredissentinelconfig K=S->Config.Discovery;K.PreferredSeed=S->Info.PreferredSeed;
    if(xrtTimer()>=H->Deadline){xredisPoolSpFailure(P,XREDIS_SENTINEL_TIMEOUT);xredisPoolConnectFailed(P,Item,XREDIS_REQUEST_TIMEOUT);xredisPoolConnectRetired(P,&P->Slots[Item],XREDIS_REQUEST_TIMEOUT);return;}
    if(__xredisSentinelDiscoveryCreateUntil(P->Engine,&K,H->Deadline,&H->Discovery)!=XREDIS_ADMISSION_ACCEPTED){
        xredisPoolSpFailure(P,XREDIS_SENTINEL_MEMORY);xredisPoolConnectFailed(P,Item,XREDIS_REQUEST_MEMORY);xredisPoolClose(P);xredisPoolConnectRetired(P,&P->Slots[Item],XREDIS_REQUEST_MEMORY);return;}
    xredisPoolSlotNotify(&P->Slots[Item]);
}
static bool xredisPoolSpSame(xredispoolsentinelstate* S,const xredissentinelendpoint* A)
{return S->Info.Generation && S->Info.Port==A->Port && S->HostBytes==A->Host.Size && !memcmp(S->Host,A->Host.Data,A->Host.Size);}
static bool xredisPoolSpTopology(xredispool* P,size_t Item,const xredissentinelresult* R)
{
    xredispoolsentinelstate* S=xredisPoolSpState(P);
    xrtMutexLock(&P->Gate);
    if(P->Info.State!=XREDIS_POOL_OPEN){xrtMutexUnlock(&P->Gate);return false;}
    S->Info.PreferredSeed=R->PreferredSeed;S->Info.LastStatus=XREDIS_SENTINEL_OK;
    if(!xredisPoolSpSame(S,&R->Endpoint)){
        if(S->Info.Generation==UINT64_MAX){xrtMutexUnlock(&P->Gate);xredisPoolSpFailure(P,XREDIS_SENTINEL_LIMIT);xredisPoolClose(P);return false;}
        bool Changed=S->Info.Generation!=0;++S->Info.Generation;
        if(Changed){++S->Info.Changes;S->Fencing=true;S->FenceNext=0;S->FencePending=0;S->Keeper=Item;
            /* Every native owned-call already checks this aggregate atomic gate. */
            xrtAtomic32Store(&P->Quota.Closing,2,XMEMORY_RELEASE);}
        memcpy(S->Host,R->Endpoint.Host.Data,R->Endpoint.Host.Size);S->HostBytes=R->Endpoint.Host.Size;S->Info.Port=R->Endpoint.Port;
    }
    xrtMutexUnlock(&P->Gate);S->Notify=true;xredisPoolSpNotify(P);return true;
}
static void xredisPoolSpAdopt(xredispool* P,size_t Item,bool Discard,xredisrequeststatus Why)
{
    xredispoolsentinelstate* S=xredisPoolSpState(P);xredispoolsentinelentry* H=&S->Entries[Item];xredisclient* C=NULL;
    if(!xredisSentinelDiscoveryTakeClient(H->Discovery,&C)){
        xredisPoolSpFailure(P,XREDIS_SENTINEL_FACTORY);xredisPoolClose(P);return;
    }
    xredisSentinelDiscoveryDestroy(H->Discovery);H->Discovery=NULL;H->Generation=S->Info.Generation;
    S->Base.Entries[Item].Phase=XREDIS_PC_CONNECTING;S->Base.Entries[Item].Due=H->Deadline;
    xredisPoolConnectAdopt(P,Item,C);
    if(Discard)xredisPoolConnectDrain(P,Item,Why);
}
static bool xredisPoolSpProcess(xredispool* P,xredispoolslot* Slot)
{
    size_t Item=(size_t)(Slot-P->Slots);xredispoolsentinelstate* S=xredisPoolSpState(P);xredispoolsentinelentry* H=&S->Entries[Item];
    bool Open=xredisPoolConnectOpen(P),Reconfigure=S->Fencing && S->Keeper!=Item;
    /* A detached Watch's final release can queue an extra event. Do not detach
     * its newly registered successor until completion, due time or first stop. */
    if(H->Future && !xrtFutureDone(H->Future) && xrtTimer()<S->Base.Entries[Item].Due &&
       ((Open && !Reconfigure) || H->Cancelled))return true;
    xredisPoolConnectHeapRemove(&S->Base,Item);
    if(!xredisPoolSpWatchStop(P,Item))return true;
    if(!H->Discovery){xredisPoolConnectRetired(P,Slot,XREDIS_REQUEST_INITIALIZATION);return true;}
    xredissentinelresult R={0};R.Size=sizeof(R);(void)xredisSentinelDiscoveryResult(H->Discovery,&R);
    if(!Open || Reconfigure){H->FenceCancel=Reconfigure && Open;
        if(R.Status==XREDIS_SENTINEL_OK){xredisPoolSpAdopt(P,Item,true,XREDIS_REQUEST_CLOSED);return true;}
        H->Cancelled=true;(void)xredisSentinelDiscoveryCancel(H->Discovery);
    }
    xredissentinelstep Step=xredisSentinelDiscoveryStep(H->Discovery);
    if(Step==XREDIS_SENTINEL_STEP_ERROR){xredisPoolSpFailure(P,XREDIS_SENTINEL_PROTOCOL);xredisPoolClose(P);xredisPoolSlotNotify(Slot);return true;}
    if(Step==XREDIS_SENTINEL_DONE){
        (void)xredisSentinelDiscoveryResult(H->Discovery,&R);
        if(R.Status==XREDIS_SENTINEL_OK){
            if(xrtTimer()>=H->Deadline){xredisPoolSpFailure(P,XREDIS_SENTINEL_TIMEOUT);if(S->Keeper==Item)S->Keeper=SIZE_MAX;xredisPoolSpAdopt(P,Item,true,XREDIS_REQUEST_TIMEOUT);return true;}
            if(S->Fencing){xredisPoolConnectHeapPut(&S->Base,Item,H->Deadline);return true;}
            if(!xredisPoolSpTopology(P,Item,&R)){xredisPoolSlotNotify(Slot);return true;}
            if(S->Fencing){H->Generation=S->Info.Generation;xredisPoolConnectHeapPut(&S->Base,Item,H->Deadline);return true;}
            xredisPoolSpAdopt(P,Item,false,XREDIS_REQUEST_OK);return true;
        }
        xredisSentinelDiscoveryDestroy(H->Discovery);H->Discovery=NULL;
        if(Open && !H->FenceCancel){xredisPoolSpFailure(P,R.Status);xredisPoolConnectFailed(P,Item,xredisPoolSpNative(R.Status));if(!xredisPoolSpTransient(R.Status))xredisPoolClose(P);}
        xredisPoolConnectRetired(P,Slot,xredisPoolSpNative(R.Status));return true;
    }
    if(Step==XREDIS_SENTINEL_MORE){xredisPoolSlotNotify(Slot);return true;}
    xfuture* F=xredisSentinelDiscoveryPending(H->Discovery);
    if(F && !xredisPoolSpWatchStart(P,Item,F)){xredisPoolSpFailure(P,XREDIS_SENTINEL_MEMORY);xredisPoolClose(P);xredisPoolSlotNotify(Slot);}
    xredisPoolConnectHeapPut(&S->Base,Item,xredisSentinelDiscoveryNextWake(H->Discovery));return true;
}
static void xredisPoolSpDrive(xredispool* P)
{
    xredispoolsentinelstate* S=xredisPoolSpState(P);
    if(S->Fencing && !xredisPoolConnectOpen(P)){S->Fencing=false;S->Notify=true;}
    if(S->Fencing){
        for(size_t N=0;N<P->Config.WorkItems && S->FenceNext<P->Count;++N){
            size_t Item=S->FenceNext++;if(Item==S->Keeper)continue;
            xredispoolslot* Slot=&P->Slots[Item];xredispoolsentinelentry* H=&S->Entries[Item];
            if(!Slot->Client && !H->Discovery)continue;
            H->FencePending=true;H->FenceCancel=true;++S->FencePending;
            if(Slot->Client){xrtMutexLock(&P->Gate);if(!Slot->Dead){Slot->Dead=true;--P->Info.Live;xredisPoolIdleRemove(P,Slot);}xrtMutexUnlock(&P->Gate);xredisClientClose(Slot->Client);}
            xredisPoolSlotNotify(Slot);
        }
        if(S->FenceNext==P->Count && !S->FencePending){
            xrtMutexLock(&P->Gate);if(P->Info.State==XREDIS_POOL_OPEN)xrtAtomic32Store(&P->Quota.Closing,0,XMEMORY_RELEASE);xrtMutexUnlock(&P->Gate);
            S->Fencing=false;S->Notify=true;if(S->Keeper!=SIZE_MAX)xredisPoolSlotNotify(&P->Slots[S->Keeper]);S->Keeper=SIZE_MAX;
        }
    }
    xredisPoolSpNotify(P);
}
static void xredisPoolSpDispose(xredispool* P)
{
    xredispoolsentinelstate* S=xredisPoolSpState(P);S->Notify=true;xredisPoolSpNotify(P);
    S->Config.Notify=NULL;S->Config.Discovery.ConnectSeed=NULL;S->Config.Discovery.ConnectMaster=NULL;S->Config.Discovery.Data=NULL;S->Config.Drop=NULL;
}
bool __xredisSentinelPoolInfo(xredispool* P,xredissentinelpoolinfo* I)
{
    if(!P || !I || !xrtMemRangeValid(I,sizeof(*I)) || I->Size!=sizeof(*I) || xrtMemRangesOverlap(P,P?P->StorageBytes:0,I,sizeof(*I)))return xredisPoolReport(XERR_ARGUMENT,"invalid Sentinel pool snapshot");
    xrtMutexLock(&P->Gate);bool Valid=P->Connector && xredisPoolSpMode(P);
    if(Valid){xredispoolsentinelstate* S=xredisPoolSpState(P);*I=S->Info;I->Connect=S->Base.Info;
        I->Phase=P->Info.State==XREDIS_POOL_CLOSED?XREDIS_SENTINEL_POOL_CLOSED:P->Info.State==XREDIS_POOL_CLOSING?XREDIS_SENTINEL_POOL_CLOSING:
            xrtAtomic32Load(&P->Quota.Closing,XMEMORY_ACQUIRE)==2?XREDIS_SENTINEL_POOL_RECONFIGURING:P->Info.Live?XREDIS_SENTINEL_POOL_ONLINE:XREDIS_SENTINEL_POOL_DISCOVERING;}
    xrtMutexUnlock(&P->Gate);return Valid?true:xredisPoolReport(XERR_STATE,"pool has no Sentinel maintenance");
}
xredisadmission __xredisSentinelPoolCreate(xnetengine* Engine,uint64 Affinity,size_t Count,const xredispoolconfig* Config,const xredissentinelpoolconfig* Sentinel,xredispool** Output)
{
    if(!Engine || !Count || Count>65536 || !Output || !xrtMemRangeValid(Output,sizeof(*Output)) || !Sentinel || !xrtMemRangeValid(Sentinel,sizeof(*Sentinel)) ||
       xrtMemRangesOverlap(Sentinel,sizeof(*Sentinel),Output,sizeof(*Output)) || (Config && (!xrtMemRangeValid(Config,sizeof(*Config)) || xrtMemRangesOverlap(Config,sizeof(*Config),Output,sizeof(*Output))))){
        xredisPoolReport(XERR_ARGUMENT,"invalid Sentinel pool config/output");return XREDIS_ADMISSION_ERROR;}
    xredissentinelpoolconfig K=*Sentinel;xredispoolconfig Options;if(Config)Options=*Config;else xredisPoolConfigInit(&Options);
    if(K.Size!=sizeof(K) || !xredisPoolBounds(&Options) || !K.MaxConnecting || K.MaxConnecting>65536 || K.Discovery.Timeout>60000 || K.RetryMin<=0 || K.RetryMax<K.RetryMin || K.RetryMax>60000 || !__xredisSentinelConfigValid(&K.Discovery,Output,sizeof(*Output))){xredisPoolReport(XERR_ARGUMENT,"invalid Sentinel pool bounds");return XREDIS_ADMISSION_ERROR;}
    size_t Per=sizeof(xredispoolconnectentry)+sizeof(size_t)+sizeof(xredispoolsentinelentry),Tail=sizeof(xredispoolsentinelstate)+K.Discovery.Service.Size+K.Discovery.MaxHostBytes;
    if(Count>(SIZE_MAX-Tail)/Per){xredisPoolReport(XERR_RANGE,"Sentinel pool storage overflow");return XREDIS_ADMISSION_ERROR;}
    xredispool* P=xredisPoolAllocate(Engine,Affinity,Count,&Options);if(!P)return XREDIS_ADMISSION_ERROR;
    xredispoolsentinelstate* S=xrtCalloc(1,Tail+Count*Per);if(!S){xrtNetEngineUnpin(Engine);xredisPoolRelease(P);return XREDIS_ADMISSION_ERROR;}
    P->Connector=&S->Base;S->Config=K;S->Keeper=SIZE_MAX;S->Notify=true;
    xredisPoolConnectConfigInit(&S->Base.Config);S->Base.Config.Size=0;S->Base.Config.Connect=NULL;S->Base.Config.Drop=K.Drop;S->Base.Config.Data=K.Discovery.Data;
    S->Base.Config.Protocol=K.Discovery.Protocol;S->Base.Config.MaxConnecting=K.MaxConnecting<Count?K.MaxConnecting:Count;S->Base.Config.AttemptTimeout=K.Discovery.Timeout;S->Base.Config.RetryMin=K.RetryMin;S->Base.Config.RetryMax=K.RetryMax;
    S->Base.Entries=(xredispoolconnectentry*)(S+1);S->Base.Heap=(size_t*)(S->Base.Entries+Count);S->Entries=(xredispoolsentinelentry*)(S->Base.Heap+Count);
    S->Service=(unsigned char*)(S->Entries+Count);S->Host=S->Service+K.Discovery.Service.Size;memcpy(S->Service,K.Discovery.Service.Data,K.Discovery.Service.Size);S->Config.Discovery.Service=(xbytesview){S->Service,K.Discovery.Service.Size};
    S->Info.Size=sizeof(S->Info);S->Info.PreferredSeed=K.Discovery.PreferredSeed;S->Base.Info.Size=sizeof(S->Base.Info);S->Base.Info.Waiting=Count;
    S->Base.Head=S->Base.Tail=SIZE_MAX;S->Base.HeapCount=Count;S->Base.Random=((uint64)(xrtTimer()*1000000.0)^(uint64)(uintptr_t)P)|1u;
    double Now=xrtTimer();for(size_t Item=0;Item<Count;++Item){xredisPoolSlotInit(P,&P->Slots[Item]);P->Slots[Item].Dead=true;
        S->Base.Entries[Item].Heap=S->Base.Heap[Item]=Item;S->Base.Entries[Item].Due=Now;S->Base.Entries[Item].Previous=S->Base.Entries[Item].Next=SIZE_MAX;}
    P->InitializeNext=Count;xredisPoolRef(P);xrtMutexLock(&P->Gate);P->Initialized=true;*Output=P;xredisPoolWakeLocked(P);xrtMutexUnlock(&P->Gate);return XREDIS_ADMISSION_ACCEPTED;
}
#endif

static void xredisPoolConnectDrive(xredispool* Pool)
{
    xredispoolconnectstate* State = Pool->Connector;
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_POOL)
    if(xredisPoolSpMode(Pool))xredisPoolSpDrive(Pool);
#endif
    /* Move due retries out of the heap even when concurrency is full. They
     * must never hide a later connecting attempt's hard timeout at the root. */
    for (size_t Count = 0; Count < Pool->Config.WorkItems && State->HeapCount; ++Count) {
        size_t Item = State->Heap[0]; xredispoolconnectentry* Entry = &State->Entries[Item];
        if (Entry->Due > xrtTimer()) break;
        xredisPoolConnectHeapRemove(State, Item);
        if (Entry->Phase == XREDIS_PC_WAIT && xredisPoolConnectOpen(Pool)) xredisPoolConnectQueue(State, Item);
        else xredisPoolSlotNotify(&Pool->Slots[Item]);
    }
    for (size_t Count = 0; Count < Pool->Config.WorkItems && State->Head != SIZE_MAX; ++Count) {
        if (State->Info.Connecting >= State->Config.MaxConnecting || !xredisPoolConnectOpen(Pool)) break;
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_POOL)
        if(!xredisPoolSpAllowed(Pool))break;
#endif
        size_t Item = State->Head; xredisPoolConnectQueueRemove(State, Item); xredisPoolConnectStart(Pool, Item);
    }
}
static bool xredisPoolConnectMore(xredispool* Pool)
{
    xredispoolconnectstate* State = Pool->Connector;
    return
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_POOL)
        xredisPoolSpMore(Pool) ||
#endif
        (xredisPoolConnectOpen(Pool)
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_POOL)
        && xredisPoolSpAllowed(Pool)
#endif
        && State->Head != SIZE_MAX && State->Info.Connecting < State->Config.MaxConnecting) ||
        (State->HeapCount && State->Entries[State->Heap[0]].Due <= xrtTimer());
}
static void xredisPoolConnectTimer(xnetworker* Worker, uint64 Id, xnetresult Result, ptr Data)
{
    (void)Worker; xredispool* Pool = Data; xredispoolconnectstate* State = Pool->Connector;
    if (State->Timer == Id) State->Timer = 0;
    if (Result != XNET_RESULT_OK && Result != XNET_RESULT_CANCELLED) xredisPoolClose(Pool);
    xrtMutexLock(&Pool->Gate); xredisPoolWakeLocked(Pool); xrtMutexUnlock(&Pool->Gate); xredisPoolRelease(Pool);
}
static void xredisPoolConnectSchedule(xredispool* Pool)
{
    xredispoolconnectstate* State = Pool->Connector;
    double Deadline = State->HeapCount ? State->Entries[State->Heap[0]].Due : INFINITY;
    if (Deadline <= xrtTimer()) Deadline = INFINITY; /* Bounded post continuation handles due work. */
    if (State->Timer && State->TimerDeadline == Deadline) return;
    if (State->Timer) xrtNetEngineTimerCancelCurrent(Pool->Engine, State->Timer);
    if (State->Timer || Deadline == INFINITY) return;
    State->TimerDeadline = Deadline; xredisPoolRef(Pool);
    State->Timer = __xrtNetEngineSchedule(Pool->Engine, xrtNetWorkerIndex(Pool->Worker), Deadline, xredisPoolConnectTimer, Pool);
    if (!State->Timer) {
        xrtMutexLock(&Pool->Gate); State->Info.LastFailure = XREDIS_REQUEST_MEMORY; xrtMutexUnlock(&Pool->Gate);
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_POOL)
        if(xredisPoolSpMode(Pool))xredisPoolSpFailure(Pool,XREDIS_SENTINEL_MEMORY);
#endif
        xredisPoolClose(Pool); xredisPoolRelease(Pool);
    }
}
static bool xredisPoolConnectDispose(xredispool* Pool)
{
    xredispoolconnectstate* State = Pool->Connector; if (State->Timer) return false;
    xrtMutexLock(&Pool->Gate);
    bool Ready = !Pool->Queued && !Pool->Info.Pending && !Pool->Info.Leased && Pool->Retired == Pool->Count && !Pool->LeaseEvents && !Pool->SlotEvents;
    xrtMutexUnlock(&Pool->Gate);
    if (!Ready) return false;
    if (!State->Dropped) {
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_POOL)
        if(xredisPoolSpMode(Pool))xredisPoolSpDispose(Pool);
#endif
        xredispoolconnectdrop Drop = State->Config.Drop; ptr Data = State->Config.Data;
        State->Dropped = true; State->Config.Connect = NULL; State->Config.Drop = NULL; State->Config.Data = NULL;
        if (Drop) Drop(Data);
    }
    return true;
}
static xredisadmission xredisPoolConnectAdmit(xnetengine* Engine, uint64 Affinity, size_t Count,
    const xredispoolconfig* Config, const xredispoolconnectconfig* Connect, xredispool** Result, bool Shared)
{
    if (!Engine || !Count || Count > 65536 || !Result || !xrtMemRangeValid(Result, sizeof(*Result)) ||
        !Connect || !xrtMemRangeValid(Connect, sizeof(*Connect)) || xrtMemRangesOverlap(Connect, sizeof(*Connect), Result, sizeof(*Result)) ||
        (Config && (!xrtMemRangeValid(Config, sizeof(*Config)) || xrtMemRangesOverlap(Config, sizeof(*Config), Result, sizeof(*Result))))) {
        xredisPoolReport(XERR_ARGUMENT, "invalid automatic pool config/output"); return XREDIS_ADMISSION_ERROR;
    }
    xredispoolconfig Options; if (Config) Options = *Config; else xredisPoolConfigInit(&Options);
    xredispoolconnectconfig Factory = *Connect;
    if (!xredisPoolBounds(&Options) || Factory.Size != sizeof(Factory) || !Factory.Connect ||
        (Factory.Protocol != XREDIS_RESP2 && Factory.Protocol != XREDIS_RESP3) || !Factory.MaxConnecting || Factory.MaxConnecting > 65536 ||
        Factory.AttemptTimeout <= 0 || Factory.AttemptTimeout > 60000 || Factory.RetryMin <= 0 || Factory.RetryMax < Factory.RetryMin || Factory.RetryMax > 60000) {
        xredisPoolReport(XERR_ARGUMENT, "invalid automatic pool bounds/factory/profile"); return XREDIS_ADMISSION_ERROR;
    }
    if (Shared) Options.LeaseTimeout = 0;
    xredispool* Pool = xredisPoolAllocate(Engine, Affinity, Count, &Options); if (!Pool) return XREDIS_ADMISSION_ERROR;
    size_t Bytes = sizeof(xredispoolconnectstate) + Count * (sizeof(xredispoolconnectentry) + sizeof(size_t));
    xredispoolconnectstate* State = Pool->Connector = xrtCalloc(1, Bytes);
    if (!State) { xrtNetEngineUnpin(Engine); xredisPoolRelease(Pool); return XREDIS_ADMISSION_ERROR; }
    State->Config = Factory; if (State->Config.MaxConnecting > Count) State->Config.MaxConnecting = Count;
    State->Entries = (xredispoolconnectentry*)(State + 1); State->Heap = (size_t*)(State->Entries + Count);
    State->Head = State->Tail = SIZE_MAX; State->HeapCount = Count;
    State->Random = ((uint64)(xrtTimer() * 1000000.0) ^ (uint64)(uintptr_t)Pool) | 1u;
    State->Info.Size = sizeof(State->Info); State->Info.Waiting = Count;
    double Now = xrtTimer();
    for (size_t Item = 0; Item < Count; ++Item) {
        xredisPoolSlotInit(Pool, &Pool->Slots[Item]); Pool->Slots[Item].Dead = true;
        State->Entries[Item].Heap = State->Heap[Item] = Item; State->Entries[Item].Due = Now;
        State->Entries[Item].Previous = State->Entries[Item].Next = SIZE_MAX;
    }
    Pool->InitializeNext = Count; xredisPoolRef(Pool);
    xrtMutexLock(&Pool->Gate); Pool->Initialized = true; *Result = Pool; xredisPoolWakeLocked(Pool); xrtMutexUnlock(&Pool->Gate);
    return XREDIS_ADMISSION_ACCEPTED;
}
xredisadmission __xredisPoolConnect(xnetengine* Engine, uint64 Affinity, size_t Count,
    const xredispoolconfig* Config, const xredispoolconnectconfig* Connect, xredispool** Result)
{ return xredisPoolConnectAdmit(Engine, Affinity, Count, Config, Connect, Result, false); }
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_CONNECT)
xredisadmission __xredisSharedPoolConnect(xnetengine* Engine, uint64 Affinity, size_t Count,
    const xredispoolconfig* Config, const xredispoolconnectconfig* Connect, xredispool** Result)
{ return xredisPoolConnectAdmit(Engine, Affinity, Count, Config, Connect, Result, true); }
#endif
bool __xredisPoolConnectInfo(xredispool* Pool, xredispoolconnectinfo* Info)
{
    if (!Pool || !Info || !xrtMemRangeValid(Info, sizeof(*Info)) || Info->Size != sizeof(*Info) ||
        xrtMemRangesOverlap(Pool, Pool ? Pool->StorageBytes : 0, Info, sizeof(*Info))) return xredisPoolReport(XERR_ARGUMENT, "invalid pool connector snapshot output");
    xrtMutexLock(&Pool->Gate); bool Valid = Pool->Connector != NULL; if (Valid) *Info = Pool->Connector->Info; xrtMutexUnlock(&Pool->Gate);
    return Valid ? true : xredisPoolReport(XERR_STATE, "pool has no automatic connector");
}
#endif

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/pool_connect/pool_connect.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
XREDIS_API bool xredisPoolConnectConfigInit(xredispoolconnectconfig* Config)
{
    if (!Config || !xrtMemRangeValid(Config, sizeof(*Config))) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xrt.redis", XREDIS_CLIENT_ARGUMENT, "invalid pool connector config output"); return false;
    }
    *Config = (xredispoolconnectconfig){sizeof(*Config), NULL, NULL, NULL,
        XREDIS_RESP3, 4, 5000, 100, 10000}; return true;
}
XREDIS_API xredisadmission xredisPoolConnect(xnetengine* Engine, uint64 Affinity, size_t Count,
    const xredispoolconfig* Config, const xredispoolconnectconfig* Connect, xredispool** Result)
{ return __xredisPoolConnect(Engine, Affinity, Count, Config, Connect, Result); }
XREDIS_API bool xredisPoolConnectInfo(xredispool* Pool, xredispoolconnectinfo* Info)
{ return __xredisPoolConnectInfo(Pool, Info); }
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/transaction/transaction.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)

#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
XREDIS_API bool xredisClientTransactionConfigInit(xredistransactionconfig* Config)
{
    if (Config == NULL || !xrtMemRangeValid(Config, sizeof(*Config))) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xrt.redis", XREDIS_CLIENT_ARGUMENT, "invalid transaction config output"); return false;
    }
    *Config = (xredistransactionconfig){sizeof(*Config), XREDIS_TRANSACTION_COMMANDS_DEFAULT}; return true;
}

XREDIS_API xredisadmission xredisClientTransactionAsync(xredisclient* Client,
    const xredistransactioncommand* Commands, size_t Count,
    const xredistransactionconfig* Config, int64 Timeout, xfuture** Future)
{
    return __xredisClientTransactionAsync(Client, Commands, Count, Config, Timeout, xrtTimer(), Future);
}

XREDIS_API xredisadmission xredisClientTransaction(xredisclient* Client,
    const xredistransactioncommand* Commands, size_t Count,
    const xredistransactionconfig* Config, int64 Timeout, xfuture** Future)
{
    return __xredisClientTransaction(Client, Commands, Count, Config, Timeout, xrtTimer(), Future);
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/watch/watch.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_WATCH)
#include <math.h>
#include <string.h>

#if defined(XREDIS_FEATURE_XREDIS_WATCH)
enum { XREDIS_W_WATCH, XREDIS_W_COMMIT, XREDIS_W_UNWATCH, XREDIS_W_CLIENT_CLOSED, XREDIS_W_SLOTS };
struct xrediswatch {
    xatomic32 References, Reason, Abandoned;
    xmutex Gate;
    xredisexclusiveowner Owner;
    xrediswatchconfig Config;
    xredisclient* Client;
    xnetengine* Engine;
    xnetworker* Worker;
    xnetpost Work;
    xfuturewatch Watches[XREDIS_W_SLOTS];
    xfuture* Futures[XREDIS_W_SLOTS];
    bool Registered[XREDIS_W_SLOTS];
    xpromise *StartPromise, *ClosePromise;
    xfuture *StartFuture, *CloseFuture;
    xrediswatchresult* StartResult;
    xrediswatchclosedresult* CloseResult;
    xcancelwatch* Cancel;
    uint64 Timer;
    size_t APIs;
    bool Initialized, Queued, Finished, Armed, WatchSeen, CommitSeen, Cleared, UnwatchSeen;
};

static void xredisWatchWork(xnetworker* Worker, ptr Data);
static void xredisWatchWake(ptr Data);
static bool xredisWatchReport(xerrkind Kind, cstr Text)
{ xrtSetErrorInfo(Kind, "xrt.redis", Kind == XERR_STATE ? XREDIS_CLIENT_STATE : XREDIS_CLIENT_ARGUMENT, Text); return false; }
static void xredisWatchRef(ptr Data)
{ (void)xrtAtomic32FetchAdd(&((xrediswatch*)Data)->References, 1, XMEMORY_RELAXED); }

XREDIS_API bool xredisWatchRetain(xrediswatch* Watch)
{
    if (Watch == NULL) return xredisWatchReport(XERR_ARGUMENT, "watch is NULL");
    xredisWatchRef(Watch); return true;
}
XREDIS_API void xredisWatchRelease(xrediswatch* Watch)
{
    if (Watch == NULL || xrtAtomic32FetchSub(&Watch->References, 1, XMEMORY_ACQ_REL) != 1) return;
    xrtCancelUnwatch(Watch->Cancel);
    xrtPromiseDestroy(Watch->StartPromise); xrtFutureDestroy(Watch->StartFuture);
    xrtPromiseDestroy(Watch->ClosePromise); xrtFutureDestroy(Watch->CloseFuture);
    xrtFree(Watch->StartResult); xrtFree(Watch->CloseResult);
    xrtMutexUnit(&Watch->Gate); xrtFree(Watch);
}
static void xredisWatchDrop(ptr Data) { xredisWatchRelease(Data); }

/* Notifications never call the session while holding the client Gate. The
 * separate engine Pin protects posts even after an external ClientClose. */
static void xredisWatchWake(ptr Data)
{
    xrediswatch* Watch = Data; xrtMutexLock(&Watch->Gate);
    if (Watch->Initialized && !Watch->Finished && !Watch->Queued) {
        Watch->Queued = true; xredisWatchRef(Watch);
        if (!xrtNetPost(Watch->Worker, &Watch->Work, xredisWatchWork, Watch)) {
            Watch->Queued = false; xredisWatchRelease(Watch);
        }
    }
    xrtMutexUnlock(&Watch->Gate);
}
static void xredisWatchReason(xrediswatch* Watch, xredisrequeststatus Status)
{
    uint32 Expected = 0;
    (void)xrtAtomic32CompareExchange(&Watch->Reason, &Expected, (uint32)Status + 1u, XMEMORY_ACQ_REL, XMEMORY_ACQUIRE);
}
static void xredisWatchStop(xrediswatch* Watch, xredisrequeststatus Status)
{
    xrtMutexLock(&Watch->Gate);
    if (!Watch->Finished) {
        xredisWatchReason(Watch, Status);
        xrtAtomic32Store(&Watch->Abandoned, 1, XMEMORY_RELEASE);
        xrtAtomic32Store(&Watch->Owner.State, XREDIS_OWNER_CLOSING, XMEMORY_RELEASE);
    }
    xrtMutexUnlock(&Watch->Gate); xredisWatchWake(Watch);
}
static void xredisWatchCancel(ptr Data) { xredisWatchStop(Data, XREDIS_REQUEST_CANCELLED); }
static void xredisWatchTimer(xnetworker* Worker, uint64 Id, xnetresult Result, ptr Data)
{
    (void)Worker; xrediswatch* Watch = Data;
    if (Watch->Timer == Id) Watch->Timer = 0;
    if (Result == XNET_RESULT_OK) xredisWatchStop(Watch, XREDIS_REQUEST_TIMEOUT);
    xredisWatchWake(Watch);
    xredisWatchRelease(Watch);
}
static bool xredisWatchObserve(xrediswatch* Watch, unsigned Slot)
{
    xredisWatchRef(Watch);
    xfuturewatchresult Result = xrtFutureWatchAdd(Watch->Futures[Slot], &Watch->Watches[Slot]);
    Watch->Registered[Slot] = Result == XFUTURE_WATCH_PENDING;
    if (Result != XFUTURE_WATCH_PENDING) xredisWatchRelease(Watch);
    return Result != XFUTURE_WATCH_ERROR;
}
static bool xredisWatchOK(const xredisrequestresult* Result)
{
    xredisreplyvalue Root;
    return Result && Result->Status == XREDIS_REQUEST_OK && Result->Reply &&
        xredisReplyGet(Result->Reply, 0, &Root) && Root.Type == XREDIS_REPLY_SIMPLE_STRING &&
        Root.Data.Size == 2 && memcmp(Root.Data.Data, "OK", 2) == 0;
}
static const xredisrequestresult* xredisWatchOutcome(xfuture* Future)
{
    return Future && xrtFutureState(Future) != XFUTURE_PENDING ? xrtFutureValue(Future) : NULL;
}
static void xredisWatchResultDrop(ptr Value, ptr Data)
{
    (void)Data; xrediswatchresult* Result = Value;
    xredisReplyRelease(Result->Request.Reply); xrtErrorFree((xerror*)Result->Request.Cause);
    xredisWatchRelease(Result->Watch); xrtFree(Result);
}
static void xredisWatchCloseResultDrop(ptr Value, ptr Data) { (void)Data; xrtFree(Value); }

static void xredisWatchPublish(xrediswatch* Watch, xredisrequeststatus Status, const xredisrequestresult* Source)
{
    if (Watch->StartPromise == NULL) return;
    xrtCancelUnwatch(Watch->Cancel); Watch->Cancel = NULL;
    xpromise* Promise = Watch->StartPromise; Watch->StartPromise = NULL;
    xrediswatchresult* Result = Watch->StartResult; Watch->StartResult = NULL;
    Result->Request = (xredisrequestresult){Status, Source ? Source->Delivery : XREDIS_MAY_HAVE_EXECUTED,
        Source ? Source->Reply : NULL, Source ? xrtErrorRef(Source->Cause) : NULL};
    if (Result->Request.Reply) xredisReplyRetain(Result->Request.Reply);
    /* Even a failed/cancelled start must expose its physical cleanup barrier.
     * The inert handle retains metadata, not the client/engine after Closed. */
    xredisWatchRef(Watch); Result->Watch = Watch;
    xfuture* Future = Watch->StartFuture; Watch->StartFuture = NULL;
    if (!xrtPromiseResolveOwned(Promise, Result, xredisWatchResultDrop, NULL)) xredisWatchResultDrop(Result, NULL);
    xrtPromiseDestroy(Promise); xrtFutureDestroy(Future);
}

/* Called only by the owning worker, with all native reservations physically
 * idle. Remove notifications before releasing the independent engine Pin.
 * A post queued by any racing notification/API must run first. */
static void xredisWatchFinish(xrediswatch* Watch, bool Reusable)
{
    /* Sealing prevents new APIs; an already admitted commit may still be
     * attaching its observer. Do not inspect/detach that storage until its
     * API tail has published and left. No wait occurs under either Gate. */
    xrtMutexLock(&Watch->Gate);
    bool Busy = Watch->APIs != 0;
    xrtMutexUnlock(&Watch->Gate);
    if (Busy) return;
    if (Watch->Timer != 0) (void)xrtNetEngineTimerCancelCurrent(Watch->Engine, Watch->Timer);
    xrtCancelUnwatch(Watch->Cancel); Watch->Cancel = NULL;
    for (unsigned Slot = 0; Slot < XREDIS_W_SLOTS; ++Slot) if (Watch->Registered[Slot]) {
        xrtFutureWatchRemove(Watch->Futures[Slot], &Watch->Watches[Slot]); Watch->Registered[Slot] = false;
    }
    xrtMutexLock(&Watch->Gate);
    if (Watch->APIs || Watch->Queued || Watch->Timer != 0) { xrtMutexUnlock(&Watch->Gate); return; }
    if (!__xredisClientUnclaim(Watch->Client, &Watch->Owner)) { xrtMutexUnlock(&Watch->Gate); return; }
    Watch->Finished = true; xrtAtomic32Store(&Watch->Owner.State, XREDIS_OWNER_CLOSED, XMEMORY_RELEASE);
    xredisclient* Client = Watch->Client; Watch->Client = NULL;
    xnetengine* Engine = Watch->Engine; Watch->Engine = NULL; Watch->Worker = NULL;
    xrtMutexUnlock(&Watch->Gate);
    for (unsigned Slot = 0; Slot < XREDIS_W_SLOTS; ++Slot) {
        xrtFutureDestroy(Watch->Futures[Slot]); Watch->Futures[Slot] = NULL;
    }
    xredisClientRelease(Client); xrtNetEngineUnpin(Engine);
    xrediswatchclosedresult* Result = Watch->CloseResult; Watch->CloseResult = NULL;
    *Result = (xrediswatchclosedresult){(xredisrequeststatus)(xrtAtomic32Load(&Watch->Reason, XMEMORY_ACQUIRE) - 1u), Reusable};
    xpromise* Promise = Watch->ClosePromise; Watch->ClosePromise = NULL;
    if (!xrtPromiseResolveOwned(Promise, Result, xredisWatchCloseResultDrop, NULL)) xrtFree(Result);
    xrtPromiseDestroy(Promise);
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
    /* The parent's pool/lease barrier must include this scope's own Pin,
     * observers and physical Closed publication, not just native request=0. */
    __xredisClientChildDone(&Watch->Owner);
#endif
    xredisWatchRelease(Watch); /* Active lifetime. */
}

static void xredisWatchWork(xnetworker* Worker, ptr Data)
{
    (void)Worker; xrediswatch* Watch = Data;
    xrtMutexLock(&Watch->Gate); Watch->Queued = false;
    bool Busy = Watch->APIs != 0; xfuture* Commit = Watch->Futures[XREDIS_W_COMMIT];
    xrtMutexUnlock(&Watch->Gate);
    if (Busy) { xredisWatchRelease(Watch); return; }
    if (!Watch->Armed) {
        Watch->Armed = true; xredisWatchRef(Watch);
        Watch->Timer = __xrtNetEngineSchedule(Watch->Engine, xrtNetWorkerIndex(Watch->Worker), Watch->Owner.Deadline, xredisWatchTimer, Watch);
        if (!Watch->Timer) { xredisWatchRelease(Watch); xredisWatchStop(Watch, XREDIS_REQUEST_MEMORY); }
    }
    if (!xrtAtomic32Load(&Watch->Reason, XMEMORY_ACQUIRE) && xrtTimer() >= Watch->Owner.Deadline)
        xredisWatchStop(Watch, XREDIS_REQUEST_TIMEOUT);
    const xredisrequestresult* Start = xredisWatchOutcome(Watch->Futures[XREDIS_W_WATCH]);
    if (Start && !Watch->WatchSeen) {
        Watch->WatchSeen = true;
        uint32 Expected = XREDIS_OWNER_STARTING;
        if (xredisWatchOK(Start) && !xrtAtomic32Load(&Watch->Reason, XMEMORY_ACQUIRE) &&
            xrtAtomic32CompareExchange(&Watch->Owner.State, &Expected, XREDIS_OWNER_ACTIVE, XMEMORY_ACQ_REL, XMEMORY_ACQUIRE)) {
            xredisWatchPublish(Watch, XREDIS_REQUEST_OK, Start);
        } else {
            uint32 Stopped = xrtAtomic32Load(&Watch->Reason, XMEMORY_ACQUIRE);
            xredisrequeststatus Status = Stopped ? (xredisrequeststatus)(Stopped - 1u) :
                Start->Status == XREDIS_REQUEST_OK ? XREDIS_REQUEST_PROTOCOL : Start->Status;
            xredisWatchReason(Watch, Status);
            xrtAtomic32Store(&Watch->Owner.State, XREDIS_OWNER_CLOSING, XMEMORY_RELEASE);
            if (!Stopped && Status == XREDIS_REQUEST_PROTOCOL) xredisClientClose(Watch->Client);
        }
    }
    const xredistransactionresult* Transaction = (const xredistransactionresult*)xredisWatchOutcome(Commit);
    if (Transaction && !Watch->CommitSeen) {
        Watch->CommitSeen = true; xredisWatchReason(Watch, Transaction->Request.Status);
        xrtAtomic32Store(&Watch->Owner.State, XREDIS_OWNER_CLOSING, XMEMORY_RELEASE);
        Watch->Cleared = Transaction->Execution == XREDIS_TRANSACTION_EXECUTED || Transaction->Execution == XREDIS_TRANSACTION_ABORTED ||
            (Transaction->Request.Status == XREDIS_REQUEST_SERVER_ERROR && Transaction->QueueErrors != 0 &&
             Transaction->Execution == XREDIS_TRANSACTION_NOT_EXECUTED);
        if (Transaction->Request.Status == XREDIS_REQUEST_TIMEOUT || Transaction->Request.Status == XREDIS_REQUEST_CANCELLED)
            xredisWatchStop(Watch, Transaction->Request.Status);
    }
    uint32 Reason = xrtAtomic32Load(&Watch->Reason, XMEMORY_ACQUIRE);
    xredisclientinfo Info = {0}; Info.Size = sizeof(Info); xredisClientInfo(Watch->Client, &Info);
    if (Info.State >= XREDIS_CLIENT_CLOSING && !Reason) {
        xredisWatchReason(Watch, Info.CloseStatus); Reason = xrtAtomic32Load(&Watch->Reason, XMEMORY_ACQUIRE);
        xrtAtomic32Store(&Watch->Owner.State, XREDIS_OWNER_CLOSING, XMEMORY_RELEASE);
    }
    if (!Reason) { xredisWatchRelease(Watch); return; }
    if (Watch->Timer != 0) (void)xrtNetEngineTimerCancelCurrent(Watch->Engine, Watch->Timer);
    xredisWatchPublish(Watch, (xredisrequeststatus)(Reason - 1u), Start);
    if (xrtAtomic32Load(&Watch->Abandoned, XMEMORY_ACQUIRE)) {
        if (Watch->Futures[XREDIS_W_WATCH]) xrtFutureCancel(Watch->Futures[XREDIS_W_WATCH]);
        if (Commit) xrtFutureCancel(Commit);
        /* Once cleanup itself owns the last native reservation, allow its
         * bounded deadline/drain to finish. Repeated Abort is idempotent. */
        if (Info.Requests && Watch->Futures[XREDIS_W_UNWATCH] == NULL) xredisClientClose(Watch->Client);
        xredisClientInfo(Watch->Client, &Info);
    }
    if (Info.State >= XREDIS_CLIENT_CLOSING) {
        if (!Info.Requests && xrtFutureState(Watch->Futures[XREDIS_W_CLIENT_CLOSED]) != XFUTURE_PENDING) xredisWatchFinish(Watch, false);
    } else if (!Info.Requests) {
        if (Watch->Cleared) xredisWatchFinish(Watch, true);
        else if (Watch->Futures[XREDIS_W_UNWATCH] == NULL) {
            xbytesview Word = XRT_BYTES_LITERAL("UNWATCH");
            xredisadmission Admission = __xredisClientOwnedCall(Watch->Client, &Watch->Owner, &Word, 1,
                Watch->Config.CleanupTimeout, xrtTimer(), true, &Watch->Futures[XREDIS_W_UNWATCH]);
            if (Admission != XREDIS_ADMISSION_ACCEPTED || !xredisWatchObserve(Watch, XREDIS_W_UNWATCH)) xredisClientClose(Watch->Client);
        } else if (!Watch->UnwatchSeen) {
            const xredisrequestresult* Result = xredisWatchOutcome(Watch->Futures[XREDIS_W_UNWATCH]);
            if (Result) {
                Watch->UnwatchSeen = true;
                if (xredisWatchOK(Result)) { Watch->Cleared = true; xredisWatchFinish(Watch, true); }
                else xredisClientClose(Watch->Client);
            }
        }
    }
    xredisWatchRelease(Watch);
}

XREDIS_API bool xredisWatchConfigInit(xrediswatchconfig* Config)
{
    if (Config == NULL || !xrtMemRangeValid(Config, sizeof(*Config))) return xredisWatchReport(XERR_ARGUMENT, "invalid watch config");
    *Config = (xrediswatchconfig){sizeof(*Config), 5000, 5000, 256, 65536, {sizeof(xredistransactionconfig), XREDIS_TRANSACTION_COMMANDS_DEFAULT}};
    return true;
}

static xredisadmission xredisWatchBegin(xredisclient* Client, const xbytesview* Keys, size_t Count,
    const xrediswatchconfig* Config, xfuture** Future, double Entered, xredisexclusiveowner* Parent)
{
    xrediswatchconfig Options;
    if (Client == NULL || Future == NULL || !xrtMemRangeValid(Future, sizeof(*Future)) ||
        (Config && (!xrtMemRangeValid(Config, sizeof(*Config)) || xrtMemRangesOverlap(Config, sizeof(*Config), Future, sizeof(*Future))))) {
        xredisWatchReport(XERR_ARGUMENT, "invalid watch arguments/output"); return XREDIS_ADMISSION_ERROR;
    }
    if (Config == NULL) xredisWatchConfigInit(&Options); else Options = *Config;
    if (Options.Size != sizeof(Options) || Options.Timeout <= 0 || Options.Timeout > 86400000 ||
        Options.CleanupTimeout <= 0 || Options.CleanupTimeout > 60000 || !Options.MaxKeys || Options.MaxKeys > 65536 ||
        !Options.MaxWireBytes || !Count || Count > Options.MaxKeys || !Keys || !xrtMemRangeValid(Keys, Count * sizeof(*Keys)) ||
        xrtMemRangesOverlap(Keys, Count * sizeof(*Keys), Future, sizeof(*Future)) ||
        Options.Transaction.Size != sizeof(Options.Transaction) || !Options.Transaction.MaxCommands || Options.Transaction.MaxCommands > XREDIS_TRANSACTION_COMMANDS_MAX) {
        xredisWatchReport(XERR_ARGUMENT, "invalid watch config/keys"); return XREDIS_ADMISSION_ERROR;
    }
    for (size_t Index = 0; Index < Count; ++Index) if (!xrtMemRangeValid(Keys[Index].Data, Keys[Index].Size) ||
        xrtMemRangesOverlap(Keys[Index].Data, Keys[Index].Size, Future, sizeof(*Future))) {
        xredisWatchReport(XERR_ARGUMENT, "invalid watch key/output alias"); return XREDIS_ADMISSION_ERROR;
    }
    xbytesview* Arguments = xrtMalloc((Count + 1u) * sizeof(*Arguments));
    if (!Arguments) return XREDIS_ADMISSION_ERROR;
    Arguments[0] = (xbytesview)XRT_BYTES_LITERAL("WATCH"); memcpy(Arguments + 1, Keys, Count * sizeof(*Keys));
    size_t Bytes = 0;
    if (!xredisCommandSize(Arguments, Count + 1u, &Bytes) || Bytes > Options.MaxWireBytes) {
        xrtFree(Arguments); xredisWatchReport(XERR_RANGE, "WATCH exceeds wire limit"); return XREDIS_ADMISSION_ERROR;
    }
    xrediswatch* Watch = xrtMalloc(sizeof(*Watch));
    if (!Watch) { xrtFree(Arguments); return XREDIS_ADMISSION_ERROR; }
    memset(Watch, 0, sizeof(*Watch));
    if (!xrtMutexInit(&Watch->Gate)) { xrtFree(Watch); xrtFree(Arguments); return XREDIS_ADMISSION_ERROR; }
    xrtAtomic32Init(&Watch->References, 1); xrtAtomic32Init(&Watch->Reason, 0); xrtAtomic32Init(&Watch->Abandoned, 0);
    Watch->Config = Options; Watch->Owner.Data = Watch;
    xrtAtomic32Init(&Watch->Owner.State, XREDIS_OWNER_STARTING);
    Watch->Owner.Deadline = nextafter(Entered + (double)Options.Timeout * 0.001, INFINITY);
    Watch->Owner.Retain = xredisWatchRef; Watch->Owner.Release = xredisWatchDrop; Watch->Owner.Wake = xredisWatchWake;
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
    Watch->Owner.Abandon = xredisWatchCancel;
    xrtAtomic32Init(&Watch->Owner.Nested, 0);
#else
    (void)Parent;
#endif
    Watch->StartResult = xrtMalloc(sizeof(*Watch->StartResult)); Watch->CloseResult = xrtMalloc(sizeof(*Watch->CloseResult));
    if (Watch->StartResult) memset(Watch->StartResult, 0, sizeof(*Watch->StartResult));
    bool Ready = Watch->StartResult && Watch->CloseResult && xrtNetPostInit(&Watch->Work) &&
        (Watch->StartPromise = xrtPromiseCreate(&Watch->StartFuture, NULL)) &&
        (Watch->ClosePromise = xrtPromiseCreate(&Watch->CloseFuture, NULL));
    for (unsigned Slot = 0; Ready && Slot < XREDIS_W_SLOTS; ++Slot)
        Ready = xrtFutureWatchInit(&Watch->Watches[Slot], xredisWatchWake, xredisWatchDrop, Watch);
    if (Ready) {
        xcancel* Token = xrtFutureCancelToken(Watch->StartFuture);
        if (Token) Watch->Cancel = xrtCancelWatch(Token, xredisWatchCancel, Watch);
        xrtCancelDestroy(Token); Ready = Watch->Cancel != NULL;
    }
    xredisadmission Admission = XREDIS_ADMISSION_ERROR;
    if (Ready) {
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
        if (Parent != NULL) Admission = __xredisClientClaimNested(Client, Parent, &Watch->Owner, &Watch->Engine, &Watch->Worker, Future);
        else
#endif
        Admission = __xredisClientClaim(Client, &Watch->Owner, &Watch->Engine, &Watch->Worker, Future);
    }
    if (Admission == XREDIS_ADMISSION_ACCEPTED) {
        Watch->Client = Client; Watch->Futures[XREDIS_W_CLIENT_CLOSED] = xredisClientClosed(Client);
        Admission = __xredisClientOwnedCall(Client, &Watch->Owner, Arguments, Count + 1u, Options.Timeout, Entered, true, &Watch->Futures[XREDIS_W_WATCH]);
        if (Admission != XREDIS_ADMISSION_ACCEPTED) {
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
            (void)__xredisClientUnclaimUnused(Client, &Watch->Owner);
#else
            (void)__xredisClientUnclaim(Client, &Watch->Owner);
#endif
            xredisClientRelease(Client); xrtNetEngineUnpin(Watch->Engine);
            xrtFutureDestroy(Watch->Futures[XREDIS_W_CLIENT_CLOSED]); Watch->Client = NULL; Watch->Engine = NULL;
        }
    }
    xrtFree(Arguments);
    if (Admission != XREDIS_ADMISSION_ACCEPTED) { xredisWatchRelease(Watch); return Admission; }
    /* Native acceptance is the effects boundary. Every subsequent failure is
     * a typed asynchronous outcome with physical cleanup, never ERROR. */
    bool Observed = xredisWatchObserve(Watch, XREDIS_W_WATCH) && xredisWatchObserve(Watch, XREDIS_W_CLIENT_CLOSED);
    xfuture* Observer = xrtFutureRef(Watch->StartFuture);
    xredisWatchRef(Watch); /* Constructor tail may race an immediate failure. */
    xrtMutexLock(&Watch->Gate); Watch->Initialized = true; xrtMutexUnlock(&Watch->Gate);
    if (!Observed) xredisWatchStop(Watch, XREDIS_REQUEST_MEMORY);
    *Future = Observer; xredisWatchWake(Watch); xredisWatchRelease(Watch); return XREDIS_ADMISSION_ACCEPTED;
}
XREDIS_API xredisadmission xredisClientWatchAsync(xredisclient* Client, const xbytesview* Keys,
    size_t Count, const xrediswatchconfig* Config, xfuture** Future)
{ return xredisWatchBegin(Client, Keys, Count, Config, Future, xrtTimer(), NULL); }
XREDIS_API xredisadmission xredisClientWatch(xredisclient* Client, const xbytesview* Keys,
    size_t Count, const xrediswatchconfig* Config, xfuture** Future)
{
    double Entered = xrtTimer();
    if (Client && __xredisClientIsWorker(Client)) { xredisWatchReport(XERR_STATE, "sync WATCH cannot block an engine worker"); return XREDIS_ADMISSION_ERROR; }
    xredisadmission Admission = xredisWatchBegin(Client, Keys, Count, Config, Future, Entered, NULL);
    if (Admission == XREDIS_ADMISSION_ACCEPTED) (void)xrtFutureWait(*Future);
    return Admission;
}
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
xredisadmission __xredisWatchBeginOwned(xredisclient* Client, xredisexclusiveowner* Parent,
    const xbytesview* Keys, size_t Count, const xrediswatchconfig* Config, xfuture** Future, double Entered)
{ return xredisWatchBegin(Client, Keys, Count, Config, Future, Entered, Parent); }
#endif

/* APIs protect the engine/client borrow through encoding and final admission.
 * Commit seals under the native client Gate, not at the start of encoding. */
static xredisadmission xredisWatchEnter(xrediswatch* Watch, bool Sync, xredisclient** Client)
{
    if (!Watch) { xredisWatchReport(XERR_ARGUMENT, "watch is NULL"); return XREDIS_ADMISSION_ERROR; }
    xrtMutexLock(&Watch->Gate);
    xredisadmission Admission = Watch->Finished || xrtAtomic32Load(&Watch->Owner.State, XMEMORY_ACQUIRE) != XREDIS_OWNER_ACTIVE ?
        XREDIS_ADMISSION_CLOSED : XREDIS_ADMISSION_ACCEPTED;
    if (Admission == XREDIS_ADMISSION_ACCEPTED) { ++Watch->APIs; *Client = Watch->Client; }
    xrtMutexUnlock(&Watch->Gate);
    if (Admission == XREDIS_ADMISSION_ACCEPTED && Sync && __xredisClientIsWorker(*Client)) {
        xrtMutexLock(&Watch->Gate); --Watch->APIs; xrtMutexUnlock(&Watch->Gate); xredisWatchWake(Watch);
        xredisWatchReport(XERR_STATE, "sync WATCH operation cannot block an engine worker"); return XREDIS_ADMISSION_ERROR;
    }
    return Admission;
}
static void xredisWatchLeave(xrediswatch* Watch)
{ xrtMutexLock(&Watch->Gate); --Watch->APIs; xrtMutexUnlock(&Watch->Gate); xredisWatchWake(Watch); }

static xredisadmission xredisWatchCallAdmit(xrediswatch* Watch, const xbytesview* Arguments, size_t Count,
    int64 Timeout, xfuture** Future, bool Sync, double Entered)
{
    if (!Future || !xrtMemRangeValid(Future, sizeof(*Future)) ||
        xrtMemRangesOverlap(Watch, Watch ? sizeof(*Watch) : 0, Future, sizeof(*Future))) {
        xredisWatchReport(XERR_ARGUMENT, "invalid watch operation output"); return XREDIS_ADMISSION_ERROR;
    }
    xredisclient* Client = NULL; xredisadmission Admission = xredisWatchEnter(Watch, Sync, &Client);
    if (Admission != XREDIS_ADMISSION_ACCEPTED) return Admission;
    Admission = __xredisClientOwnedCall(Client, &Watch->Owner, Arguments, Count, Timeout, Entered, false, Future);
    xredisWatchLeave(Watch);
    if (Sync && Admission == XREDIS_ADMISSION_ACCEPTED) (void)xrtFutureWait(*Future);
    return Admission;
}
XREDIS_API xredisadmission xredisWatchCallAsync(xrediswatch* Watch, const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future)
{ return xredisWatchCallAdmit(Watch, Arguments, Count, Timeout, Future, false, xrtTimer()); }
XREDIS_API xredisadmission xredisWatchCall(xrediswatch* Watch, const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future)
{ return xredisWatchCallAdmit(Watch, Arguments, Count, Timeout, Future, true, xrtTimer()); }

static xredisadmission xredisWatchCommitAdmit(xrediswatch* Watch, const xredistransactioncommand* Commands,
    size_t Count, int64 Timeout, xfuture** Future, bool Sync, double Entered)
{
    if (!Future || !xrtMemRangeValid(Future, sizeof(*Future)) ||
        xrtMemRangesOverlap(Watch, Watch ? sizeof(*Watch) : 0, Future, sizeof(*Future))) {
        xredisWatchReport(XERR_ARGUMENT, "invalid watch operation output"); return XREDIS_ADMISSION_ERROR;
    }
    xredisclient* Client = NULL; xredisadmission Admission = xredisWatchEnter(Watch, Sync, &Client);
    if (Admission != XREDIS_ADMISSION_ACCEPTED) return Admission;
    Admission = __xredisClientOwnedTransaction(Client, &Watch->Owner, Commands, Count, &Watch->Config.Transaction, Timeout, Entered, Future);
    if (Admission == XREDIS_ADMISSION_ACCEPTED) {
        xrtMutexLock(&Watch->Gate); Watch->Futures[XREDIS_W_COMMIT] = xrtFutureRef(*Future); xrtMutexUnlock(&Watch->Gate);
        if (!xredisWatchObserve(Watch, XREDIS_W_COMMIT)) xredisWatchStop(Watch, XREDIS_REQUEST_MEMORY);
    }
    xredisWatchLeave(Watch);
    if (Sync && Admission == XREDIS_ADMISSION_ACCEPTED) (void)xrtFutureWait(*Future);
    return Admission;
}
XREDIS_API xredisadmission xredisWatchCommitAsync(xrediswatch* Watch, const xredistransactioncommand* Commands,
    size_t Count, int64 Timeout, xfuture** Future)
{ return xredisWatchCommitAdmit(Watch, Commands, Count, Timeout, Future, false, xrtTimer()); }
XREDIS_API xredisadmission xredisWatchCommit(xrediswatch* Watch, const xredistransactioncommand* Commands,
    size_t Count, int64 Timeout, xfuture** Future)
{ return xredisWatchCommitAdmit(Watch, Commands, Count, Timeout, Future, true, xrtTimer()); }
XREDIS_API bool xredisWatchAbort(xrediswatch* Watch)
{
    if (!Watch) return xredisWatchReport(XERR_ARGUMENT, "watch is NULL");
    xredisWatchStop(Watch, XREDIS_REQUEST_CANCELLED); return true;
}
XREDIS_API xfuture* xredisWatchClosed(xrediswatch* Watch)
{
    if (!Watch) { xredisWatchReport(XERR_ARGUMENT, "watch is NULL"); return NULL; }
    return xrtFutureRef(Watch->CloseFuture);
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/pool_watch/pool_watch.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
XREDIS_API xredisadmission xredisLeaseWatchAsync(xredislease* Lease, const xbytesview* Keys,
    size_t Count, const xrediswatchconfig* Config, xfuture** Future)
{ return __xredisLeaseWatch(Lease, Keys, Count, Config, Future, xrtTimer(), false); }
XREDIS_API xredisadmission xredisLeaseWatch(xredislease* Lease, const xbytesview* Keys,
    size_t Count, const xrediswatchconfig* Config, xfuture** Future)
{ return __xredisLeaseWatch(Lease, Keys, Count, Config, Future, xrtTimer(), true); }
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/pool_transaction/pool_transaction.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_POOL_TRANSACTION)
#if defined(XREDIS_FEATURE_XREDIS_POOL_TRANSACTION)
XREDIS_API xredisadmission xredisLeaseTransactionAsync(xredislease* Lease, const xredistransactioncommand* Commands,
    size_t Count, const xredistransactionconfig* Config, int64 Timeout, xfuture** Future)
{ return __xredisLeaseTransaction(Lease, Commands, Count, Config, Timeout, Future, xrtTimer(), false); }
XREDIS_API xredisadmission xredisLeaseTransaction(xredislease* Lease, const xredistransactioncommand* Commands,
    size_t Count, const xredistransactionconfig* Config, int64 Timeout, xfuture** Future)
{ return __xredisLeaseTransaction(Lease, Commands, Count, Config, Timeout, Future, xrtTimer(), true); }
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/dial/dial.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_DIAL)
#if defined(XREDIS_FEATURE_XREDIS_TLS)
#endif
#if defined(XREDIS_FEATURE_XREDIS_TLS_AUTH)
#endif
#include <string.h>

#if defined(XREDIS_FEATURE_XREDIS_DIAL)
typedef struct xredisconnectdata {
	xredisconnectplan Plan;
	xredisclient* Client; /* Borrowed through this source's retirement callback. */
	xnetresolver* Resolver;
	xnetaddrlist* Addresses;
	xnetdial* Dial;
	xnetdialconfig Config;
	uint16 Port;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	xredistlsplan TLS;
#endif
	char Host[];
} xredisconnectdata;

static bool xredisDialArgument(cstr Message)
{ xrtSetErrorInfo(XERR_ARGUMENT, "xrt.redis", XREDIS_CLIENT_ARGUMENT, Message); return false; }

XREDIS_API bool xredisClientDialConfigInit(xredisclientdialconfig* Config)
{
	if (Config == NULL || !xrtMemRangeValid(Config, sizeof(*Config))) return xredisDialArgument("invalid Dial config output");
	*Config = (xredisclientdialconfig){sizeof(*Config), XNET_FAMILY_UNSPEC, 30000, 250, 8, 1024}; return true;
}

static void xredisDialDone(xnetdial* Dial, xnetresult Result, xnetstream* Stream, const xerror* Error, ptr Data)
{
	(void)Dial; xredisconnectdata* Context = (xredisconnectdata*)Data;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Context->TLS.Session != NULL && Stream != NULL) {
		if (__xredisClientConnectTls(Context->Client, Stream, &Context->TLS)) Context->TLS.Session = NULL;
	} else
#endif
	__xredisClientConnectDone(Context->Client, Result, Stream, Error);
	/* Last access. This same Worker's queued client turn executes after the
	 * native callback's return; no fabricated owned/collector policy is used. */
	__xredisClientConnectRetired(Context->Client);
}

static bool xredisDialStart(ptr Data, xredisclient* Client, xnetengine* Engine, uint64 Affinity,
	const xnetstreamconfig* Config, const xnetstreamevents* Events)
{
	xredisconnectdata* Context = (xredisconnectdata*)Data; Context->Client = Client;
	Context->Config.Stream = *Config; Context->Config.Affinity = Affinity;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Context->TLS.Session != NULL) Events = __xredisClientTlsPendingEvents();
#endif
	if (Context->Addresses != NULL) Context->Dial = xrtNetDialAddresses(Engine, Context->Addresses, &Context->Config, Events, Client, xredisDialDone, Context);
	else Context->Dial = xrtNetDial(Engine, Context->Resolver, Context->Host, Context->Port, &Context->Config, Events, Client, xredisDialDone, Context);
	if (Context->Dial == NULL) return false;
	xrtNetAddrListDestroy(Context->Addresses); Context->Addresses = NULL; return true;
}

static void xredisDialCancel(ptr Data)
{ xredisconnectdata* Context = (xredisconnectdata*)Data; (void)xrtNetDialCancel(Context->Dial); }

static void xredisDialDrop(ptr Data)
{
	xredisconnectdata* Context = (xredisconnectdata*)Data;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	xrtTlsSessionDestroy(Context->TLS.Session);
#endif
	xrtNetDialDestroy(Context->Dial); xrtNetAddrListDestroy(Context->Addresses); xrtFree(Context);
}

static bool xredisDialCreate(xnetengine* Engine, xnetresolver* Resolver, xnetaddrlist* Addresses,
	xstrview Host, uint16 Port, uint64 Affinity, const xredisclientconfig* Config,
	const xredisclientinitconfig* Init, const xredisclientdialconfig* Dial,
	const xredisclienttlsconfig* TLS, const xtlsclientauthconfigv1* Auth, double Entered, xredisclient** Result)
{
	if (Engine == NULL || Result == NULL || !xrtMemRangeValid(Result, sizeof(*Result)) ||
		(Dial != NULL && (!xrtMemRangeValid(Dial, sizeof(*Dial)) || xrtMemRangesOverlap(Dial, sizeof(*Dial), Result, sizeof(*Result)))))
		return xredisDialArgument("invalid Dial arguments or config/output overlap");
	xredisclientdialconfig Policy;
	if (Dial == NULL) xredisClientDialConfigInit(&Policy); else Policy = *Dial;
	if (Policy.Size != sizeof(Policy) || (Policy.Family != XNET_FAMILY_UNSPEC && Policy.Family != XNET_FAMILY_IPV4 && Policy.Family != XNET_FAMILY_IPV6) ||
		Policy.Timeout < XRT_WAIT_FOREVER || Policy.FallbackDelay < 0 || Policy.MaxAttempts == 0 || Policy.MaxAttempts > XREDIS_DIAL_ADDRESSES_MAX ||
		Policy.MaxHostBytes == 0 || Policy.MaxHostBytes > XREDIS_DIAL_HOST_MAX) return xredisDialArgument("invalid Dial family, deadline or bounds");
	if (Addresses == NULL) {
		if (Resolver == NULL || Port == 0 || Host.Size == 0 || Host.Size > Policy.MaxHostBytes ||
			Host.Size > SIZE_MAX - sizeof(xredisconnectdata) - 1u || !xrtMemRangeValid(Host.Data, Host.Size) ||
			xrtMemRangesOverlap(Host.Data, Host.Size, Result, sizeof(*Result)) || memchr(Host.Data, 0, Host.Size) != NULL)
			return xredisDialArgument("invalid host, port, Resolver or input/output overlap");
	} else {
		size_t Count = xrtNetAddrListCount(Addresses), Eligible = 0;
		if (Count == 0 || Count > XREDIS_DIAL_ADDRESSES_MAX) return xredisDialArgument("resolved endpoint list exceeds bounds");
		for (size_t Index = 0; Index < Count; ++Index) {
			const xnetaddr* Address = xrtNetAddrListGet(Addresses, Index); if (Address == NULL) return false;
			if (xrtMemRangesOverlap(Address, sizeof(*Address), Result, sizeof(*Result))) return xredisDialArgument("endpoint output overlaps address storage");
			if (Policy.Family != XNET_FAMILY_UNSPEC && Address->Family != Policy.Family) continue;
			if (Address->Port == 0) return xredisDialArgument("resolved endpoint port is zero");
			++Eligible;
		}
		if (Eligible == 0) return xredisDialArgument("no endpoint matches the selected family");
	}
	xredisclientinitconfig Defaults;
	if (Init == NULL) { xredisClientInitConfigInit(&Defaults); Init = &Defaults; }
	xredisconnectdata* Context = (xredisconnectdata*)xrtCalloc(1, sizeof(*Context) + Host.Size + 1u);
	if (Context == NULL) return false;
	Context->Plan = (xredisconnectplan){Context, xredisDialStart, xredisDialCancel, xredisDialDrop};
	xrtNetDialConfigInit(&Context->Config); Context->Config.Family = Policy.Family; Context->Config.Timeout = Policy.Timeout;
	Context->Config.FallbackDelay = Policy.FallbackDelay; Context->Config.MaxAttempts = Policy.MaxAttempts;
	Context->Resolver = Resolver; Context->Port = Port;
	if (Addresses != NULL) {
		Context->Addresses = xrtNetAddrListRef(Addresses);
		if (Context->Addresses == NULL) { xredisDialDrop(Context); return false; }
	} else memcpy(Context->Host, Host.Data, Host.Size);
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (TLS != NULL && !__xredisTlsPlanCreate(TLS, Auth, (xstrview){Context->Host, Host.Size}, Result, &Context->TLS)) { xredisDialDrop(Context); return false; }
#else
	(void)TLS; (void)Auth;
#endif
	if (__xredisClientCreatePlanned(Engine, Affinity, Config, Init, Entered, &Context->Plan, Result)) return true;
	xredisDialDrop(Context); return false;
}

XREDIS_API bool xredisClientDial(xnetengine* Engine, xnetresolver* Resolver,
	xstrview Host, uint16 Port, uint64 Affinity, const xredisclientconfig* Config,
	const xredisclientinitconfig* Init, const xredisclientdialconfig* Dial, xredisclient** Result)
{ return xredisDialCreate(Engine, Resolver, NULL, Host, Port, Affinity, Config, Init, Dial, NULL, NULL, xrtTimer(), Result); }

XREDIS_API bool xredisClientDialAddresses(xnetengine* Engine, xnetaddrlist* Addresses,
	uint64 Affinity, const xredisclientconfig* Config, const xredisclientinitconfig* Init,
	const xredisclientdialconfig* Dial, xredisclient** Result)
{
	if (Addresses == NULL) return xredisDialArgument("resolved endpoint list is NULL");
	return xredisDialCreate(Engine, NULL, Addresses, (xstrview){NULL, 0}, 0, Affinity, Config, Init, Dial, NULL, NULL, xrtTimer(), Result);
}
#if defined(XREDIS_FEATURE_XREDIS_TLS)
bool __xredisClientDialTls(xnetengine* Engine, xnetresolver* Resolver, xnetaddrlist* Addresses,
	xstrview Host, uint16 Port, uint64 Affinity, const xredisclientconfig* Config,
	const xredisclientinitconfig* Init, const xredisclienttlsconfig* TLS, double Entered, xredisclient** Result)
{
	/* Read the public TLS descriptor only after its full span/output validation. */
	if (TLS == NULL || !xrtMemRangeValid(TLS, sizeof(*TLS)) || Result == NULL ||
		xrtMemRangesOverlap(TLS, sizeof(*TLS), Result, sizeof(*Result))) return xredisDialArgument("invalid TLS config/output");
	return xredisDialCreate(Engine, Resolver, Addresses, Host, Port, Affinity, Config, Init, &TLS->Dial, TLS, NULL, Entered, Result);
}
#endif
#if defined(XREDIS_FEATURE_XREDIS_TLS_AUTH)
bool __xredisClientDialTlsAuthV1(xnetengine* Engine, xnetresolver* Resolver, xnetaddrlist* Addresses,
	xstrview Host, uint16 Port, uint64 Affinity, const xredisclientconfig* Config,
	const xredisclientinitconfig* Init, const xredisclienttlsconfig* TLS,
	const xtlsclientauthconfigv1* Auth, double Entered, xredisclient** Result)
{
	if (TLS == NULL || !xrtMemRangeValid(TLS, sizeof(*TLS)) || Auth == NULL ||
		!xrtMemRangeValid(Auth, sizeof(*Auth)) || Auth->Size != sizeof(*Auth) || Result == NULL ||
		xrtMemRangesOverlap(TLS, sizeof(*TLS), Result, sizeof(*Result)) ||
		xrtMemRangesOverlap(Auth, sizeof(*Auth), Result, sizeof(*Result)))
		return xredisDialArgument("invalid TLS authentication config/output");
	return xredisDialCreate(Engine, Resolver, Addresses, Host, Port, Affinity, Config, Init, &TLS->Dial, TLS, Auth, Entered, Result);
}
#endif
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/tls/tls.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_TLS)
#if defined(XREDIS_FEATURE_XREDIS_TLS_AUTH)
#endif
#include <string.h>

#if defined(XREDIS_FEATURE_XREDIS_TLS)
static bool xredisTlsArgument(cstr Message)
{ xrtSetErrorInfo(XERR_ARGUMENT, "xrt.redis", XREDIS_CLIENT_ARGUMENT, Message); return false; }

XREDIS_API bool xredisClientTlsConfigInit(xredisclienttlsconfig* Config)
{
	if (Config == NULL || !xrtMemRangeValid(Config, sizeof(*Config))) return xredisTlsArgument("invalid TLS config output");
	memset(Config, 0, sizeof(*Config)); Config->Size = sizeof(*Config);
	xrtTlsClientConfigInit(&Config->Session); xrtTlsStreamConfigInit(&Config->Stream); xredisClientDialConfigInit(&Config->Dial);
	Config->MaxMetadataBytes = 64u * 1024u; Config->WriteBytes = 64u * 1024u; Config->ServerNameFromHost = true; return true;
}

bool __xredisTlsPlanCreate(const xredisclienttlsconfig* TLS, const xtlsclientauthconfigv1* Auth,
	xstrview Host, xredisclient** Output, xredistlsplan* Plan)
{
	if (TLS == NULL || !xrtMemRangeValid(TLS, sizeof(*TLS)) || TLS->Size != sizeof(*TLS) ||
		xrtMemRangesOverlap(TLS, sizeof(*TLS), Output, sizeof(*Output)) || TLS->MaxMetadataBytes == 0 || TLS->MaxMetadataBytes > 1024u * 1024u ||
		TLS->WriteBytes == 0 || TLS->WriteBytes > 1024u * 1024u || TLS->Stream.HandshakeTimeout < 0 || TLS->Stream.CloseTimeout < 0 ||
		TLS->Stream.AsyncBytesLimit == 0 || TLS->Stream.AsyncCountLimit == 0 || TLS->Stream.AsyncBatch == 0 || TLS->Session.ProtocolCount > 256u)
		return xredisTlsArgument("invalid TLS descriptor, bounds or output overlap");
	xtlsclientconfig Session = TLS->Session;
	if (TLS->ServerNameFromHost && Session.ServerName.Size == 0 && Host.Size != 0) {
		xerror* Saved = xrtTakeError(); xnetaddr Numeric;
		bool Literal = xrtNetAddrParse(&Numeric, Host.Data, 1); xrtSetErrorTake(Saved);
		if (!Literal) Session.ServerName = Host;
		else if (Session.VerifyName.Size == 0) Session.VerifyName = Host;
	}
	if (Session.ServerName.Size == 0 && Session.VerifyName.Size == 0) return xredisTlsArgument("TLS peer identity is required");
	size_t Bytes = Session.ProtocolCount * sizeof(xstrview);
	if (!xrtMemRangeValid(Session.Protocols, Bytes) || xrtMemRangesOverlap(Session.Protocols, Bytes, Output, sizeof(*Output)) || Bytes > TLS->MaxMetadataBytes)
		return xredisTlsArgument("invalid TLS ALPN descriptors");
	for (size_t Index = 0; Index < Session.ProtocolCount + 2u; ++Index) {
		xstrview View = Index == 0 ? Session.ServerName : Index == 1 ? Session.VerifyName : Session.Protocols[Index - 2u];
		if (View.Size > TLS->MaxMetadataBytes - Bytes || !xrtMemRangeValid(View.Data, View.Size) ||
			xrtMemRangesOverlap(View.Data, View.Size, Output, sizeof(*Output)) || (Index < 2u && View.Size != 0 && memchr(View.Data, 0, View.Size) != NULL))
			return xredisTlsArgument("invalid or excessive TLS name/ALPN bytes");
		Bytes += View.Size;
	}
#if defined(XREDIS_FEATURE_XREDIS_TLS_AUTH)
	Plan->Session = Auth != NULL ? xrtTlsClientCreateAuthV1(&Session, Auth, NULL) : xrtTlsClientCreate(&Session, NULL);
#else
	(void)Auth; Plan->Session = xrtTlsClientCreate(&Session, NULL);
#endif
	if (Plan->Session == NULL) return false;
	Plan->Config = TLS->Stream; Plan->WriteBytes = TLS->WriteBytes; return true;
}

XREDIS_API bool xredisClientTlsDial(xnetengine* Engine, xnetresolver* Resolver, xstrview Host, uint16 Port, uint64 Affinity,
	const xredisclientconfig* Config, const xredisclientinitconfig* Init, const xredisclienttlsconfig* TLS, xredisclient** Result)
{ return __xredisClientDialTls(Engine, Resolver, NULL, Host, Port, Affinity, Config, Init, TLS, xrtTimer(), Result); }

XREDIS_API bool xredisClientTlsDialAddresses(xnetengine* Engine, xnetaddrlist* Addresses, uint64 Affinity,
	const xredisclientconfig* Config, const xredisclientinitconfig* Init, const xredisclienttlsconfig* TLS, xredisclient** Result)
{
	if (Addresses == NULL) return xredisTlsArgument("TLS endpoint list is NULL");
	return __xredisClientDialTls(Engine, NULL, Addresses, (xstrview){NULL, 0}, 0, Affinity, Config, Init, TLS, xrtTimer(), Result);
}

XREDIS_API bool xredisClientTlsConnect(xnetengine* Engine, const xnetaddr* Remote, uint64 Affinity,
	const xredisclientconfig* Config, const xredisclientinitconfig* Init, const xredisclienttlsconfig* TLS, xredisclient** Result)
{
	double Entered = xrtTimer();
	if (Remote == NULL || Result == NULL || !xrtMemRangeValid(Remote, sizeof(*Remote)) ||
		xrtMemRangesOverlap(Remote, sizeof(*Remote), Result, sizeof(*Result))) return xredisTlsArgument("invalid TLS numeric endpoint/output");
	xnetaddrlist* Addresses = xrtNetAddrListCreate(Remote, 1); if (Addresses == NULL) return false;
	bool Success = __xredisClientDialTls(Engine, NULL, Addresses, (xstrview){NULL, 0}, 0, Affinity, Config, Init, TLS, Entered, Result);
	xrtNetAddrListDestroy(Addresses); return Success;
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/tls_auth/tls_auth.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_TLS_AUTH)

#if defined(XREDIS_FEATURE_XREDIS_TLS_AUTH)
static bool xredisTlsAuthArgument(cstr Message)
{ xrtSetErrorInfo(XERR_ARGUMENT, "xrt.redis", XREDIS_CLIENT_ARGUMENT, Message); return false; }

XREDIS_API bool xredisClientTlsAuthDialV1(xnetengine* Engine, xnetresolver* Resolver,
    xstrview Host, uint16 Port, uint64 Affinity, const xredisclientconfig* Config,
    const xredisclientinitconfig* Init, const xredisclienttlsconfig* TLS,
    const xtlsclientauthconfigv1* Auth, xredisclient** Result)
{
    return __xredisClientDialTlsAuthV1(Engine, Resolver, NULL, Host, Port, Affinity, Config, Init, TLS, Auth, xrtTimer(), Result);
}

XREDIS_API bool xredisClientTlsAuthDialAddressesV1(xnetengine* Engine, xnetaddrlist* Addresses,
    uint64 Affinity, const xredisclientconfig* Config, const xredisclientinitconfig* Init,
    const xredisclienttlsconfig* TLS, const xtlsclientauthconfigv1* Auth, xredisclient** Result)
{
    if (Addresses == NULL) return xredisTlsAuthArgument("TLS authentication endpoint list is NULL");
    return __xredisClientDialTlsAuthV1(Engine, NULL, Addresses, (xstrview){NULL, 0}, 0, Affinity, Config, Init, TLS, Auth, xrtTimer(), Result);
}

XREDIS_API bool xredisClientTlsAuthConnectV1(xnetengine* Engine, const xnetaddr* Remote,
    uint64 Affinity, const xredisclientconfig* Config, const xredisclientinitconfig* Init,
    const xredisclienttlsconfig* TLS, const xtlsclientauthconfigv1* Auth, xredisclient** Result)
{
    double Entered = xrtTimer();
    if (Remote == NULL || Result == NULL || !xrtMemRangeValid(Remote, sizeof(*Remote)) ||
        xrtMemRangesOverlap(Remote, sizeof(*Remote), Result, sizeof(*Result)))
        return xredisTlsAuthArgument("invalid TLS authentication numeric endpoint/output");
    xnetaddrlist* Addresses = xrtNetAddrListCreate(Remote, 1); if (Addresses == NULL) return false;
    bool Success = __xredisClientDialTlsAuthV1(Engine, NULL, Addresses, (xstrview){NULL, 0}, 0, Affinity, Config, Init, TLS, Auth, Entered, Result);
    xrtNetAddrListDestroy(Addresses); return Success;
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/tracking/tracking.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
XREDIS_API bool xredisTrackingConfigInit(xredistrackingconfig* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K))){
        xrtSetErrorInfo(XERR_ARGUMENT,"xrt.redis",XREDIS_CLIENT_ARGUMENT,"invalid tracking config output");return false;
    }
    *K=(xredistrackingconfig){sizeof(*K),true,0,0,NULL,0};return true;
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/pubsub/pubsub.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_PUBSUB)
#include <string.h>
#if defined(XREDIS_FEATURE_XREDIS_PUBSUB)
static bool xredisPubSubFail(xerrkind Kind,int32 Code,cstr Text)
{xrtSetErrorInfo(Kind,"xrt.redis",Code,Text);return false;}
static size_t xredisPubSubDigits(size_t N)
{size_t D=1;while(N>=10){N/=10;++D;}return D;}
static bool xredisPubSubMeasure(xredispubsubcommand Command,const xbytesview* Names,size_t Count,size_t* Bytes,xbytesview* Word)
{
    static const xbytesview Words[]={XRT_BYTES_LITERAL("SUBSCRIBE"),XRT_BYTES_LITERAL("PSUBSCRIBE"),XRT_BYTES_LITERAL("SSUBSCRIBE"),
        XRT_BYTES_LITERAL("UNSUBSCRIBE"),XRT_BYTES_LITERAL("PUNSUBSCRIBE"),XRT_BYTES_LITERAL("SUNSUBSCRIBE"),XRT_BYTES_LITERAL("PING")};
    if((unsigned)Command>=sizeof(Words)/sizeof(*Words) || (Command<=XREDIS_PUBSUB_CMD_SSUBSCRIBE && !Count) ||
        (Command==XREDIS_PUBSUB_CMD_PING && Count>1))return xredisPubSubFail(XERR_ARGUMENT,XREDIS_PUBSUB_ARGUMENT,"invalid Pub/Sub command or cardinality");
    if(Count>XREDIS_PUBSUB_NAMES_MAX)return xredisPubSubFail(XERR_RANGE,XREDIS_PUBSUB_LIMIT,"Pub/Sub name count exceeds limit");
    /* Validate names once; report existing command input diagnostics directly.
     * A nested public size call would add an irrelevant output-alias pass. */
    if(Count && !Names)return xredisPubSubFail(XERR_ARGUMENT,XREDIS_COMMAND_ARGUMENT,"a command requires a nonempty argument vector");
    if(Count && !xrtMemRangeValid(Names,Count*sizeof(*Names)))return xredisPubSubFail(XERR_RANGE,XREDIS_COMMAND_OVERFLOW,"command argument count or descriptor span overflows");
    *Word=Words[Command];size_t Total=3+xredisPubSubDigits(Count+1)+5+xredisPubSubDigits(Word->Size)+Word->Size;
    for(size_t N=0;N<Count;++N){
        size_t Length=Names[N].Size;
        if(!Names[N].Data && Length)return xredisPubSubFail(XERR_ARGUMENT,XREDIS_COMMAND_ARGUMENT,"a nonempty command argument requires data");
        if(Length>(uint64)INT64_MAX || !xrtMemRangeValid(Names[N].Data,Length))return xredisPubSubFail(XERR_RANGE,XREDIS_COMMAND_OVERFLOW,"command argument length or address span overflows");
        size_t Overhead=5+xredisPubSubDigits(Length);
        if(Length>SIZE_MAX-Overhead || Total>SIZE_MAX-Length-Overhead)return xredisPubSubFail(XERR_RANGE,XREDIS_PUBSUB_LIMIT,"Pub/Sub encoded size overflows");
        Total+=Length+Overhead;
    }
    *Bytes=Total;return true;
}
static bool xredisPubSubSafe(const void* Output,size_t Bytes,const xbytesview* Names,size_t Count)
{
    if(!Output || !xrtMemRangeValid(Output,Bytes))return xredisPubSubFail(XERR_ARGUMENT,XREDIS_PUBSUB_ARGUMENT,"invalid Pub/Sub output span");
    if(xrtMemRangesOverlap(Output,Bytes,Names,Count*sizeof(*Names)))return xredisPubSubFail(XERR_ARGUMENT,XREDIS_PUBSUB_OVERLAP,"Pub/Sub output overlaps name descriptors");
    for(size_t N=0;N<Count;++N)if(xrtMemRangesOverlap(Output,Bytes,Names[N].Data,Names[N].Size))
        return xredisPubSubFail(XERR_ARGUMENT,XREDIS_PUBSUB_OVERLAP,"Pub/Sub output overlaps name payload");
    return true;
}
static unsigned char* xredisPubSubNumber(unsigned char* P,size_t N)
{
    unsigned char Digits[3*sizeof(size_t)];size_t Count=0;
    do{Digits[Count++]=(unsigned char)('0'+N%10);N/=10;}while(N);
    while(Count){*P++=Digits[--Count];}
    *P++='\r';*P++='\n';return P;
}
static inline unsigned char* xredisPubSubBulk(unsigned char* P,xbytesview V)
{
    *P++='$';P=xredisPubSubNumber(P,V.Size);if(V.Size){memcpy(P,V.Data,V.Size);P+=V.Size;}*P++='\r';*P++='\n';return P;
}
XREDIS_API bool xredisPubSubCommandSize(xredispubsubcommand Command,const xbytesview* Names,size_t Count,size_t* Required)
{
    size_t Bytes;xbytesview Word;
    if(!Required)return xredisPubSubFail(XERR_ARGUMENT,XREDIS_PUBSUB_ARGUMENT,"missing Pub/Sub size output");
    if(!xredisPubSubMeasure(Command,Names,Count,&Bytes,&Word) || !xredisPubSubSafe(Required,sizeof(*Required),Names,Count))return false;
    *Required=Bytes;return true;
}
XREDIS_API bool xredisPubSubCommandWrite(ptr Output,size_t Capacity,xredispubsubcommand Command,const xbytesview* Names,size_t Count,size_t* Written)
{
    size_t Bytes;xbytesview Word;
    if(!xredisPubSubMeasure(Command,Names,Count,&Bytes,&Word))return false;
    if(Capacity<Bytes)return xredisPubSubFail(XERR_RANGE,XREDIS_PUBSUB_CAPACITY,"Pub/Sub output capacity is insufficient");
    if(!xredisPubSubSafe(Output,Bytes,Names,Count))return false;
    if(Written && (!xredisPubSubSafe(Written,sizeof(*Written),Names,Count) || xrtMemRangesOverlap(Output,Bytes,Written,sizeof(*Written)))){
        if(xrtMemRangesOverlap(Output,Bytes,Written,sizeof(*Written)))xredisPubSubFail(XERR_ARGUMENT,XREDIS_PUBSUB_OVERLAP,"Pub/Sub outputs overlap");
        return false;
    }
    unsigned char* P=Output;*P++='*';P=xredisPubSubNumber(P,Count+1);P=xredisPubSubBulk(P,Word);
    for(size_t N=0;N<Count;++N)P=xredisPubSubBulk(P,Names[N]);
    if(Written)*Written=Bytes;
    return true;
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/subscriber/subscriber.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
XREDIS_API bool xredisSubscriberConfigInit(xredissubscriberconfig* Config)
{
    if(!Config || !xrtMemRangeValid(Config,sizeof(*Config))){xrtSetErrorInfo(XERR_ARGUMENT,"xrt.redis",XREDIS_CLIENT_ARGUMENT,"invalid subscriber config output");return false;}
    *Config=(xredissubscriberconfig){sizeof(*Config),4096,1024u*1024u};return true;
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/subscriptions/subscriptions.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS)
#include <stdlib.h>
#include <string.h>
#include <math.h>
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS)
typedef struct xredissubscriptionentry {
    xatomic32 References;uint16 Family,Slot;uint64 Hash;size_t Bytes;unsigned char Name[];
} xredissubscriptionentry;
struct xredissubscriptionsnapshot {
    xatomic32 References;uint64 Revision;size_t Count,NameBytes,EntryBytes,StorageBytes;
    xredissubscriptionentry* Entries[];
};
struct xredissubscriptions {
    xatomic32 References;xmutex Gate;xredissubscriptionsconfig Config;
    xredissubscriptionsnapshot* Current;bool Preparing;
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION)
    ptr EditOwner;
#endif
};
typedef struct xredissubscriptionedit {xbytesview Name;uint64 Hash;uint16 Slot;} xredissubscriptionedit;
typedef struct xredissubscriptionmerge {size_t Count,NameBytes,EntryBytes;bool Changed;} xredissubscriptionmerge;
struct xredissubscriptionreplay {
    xredissubscriptions* Store;xredissubscriptionsnapshot* Snapshot;xredisclient* Client;
    xredissubscriptionreplayconfig Config;xredissubscriptionreplayresult Result;
    xfuture* Future;size_t Offset,InFlight,StorageBytes;uint64 ExpectedRegular,ExpectedShard;double Entered,Deadline;
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION)
    ptr Scope;xredissubscriptionsnapshot* Base;size_t OldCursor,NewCursor;bool Adding;
#endif
    xbytesview Names[];
};
static bool xredisSubscriptionsReport(xerrkind Kind,cstr Text)
{xrtSetErrorInfo(Kind,"xrt.redis",Kind==XERR_STATE?XREDIS_CLIENT_STATE:XREDIS_CLIENT_ARGUMENT,Text);return false;}
static bool xredisSubscriptionsRef(xatomic32* Ref)
{
    uint32 N=xrtAtomic32Load(Ref,XMEMORY_RELAXED);
    for(;;){if(N==UINT32_MAX)return xredisSubscriptionsReport(XERR_RANGE,"subscription reference overflow");
        if(xrtAtomic32CompareExchange(Ref,&N,N+1,XMEMORY_RELAXED,XMEMORY_RELAXED))return true;}
}
static void xredisSubscriptionEntryRelease(xredissubscriptionentry* E)
{if(xrtAtomic32FetchSub(&E->References,1,XMEMORY_ACQ_REL)==1)xrtFree(E);}
XREDIS_API bool xredisSubscriptionSnapshotRetain(xredissubscriptionsnapshot* P)
{return P?xredisSubscriptionsRef(&P->References):xredisSubscriptionsReport(XERR_ARGUMENT,"snapshot is NULL");}
XREDIS_API void xredisSubscriptionSnapshotRelease(xredissubscriptionsnapshot* P)
{
    if(!P || xrtAtomic32FetchSub(&P->References,1,XMEMORY_ACQ_REL)!=1)return;
    for(size_t N=0;N<P->Count;++N){xredisSubscriptionEntryRelease(P->Entries[N]);}
    xrtFree(P);
}
XREDIS_API bool xredisSubscriptionsRetain(xredissubscriptions* S)
{return S?xredisSubscriptionsRef(&S->References):xredisSubscriptionsReport(XERR_ARGUMENT,"subscription store is NULL");}
XREDIS_API void xredisSubscriptionsRelease(xredissubscriptions* S)
{
    if(!S || xrtAtomic32FetchSub(&S->References,1,XMEMORY_ACQ_REL)!=1)return;
    xredisSubscriptionSnapshotRelease(S->Current);xrtMutexUnit(&S->Gate);xrtFree(S);
}
XREDIS_API bool xredisSubscriptionsConfigInit(xredissubscriptionsconfig* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K)))return xredisSubscriptionsReport(XERR_ARGUMENT,"invalid subscription config");
    *K=(xredissubscriptionsconfig){sizeof(*K),4096,1024u*1024u,1024u*1024u};return true;
}
XREDIS_API bool xredisSubscriptionsCreate(const xredissubscriptionsconfig* Config,xredissubscriptions** Output)
{
    xredissubscriptionsconfig K;
    if(!Output || !xrtMemRangeValid(Output,sizeof(*Output)) || (Config && (!xrtMemRangeValid(Config,sizeof(*Config)) || xrtMemRangesOverlap(Config,sizeof(*Config),Output,sizeof(*Output)))))
        return xredisSubscriptionsReport(XERR_ARGUMENT,"invalid subscription creation output");
    if(Config)K=*Config;else xredisSubscriptionsConfigInit(&K);
    if(K.Size!=sizeof(K) || !K.MaxSubscriptions || K.MaxSubscriptions>100000 || !K.MaxNameBytes || !K.MaxChangeBytes ||
        K.MaxNameBytes>SIZE_MAX-sizeof(xredissubscriptionsnapshot)-sizeof(xredissubscriptions)-K.MaxSubscriptions*(sizeof(xredissubscriptionentry)+sizeof(void*)))
        return xredisSubscriptionsReport(XERR_ARGUMENT,"invalid subscription bounds");
    xredissubscriptions* S=xrtCalloc(1,sizeof(*S));if(!S)return false;
    xrtAtomic32Init(&S->References,1);S->Config=K;
    if(!xrtMutexInit(&S->Gate)){xrtFree(S);return false;}
    S->Current=xrtCalloc(1,sizeof(*S->Current));if(!S->Current){xrtMutexUnit(&S->Gate);xrtFree(S);return false;}
    xrtAtomic32Init(&S->Current->References,1);S->Current->StorageBytes=sizeof(*S->Current);*Output=S;return true;
}
/* Compute once for edited shard names; recovery reads the cached slot. */
static uint16 xredisSubscriptionSlot(xbytesview Name)
{return xredisClusterSlotRaw(Name);}
static int xredisSubscriptionCompareName(uint16 ASlot,uint64 AHash,xbytesview A,uint16 BSlot,uint64 BHash,xbytesview B)
{
    if(ASlot!=BSlot)return ASlot<BSlot?-1:1;
    if(AHash!=BHash)return AHash<BHash?-1:1;
    if(A.Size!=B.Size)return A.Size<B.Size?-1:1;
    return A.Size?memcmp(A.Data,B.Data,A.Size):0;
}
static int xredisSubscriptionCompareEdit(const void* A,const void* B)
{
    const xredissubscriptionedit *X=A,*Y=B;return xredisSubscriptionCompareName(X->Slot,X->Hash,X->Name,Y->Slot,Y->Hash,Y->Name);
}
static bool xredisSubscriptionEmit(xredissubscriptionsnapshot* Dest,xredissubscriptionmerge* M,
    const xredissubscriptionsconfig* K,xredissubscriptionentry* Existing,const xredissubscriptionedit* Added,unsigned Family)
{
    size_t Bytes=Existing?Existing->Bytes:Added->Name.Size;
    if(M->Count==K->MaxSubscriptions || Bytes>K->MaxNameBytes-M->NameBytes)return false;
    ++M->Count;M->NameBytes+=Bytes;M->EntryBytes+=sizeof(xredissubscriptionentry)+Bytes;
    if(Dest){xredissubscriptionentry* E=Existing;
        if(E){if(!xredisSubscriptionsRef(&E->References))return false;}
        else{E=xrtMalloc(sizeof(*E)+Bytes);if(!E)return false;xrtAtomic32Init(&E->References,1);E->Family=(uint16)Family;E->Slot=Added->Slot;E->Hash=Added->Hash;E->Bytes=Bytes;if(Bytes)memcpy(E->Name,Added->Name.Data,Bytes);}
        Dest->Entries[Dest->Count++]=E;
    }
    return true;
}
static bool xredisSubscriptionMerge(const xredissubscriptionsnapshot* Old,const xredissubscriptionedit* Edits,size_t Count,
    bool Add,bool All,unsigned Family,const xredissubscriptionsconfig* K,xredissubscriptionsnapshot* Dest,xredissubscriptionmerge* M)
{
    *M=(xredissubscriptionmerge){0};size_t A=0,B=0;
    while(A<Old->Count || B<Count){
        xredissubscriptionentry* E=A<Old->Count?Old->Entries[A]:NULL;
        if(All){if(E->Family==Family){M->Changed=true;++A;continue;}if(!xredisSubscriptionEmit(Dest,M,K,E,NULL,Family))return false;++A;continue;}
        int Compare=!E?1:B==Count?-1:E->Family!=Family?(E->Family<Family?-1:1):
            xredisSubscriptionCompareName(E->Slot,E->Hash,(xbytesview){E->Name,E->Bytes},Edits[B].Slot,Edits[B].Hash,Edits[B].Name);
        if(Compare<0){if(!xredisSubscriptionEmit(Dest,M,K,E,NULL,Family))return false;++A;}
        else if(!Compare){if(Add){if(!xredisSubscriptionEmit(Dest,M,K,E,NULL,Family))return false;}else M->Changed=true;++A;++B;}
        else {if(Add){if(!xredisSubscriptionEmit(Dest,M,K,NULL,Edits+B,Family))return false;M->Changed=true;}++B;}
    }
    return true;
}
static xredisadmission xredisSubscriptionsApplyScope(xredissubscriptions* S,xredispubsubcommand Command,const xbytesview* Names,size_t Count,uint64* Revision,ptr Scope)
{
    if(!S || (unsigned)Command>XREDIS_PUBSUB_CMD_SUNSUBSCRIBE || !Revision || !xrtMemRangeValid(Revision,sizeof(*Revision)) ||
        xrtMemRangesOverlap(S,S?sizeof(*S):0,Revision,sizeof(*Revision)) || Count>XREDIS_PUBSUB_NAMES_MAX || ((unsigned)Command<3 && !Count) || !xrtMemRangeValid(Names,Count*sizeof(*Names)) ||
        xrtMemRangesOverlap(Names,Count*sizeof(*Names),Revision,sizeof(*Revision))){
        xredisSubscriptionsReport(XERR_ARGUMENT,"invalid desired subscription edit");return XREDIS_ADMISSION_ERROR;}
    size_t InputBytes=0;
    for(size_t N=0;N<Count;++N){if(!xrtMemRangeValid(Names[N].Data,Names[N].Size) || xrtMemRangesOverlap(Names[N].Data,Names[N].Size,Revision,sizeof(*Revision))){xredisSubscriptionsReport(XERR_ARGUMENT,"edit output aliases name");return XREDIS_ADMISSION_ERROR;}
        if(Names[N].Size>S->Config.MaxChangeBytes-InputBytes)return XREDIS_ADMISSION_OVERLOADED;
        InputBytes+=Names[N].Size;}
    xrtMutexLock(&S->Gate);
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION)
    if(S->EditOwner!=Scope){xrtMutexUnlock(&S->Gate);return XREDIS_ADMISSION_OVERLOADED;}
#else
    (void)Scope;
#endif
    if(S->Preparing){xrtMutexUnlock(&S->Gate);return XREDIS_ADMISSION_OVERLOADED;}
    S->Preparing=true;xredissubscriptionsnapshot* Old=S->Current;xrtMutexUnlock(&S->Gate);
    xredisadmission Result=XREDIS_ADMISSION_ERROR;xredissubscriptionsnapshot* Next=NULL;xredissubscriptionedit* Edits=NULL;
    if(Count){Edits=xrtMalloc(Count*sizeof(*Edits));if(!Edits)goto Done;
        for(size_t N=0;N<Count;++N)Edits[N]=(xredissubscriptionedit){Names[N],xrtHash64(Names[N].Data,Names[N].Size),(unsigned)Command%3==2?xredisSubscriptionSlot(Names[N]):0};
        qsort(Edits,Count,sizeof(*Edits),xredisSubscriptionCompareEdit);
        size_t Unique=0;for(size_t N=0;N<Count;++N)if(!Unique || xredisSubscriptionCompareEdit(Edits+N,Edits+Unique-1))Edits[Unique++]=Edits[N];Count=Unique;
    }
    xredissubscriptionmerge M;bool Add=(unsigned)Command<3,All=!Add && !Count;unsigned Family=(unsigned)Command%3;
    if(!xredisSubscriptionMerge(Old,Edits,Count,Add,All,Family,&S->Config,NULL,&M)){Result=XREDIS_ADMISSION_OVERLOADED;goto Done;}
    if(!M.Changed){*Revision=Old->Revision;Result=XREDIS_ADMISSION_ACCEPTED;goto Done;}
    if(Old->Revision==UINT64_MAX){xredisSubscriptionsReport(XERR_RANGE,"subscription revision exhausted");goto Done;}
    size_t Bytes=sizeof(*Next)+M.Count*sizeof(*Next->Entries);Next=xrtCalloc(1,Bytes);if(!Next)goto Done;
    xrtAtomic32Init(&Next->References,1);Next->StorageBytes=Bytes;Next->Revision=Old->Revision+1;
    if(!xredisSubscriptionMerge(Old,Edits,Count,Add,All,Family,&S->Config,Next,&M))goto Done;
    Next->NameBytes=M.NameBytes;Next->EntryBytes=M.EntryBytes;
    xrtMutexLock(&S->Gate);S->Current=Next;*Revision=Next->Revision;Next=NULL;xrtMutexUnlock(&S->Gate);
    xredisSubscriptionSnapshotRelease(Old);Result=XREDIS_ADMISSION_ACCEPTED;
Done:
    xredisSubscriptionSnapshotRelease(Next);xrtFree(Edits);
    xrtMutexLock(&S->Gate);S->Preparing=false;xrtMutexUnlock(&S->Gate);return Result;
}
XREDIS_API xredisadmission xredisSubscriptionsApply(xredissubscriptions* S,xredispubsubcommand Command,const xbytesview* Names,size_t Count,uint64* Revision)
{return xredisSubscriptionsApplyScope(S,Command,Names,Count,Revision,NULL);}
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION)
bool __xredisSubscriptionsFits(xredissubscriptions* S,const xredissubscriberconfig* K)
{return S->Config.MaxSubscriptions<=K->MaxSubscriptions && S->Config.MaxNameBytes<=K->MaxNameBytes;}
xredisadmission __xredisSubscriptionsClaim(xredissubscriptions* S,ptr Owner,ptr Output)
{
    if(xrtMemRangesOverlap(S,sizeof(*S),Output,sizeof(void*))){xredisSubscriptionsReport(XERR_ARGUMENT,"session output overlaps desired store");return XREDIS_ADMISSION_ERROR;}
    xrtMutexLock(&S->Gate);xredisadmission A=S->Preparing || S->EditOwner?XREDIS_ADMISSION_OVERLOADED:XREDIS_ADMISSION_ACCEPTED;
    if(A==XREDIS_ADMISSION_ACCEPTED)S->EditOwner=Owner;
    xrtMutexUnlock(&S->Gate);return A;
}
void __xredisSubscriptionsUnclaim(xredissubscriptions* S,ptr Owner)
{xrtMutexLock(&S->Gate);if(S->EditOwner==Owner)S->EditOwner=NULL;xrtMutexUnlock(&S->Gate);}
xredisadmission __xredisSubscriptionsApply(xredissubscriptions* S,xredispubsubcommand Command,const xbytesview* Names,size_t Count,uint64* Revision,ptr Owner)
{return xredisSubscriptionsApplyScope(S,Command,Names,Count,Revision,Owner);}
#endif
static xredissubscriptionsinfo xredisSubscriptionInfoValue(const xredissubscriptionsnapshot* P,bool Preparing)
{return (xredissubscriptionsinfo){sizeof(xredissubscriptionsinfo),P->Revision,P->Count,P->NameBytes,P->StorageBytes,P->EntryBytes,Preparing};}
XREDIS_API bool xredisSubscriptionsInfo(xredissubscriptions* S,xredissubscriptionsinfo* Info)
{
    if(!S || !Info || !xrtMemRangeValid(Info,sizeof(*Info)) || Info->Size!=sizeof(*Info) || xrtMemRangesOverlap(S,S?sizeof(*S):0,Info,sizeof(*Info)))return xredisSubscriptionsReport(XERR_ARGUMENT,"invalid desired-state output");
    xrtMutexLock(&S->Gate);*Info=xredisSubscriptionInfoValue(S->Current,S->Preparing);xrtMutexUnlock(&S->Gate);return true;
}
XREDIS_API bool xredisSubscriptionsSnapshot(xredissubscriptions* S,xredissubscriptionsnapshot** Output)
{
    if(!S || !Output || !xrtMemRangeValid(Output,sizeof(*Output)) || xrtMemRangesOverlap(S,S?sizeof(*S):0,Output,sizeof(*Output)))return xredisSubscriptionsReport(XERR_ARGUMENT,"invalid snapshot output");
    xrtMutexLock(&S->Gate);bool OK=xredisSubscriptionSnapshotRetain(S->Current);if(OK)*Output=S->Current;xrtMutexUnlock(&S->Gate);return OK;
}
XREDIS_API bool xredisSubscriptionSnapshotInfo(const xredissubscriptionsnapshot* P,xredissubscriptionsinfo* Info)
{
    if(!P || !Info || !xrtMemRangeValid(Info,sizeof(*Info)) || Info->Size!=sizeof(*Info) || xrtMemRangesOverlap(P,P?P->StorageBytes:0,Info,sizeof(*Info)))return xredisSubscriptionsReport(XERR_ARGUMENT,"invalid immutable snapshot output");
    *Info=xredisSubscriptionInfoValue(P,false);return true;
}
XREDIS_API bool xredisSubscriptionSnapshotAt(const xredissubscriptionsnapshot* P,size_t Index,xredissubscriptionview* View)
{
    if(!P || !View || !xrtMemRangeValid(View,sizeof(*View)) || xrtMemRangesOverlap(P,P?P->StorageBytes:0,View,sizeof(*View)))return xredisSubscriptionsReport(XERR_ARGUMENT,"invalid snapshot view output");
    if(Index>=P->Count)return false;
    xredissubscriptionentry* E=P->Entries[Index];*View=(xredissubscriptionview){(xredispubsubcommand)E->Family,{E->Name,E->Bytes}};return true;
}
XREDIS_API bool xredisSubscriptionReplayConfigInit(xredissubscriptionreplayconfig* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K)))return xredisSubscriptionsReport(XERR_ARGUMENT,"invalid replay config");
    *K=(xredissubscriptionreplayconfig){0};K->Size=sizeof(*K);xredisSubscriberConfigInit(&K->Registry);K->MaxBatchNames=64;K->MaxBatchBytes=65536;K->Timeout=5000;return true;
}
static xredisadmission xredisSubscriptionReplayCreateBudget(xredissubscriptions* S,xredisclient* C,const xredissubscriptionreplayconfig* Config,xredissubscriptionreplay** Output,double Entered,ptr Scope,xredissubscriptionsnapshot* Base)
{xredissubscriptionreplayconfig K;
    if(!S || !C || !Output || !xrtMemRangeValid(Output,sizeof(*Output)) || xrtMemRangesOverlap(S,S?sizeof(*S):0,Output,sizeof(*Output)) ||
       (Config && (!xrtMemRangeValid(Config,sizeof(*Config)) || xrtMemRangesOverlap(Config,sizeof(*Config),Output,sizeof(*Output))))){xredisSubscriptionsReport(XERR_ARGUMENT,"invalid replay arguments");return XREDIS_ADMISSION_ERROR;}
    if(Config)K=*Config;else xredisSubscriptionReplayConfigInit(&K);
    if(K.Size!=sizeof(K) || !K.MaxBatchNames || K.MaxBatchNames>XREDIS_PUBSUB_NAMES_MAX || !K.MaxBatchBytes || K.Timeout<XRT_WAIT_FOREVER){xredisSubscriptionsReport(XERR_ARGUMENT,"invalid replay work bounds");return XREDIS_ADMISSION_ERROR;}
    xredissubscriptionsnapshot* P=NULL;if(!xredisSubscriptionsSnapshot(S,&P))return XREDIS_ADMISSION_ERROR;
    if(P->Count>K.Registry.MaxSubscriptions || P->NameBytes>K.Registry.MaxNameBytes){xredisSubscriptionSnapshotRelease(P);xredisSubscriptionsReport(XERR_RANGE,"native registry cannot fit desired set");return XREDIS_ADMISSION_ERROR;}
    size_t Capacity=P->Count;
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION)
    if(Base && Base->Count>Capacity)Capacity=Base->Count;
#else
    (void)Base;
#endif
    size_t Slots=Capacity<K.MaxBatchNames?Capacity:K.MaxBatchNames;
    size_t Bytes=sizeof(xredissubscriptionreplay)+Slots*sizeof(xbytesview);xredissubscriptionreplay* R=xrtCalloc(1,Bytes);
    if(!R){xredisSubscriptionSnapshotRelease(P);return XREDIS_ADMISSION_ERROR;}
    if(!xredisSubscriptionsRetain(S)){xredisSubscriptionSnapshotRelease(P);xrtFree(R);return XREDIS_ADMISSION_ERROR;}
    if(!xredisClientRetain(C)){xredisSubscriptionsRelease(S);xredisSubscriptionSnapshotRelease(P);xrtFree(R);return XREDIS_ADMISSION_ERROR;}
    xredisadmission A=Scope?XREDIS_ADMISSION_ACCEPTED:__xredisSubscriberReplayStart(C,&K.Registry,R);
    if(A!=XREDIS_ADMISSION_ACCEPTED){xredisClientRelease(C);xredisSubscriptionsRelease(S);xredisSubscriptionSnapshotRelease(P);xrtFree(R);return A;}
    R->Store=S;R->Snapshot=P;R->Client=C;R->Config=K;R->StorageBytes=Bytes;R->Entered=Entered;
    R->Deadline=K.Timeout==XRT_WAIT_FOREVER?INFINITY:K.Timeout?nextafter(Entered+(double)K.Timeout*.001,INFINITY):Entered;
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION)
    R->Scope=Scope;R->Base=Base;R->Adding=Base==NULL;
    if(Base && !xredisSubscriptionSnapshotRetain(Base)){xredisClientRelease(C);xredisSubscriptionsRelease(S);xredisSubscriptionSnapshotRelease(P);xrtFree(R);return XREDIS_ADMISSION_ERROR;}
    if(Scope){xredissubscriberinfo I={0};I.Size=sizeof(I);if(xredisSubscriberInfo(C,&I)){R->ExpectedRegular=I.Subscriptions;R->ExpectedShard=I.ShardSubscriptions;}}
#endif
    R->Result.Size=sizeof(R->Result);R->Result.Revision=P->Revision;*Output=R;return XREDIS_ADMISSION_ACCEPTED;
}
XREDIS_API xredisadmission xredisSubscriptionReplayCreate(xredissubscriptions* S,xredisclient* C,const xredissubscriptionreplayconfig* Config,xredissubscriptionreplay** Output)
{return xredisSubscriptionReplayCreateBudget(S,C,Config,Output,xrtTimer(),NULL,NULL);}
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION)
xredisadmission __xredisSubscriptionTransitionCreate(xredissubscriptions* S,xredisclient* C,xredissubscriptionsnapshot* Base,const xredissubscriptionreplayconfig* Config,double Entered,ptr Scope,xredissubscriptionreplay** Output)
{return xredisSubscriptionReplayCreateBudget(S,C,Config,Output,Entered,Scope,Base);}
xredissubscriptionsnapshot* __xredisSubscriptionTransitionSnapshot(xredissubscriptionreplay* R)
{return xredisSubscriptionSnapshotRetain(R->Snapshot)?R->Snapshot:NULL;}
#endif
static xredissubscriptionstatus xredisSubscriptionNativeStatus(xredisrequeststatus S)
{
    switch(S){case XREDIS_REQUEST_OK:return XREDIS_SUBSCRIPTIONS_OK;case XREDIS_REQUEST_SERVER_ERROR:return XREDIS_SUBSCRIPTIONS_SERVER_ERROR;
    case XREDIS_REQUEST_TIMEOUT:return XREDIS_SUBSCRIPTIONS_TIMEOUT;case XREDIS_REQUEST_CANCELLED:return XREDIS_SUBSCRIPTIONS_CANCELLED;
    case XREDIS_REQUEST_PROTOCOL:return XREDIS_SUBSCRIPTIONS_PROTOCOL;case XREDIS_REQUEST_LIMIT:return XREDIS_SUBSCRIPTIONS_LIMIT;
    case XREDIS_REQUEST_MEMORY:return XREDIS_SUBSCRIPTIONS_MEMORY;default:return XREDIS_SUBSCRIPTIONS_CLOSED;}
}
static xredissubscriptionstep xredisSubscriptionFinish(xredissubscriptionreplay* R,xredissubscriptionstatus Status,xredisrequeststatus Native)
{
    R->Result.Status=Status;R->Result.NativeStatus=Native;if(Status!=XREDIS_SUBSCRIPTIONS_OK)(void)xredisClientClose(R->Client);
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION)
    if(!R->Scope)
#endif
    __xredisSubscriberReplayRelease(R->Client,R);
    return XREDIS_SUBSCRIPTIONS_DONE;
}
static size_t xredisSubscriptionDigits(size_t N)
{size_t D=1;while(N>=10){N/=10;++D;}return D;}
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION)
/* Session-private: finish this immutable target even if newer intent arrives.
 * Remove only absent keys before adding only new keys; unchanged names stay live.
 * At most1024 merge inspections and one wire batch per scheduled turn. */
static int xredisSubscriptionEntryCompare(const xredissubscriptionentry* A,const xredissubscriptionentry* B)
{
    if(A==B)return 0;
    if(A->Family!=B->Family)return A->Family<B->Family?-1:1;
    if(A->Slot!=B->Slot)return A->Slot<B->Slot?-1:1;
    if(A->Hash!=B->Hash)return A->Hash<B->Hash?-1:1;
    size_t N=A->Bytes<B->Bytes?A->Bytes:B->Bytes;int C=N?memcmp(A->Name,B->Name,N):0;
    return C?C:(A->Bytes>B->Bytes)-(A->Bytes<B->Bytes);
}
static xredissubscriptionstep xredisSubscriptionTransitionStep(xredissubscriptionreplay* R)
{
    bool Observed=false;
    if(R->Future && xrtFutureDone(R->Future)){
        xfutureresult F;if(!xrtFutureResult(R->Future,&F) || F.State!=XFUTURE_RESOLVED || !F.Value)return xredisSubscriptionFinish(R,XREDIS_SUBSCRIPTIONS_PROTOCOL,XREDIS_REQUEST_PROTOCOL);
        const xredissubscriberresult* V=F.Value;R->Result.Acknowledged+=V->Acknowledged;R->Result.Subscriptions=V->Subscriptions;R->Result.ShardSubscriptions=V->ShardSubscriptions;
        if(V->Request.Status!=XREDIS_REQUEST_OK){R->Result.ErrorReply=V->Request.Reply;R->Result.Cause=V->Request.Cause;return xredisSubscriptionFinish(R,xredisSubscriptionNativeStatus(V->Request.Status),V->Request.Status);}
        if(V->Acknowledged!=R->InFlight || V->Subscriptions!=R->ExpectedRegular || V->ShardSubscriptions!=R->ExpectedShard)return xredisSubscriptionFinish(R,XREDIS_SUBSCRIPTIONS_PROTOCOL,XREDIS_REQUEST_PROTOCOL);
        ++R->Result.Batches;xrtFutureDestroy(R->Future);R->Future=NULL;Observed=true;
    }
    if(xrtTimer()>=R->Deadline)return xredisSubscriptionFinish(R,XREDIS_SUBSCRIPTIONS_TIMEOUT,XREDIS_REQUEST_TIMEOUT);
    xredisclientinfo I={0};I.Size=sizeof(I);if(!xredisClientInfo(R->Client,&I))return XREDIS_SUBSCRIPTIONS_STEP_ERROR;
    if(I.State>=XREDIS_CLIENT_CLOSING)return xredisSubscriptionFinish(R,XREDIS_SUBSCRIPTIONS_CLOSED,I.CloseStatus==XREDIS_REQUEST_OK?XREDIS_REQUEST_CLOSED:I.CloseStatus);
    if(R->Future)return XREDIS_SUBSCRIPTIONS_WAIT;
    if(Observed || I.Requests)return XREDIS_SUBSCRIPTIONS_MORE;
    size_t Count=0,Sum=0,Scanned=0;unsigned Family=0,Slot=0;bool Adding=R->Adding;
    while(Scanned++<1024 && Count<R->Config.MaxBatchNames){
        size_t OldCount=R->Base?R->Base->Count:0;
        if(!R->Adding && R->OldCursor==OldCount){if(Count)break;R->Adding=true;Adding=true;R->OldCursor=R->NewCursor=0;}
        if(R->Adding && R->NewCursor==R->Snapshot->Count)break;
        xredissubscriptionentry *Old=R->OldCursor<OldCount?R->Base->Entries[R->OldCursor]:NULL,*New=R->NewCursor<R->Snapshot->Count?R->Snapshot->Entries[R->NewCursor]:NULL;
        int Compare=Old && New?xredisSubscriptionEntryCompare(Old,New):Old?-1:1;
        if(Old && New && !Compare){++R->OldCursor;++R->NewCursor;continue;}
        if(!R->Adding && Compare>0){++R->NewCursor;continue;}
        if(R->Adding && Compare<0){++R->OldCursor;continue;}
        xredissubscriptionentry* E=R->Adding?New:Old;
        if(Count && (E->Family!=Family || (Family==2 && E->Slot!=Slot)))break;
        Family=E->Family;Slot=E->Slot;size_t Word=(R->Adding?9:11)+(Family!=0),CommandBytes=Word+5+xredisSubscriptionDigits(Word),Digits=xredisSubscriptionDigits(E->Bytes),Header=CommandBytes+3+xredisSubscriptionDigits(Count+2);
        if(E->Bytes>SIZE_MAX-5-Digits || Header>R->Config.MaxBatchBytes || Sum>R->Config.MaxBatchBytes-Header || E->Bytes+5+Digits>R->Config.MaxBatchBytes-Header-Sum)break;
        R->Names[Count++]=(xbytesview){E->Name,E->Bytes};Sum+=E->Bytes+5+Digits;
        if(R->Adding)++R->NewCursor;else ++R->OldCursor;
    }
    if(!Count){
        if(R->Adding && R->NewCursor==R->Snapshot->Count){R->Result.Subscriptions=R->ExpectedRegular;R->Result.ShardSubscriptions=R->ExpectedShard;return xredisSubscriptionFinish(R,XREDIS_SUBSCRIPTIONS_OK,XREDIS_REQUEST_OK);}
        if(Scanned>1024)return XREDIS_SUBSCRIPTIONS_MORE;
        return xredisSubscriptionFinish(R,XREDIS_SUBSCRIPTIONS_LIMIT,XREDIS_REQUEST_LIMIT);
    }
    uint64* Expected=Family==2?&R->ExpectedShard:&R->ExpectedRegular;if(Adding)*Expected+=Count;else *Expected-=Count;
    xfuture* F=NULL;xredisadmission A=__xredisSubscriberCommandBudget(R->Client,(xredispubsubcommand)(Family+(Adding?0:3)),R->Names,Count,R->Config.Timeout,R->Entered,R->Scope,&F);
    if(A!=XREDIS_ADMISSION_ACCEPTED)return xredisSubscriptionFinish(R,A==XREDIS_ADMISSION_ERROR?XREDIS_SUBSCRIPTIONS_MEMORY:A==XREDIS_ADMISSION_CLOSED?XREDIS_SUBSCRIPTIONS_CLOSED:XREDIS_SUBSCRIPTIONS_LIMIT,A==XREDIS_ADMISSION_ERROR?XREDIS_REQUEST_MEMORY:A==XREDIS_ADMISSION_CLOSED?XREDIS_REQUEST_CLOSED:XREDIS_REQUEST_LIMIT);
    R->InFlight=Count;R->Future=F;return XREDIS_SUBSCRIPTIONS_WAIT;
}
#endif
XREDIS_API xredissubscriptionstep xredisSubscriptionReplayStep(xredissubscriptionreplay* R)
{
    if(!R){xredisSubscriptionsReport(XERR_ARGUMENT,"replay is NULL");return XREDIS_SUBSCRIPTIONS_STEP_ERROR;}
    if(R->Result.Status!=XREDIS_SUBSCRIPTIONS_PENDING)return XREDIS_SUBSCRIPTIONS_DONE;
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION)
    if(R->Scope)return xredisSubscriptionTransitionStep(R);
#endif
    bool Observed=false;
    if(R->Future && xrtFutureDone(R->Future)){
        xfutureresult F;if(!xrtFutureResult(R->Future,&F) || F.State!=XFUTURE_RESOLVED || !F.Value)return xredisSubscriptionFinish(R,XREDIS_SUBSCRIPTIONS_PROTOCOL,XREDIS_REQUEST_PROTOCOL);
        const xredissubscriberresult* V=F.Value;R->Result.Acknowledged+=V->Acknowledged;R->Result.Subscriptions=V->Subscriptions;R->Result.ShardSubscriptions=V->ShardSubscriptions;
        if(V->Request.Status!=XREDIS_REQUEST_OK){R->Result.ErrorReply=V->Request.Reply;R->Result.Cause=V->Request.Cause;return xredisSubscriptionFinish(R,xredisSubscriptionNativeStatus(V->Request.Status),V->Request.Status);}
        if(V->Acknowledged!=R->InFlight || V->Subscriptions!=R->ExpectedRegular || V->ShardSubscriptions!=R->ExpectedShard)return xredisSubscriptionFinish(R,XREDIS_SUBSCRIPTIONS_PROTOCOL,XREDIS_REQUEST_PROTOCOL);
        R->Offset+=R->InFlight;++R->Result.Batches;xrtFutureDestroy(R->Future);R->Future=NULL;Observed=true;
    }
    xrtMutexLock(&R->Store->Gate);bool Stale=R->Store->Current->Revision!=R->Snapshot->Revision;xrtMutexUnlock(&R->Store->Gate);
    if(Stale)return xredisSubscriptionFinish(R,XREDIS_SUBSCRIPTIONS_STALE,XREDIS_REQUEST_CANCELLED);
    if(xrtTimer()>=R->Deadline)return xredisSubscriptionFinish(R,XREDIS_SUBSCRIPTIONS_TIMEOUT,XREDIS_REQUEST_TIMEOUT);
    xredisclientinfo I={0};I.Size=sizeof(I);if(!xredisClientInfo(R->Client,&I))return XREDIS_SUBSCRIPTIONS_STEP_ERROR;
    if(I.State>=XREDIS_CLIENT_CLOSING){xredisrequeststatus Closed=I.CloseStatus==XREDIS_REQUEST_OK?XREDIS_REQUEST_CLOSED:I.CloseStatus;return xredisSubscriptionFinish(R,xredisSubscriptionNativeStatus(Closed),Closed);}
    if(R->Offset==R->Snapshot->Count)return xredisSubscriptionFinish(R,XREDIS_SUBSCRIPTIONS_OK,XREDIS_REQUEST_OK);
    if(R->Future)return XREDIS_SUBSCRIPTIONS_WAIT;
    if(Observed || I.Requests)return XREDIS_SUBSCRIPTIONS_MORE;
    unsigned Family=R->Snapshot->Entries[R->Offset]->Family;size_t Count=0,Sum=0,Word=Family?10:9,CommandBytes=Word+5+xredisSubscriptionDigits(Word);
    while(R->Offset+Count<R->Snapshot->Count && Count<R->Config.MaxBatchNames){xredissubscriptionentry* E=R->Snapshot->Entries[R->Offset+Count];if(E->Family!=Family || (Family==2 && E->Slot!=R->Snapshot->Entries[R->Offset]->Slot))break;
        size_t Digits=xredisSubscriptionDigits(E->Bytes),Header=CommandBytes+3+xredisSubscriptionDigits(Count+2);
        if(E->Bytes>SIZE_MAX-5-Digits || Header>R->Config.MaxBatchBytes || Sum>R->Config.MaxBatchBytes-Header || E->Bytes+5+Digits>R->Config.MaxBatchBytes-Header-Sum)break;
        Sum+=E->Bytes+5+Digits;R->Names[Count++]=(xbytesview){E->Name,E->Bytes};
    }
    if(!Count)return xredisSubscriptionFinish(R,XREDIS_SUBSCRIPTIONS_LIMIT,XREDIS_REQUEST_LIMIT);
    R->InFlight=Count;R->ExpectedRegular=R->Result.Subscriptions+(Family<2?Count:0);R->ExpectedShard=R->Result.ShardSubscriptions+(Family==2?Count:0);
    xredisadmission A=__xredisSubscriberCommandBudget(R->Client,(xredispubsubcommand)Family,R->Names,Count,R->Config.Timeout,R->Entered,R,&R->Future);
    if(A==XREDIS_ADMISSION_ACCEPTED)return XREDIS_SUBSCRIPTIONS_WAIT;
    return xredisSubscriptionFinish(R,A==XREDIS_ADMISSION_OVERLOADED?XREDIS_SUBSCRIPTIONS_LIMIT:A==XREDIS_ADMISSION_CLOSED?XREDIS_SUBSCRIPTIONS_CLOSED:XREDIS_SUBSCRIPTIONS_MEMORY,
        A==XREDIS_ADMISSION_OVERLOADED?XREDIS_REQUEST_LIMIT:A==XREDIS_ADMISSION_CLOSED?XREDIS_REQUEST_CLOSED:XREDIS_REQUEST_MEMORY);
}
XREDIS_API xfuture* xredisSubscriptionReplayPending(xredissubscriptionreplay* R)
{if(!R){xredisSubscriptionsReport(XERR_ARGUMENT,"replay is NULL");return NULL;}return R->Future && !xrtFutureDone(R->Future)?xrtFutureRef(R->Future):NULL;}
XREDIS_API bool xredisSubscriptionReplayResult(xredissubscriptionreplay* R,xredissubscriptionreplayresult* Output)
{
    if(!R || !Output || !xrtMemRangeValid(Output,sizeof(*Output)) || Output->Size!=sizeof(*Output) || xrtMemRangesOverlap(R,R?R->StorageBytes:0,Output,sizeof(*Output)))return xredisSubscriptionsReport(XERR_ARGUMENT,"invalid replay result");
    *Output=R->Result;return true;
}
XREDIS_API bool xredisSubscriptionReplayCancel(xredissubscriptionreplay* R)
{
    if(!R)return xredisSubscriptionsReport(XERR_ARGUMENT,"replay is NULL");
    if(R->Result.Status!=XREDIS_SUBSCRIPTIONS_PENDING)return false;
    (void)xredisSubscriptionFinish(R,XREDIS_SUBSCRIPTIONS_CANCELLED,XREDIS_REQUEST_CANCELLED);return true;
}
XREDIS_API void xredisSubscriptionReplayDestroy(xredissubscriptionreplay* R)
{
    if(!R)return;
    if(R->Result.Status==XREDIS_SUBSCRIPTIONS_PENDING)(void)xredisClientClose(R->Client);
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION)
    if(!R->Scope)
#endif
    __xredisSubscriberReplayRelease(R->Client,R);
    xrtFutureDestroy(R->Future);xredisClientRelease(R->Client);
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION)
    xredisSubscriptionSnapshotRelease(R->Base);
#endif
    xredisSubscriptionSnapshotRelease(R->Snapshot);xredisSubscriptionsRelease(R->Store);xrtFree(R);
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/subscription_session/subscription_session.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION)
#include <math.h>
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION)
typedef struct xredissessionwatch {
    xfuturewatch Watch;xfuture* Future;xredissubscriptionsession* Session;bool Active;
} xredissessionwatch;
struct xredissubscriptionsession {
    xatomic32 References;xmutex Gate;xnetengine* Engine;xnetworker* Worker;xnetpost Work;
    xredissubscriptions* Store;xredissubscriptionsessionconfig Config;xredissubscriptionsessioninfo Info;
    xredisclient *Client,*Published;xredissubscriptionreplay* Replay;xredissubscriptionsnapshot* Applied;
    xredissessionwatch Ready,Closed,Control;xpromise* Promise;xfuture* Future;
    xredissubscriptionsessionclosed* Result;
    xredisrequeststatus StopStatus;size_t APIs;bool Queued,Closing,Finished,Notify,Draining,ReadyOK;
    uint32 Consecutive;uint64 Timer,Random;double Due,TimerDue,Entered,WakeDue;
};
static void xredisSessionWork(xnetworker*,ptr);
static bool xredisSessionReport(xerrkind Kind,cstr Text)
{xrtSetErrorInfo(Kind,"xrt.redis",Kind==XERR_STATE?XREDIS_CLIENT_STATE:XREDIS_CLIENT_ARGUMENT,Text);return false;}
static void xredisSessionRef(xredissubscriptionsession* S){xrtAtomic32FetchAdd(&S->References,1,XMEMORY_RELAXED);}
static void xredisSessionIncrement(uint64* N){if(*N!=UINT64_MAX)++*N;}
static double xredisSessionDeadline(double Now,int64 Timeout)
{return nextafter(Now+(double)Timeout*.001,INFINITY);}
XREDIS_API bool xredisSubscriptionSessionRetain(xredissubscriptionsession* S)
{
    if(!S)return xredisSessionReport(XERR_ARGUMENT,"session is NULL");
    uint32 N=xrtAtomic32Load(&S->References,XMEMORY_RELAXED);
    for(;;){if(N>=UINT32_MAX-32)return xredisSessionReport(XERR_RANGE,"session reference overflow");if(xrtAtomic32CompareExchange(&S->References,&N,N+1,XMEMORY_RELAXED,XMEMORY_RELAXED))return true;}
}
XREDIS_API void xredisSubscriptionSessionRelease(xredissubscriptionsession* S)
{
    if(!S || xrtAtomic32FetchSub(&S->References,1,XMEMORY_ACQ_REL)!=1)return;
    xrtPromiseDestroy(S->Promise);xrtFutureDestroy(S->Future);xrtFree(S->Result);xrtMutexUnit(&S->Gate);xrtFree(S);
}
/* Gate protects worker borrowing and post admission; no allocation. */
static void xredisSessionWakeLocked(xredissubscriptionsession* S)
{
    if(S->Finished || S->Queued)return;
    S->Queued=true;xredisSessionRef(S);
    if(!xrtNetPost(S->Worker,&S->Work,xredisSessionWork,S)){S->Queued=false;xredisSubscriptionSessionRelease(S);}
}
static void xredisSessionWake(xredissubscriptionsession* S)
{xrtMutexLock(&S->Gate);xredisSessionWakeLocked(S);xrtMutexUnlock(&S->Gate);}
/* Release follows Core's final access to Watch storage. Only this callback
 * publishes reuse; logical Future completion alone is not mechanical retirement. */
static void xredisSessionWatchNotify(ptr Data){(void)Data;}
static void xredisSessionWatchDrop(ptr Data)
{
    xredissessionwatch* W=Data;xredissubscriptionsession* S=W->Session;
    xrtMutexLock(&S->Gate);W->Active=false;xredisSessionWakeLocked(S);xrtMutexUnlock(&S->Gate);
    xredisSubscriptionSessionRelease(S); /* No further Watch/Engine access. */
}
static bool xredisSessionWatchActive(xredissubscriptionsession* S,xredissessionwatch* W)
{xrtMutexLock(&S->Gate);bool Active=W->Active;xrtMutexUnlock(&S->Gate);return Active;}
static bool xredisSessionWatchStop(xredissubscriptionsession* S,xredissessionwatch* W)
{
    if(!W->Future)return true;
    if(xredisSessionWatchActive(S,W))(void)xrtFutureWatchDetach(W->Future,&W->Watch);
    if(xredisSessionWatchActive(S,W))return false;
    xrtFutureDestroy(W->Future);W->Future=NULL;return true;
}
/* Takes the supplied observer reference even if registration fails. */
static bool xredisSessionWatchStart(xredissubscriptionsession* S,xredissessionwatch* W,xfuture* F)
{
    W->Future=F;W->Session=S;if(!F || !xrtFutureWatchInit(&W->Watch,xredisSessionWatchNotify,xredisSessionWatchDrop,W))return false;
    xredisSessionRef(S);xrtMutexLock(&S->Gate);W->Active=true;xrtMutexUnlock(&S->Gate);
    xfuturewatchresult A=xrtFutureWatchAdd(F,&W->Watch);
    if(A!=XFUTURE_WATCH_PENDING){xrtMutexLock(&S->Gate);W->Active=false;xrtMutexUnlock(&S->Gate);xredisSubscriptionSessionRelease(S);}
    return A!=XFUTURE_WATCH_ERROR;
}
static void xredisSessionState(xredissubscriptionsession* S,xredissubscriptionsessionstate State)
{xrtMutexLock(&S->Gate);if(!S->Closing && S->Info.State!=State){S->Info.State=State;S->Notify=true;}xrtMutexUnlock(&S->Gate);}
static bool xredisSessionClosing(xredissubscriptionsession* S)
{xrtMutexLock(&S->Gate);bool Closing=S->Closing;xrtMutexUnlock(&S->Gate);return Closing;}
static void xredisSessionNotify(xredissubscriptionsession* S)
{
    xrtMutexLock(&S->Gate);bool Needed=S->Notify;S->Notify=false;
    xredissubscriptionsessionnotify Notify=Needed?S->Config.Notify:NULL;ptr Data=S->Config.Data;xredissubscriptionsessioninfo Info=S->Info;xrtMutexUnlock(&S->Gate);
    if(Notify)Notify(S,&Info,Data);
}
static void xredisSessionTimer(xnetworker* Worker,uint64 Id,xnetresult Result,ptr Data)
{
    (void)Worker;xredissubscriptionsession* S=Data;if(S->Timer==Id)S->Timer=0;
    if(Result!=XNET_RESULT_OK && Result!=XNET_RESULT_CANCELLED)(void)xredisSubscriptionSessionClose(S);
    xredisSessionWake(S);xredisSubscriptionSessionRelease(S);
}
static bool xredisSessionArm(xredissubscriptionsession* S,double Due)
{
    if(S->Timer && S->TimerDue==Due)return true;
    if(S->Timer)(void)xrtNetEngineTimerCancelCurrent(S->Engine,S->Timer);
    if(S->Timer)return false;
    if(Due==INFINITY)return true;
    S->TimerDue=Due;xredisSessionRef(S);
    S->Timer=__xrtNetEngineSchedule(S->Engine,xrtNetWorkerIndex(S->Worker),Due,xredisSessionTimer,S);
    if(S->Timer)return true;
    xredisSubscriptionSessionRelease(S);return false;
}
static void xredisSessionFailure(xredissubscriptionsession* S,xredisrequeststatus Status,bool Fatal)
{
    xrtMutexLock(&S->Gate);
    if(!S->Closing){xredisSessionIncrement(&S->Info.Failures);S->Info.LastFailure=Status;S->Notify=true;S->Published=NULL;
        if(S->Consecutive!=UINT32_MAX)++S->Consecutive;
        if(Fatal || (S->Config.MaxFailures && S->Info.Failures && S->Consecutive>=S->Config.MaxFailures)){S->Closing=true;S->StopStatus=Status;S->Info.State=XREDIS_SESSION_CLOSING;}else S->Info.State=XREDIS_SESSION_DRAINING;}
    xrtMutexUnlock(&S->Gate);
    S->Draining=true;xredisSessionState(S,XREDIS_SESSION_DRAINING);if(S->Client)(void)xredisClientClose(S->Client);xredisSessionWake(S);
}
static void xredisSessionBackoff(xredissubscriptionsession* S)
{
    uint64 Cap=(uint64)S->Config.RetryMin<<(S->Consecutive<31?S->Consecutive:31);if(Cap>(uint64)S->Config.RetryMax)Cap=(uint64)S->Config.RetryMax;
    uint64 R=S->Random;R^=R<<13;R^=R>>7;R^=R<<17;S->Random=R;
    int64 Delay=S->Config.RetryMin+(int64)(R%(Cap-(uint64)S->Config.RetryMin+1));S->Due=xredisSessionDeadline(xrtTimer(),Delay);
    xrtMutexLock(&S->Gate);S->Info.NextAttempt=S->Due;xrtMutexUnlock(&S->Gate);xredisSessionState(S,XREDIS_SESSION_BACKOFF);
}
static xredisrequeststatus xredisSessionReadyStatus(xredisreadystatus Status)
{
    switch(Status){case XREDIS_READY_TIMEOUT:return XREDIS_REQUEST_TIMEOUT;case XREDIS_READY_DISCONNECTED:return XREDIS_REQUEST_DISCONNECTED;
    case XREDIS_READY_CLOSED:return XREDIS_REQUEST_CLOSED;case XREDIS_READY_PROTOCOL:return XREDIS_REQUEST_PROTOCOL;
    case XREDIS_READY_LIMIT:return XREDIS_REQUEST_LIMIT;case XREDIS_READY_MEMORY:return XREDIS_REQUEST_MEMORY;default:return XREDIS_REQUEST_INITIALIZATION;}
}
static void xredisSessionResultDrop(ptr Value,ptr Data){(void)Data;xrtFree(Value);}
static void xredisSessionFinish(xredissubscriptionsession* S)
{
    xrtMutexLock(&S->Gate);
    if(S->Queued || S->APIs || S->Client || S->Timer || S->Ready.Active || S->Closed.Active || S->Control.Active){xrtMutexUnlock(&S->Gate);return;}
    S->Finished=true;S->Info.State=XREDIS_SESSION_CLOSED;S->Info.NextAttempt=0;S->Notify=true;
    xnetengine* Engine=S->Engine;xredissubscriptions* Store=S->Store;S->Engine=NULL;S->Worker=NULL;S->Store=NULL;
    xredissubscriptionsessiondrop Drop=S->Config.Drop;ptr Data=S->Config.Data;xrtMutexUnlock(&S->Gate);
    __xredisSubscriptionsUnclaim(Store,S);xredisSubscriptionsRelease(Store);xredisSessionNotify(S);
    S->Config.Notify=NULL;S->Config.Connect=NULL;S->Config.Drop=NULL;S->Config.Data=NULL;if(Drop)Drop(Data);
    (void)xrtNetEngineUnpin(Engine);xpromise* P=S->Promise;S->Promise=NULL;xredissubscriptionsessionclosed* R=S->Result;S->Result=NULL;
    *R=(xredissubscriptionsessionclosed){S->StopStatus,S->Info.Attempts,S->Info.Generations};
    if(!xrtPromiseResolveOwned(P,R,xredisSessionResultDrop,NULL))xrtFree(R);
    xrtPromiseDestroy(P);xredisSubscriptionSessionRelease(S); /* Runtime. */
}
static bool xredisSessionDrain(xredissubscriptionsession* S)
{
    bool Ready=xredisSessionWatchStop(S,&S->Ready),Control=xredisSessionWatchStop(S,&S->Control);
    if(S->Replay){xredisSubscriptionReplayDestroy(S->Replay);S->Replay=NULL;}
    if(S->Client){
        (void)xredisClientClose(S->Client);
        if(!S->Closed.Future){if(!xredisSessionWatchStart(S,&S->Closed,xredisClientClosed(S->Client)))return false;}
        if(!xrtFutureDone(S->Closed.Future) || !Ready || !Control)return false;
        if(!xredisSessionWatchStop(S,&S->Closed))return false;
        xredisclientinfo I={0};I.Size=sizeof(I);(void)xredisClientInfo(S->Client,&I);
        if(I.Requests || I.InitBytes){S->WakeDue=xredisSessionDeadline(xrtTimer(),1);return false;}
        __xredisSubscriberReplayRelease(S->Client,S);xredisClientRelease(S->Client);S->Client=NULL;
    }
    if(!Ready || !Control)return false;
    xredisSubscriptionSnapshotRelease(S->Applied);S->Applied=NULL;S->Draining=false;
    if(!xredisSessionClosing(S))xredisSessionBackoff(S);
    return true;
}
static void xredisSessionStart(xredissubscriptionsession* S)
{
    S->ReadyOK=false;S->Entered=xrtTimer();S->Due=xredisSessionDeadline(S->Entered,S->Config.AttemptTimeout);
    xrtMutexLock(&S->Gate);xredisSessionIncrement(&S->Info.Attempts);uint64 Attempt=S->Info.Attempts;S->Info.NextAttempt=0;xrtMutexUnlock(&S->Gate);
    xredisSessionState(S,XREDIS_SESSION_CONNECTING);xredisclient* C=NULL;
    bool OK=S->Config.Connect(S->Engine,Attempt,S->Config.AttemptTimeout,S->Config.Data,&C);
    if(!OK || !C){xredisSessionFailure(S,XREDIS_REQUEST_INITIALIZATION,false);return;}
    S->Client=C;
    if(!__xredisSubscriptionClientProfile(C,S->Engine)){xredisSessionFailure(S,XREDIS_REQUEST_INITIALIZATION,true);return;}
    bool A=xredisSessionWatchStart(S,&S->Closed,xredisClientClosed(C));bool B=xredisSessionWatchStart(S,&S->Ready,xredisClientReady(C));
    if(!A || !B)xredisSessionFailure(S,XREDIS_REQUEST_MEMORY,true);
    else if(xrtFutureDone(S->Ready.Future))xredisSessionWake(S);
}
static void xredisSessionRestore(xredissubscriptionsession* S)
{
    if((S->Replay || !S->Applied) && xrtTimer()>=S->Due){xredisSessionFailure(S,XREDIS_REQUEST_TIMEOUT,false);return;}
    if(!S->Replay){
        if(!S->Applied){
            if(xrtTimer()>=S->Due){xredisSessionFailure(S,XREDIS_REQUEST_TIMEOUT,false);return;}
            if(!S->ReadyOK){
            if(!S->Ready.Future || !xrtFutureDone(S->Ready.Future))return;
            const xredisreadyresult* Ready=xrtFutureValue(S->Ready.Future);
            if(!Ready || Ready->Status!=XREDIS_READY_OK || Ready->Protocol!=S->Config.Protocol){xredisrequeststatus Why=Ready?xredisSessionReadyStatus(Ready->Status):XREDIS_REQUEST_INITIALIZATION;
                xredisSessionFailure(S,Why,Why!=XREDIS_REQUEST_TIMEOUT && Why!=XREDIS_REQUEST_DISCONNECTED && Why!=XREDIS_REQUEST_CLOSED);return;}
            if(!xredisSessionWatchStop(S,&S->Ready))return;
            S->ReadyOK=true;}
            xredisclientinfo I={0};I.Size=sizeof(I);(void)xredisClientInfo(S->Client,&I);
            if(I.Requests || I.InitBytes){S->WakeDue=xredisSessionDeadline(xrtTimer(),1);return;}
            if(__xredisSubscriberReplayStart(S->Client,&S->Config.Replay.Registry,S)!=XREDIS_ADMISSION_ACCEPTED){xredisSessionFailure(S,XREDIS_REQUEST_MEMORY,true);return;}
            xredisSessionState(S,XREDIS_SESSION_RESTORING);
        }else{S->Entered=xrtTimer();S->Due=xredisSessionDeadline(S->Entered,S->Config.AttemptTimeout);xredisSessionState(S,XREDIS_SESSION_UPDATING);}
        xredissubscriptionreplayconfig K=S->Config.Replay;K.Timeout=S->Config.AttemptTimeout;
        if(__xredisSubscriptionTransitionCreate(S->Store,S->Client,S->Applied,&K,S->Entered,S,&S->Replay)!=XREDIS_ADMISSION_ACCEPTED){xredisSessionFailure(S,XREDIS_REQUEST_MEMORY,true);return;}
    }
    if(S->Control.Future){if(!xrtFutureDone(S->Control.Future))return;if(!xredisSessionWatchStop(S,&S->Control))return;}
    xredissubscriptionstep Step=xredisSubscriptionReplayStep(S->Replay);
    if(Step==XREDIS_SUBSCRIPTIONS_WAIT){xfuture* F=xredisSubscriptionReplayPending(S->Replay);if(F){if(!xredisSessionWatchStart(S,&S->Control,F))xredisSessionFailure(S,XREDIS_REQUEST_MEMORY,true);else if(xrtFutureDone(F))xredisSessionWake(S);}else xredisSessionWake(S);return;}
    if(Step==XREDIS_SUBSCRIPTIONS_MORE){xredisSessionWake(S);return;}
    xredissubscriptionreplayresult R={0};R.Size=sizeof(R);
    if(Step==XREDIS_SUBSCRIPTIONS_STEP_ERROR || !xredisSubscriptionReplayResult(S->Replay,&R)){xredisSessionFailure(S,XREDIS_REQUEST_PROTOCOL,true);return;}
    if(R.Status!=XREDIS_SUBSCRIPTIONS_OK){xredisSessionFailure(S,R.NativeStatus,R.NativeStatus!=XREDIS_REQUEST_TIMEOUT && R.NativeStatus!=XREDIS_REQUEST_DISCONNECTED && R.NativeStatus!=XREDIS_REQUEST_CLOSED);return;}
    xredissubscriptionsnapshot* Applied=__xredisSubscriptionTransitionSnapshot(S->Replay);if(!Applied){xredisSessionFailure(S,XREDIS_REQUEST_MEMORY,true);return;}
    bool Fresh=S->Applied==NULL;xredisSubscriptionSnapshotRelease(S->Applied);S->Applied=Applied;xredisSubscriptionReplayDestroy(S->Replay);S->Replay=NULL;S->Consecutive=0;
    xrtMutexLock(&S->Gate);if(Fresh)xredisSessionIncrement(&S->Info.Generations);xredisSessionIncrement(&S->Info.Restored);S->Info.AppliedRevision=R.Revision;S->Published=S->Closing?NULL:S->Client;
    if(!S->Closing){S->Info.State=S->Info.DesiredRevision==R.Revision?XREDIS_SESSION_ONLINE:XREDIS_SESSION_UPDATING;S->Notify=true;}xrtMutexUnlock(&S->Gate);xredisSessionWake(S);
}
static void xredisSessionWork(xnetworker* Worker,ptr Data)
{
    (void)Worker;xredissubscriptionsession* S=Data;S->WakeDue=INFINITY;xrtMutexLock(&S->Gate);S->Queued=false;bool Closing=S->Closing;xrtMutexUnlock(&S->Gate);
    if(Closing || S->Draining){
        if(S->Timer)(void)xrtNetEngineTimerCancelCurrent(S->Engine,S->Timer);
        S->Draining=true;(void)xredisSessionDrain(S);
        if(xredisSessionClosing(S) && !S->Client && !S->Draining)xredisSessionFinish(S);
    }else if(!S->Client){if(xrtTimer()>=S->Due)xredisSessionStart(S);}
    else if(xrtFutureDone(S->Closed.Future)){xredisclientinfo I={0};I.Size=sizeof(I);(void)xredisClientInfo(S->Client,&I);xredisSessionFailure(S,I.CloseStatus==XREDIS_REQUEST_OK?XREDIS_REQUEST_DISCONNECTED:I.CloseStatus,false);}
    else{
        xredissubscriptionsinfo I={0};I.Size=sizeof(I);(void)xredisSubscriptionsInfo(S->Store,&I);
        xrtMutexLock(&S->Gate);if(S->Info.DesiredRevision<I.Revision)S->Info.DesiredRevision=I.Revision;bool Needed=!S->Applied || S->Replay || S->Info.AppliedRevision!=S->Info.DesiredRevision;if(!Needed && !S->Closing && S->Info.State!=XREDIS_SESSION_ONLINE){S->Info.State=XREDIS_SESSION_ONLINE;S->Notify=true;}xrtMutexUnlock(&S->Gate);
        if(Needed)xredisSessionRestore(S);
    }
    if(!S->Finished){
        double Due=INFINITY;
        if(S->Draining || xredisSessionClosing(S)){
            xrtMutexLock(&S->Gate);bool Watching=S->Ready.Active || S->Closed.Active || S->Control.Active;xrtMutexUnlock(&S->Gate);
            if(S->Draining && !Watching && (!S->Client || (S->Closed.Future && xrtFutureDone(S->Closed.Future) && S->WakeDue==INFINITY)))xredisSessionWake(S);
        }else Due=(!S->Client || S->Replay || !S->Applied)?S->Due:INFINITY;
        if(S->WakeDue<Due)Due=S->WakeDue;
        if(!xredisSessionArm(S,Due)){xredisSessionFailure(S,XREDIS_REQUEST_MEMORY,true);xredisSessionWake(S);}
        xredisSessionNotify(S);
    }
    xredisSubscriptionSessionRelease(S); /* Post. */
}
XREDIS_API bool xredisSubscriptionSessionConfigInit(xredissubscriptionsessionconfig* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K)))return xredisSessionReport(XERR_ARGUMENT,"invalid session config");
    *K=(xredissubscriptionsessionconfig){0};K->Size=sizeof(*K);K->Protocol=XREDIS_RESP3;K->AttemptTimeout=5000;K->RetryMin=100;K->RetryMax=10000;return xredisSubscriptionReplayConfigInit(&K->Replay);
}
XREDIS_API xredisadmission xredisSubscriptionSessionCreate(xnetengine* Engine,uint64 Affinity,xredissubscriptions* Store,const xredissubscriptionsessionconfig* Config,xredissubscriptionsession** Output)
{
    if(!Engine || !Store || !Config || !xrtMemRangeValid(Config,sizeof(*Config)) || !Output || !xrtMemRangeValid(Output,sizeof(*Output)) || xrtMemRangesOverlap(Config,sizeof(*Config),Output,sizeof(*Output))){xredisSessionReport(XERR_ARGUMENT,"invalid session arguments");return XREDIS_ADMISSION_ERROR;}
    xredissubscriptionsessionconfig K=*Config;
    if(K.Size!=sizeof(K) || !K.Connect || (K.Protocol!=XREDIS_RESP2 && K.Protocol!=XREDIS_RESP3) || K.AttemptTimeout<=0 || K.AttemptTimeout>60000 || K.RetryMin<=0 || K.RetryMax<K.RetryMin || K.RetryMax>60000 || K.Replay.Size!=sizeof(K.Replay) || !K.Replay.MaxBatchNames || K.Replay.MaxBatchNames>XREDIS_PUBSUB_NAMES_MAX || !K.Replay.MaxBatchBytes || K.Replay.Registry.Size!=sizeof(K.Replay.Registry) || !K.Replay.Registry.MaxSubscriptions || K.Replay.Registry.MaxSubscriptions>100000 || !K.Replay.Registry.MaxNameBytes){xredisSessionReport(XERR_ARGUMENT,"invalid session bounds/profile");return XREDIS_ADMISSION_ERROR;}
    xredissubscriptionsinfo Info={0};Info.Size=sizeof(Info);(void)xredisSubscriptionsInfo(Store,&Info);
    if(!__xredisSubscriptionsFits(Store,&K.Replay.Registry) || !__xredisSubscriptionRegistryBounds(&K.Replay.Registry)){xredisSessionReport(XERR_RANGE,"session registry cannot fit intent");return XREDIS_ADMISSION_ERROR;}
    if(!xrtNetEnginePin(Engine))return XREDIS_ADMISSION_ERROR;
    xredissubscriptionsession* S=xrtCalloc(1,sizeof(*S));if(!S || !xrtMutexInit(&S->Gate)){xrtFree(S);(void)xrtNetEngineUnpin(Engine);return XREDIS_ADMISSION_ERROR;}
    xrtAtomic32Init(&S->References,1);S->Engine=Engine;S->Worker=xrtNetEngineWorker(Engine,(uint32)(Affinity%xrtNetEngineWorkerCount(Engine)));S->Config=K;
    S->Info.Size=sizeof(S->Info);S->Info.State=XREDIS_SESSION_CONNECTING;S->Info.DesiredRevision=Info.Revision;S->Random=((uint64)(xrtTimer()*1000000.0)^(uint64)(uintptr_t)S)|1u;S->Notify=true;
    S->Result=xrtMalloc(sizeof(*S->Result));
    if(!S->Result || !xrtNetPostInit(&S->Work) || !(S->Promise=xrtPromiseCreate(&S->Future,NULL)) || !xredisSubscriptionsRetain(Store)){(void)xrtNetEngineUnpin(Engine);xredisSubscriptionSessionRelease(S);return XREDIS_ADMISSION_ERROR;}
    xredisadmission A=__xredisSubscriptionsClaim(Store,S,Output);if(A!=XREDIS_ADMISSION_ACCEPTED){xredisSubscriptionsRelease(Store);(void)xrtNetEngineUnpin(Engine);xredisSubscriptionSessionRelease(S);return A;}
    (void)xredisSubscriptionsInfo(Store,&Info);S->Info.DesiredRevision=Info.Revision;
    S->Store=Store;xredisSessionRef(S); /* Runtime. */
    xrtMutexLock(&S->Gate);*Output=S;xredisSessionWakeLocked(S);xrtMutexUnlock(&S->Gate);return XREDIS_ADMISSION_ACCEPTED;
}
XREDIS_API xredisadmission xredisSubscriptionSessionApply(xredissubscriptionsession* S,xredispubsubcommand Command,const xbytesview* Names,size_t Count,uint64* Revision)
{
    if(!S || !Revision || xrtMemRangesOverlap(S,S?sizeof(*S):0,Revision,sizeof(*Revision))){xredisSessionReport(XERR_ARGUMENT,"invalid session edit");return XREDIS_ADMISSION_ERROR;}
    xrtMutexLock(&S->Gate);if(S->Closing){xrtMutexUnlock(&S->Gate);return XREDIS_ADMISSION_CLOSED;}++S->APIs;xredissubscriptions* Store=S->Store;xrtMutexUnlock(&S->Gate);
    xredisadmission A=__xredisSubscriptionsApply(Store,Command,Names,Count,Revision,S);
    xrtMutexLock(&S->Gate);--S->APIs;if(A==XREDIS_ADMISSION_ACCEPTED && S->Info.DesiredRevision<*Revision){S->Info.DesiredRevision=*Revision;S->Notify=true;if(!S->Closing && S->Info.State==XREDIS_SESSION_ONLINE)S->Info.State=XREDIS_SESSION_UPDATING;}xredisSessionWakeLocked(S);xrtMutexUnlock(&S->Gate);return A;
}
XREDIS_API bool xredisSubscriptionSessionInfo(xredissubscriptionsession* S,xredissubscriptionsessioninfo* Info)
{
    if(!S || !Info || !xrtMemRangeValid(Info,sizeof(*Info)) || Info->Size!=sizeof(*Info) || xrtMemRangesOverlap(S,S?sizeof(*S):0,Info,sizeof(*Info)))return xredisSessionReport(XERR_ARGUMENT,"invalid session info");
    xrtMutexLock(&S->Gate);*Info=S->Info;xrtMutexUnlock(&S->Gate);return true;
}
XREDIS_API xredisadmission xredisSubscriptionSessionCurrent(xredissubscriptionsession* S,xredissubscriptionsessioncurrent* Output)
{
    if(!S || !Output || !xrtMemRangeValid(Output,sizeof(*Output)) || xrtMemRangesOverlap(S,S?sizeof(*S):0,Output,sizeof(*Output))){xredisSessionReport(XERR_ARGUMENT,"invalid current output");return XREDIS_ADMISSION_ERROR;}
    xrtMutexLock(&S->Gate);xredisadmission A=S->Closing?XREDIS_ADMISSION_CLOSED:S->Published && S->Info.State==XREDIS_SESSION_ONLINE && S->Info.AppliedRevision==S->Info.DesiredRevision?XREDIS_ADMISSION_ACCEPTED:XREDIS_ADMISSION_OVERLOADED;
    if(A==XREDIS_ADMISSION_ACCEPTED){if(xredisClientRetain(S->Published))*Output=(xredissubscriptionsessioncurrent){S->Published,S->Info.Generations,S->Info.AppliedRevision};else A=XREDIS_ADMISSION_ERROR;}xrtMutexUnlock(&S->Gate);return A;
}
XREDIS_API bool xredisSubscriptionSessionClose(xredissubscriptionsession* S)
{
    if(!S)return xredisSessionReport(XERR_ARGUMENT,"session is NULL");
    xrtMutexLock(&S->Gate);if(!S->Closing){S->Closing=true;S->Info.State=XREDIS_SESSION_CLOSING;S->Notify=true;S->Published=NULL;}xredisSessionWakeLocked(S);xrtMutexUnlock(&S->Gate);return true;
}
XREDIS_API xfuture* xredisSubscriptionSessionClosed(xredissubscriptionsession* S)
{if(!S){xredisSessionReport(XERR_ARGUMENT,"session is NULL");return NULL;}return xrtFutureRef(S->Future);}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/sentinel/sentinel.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_SENTINEL)
#include <math.h>
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL)
enum {XS_SEED,XS_SEED_READY,XS_QUERY,XS_SEED_DRAIN,XS_MASTER,XS_MASTER_READY,XS_ROLE,XS_MASTER_DRAIN,XS_BACKOFF,XS_SUCCESS,XS_STOP,XS_DONE};
struct xredissentineldiscovery {
    xnetengine* Engine;xredissentinelconfig Config;xredissentinelresult Result;
    xredisclient* Client;xfuture* Pending;xfuture* Closed;
    double Deadline,SeedDue,RetryDue,NextWake;
    size_t State,SeedOffset,RoundNull;bool Taken,Stepping;
    xredissentinelstatus AfterDrain;
    unsigned char Data[];
};
static bool xredisSentinelArgument(cstr Text)
{xrtSetErrorInfo(XERR_ARGUMENT,"xrt.redis",XREDIS_CLIENT_ARGUMENT,Text);return false;}
static bool xredisSentinelDisjoint(xredissentineldiscovery* D,ptr P,size_t N)
{return P && xrtMemRangeValid(P,N) && !xrtMemRangesOverlap(D,sizeof(*D)+D->Config.Service.Size+D->Config.MaxHostBytes,P,N);}
static double xredisSentinelDue(double Now,int64 Timeout)
{return nextafter(Now+(double)Timeout*.001,INFINITY);}
static int64 xredisSentinelRemaining(double Due)
{double N=ceil((Due-xrtTimer())*1000.0);return N>0?(int64)N:0;}
static void xredisSentinelDropPending(xredissentineldiscovery* D)
{if(D->Pending){xrtFutureDestroy(D->Pending);D->Pending=NULL;}}
static void xredisSentinelStop(xredissentineldiscovery* D,xredissentinelstatus Status)
{D->Result.Status=Status;D->State=XS_STOP;if(D->Client)(void)xredisClientClose(D->Client);}
static void xredisSentinelFinish(xredissentineldiscovery* D)
{D->State=XS_DONE;D->NextWake=0;if(D->Engine){(void)xrtNetEngineUnpin(D->Engine);D->Engine=NULL;}}
static xredissentinelstatus xredisSentinelReadyClass(xredisreadystatus S)
{
    switch(S){case XREDIS_READY_TIMEOUT:return XREDIS_SENTINEL_TIMEOUT;
    case XREDIS_READY_DISCONNECTED:case XREDIS_READY_CLOSED:return XREDIS_SENTINEL_UNREACHABLE;
    case XREDIS_READY_AUTH_FAILED:return XREDIS_SENTINEL_AUTH_FAILED;
    case XREDIS_READY_SERVER_ERROR:return XREDIS_SENTINEL_SERVER_ERROR;
    case XREDIS_READY_LIMIT:return XREDIS_SENTINEL_LIMIT;case XREDIS_READY_MEMORY:return XREDIS_SENTINEL_MEMORY;
    case XREDIS_READY_ROLE_MISMATCH:return XREDIS_SENTINEL_STALE_ROLE;default:return XREDIS_SENTINEL_PROTOCOL;}
}
static xredissentinelstatus xredisSentinelRequestClass(xredisrequeststatus S)
{
    switch(S){case XREDIS_REQUEST_TIMEOUT:return XREDIS_SENTINEL_TIMEOUT;
    case XREDIS_REQUEST_DISCONNECTED:case XREDIS_REQUEST_CLOSED:return XREDIS_SENTINEL_UNREACHABLE;
    case XREDIS_REQUEST_CANCELLED:return XREDIS_SENTINEL_CANCELLED;
    case XREDIS_REQUEST_SERVER_ERROR:return XREDIS_SENTINEL_SERVER_ERROR;
    case XREDIS_REQUEST_MEMORY:return XREDIS_SENTINEL_MEMORY;case XREDIS_REQUEST_LIMIT:return XREDIS_SENTINEL_LIMIT;
    default:return XREDIS_SENTINEL_PROTOCOL;}
}
static bool xredisSentinelTransient(xredissentinelstatus S)
{return S==XREDIS_SENTINEL_TIMEOUT || S==XREDIS_SENTINEL_UNREACHABLE || S==XREDIS_SENTINEL_STALE_ROLE;}
static void xredisSentinelDrainStart(xredissentineldiscovery* D,bool Seed,xredissentinelstatus Why)
{D->AfterDrain=Why;D->State=Seed?XS_SEED_DRAIN:XS_MASTER_DRAIN;if(D->Client)(void)xredisClientClose(D->Client);}
static bool xredisSentinelDrain(xredissentineldiscovery* D)
{
    xredisSentinelDropPending(D);
    if(!D->Client)return true;
    (void)xredisClientClose(D->Client);
    if(!D->Closed)D->Closed=xredisClientClosed(D->Client);
    if(!D->Closed || !xrtFutureDone(D->Closed))return false;
    xredisclientinfo I={0};I.Size=sizeof(I);(void)xredisClientInfo(D->Client,&I);
    if(I.Requests || I.InitBytes){D->NextWake=xredisSentinelDue(xrtTimer(),1);return false;}
    xrtFutureDestroy(D->Closed);D->Closed=NULL;xredisClientRelease(D->Client);D->Client=NULL;return true;
}
static void xredisSentinelBackoff(xredissentineldiscovery* D,xredissentinelstatus Last)
{
    if(D->Result.Rounds>=D->Config.MaxRounds){xredisSentinelStop(D,Last);return;}
    D->RetryDue=xredisSentinelDue(xrtTimer(),D->Config.RetryDelay);D->State=XS_BACKOFF;
}
static void xredisSentinelNextSeed(xredissentineldiscovery* D)
{
    if(++D->SeedOffset<D->Config.SeedCount){D->State=XS_SEED;return;}
    xredissentinelstatus Why=D->RoundNull==D->Config.SeedCount?XREDIS_SENTINEL_UNKNOWN_SERVICE:XREDIS_SENTINEL_UNREACHABLE;
    /* An all-unknown round has no usable topology; do not hide it behind retry. */
    if(Why==XREDIS_SENTINEL_UNKNOWN_SERVICE)xredisSentinelStop(D,Why);else xredisSentinelBackoff(D,Why);
}
static void xredisSentinelFactory(xredissentineldiscovery* D,bool Seed)
{
    double Now=xrtTimer();if(Seed){D->SeedDue=fmin(D->Deadline,xredisSentinelDue(Now,D->Config.SeedTimeout));++D->Result.SeedAttempts;}
    else ++D->Result.MasterAttempts;
    xredisclient* C=NULL;xredissentinelstatus Status=Seed?D->Config.ConnectSeed(D->Engine,(D->Result.PreferredSeed+D->SeedOffset)%D->Config.SeedCount,xredisSentinelRemaining(D->SeedDue),D->Config.Data,&C):
        D->Config.ConnectMaster(D->Engine,&D->Result.Endpoint,xredisSentinelRemaining(D->Deadline),D->Config.Data,&C);
    /* Factory failure is its explicit contract, never classified from ambient error. */
    if(Status!=XREDIS_SENTINEL_OK){
        if(xredisSentinelTransient(Status))xredisSentinelDrainStart(D,Seed,Status);
        else xredisSentinelStop(D,Status>=XREDIS_SENTINEL_UNREACHABLE && Status<=XREDIS_SENTINEL_FACTORY?Status:XREDIS_SENTINEL_FACTORY);
        return;
    }
    if(!C){xredisSentinelStop(D,XREDIS_SENTINEL_FACTORY);return;}
    D->Client=C;
    if(!__xredisSentinelClientProfile(C,D->Engine)){xredisSentinelStop(D,XREDIS_SENTINEL_FACTORY);return;}
    D->Pending=xredisClientReady(C);
    if(!D->Pending){xredisSentinelStop(D,XREDIS_SENTINEL_MEMORY);return;}
    D->State=Seed?XS_SEED_READY:XS_MASTER_READY;
}
static void xredisSentinelReady(xredissentineldiscovery* D,bool Seed)
{
    if(!xrtFutureDone(D->Pending))return;
    const xredisreadyresult* R=xrtFutureValue(D->Pending);
    if(!R){xredisSentinelStop(D,XREDIS_SENTINEL_PROTOCOL);return;}
    D->Result.ReadyObserved=true;D->Result.ReadyStatus=R->Status;
    if(R->Status!=XREDIS_READY_OK){xredissentinelstatus Why=xredisSentinelReadyClass(R->Status);
        if(xredisSentinelTransient(Why))xredisSentinelDrainStart(D,Seed,Why);else xredisSentinelStop(D,Why);return;}
    if(R->Protocol!=D->Config.Protocol){xredisSentinelStop(D,XREDIS_SENTINEL_PROTOCOL);return;}
    xredisSentinelDropPending(D);
    xbytesview Args[]={XRT_BYTES_LITERAL("SENTINEL"),XRT_BYTES_LITERAL("GET-MASTER-ADDR-BY-NAME"),D->Config.Service};
    xbytesview Role=XRT_BYTES_LITERAL("ROLE");
    xredisadmission A=xredisClientCallAsync(D->Client,Seed?Args:&Role,Seed?3:1,xredisSentinelRemaining(Seed?D->SeedDue:D->Deadline),&D->Pending);
    if(A!=XREDIS_ADMISSION_ACCEPTED){xredissentinelstatus Why=A==XREDIS_ADMISSION_OVERLOADED?XREDIS_SENTINEL_LIMIT:A==XREDIS_ADMISSION_CLOSED?XREDIS_SENTINEL_UNREACHABLE:XREDIS_SENTINEL_MEMORY;
        if(xredisSentinelTransient(Why))xredisSentinelDrainStart(D,Seed,Why);else xredisSentinelStop(D,Why);return;}
    D->State=Seed?XS_QUERY:XS_ROLE;
}
static bool xredisSentinelString(const xredisreplyvalue* V)
{return (V->Type==XREDIS_REPLY_BULK_STRING || V->Type==XREDIS_REPLY_SIMPLE_STRING) && !(V->Flags&XREDIS_REPLY_NULL_VALUE);}
static xredissentinelstatus xredisSentinelEndpoint(xredissentineldiscovery* D,const xredisreply* Reply)
{
    xredisreplyinfo Info={0};Info.Size=sizeof(Info);(void)xredisReplyInfo(Reply,&Info);
    xredisreplyvalue Root,H,P;(void)xredisReplyGet(Reply,Info.Root,&Root);
    if(Root.Type==XREDIS_REPLY_NULL || ((Root.Type==XREDIS_REPLY_ARRAY || Root.Type==XREDIS_REPLY_BULK_STRING) && (Root.Flags&XREDIS_REPLY_NULL_VALUE)))return XREDIS_SENTINEL_UNKNOWN_SERVICE;
    if(Root.Type!=XREDIS_REPLY_ARRAY || Root.ChildCount!=2)return XREDIS_SENTINEL_PROTOCOL;
    (void)xredisReplyGet(Reply,Root.FirstChild,&H);(void)xredisReplyGet(Reply,H.Next,&P);
    if(!xredisSentinelString(&H) || !H.Data.Size || !xredisSentinelString(&P) || !P.Data.Size || P.Data.Size>5)return XREDIS_SENTINEL_PROTOCOL;
    if(H.Data.Size>D->Config.MaxHostBytes)return XREDIS_SENTINEL_LIMIT;
    for(size_t I=0;I<H.Data.Size;++I)if(H.Data.Data[I]<=32 || H.Data.Data[I]==127)return XREDIS_SENTINEL_PROTOCOL;
    uint32 Port=0;for(size_t I=0;I<P.Data.Size;++I){unsigned C=P.Data.Data[I];if(C<'0' || C>'9' || Port>(65535u-(C-'0'))/10u)return XREDIS_SENTINEL_PROTOCOL;Port=Port*10u+C-'0';}
    if(!Port)return XREDIS_SENTINEL_PROTOCOL;
    unsigned char* Host=D->Data+D->Config.Service.Size;memcpy(Host,H.Data.Data,H.Data.Size);
    D->Result.Endpoint=(xredissentinelendpoint){{Host,H.Data.Size},(uint16)Port};return XREDIS_SENTINEL_OK;
}
static void xredisSentinelReply(xredissentineldiscovery* D,bool Seed)
{
    if(!xrtFutureDone(D->Pending))return;
    const xredisrequestresult* R=xrtFutureValue(D->Pending);
    if(!R){xredisSentinelStop(D,XREDIS_SENTINEL_PROTOCOL);return;}
    D->Result.RequestObserved=true;D->Result.RequestStatus=R->Status;
    if(R->Status!=XREDIS_REQUEST_OK){xredissentinelstatus Why=xredisSentinelRequestClass(R->Status);
        if(xredisSentinelTransient(Why))xredisSentinelDrainStart(D,Seed,Why);else xredisSentinelStop(D,Why);return;}
    if(!R->Reply){xredisSentinelStop(D,XREDIS_SENTINEL_PROTOCOL);return;}
    if(Seed){
        xredissentinelstatus S=xredisSentinelEndpoint(D,R->Reply);
        if(S==XREDIS_SENTINEL_OK){D->Result.PreferredSeed=(D->Result.PreferredSeed+D->SeedOffset)%D->Config.SeedCount;xredisSentinelDrainStart(D,true,XREDIS_SENTINEL_OK);}
        else if(S==XREDIS_SENTINEL_UNKNOWN_SERVICE){++D->Result.UnknownReplies;++D->RoundNull;xredisSentinelDrainStart(D,true,S);}
        else xredisSentinelStop(D,S);
    }else{
        xredisroleinfo I;xredisrolestatus S=xredisRoleRead(R->Reply,&D->Config.Role,&I);
        if(S!=XREDIS_ROLE_VALUE){xredisSentinelStop(D,S==XREDIS_ROLE_LIMIT?XREDIS_SENTINEL_LIMIT:XREDIS_SENTINEL_PROTOCOL);return;}
        if(I.Kind!=XREDIS_ROLE_MASTER){++D->Result.StaleReplies;xredisSentinelDrainStart(D,false,XREDIS_SENTINEL_STALE_ROLE);return;}
        xredisSentinelDropPending(D);D->State=XS_SUCCESS;
    }
}
XREDIS_API bool xredisSentinelConfigInit(xredissentinelconfig* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K)))return xredisSentinelArgument("invalid Sentinel config output");
    *K=(xredissentinelconfig){0};K->Size=sizeof(*K);K->SeedCount=1;K->MaxRounds=3;K->MaxServiceBytes=65536;K->MaxHostBytes=1024;
    K->Timeout=5000;K->SeedTimeout=300;K->RetryDelay=100;K->Protocol=XREDIS_RESP3;return xredisRoleLimitsInit(&K->Role);
}
bool __xredisSentinelConfigValid(const xredissentinelconfig* Config,ptr Output,size_t Bytes)
{
    if(!Config || !xrtMemRangeValid(Config,sizeof(*Config)))return xredisSentinelArgument("invalid Sentinel config input");
    xredissentinelconfig K=*Config;
    if(K.Size!=sizeof(K) || K.Role.Size!=sizeof(K.Role) || !K.ConnectSeed || !K.ConnectMaster || !K.SeedCount || K.SeedCount>4096 || K.PreferredSeed>=K.SeedCount || !K.MaxRounds || K.MaxRounds>1024 ||
       K.Timeout<=0 || K.Timeout>86400000 || K.SeedTimeout<=0 || K.SeedTimeout>60000 || K.RetryDelay<=0 || K.RetryDelay>60000 ||
       !K.MaxServiceBytes || K.MaxServiceBytes>65536 || !K.MaxHostBytes || K.MaxHostBytes>65536 || !K.Service.Size || K.Service.Size>K.MaxServiceBytes || !xrtMemRangeValid(K.Service.Data,K.Service.Size) ||
       xrtMemRangesOverlap(K.Service.Data,K.Service.Size,Output,Bytes) || (K.Protocol!=XREDIS_RESP2 && K.Protocol!=XREDIS_RESP3)){
        xredisSentinelArgument("invalid Sentinel bounds/service/factories");return false;}
    return true;
}
xredisadmission __xredisSentinelDiscoveryCreateUntil(xnetengine* Engine,const xredissentinelconfig* Config,double Deadline,xredissentineldiscovery** Output)
{
    if(!isfinite(Deadline) || !Engine || !Config || !Output || !xrtMemRangeValid(Config,sizeof(*Config)) || !xrtMemRangeValid(Output,sizeof(*Output)) || xrtMemRangesOverlap(Config,sizeof(*Config),Output,sizeof(*Output))){xredisSentinelArgument("invalid Sentinel discovery arguments");return XREDIS_ADMISSION_ERROR;}
    xredissentinelconfig K=*Config;
    if(!__xredisSentinelConfigValid(&K,Output,sizeof(*Output)))return XREDIS_ADMISSION_ERROR;
    if(!xrtNetEnginePin(Engine))return XREDIS_ADMISSION_ERROR;
    xredissentineldiscovery* D=xrtCalloc(1,sizeof(*D)+K.Service.Size+K.MaxHostBytes);
    if(!D){(void)xrtNetEngineUnpin(Engine);return XREDIS_ADMISSION_ERROR;}
    D->Engine=Engine;D->Config=K;memcpy(D->Data,K.Service.Data,K.Service.Size);D->Config.Service=(xbytesview){D->Data,K.Service.Size};
    D->Deadline=Deadline;D->NextWake=xrtTimer();D->Result.Size=sizeof(D->Result);D->Result.PreferredSeed=K.PreferredSeed;D->Result.Rounds=1;*Output=D;return XREDIS_ADMISSION_ACCEPTED;
}
XREDIS_API xredisadmission xredisSentinelDiscoveryCreate(xnetengine* Engine,const xredissentinelconfig* Config,xredissentineldiscovery** Output)
{
    double Entered=xrtTimer();
    if(!Engine || !Config || !Output || !xrtMemRangeValid(Config,sizeof(*Config)) || !xrtMemRangeValid(Output,sizeof(*Output)) || xrtMemRangesOverlap(Config,sizeof(*Config),Output,sizeof(*Output))){xredisSentinelArgument("invalid Sentinel discovery arguments");return XREDIS_ADMISSION_ERROR;}
    return __xredisSentinelDiscoveryCreateUntil(Engine,Config,xredisSentinelDue(Entered,Config->Timeout),Output);
}
XREDIS_API xredissentinelstep xredisSentinelDiscoveryStep(xredissentineldiscovery* D)
{
    if(!D || D->Stepping){xredisSentinelArgument("invalid/reentrant Sentinel Step");return XREDIS_SENTINEL_STEP_ERROR;}
    if(D->State==XS_DONE)return XREDIS_SENTINEL_DONE;
    size_t Before=D->State;D->Stepping=true;double Now=xrtTimer();D->NextWake=D->Deadline;
    if(Now>=D->Deadline && D->State!=XS_STOP)xredisSentinelStop(D,XREDIS_SENTINEL_TIMEOUT);
    bool Seed=D->State==XS_SEED_READY || D->State==XS_QUERY;
    if(Seed && Now>=D->SeedDue)xredisSentinelDrainStart(D,true,XREDIS_SENTINEL_TIMEOUT);
    switch(D->State){
    case XS_SEED:xredisSentinelFactory(D,true);break;
    case XS_SEED_READY:xredisSentinelReady(D,true);break;
    case XS_QUERY:xredisSentinelReply(D,true);break;
    case XS_SEED_DRAIN:
        if(xredisSentinelDrain(D)){if(D->AfterDrain==XREDIS_SENTINEL_OK)D->State=XS_MASTER;else xredisSentinelNextSeed(D);}break;
    case XS_MASTER:xredisSentinelFactory(D,false);break;
    case XS_MASTER_READY:xredisSentinelReady(D,false);break;
    case XS_ROLE:xredisSentinelReply(D,false);break;
    case XS_MASTER_DRAIN:if(xredisSentinelDrain(D))xredisSentinelBackoff(D,D->AfterDrain);break;
    case XS_BACKOFF:if(Now>=D->RetryDue){D->SeedOffset=0;D->RoundNull=0;++D->Result.Rounds;D->State=XS_SEED;}break;
    case XS_SUCCESS:{xredisclientinfo I={0};I.Size=sizeof(I);(void)xredisClientInfo(D->Client,&I);
        if(I.State!=XREDIS_CLIENT_OPEN)xredisSentinelDrainStart(D,false,XREDIS_SENTINEL_UNREACHABLE);
        else if(I.Requests || I.InitBytes)D->NextWake=xredisSentinelDue(Now,1);
        else {D->Result.Status=XREDIS_SENTINEL_OK;xredisSentinelFinish(D);}break;}
    case XS_STOP:if(xredisSentinelDrain(D))xredisSentinelFinish(D);break;
    default:break;}
    D->Stepping=false;
    if(D->State==XS_DONE)return XREDIS_SENTINEL_DONE;
    Now=xrtTimer();
    if(D->State==XS_SEED_READY || D->State==XS_QUERY)D->NextWake=fmin(D->NextWake,D->SeedDue);
    if(D->State==XS_BACKOFF)D->NextWake=fmin(D->NextWake,D->RetryDue);
    if(D->State==XS_STOP || D->State==XS_MASTER_DRAIN || D->State==XS_SEED_DRAIN){
        /* Close may retire before request/control release tails; no expired-deadline spin. */
        D->NextWake=xredisSentinelDue(Now,1);
        return Before!=D->State?XREDIS_SENTINEL_MORE:XREDIS_SENTINEL_WAIT;
    }
    if(D->State==XS_SUCCESS){D->NextWake=fmin(D->Deadline,xredisSentinelDue(Now,1));return Before!=D->State?XREDIS_SENTINEL_MORE:XREDIS_SENTINEL_WAIT;}
    if((D->Pending && !xrtFutureDone(D->Pending)) || (D->State==XS_BACKOFF && Now<D->RetryDue))return XREDIS_SENTINEL_WAIT;
    return XREDIS_SENTINEL_MORE;
}
XREDIS_API xfuture* xredisSentinelDiscoveryPending(xredissentineldiscovery* D)
{if(!D){xredisSentinelArgument("invalid Sentinel discovery");return NULL;}xfuture* F=D->Closed?D->Closed:D->Pending;return F && !xrtFutureDone(F)?xrtFutureRef(F):NULL;}
XREDIS_API double xredisSentinelDiscoveryNextWake(xredissentineldiscovery* D)
{if(!D){xredisSentinelArgument("invalid Sentinel discovery");return 0;}return D->NextWake;}
XREDIS_API bool xredisSentinelDiscoveryResult(xredissentineldiscovery* D,xredissentinelresult* R)
{if(!D || !xredisSentinelDisjoint(D,R,sizeof(*R)) || R->Size!=sizeof(*R))return xredisSentinelArgument("invalid Sentinel result");*R=D->Result;return true;}
XREDIS_API bool xredisSentinelDiscoveryTakeClient(xredissentineldiscovery* D,xredisclient** C)
{
    if(!D || !xredisSentinelDisjoint(D,C,sizeof(*C)) || D->State!=XS_DONE || D->Result.Status!=XREDIS_SENTINEL_OK || D->Taken)return xredisSentinelArgument("Sentinel client is not available");
    *C=D->Client;D->Client=NULL;D->Taken=true;return true;
}
XREDIS_API bool xredisSentinelDiscoveryCancel(xredissentineldiscovery* D)
{if(!D || D->Stepping)return xredisSentinelArgument("invalid/reentrant Sentinel cancel");if(D->State!=XS_DONE)xredisSentinelStop(D,XREDIS_SENTINEL_CANCELLED);return true;}
XREDIS_API void xredisSentinelDiscoveryDestroy(xredissentineldiscovery* D)
{
    if(!D)return;
    if(D->Stepping){(void)xredisSentinelArgument("reentrant Sentinel destroy");return;}
    if(D->Client){(void)xredisClientClose(D->Client);xredisClientRelease(D->Client);}
    if(D->Pending)(void)xrtFutureCancel(D->Pending);
    xredisSentinelDropPending(D);xrtFutureDestroy(D->Closed);if(D->Engine)(void)xrtNetEngineUnpin(D->Engine);xrtFree(D);
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/sentinel_pool/sentinel_pool.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_POOL)
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_POOL)
XREDIS_API bool xredisSentinelPoolConfigInit(xredissentinelpoolconfig* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K))){xrtSetErrorInfo(XERR_ARGUMENT,"xrt.redis",XREDIS_CLIENT_ARGUMENT,"invalid Sentinel pool config");return false;}
    *K=(xredissentinelpoolconfig){0};K->Size=sizeof(*K);K->MaxConnecting=4;K->RetryMin=100;K->RetryMax=10000;return xredisSentinelConfigInit(&K->Discovery);
}
XREDIS_API xredisadmission xredisSentinelPoolCreate(xnetengine* E,uint64 A,size_t N,const xredispoolconfig* C,const xredissentinelpoolconfig* S,xredispool** P)
{return __xredisSentinelPoolCreate(E,A,N,C,S,P);}
XREDIS_API bool xredisSentinelPoolInfo(xredispool* P,xredissentinelpoolinfo* I)
{return __xredisSentinelPoolInfo(P,I);}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/cluster_topology/cluster_topology.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_TOPOLOGY)
#include <string.h>
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_TOPOLOGY)
struct xredisclustertopology {
    xatomic32 References;xredisreply* Reply;xredisclustertopologyinfo Info;
    xredisclusternode* Nodes;xredisclusterrange* Ranges;
    uint16 Slots[XREDIS_CLUSTER_SLOTS];
};
typedef struct xredisclusterbuild {
    xredisreply* Reply;const xredisclusterlimits* Limits;xredisclustertopology* Output;
    size_t Shards,Nodes,Ranges,Assigned;unsigned char Seen[XREDIS_CLUSTER_SLOTS/8];
} xredisclusterbuild;
static bool xredisClusterArgument(cstr Text)
{xrtSetErrorInfo(XERR_ARGUMENT,"xrt.redis",XREDIS_REPLY_ARGUMENT,Text);return false;}
static bool xredisClusterString(xredisreplyvalue V)
{return (V.Type==XREDIS_REPLY_SIMPLE_STRING || V.Type==XREDIS_REPLY_BULK_STRING) && !(V.Flags&XREDIS_REPLY_NULL_VALUE);}
static bool xredisClusterNull(xredisreplyvalue V)
{return V.Type==XREDIS_REPLY_NULL || (V.Type==XREDIS_REPLY_BULK_STRING && (V.Flags&XREDIS_REPLY_NULL_VALUE));}
static bool xredisClusterArray(xredisreplyvalue V)
{return V.Type==XREDIS_REPLY_ARRAY && !(V.Flags&XREDIS_REPLY_NULL_VALUE);}
static bool xredisClusterWord(xbytesview V,cstr Word)
{size_t N=strlen(Word);return V.Size==N && (!N || !memcmp(V.Data,Word,N));}
static xredisreplyvalue xredisClusterGet(const xredisreply* R,size_t Index)
{xredisreplyvalue V={0};xredisReplyGet(R,Index,&V);return V;}
static bool xredisClusterHostText(xbytesview Host)
{
    for(size_t N=0;N<Host.Size;++N){unsigned C=Host.Data[N];if(C<=32 || C==127 || C=='/' || C=='\\' || C=='@' || C=='#')return false;}
    return true;
}
static xredisclusterendpoint xredisClusterEndpoint(xbytesview Host)
{return !Host.Size?XREDIS_CLUSTER_ENDPOINT_SOURCE:xredisClusterWord(Host,"?")?XREDIS_CLUSTER_ENDPOINT_UNKNOWN:XREDIS_CLUSTER_ENDPOINT_HOST;}
XREDIS_API bool xredisClusterKeySlot(xbytesview Key,uint16* Slot)
{
    if(!Slot || !xrtMemRangeValid(Slot,sizeof(*Slot)) || !xrtMemRangeValid(Key.Data,Key.Size) ||
        xrtMemRangesOverlap(Key.Data,Key.Size,Slot,sizeof(*Slot)))return xredisClusterArgument("invalid Cluster key/slot output");
    *Slot=xredisClusterSlotRaw(Key);return true;
}
static bool xredisClusterDecimal(xbytesview V,uint32 Max,uint16* Output)
{
    if(!V.Size || V.Size>5)return false;
    uint32 N=0;for(size_t I=0;I<V.Size;++I){unsigned C=V.Data[I];if(C<'0' || C>'9')return false;N=N*10+C-'0';}
    if(N>Max)return false;
    *Output=(uint16)N;return true;
}
XREDIS_API xredisclusterstatus xredisClusterRedirectRead(const xredisreply* R,size_t MaxHost,xredisclusterredirect* Output)
{
    if(!__xredisClusterReplyOutput(R,Output,sizeof(*Output))){xredisClusterArgument("invalid Cluster redirection output");return XREDIS_CLUSTER_ERROR;}
    xredisreplyinfo I={0};I.Size=sizeof(I);xredisReplyInfo(R,&I);xredisreplyvalue V=xredisClusterGet(R,I.Root);
    if(V.Type!=XREDIS_REPLY_SIMPLE_ERROR && V.Type!=XREDIS_REPLY_BULK_ERROR)return XREDIS_CLUSTER_ABSENT;
    xbytesview T=V.Data;size_t Prefix=0;xredisclusterredirect E={0};
    if(T.Size>=5 && !memcmp(T.Data,"MOVED",5) && (T.Size==5 || T.Data[5]==' ')){Prefix=5;E.Kind=XREDIS_CLUSTER_MOVED;}
    else if(T.Size>=3 && !memcmp(T.Data,"ASK",3) && (T.Size==3 || T.Data[3]==' ')){Prefix=3;E.Kind=XREDIS_CLUSTER_ASK;}
    else return XREDIS_CLUSTER_ABSENT;
    if(T.Size<=Prefix+1)return XREDIS_CLUSTER_MALFORMED;
    size_t Start=Prefix+1,End=Start;while(End<T.Size && End-Start<6 && T.Data[End]!=' ')++End;
    if(End==T.Size || T.Data[End]!=' ' || !xredisClusterDecimal((xbytesview){T.Data+Start,End-Start},16383,&E.Slot))return XREDIS_CLUSTER_MALFORMED;
    Start=End+1;if(Start==T.Size)return XREDIS_CLUSTER_MALFORMED;
    End=T.Size;size_t Digits=0;while(End>Start && T.Data[End-1]!=':' && Digits<6){--End;++Digits;}
    if(End==Start || T.Data[End-1]!=':' || !xredisClusterDecimal((xbytesview){T.Data+End,Digits},65535,&E.Port) || !E.Port)return XREDIS_CLUSTER_MALFORMED;
    E.Host=(xbytesview){T.Data+Start,End-1-Start};
    if(E.Host.Size>MaxHost)return XREDIS_CLUSTER_LIMIT;
    if(E.Host.Size && E.Host.Data[0]=='['){if(E.Host.Size<3 || E.Host.Data[E.Host.Size-1]!=']')return XREDIS_CLUSTER_MALFORMED;++E.Host.Data;E.Host.Size-=2;}
    for(size_t N=0;N<E.Host.Size;++N)if(E.Host.Data[N]=='[' || E.Host.Data[N]==']')return XREDIS_CLUSTER_MALFORMED;
    if(!xredisClusterHostText(E.Host))return XREDIS_CLUSTER_MALFORMED;
    E.Endpoint=xredisClusterEndpoint(E.Host);*Output=E;return XREDIS_CLUSTER_VALUE;
}
XREDIS_API bool xredisClusterLimitsInit(xredisclusterlimits* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K)))return xredisClusterArgument("invalid Cluster limits output");
    *K=(xredisclusterlimits){sizeof(*K),4096,4096,16384,262144,4u*1024u*1024u,1024,32u*1024u*1024u,false};return true;
}
/* Fixed known-field lists: each map is scanned once, future fields are skipped.
 * The immutable reader already guarantees complete child/attribute chains. */
static bool xredisClusterFields(const xredisreply* R,xredisreplyvalue V,const cstr* Names,size_t Count,size_t* Fields)
{
    if((V.Type!=XREDIS_REPLY_MAP && !xredisClusterArray(V)) || V.ChildCount%2)return false;
    for(size_t N=0;N<Count;++N)Fields[N]=XREDIS_REPLY_NONE;
    for(size_t I=V.FirstChild;I!=XREDIS_REPLY_NONE;){xredisreplyvalue Key=xredisClusterGet(R,I);if(!xredisClusterString(Key))return false;
        size_t Value=Key.Next;xredisreplyvalue Data=xredisClusterGet(R,Value);
        for(size_t N=0;N<Count;++N)if(xredisClusterWord(Key.Data,Names[N])){if(Fields[N]!=XREDIS_REPLY_NONE)return false;Fields[N]=Value;break;}
        I=Data.Next;
    }
    return true;
}
static xredisclusterstatus xredisClusterHost(xredisreplyvalue V,size_t Max,xbytesview* Host)
{
    if(xredisClusterNull(V)){*Host=(xbytesview){0};return XREDIS_CLUSTER_VALUE;}
    if(!xredisClusterString(V))return XREDIS_CLUSTER_MALFORMED;
    if(V.Data.Size>Max)return XREDIS_CLUSTER_LIMIT;
    if(!xredisClusterHostText(V.Data))return XREDIS_CLUSTER_MALFORMED;
    *Host=V.Data;return XREDIS_CLUSTER_VALUE;
}
static bool xredisClusterPort(xredisreplyvalue V,uint16* Port)
{if(V.Type!=XREDIS_REPLY_INTEGER || V.Integer<0 || V.Integer>65535)return false;*Port=(uint16)V.Integer;return true;}
static xredisclusterstatus xredisClusterReadNode(xredisclusterbuild* B,size_t Index,bool Legacy,bool Master,xredisclusternode* N)
{
    const xredisreply* R=B->Reply;xredisreplyvalue V=xredisClusterGet(R,Index);*N=(xredisclusternode){0};N->Shard=B->Shards;N->ReplicationOffset=-1;
    xredisclusterstatus Status;
    if(Legacy){
        if(!xredisClusterArray(V) || V.ChildCount<2)return XREDIS_CLUSTER_MALFORMED;
        xredisreplyvalue Host=xredisClusterGet(R,V.FirstChild),Port=xredisClusterGet(R,Host.Next);
        Status=xredisClusterHost(Host,B->Limits->MaxEndpointBytes,&N->Host);if(Status!=XREDIS_CLUSTER_VALUE)return Status;
        if(!xredisClusterPort(Port,&N->Port))return XREDIS_CLUSTER_MALFORMED;
        if(V.ChildCount>=3){xredisreplyvalue Id=xredisClusterGet(R,Port.Next);if(!xredisClusterString(Id) && !xredisClusterNull(Id))return XREDIS_CLUSTER_MALFORMED;N->Id=Id.Data;
            if(V.ChildCount>=4){static const cstr Names[]={"ip","hostname"};size_t F[2];if(!xredisClusterFields(R,xredisClusterGet(R,Id.Next),Names,2,F))return XREDIS_CLUSTER_MALFORMED;
                for(unsigned K=0;K<2;++K)if(F[K]!=XREDIS_REPLY_NONE){xbytesview Text;Status=xredisClusterHost(xredisClusterGet(R,F[K]),B->Limits->MaxEndpointBytes,&Text);if(Status!=XREDIS_CLUSTER_VALUE)return Status;if(K)N->Hostname=Text;else N->Ip=Text;}}}
        N->Role=Master?XREDIS_CLUSTER_MASTER:XREDIS_CLUSTER_REPLICA;N->Health=XREDIS_CLUSTER_UNREPORTED;
    }else{
        static const cstr Names[]={"id","endpoint","ip","hostname","port","tls-port","role","replication-offset","health"};size_t F[9];
        if(!xredisClusterFields(R,V,Names,9,F))return XREDIS_CLUSTER_MALFORMED;
        const unsigned Required[]={0,1,2,6,7,8};for(size_t K=0;K<sizeof(Required)/sizeof(*Required);++K)if(F[Required[K]]==XREDIS_REPLY_NONE)return XREDIS_CLUSTER_MALFORMED;
        xredisreplyvalue Id=xredisClusterGet(R,F[0]),Ip=xredisClusterGet(R,F[2]),Role=xredisClusterGet(R,F[6]),Offset=xredisClusterGet(R,F[7]),Health=xredisClusterGet(R,F[8]);
        if(!xredisClusterString(Id) || !Id.Data.Size || !xredisClusterString(Ip) || !xredisClusterString(Role) || !xredisClusterString(Health) || Offset.Type!=XREDIS_REPLY_INTEGER || Offset.Integer< -1)return XREDIS_CLUSTER_MALFORMED;
        N->Id=Id.Data;N->RoleText=Role.Data;N->HealthText=Health.Data;N->ReplicationOffset=Offset.Integer;
        Status=xredisClusterHost(xredisClusterGet(R,F[1]),B->Limits->MaxEndpointBytes,&N->Host);if(Status!=XREDIS_CLUSTER_VALUE)return Status;
        Status=xredisClusterHost(Ip,B->Limits->MaxEndpointBytes,&N->Ip);if(Status!=XREDIS_CLUSTER_VALUE)return Status;
        if(F[3]!=XREDIS_REPLY_NONE){Status=xredisClusterHost(xredisClusterGet(R,F[3]),B->Limits->MaxEndpointBytes,&N->Hostname);if(Status!=XREDIS_CLUSTER_VALUE)return Status;}
        if(F[4]==XREDIS_REPLY_NONE && F[5]==XREDIS_REPLY_NONE)return XREDIS_CLUSTER_MALFORMED;
        if(F[4]!=XREDIS_REPLY_NONE && !xredisClusterPort(xredisClusterGet(R,F[4]),&N->Port))return XREDIS_CLUSTER_MALFORMED;
        if(F[5]!=XREDIS_REPLY_NONE && !xredisClusterPort(xredisClusterGet(R,F[5]),&N->TlsPort))return XREDIS_CLUSTER_MALFORMED;
        N->Role=xredisClusterWord(N->RoleText,"master")?XREDIS_CLUSTER_MASTER:xredisClusterWord(N->RoleText,"replica")?XREDIS_CLUSTER_REPLICA:XREDIS_CLUSTER_ROLE_OTHER;
        N->Health=xredisClusterWord(N->HealthText,"online")?XREDIS_CLUSTER_ONLINE:xredisClusterWord(N->HealthText,"failed")?XREDIS_CLUSTER_FAILED:xredisClusterWord(N->HealthText,"loading")?XREDIS_CLUSTER_LOADING:XREDIS_CLUSTER_HEALTH_OTHER;
    }
    N->Endpoint=xredisClusterEndpoint(N->Host);return XREDIS_CLUSTER_VALUE;
}
static xredisclusterstatus xredisClusterBuildRange(xredisclusterbuild* B,xredisreplyvalue First,xredisreplyvalue Last,size_t Master)
{
    if(First.Type!=XREDIS_REPLY_INTEGER || Last.Type!=XREDIS_REPLY_INTEGER || First.Integer<0 || Last.Integer<First.Integer || Last.Integer>=XREDIS_CLUSTER_SLOTS)return XREDIS_CLUSTER_MALFORMED;
    if(B->Ranges==B->Limits->MaxRanges)return XREDIS_CLUSTER_LIMIT;
    if(B->Output)B->Output->Ranges[B->Ranges]=(xredisclusterrange){(uint16)First.Integer,(uint16)Last.Integer,Master,B->Shards};
    ++B->Ranges;
    for(size_t Slot=(size_t)First.Integer;Slot<=(size_t)Last.Integer;++Slot){unsigned char Bit=(unsigned char)(1u<<(Slot%8));
        if(B->Seen[Slot/8]&Bit)return XREDIS_CLUSTER_MALFORMED;
        B->Seen[Slot/8]|=Bit;++B->Assigned;if(B->Output)B->Output->Slots[Slot]=(uint16)(Master+1);}
    return XREDIS_CLUSTER_VALUE;
}
static xredisclusterstatus xredisClusterBuild(xredisclusterbuild* B,xredisreplyvalue Root,bool Legacy)
{
    if(!xredisClusterArray(Root))return XREDIS_CLUSTER_MALFORMED;
    for(size_t Row=Root.FirstChild;Row!=XREDIS_REPLY_NONE;){
        if(B->Shards==B->Limits->MaxShards)return XREDIS_CLUSTER_LIMIT;
        xredisreplyvalue Item=xredisClusterGet(B->Reply,Row),Ranges={0},Nodes={0};size_t Node,Count;size_t Master=SIZE_MAX;
        if(Legacy){if(!xredisClusterArray(Item) || Item.ChildCount<3)return XREDIS_CLUSTER_MALFORMED;
            xredisreplyvalue First=xredisClusterGet(B->Reply,Item.FirstChild),Last=xredisClusterGet(B->Reply,First.Next);Node=Last.Next;Count=Item.ChildCount-2;
        }else{static const cstr Names[]={"slots","nodes"};size_t F[2];
            if(!xredisClusterFields(B->Reply,Item,Names,2,F) || F[0]==XREDIS_REPLY_NONE || F[1]==XREDIS_REPLY_NONE)return XREDIS_CLUSTER_MALFORMED;
            Ranges=xredisClusterGet(B->Reply,F[0]);Nodes=xredisClusterGet(B->Reply,F[1]);
            if(!xredisClusterArray(Ranges) || Ranges.ChildCount%2 || !xredisClusterArray(Nodes) || !Nodes.ChildCount)return XREDIS_CLUSTER_MALFORMED;
            Node=Nodes.FirstChild;Count=Nodes.ChildCount;
        }
        if(Count>B->Limits->MaxNodes-B->Nodes)return XREDIS_CLUSTER_LIMIT;
        for(size_t K=0;K<Count;++K){xredisclusternode N;xredisclusterstatus S=xredisClusterReadNode(B,Node,Legacy,K==0,&N);if(S!=XREDIS_CLUSTER_VALUE)return S;
            if(N.Role==XREDIS_CLUSTER_MASTER){if(Master!=SIZE_MAX)return XREDIS_CLUSTER_MALFORMED;Master=B->Nodes;}
            if(B->Output)B->Output->Nodes[B->Nodes]=N;
            ++B->Nodes;Node=xredisClusterGet(B->Reply,Node).Next;
        }
        if(Master==SIZE_MAX)return XREDIS_CLUSTER_MALFORMED;
        if(Legacy){xredisreplyvalue First=xredisClusterGet(B->Reply,Item.FirstChild);xredisclusterstatus S=xredisClusterBuildRange(B,First,xredisClusterGet(B->Reply,First.Next),Master);if(S!=XREDIS_CLUSTER_VALUE)return S;}
        else for(size_t Pair=Ranges.FirstChild;Pair!=XREDIS_REPLY_NONE;){xredisreplyvalue First=xredisClusterGet(B->Reply,Pair),Last=xredisClusterGet(B->Reply,First.Next);xredisclusterstatus S=xredisClusterBuildRange(B,First,Last,Master);if(S!=XREDIS_CLUSTER_VALUE)return S;Pair=Last.Next;}
        ++B->Shards;Row=Item.Next;
    }
    if(B->Limits->RequireFullCoverage && B->Assigned!=XREDIS_CLUSTER_SLOTS)return XREDIS_CLUSTER_MALFORMED;
    return XREDIS_CLUSTER_VALUE;
}
XREDIS_API xredisclusterstatus xredisClusterTopologyCreate(xredisreply* R,xredisclusterformat Format,const xredisclusterlimits* Config,xredisclustertopology** Output)
{
    if(!__xredisClusterReplyOutput(R,Output,sizeof(*Output)) || (Format!=XREDIS_CLUSTER_SHARDS && Format!=XREDIS_CLUSTER_LEGACY_SLOTS) ||
        (Config && (!__xredisClusterReplyOutput(R,Config,sizeof(*Config)) || xrtMemRangesOverlap(Config,sizeof(*Config),Output,sizeof(*Output)) || Config->Size!=sizeof(*Config)))){
        xredisClusterArgument("invalid Cluster topology arguments");return XREDIS_CLUSTER_ERROR;}
    xredisclusterlimits K;if(Config)K=*Config;else xredisClusterLimitsInit(&K);
    if(K.MaxNodes>65535 || K.MaxShards>XREDIS_CLUSTER_SLOTS || K.MaxRanges>XREDIS_CLUSTER_SLOTS){xredisClusterArgument("invalid Cluster topology bounds");return XREDIS_CLUSTER_ERROR;}
    xredisreplyinfo I={0};I.Size=sizeof(I);xredisReplyInfo(R,&I);
    if(I.Nodes>K.MaxReplyNodes || I.PayloadBytes>K.MaxPayloadBytes)return XREDIS_CLUSTER_LIMIT;
    xredisreplyvalue Root=xredisClusterGet(R,I.Root);
    if(Root.Type==XREDIS_REPLY_SIMPLE_ERROR || Root.Type==XREDIS_REPLY_BULK_ERROR)return XREDIS_CLUSTER_SERVER_ERROR;
    xredisclusterbuild B={0};B.Reply=R;B.Limits=&K;
    xredisclusterstatus S=xredisClusterBuild(&B,Root,Format==XREDIS_CLUSTER_LEGACY_SLOTS);if(S!=XREDIS_CLUSTER_VALUE)return S;
    size_t Bytes=sizeof(xredisclustertopology)+B.Nodes*sizeof(xredisclusternode)+B.Ranges*sizeof(xredisclusterrange);
    if(Bytes>K.MaxOwnedBytes || I.ReservedBytes>K.MaxOwnedBytes-Bytes)return XREDIS_CLUSTER_LIMIT;
    xredisclustertopology* T=xrtCalloc(1,Bytes);if(!T)return XREDIS_CLUSTER_MEMORY;
    T->Nodes=(xredisclusternode*)(T+1);T->Ranges=(xredisclusterrange*)(T->Nodes+B.Nodes);
    T->Info=(xredisclustertopologyinfo){sizeof(T->Info),B.Shards,B.Nodes,B.Ranges,B.Assigned,Bytes,I.ReservedBytes,Format};
    xredisclusterbuild Fill={0};Fill.Reply=R;Fill.Limits=&K;Fill.Output=T;
    S=xredisClusterBuild(&Fill,Root,Format==XREDIS_CLUSTER_LEGACY_SLOTS);
    if(S!=XREDIS_CLUSTER_VALUE){xrtFree(T);return S;}
    if(!xredisReplyRetain(R)){xrtFree(T);return XREDIS_CLUSTER_ERROR;}
    T->Reply=R;xrtAtomic32Init(&T->References,1);*Output=T;return XREDIS_CLUSTER_VALUE;
}
XREDIS_API bool xredisClusterTopologyRetain(xredisclustertopology* T)
{
    if(!T)return xredisClusterArgument("Cluster topology is NULL");
    uint32 N=xrtAtomic32Load(&T->References,XMEMORY_RELAXED);
    for(;;){if(N==UINT32_MAX){xrtSetErrorInfo(XERR_RANGE,"xrt.redis",XREDIS_REPLY_LIMIT,"Cluster topology reference overflow");return false;}
        if(xrtAtomic32CompareExchange(&T->References,&N,N+1,XMEMORY_RELAXED,XMEMORY_RELAXED))return true;}
}
XREDIS_API void xredisClusterTopologyRelease(xredisclustertopology* T)
{if(T && xrtAtomic32FetchSub(&T->References,1,XMEMORY_ACQ_REL)==1){xredisReplyRelease(T->Reply);xrtFree(T);}}
static bool xredisClusterOutput(const xredisclustertopology* T,const void* Output,size_t Bytes)
{
    if(!T || !__xredisClusterReplyOutput(T->Reply,Output,Bytes) || xrtMemRangesOverlap(T,T->Info.StorageBytes,Output,Bytes))return xredisClusterArgument("invalid Cluster topology output");
    return true;
}
bool __xredisClusterTopologyOutput(const xredisclustertopology* T,const void* Output,size_t Bytes)
{return xredisClusterOutput(T,Output,Bytes);}
XREDIS_API bool xredisClusterTopologyInfo(const xredisclustertopology* T,xredisclustertopologyinfo* I)
{if(!xredisClusterOutput(T,I,sizeof(*I)) || I->Size!=sizeof(*I))return xredisClusterArgument("invalid Cluster topology info size");*I=T->Info;return true;}
XREDIS_API xredisclusterstatus xredisClusterTopologyLookup(const xredisclustertopology* T,uint16 Slot,xredisclusternode* N)
{
    if(!xredisClusterOutput(T,N,sizeof(*N)) || Slot>=XREDIS_CLUSTER_SLOTS){xredisClusterArgument("invalid Cluster slot lookup");return XREDIS_CLUSTER_ERROR;}
    uint16 Index=T->Slots[Slot];if(!Index)return XREDIS_CLUSTER_ABSENT;
    *N=T->Nodes[Index-1];return XREDIS_CLUSTER_VALUE;
}
XREDIS_API bool xredisClusterTopologyNode(const xredisclustertopology* T,size_t Index,xredisclusternode* N)
{if(!xredisClusterOutput(T,N,sizeof(*N)) || Index>=T->Info.Nodes)return xredisClusterArgument("invalid Cluster node index");*N=T->Nodes[Index];return true;}
XREDIS_API bool xredisClusterTopologyRange(const xredisclustertopology* T,size_t Index,xredisclusterrange* N)
{if(!xredisClusterOutput(T,N,sizeof(*N)) || Index>=T->Info.Ranges)return xredisClusterArgument("invalid Cluster range index");*N=T->Ranges[Index];return true;}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/pool_asking/pool_asking.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
#if defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
XREDIS_API xredisadmission xredisLeaseAskingCallAsync(xredislease* L,const xbytesview* A,size_t N,int64 T,xfuture** F)
{return __xredisLeaseAsking(L,A,N,T,F,xrtTimer(),false);}
XREDIS_API xredisadmission xredisLeaseAskingCall(xredislease* L,const xbytesview* A,size_t N,int64 T,xfuture** F)
{return __xredisLeaseAsking(L,A,N,T,F,xrtTimer(),true);}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/pool_shared/pool_shared.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED)
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED)
XREDIS_API xredisadmission xredisSharedPoolCreate(xnetengine* Engine, uint64 Affinity,
    xredisclient* const* Clients, size_t Count, const xredispoolconfig* Config, xredispool** Pool)
{ return __xredisSharedPoolCreate(Engine, Affinity, Clients, Count, Config, Pool); }
XREDIS_API xredisadmission xredisSharedPoolCallAsync(xredispool* Pool,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future)
{ return __xredisSharedPoolCall(Pool, Arguments, Count, Timeout, Future, xrtTimer(), false); }
XREDIS_API xredisadmission xredisSharedPoolCall(xredispool* Pool,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future)
{ return __xredisSharedPoolCall(Pool, Arguments, Count, Timeout, Future, xrtTimer(), true); }
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/pool_shared_connect/pool_shared_connect.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_CONNECT)
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_CONNECT)
XREDIS_API xredisadmission xredisSharedPoolConnect(xnetengine* Engine, uint64 Affinity,
    size_t Count, const xredispoolconfig* Config,
    const xredispoolconnectconfig* Connect, xredispool** Result)
{ return __xredisSharedPoolConnect(Engine, Affinity, Count, Config, Connect, Result); }
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/pool_shared_asking/pool_shared_asking.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_ASKING)
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_ASKING)
XREDIS_API xredisadmission xredisSharedPoolAskingCallAsync(xredispool* Pool,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future)
{ return __xredisSharedPoolAsking(Pool, Arguments, Count, Timeout, Future, xrtTimer(), false); }
XREDIS_API xredisadmission xredisSharedPoolAskingCall(xredispool* Pool,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future)
{ return __xredisSharedPoolAsking(Pool, Arguments, Count, Timeout, Future, xrtTimer(), true); }
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/cluster_route/cluster_route.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_ROUTE)
#include <math.h>
#include <string.h>
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_ROUTE)
enum {XCR_RESOLVE, XCR_NATIVE, XCR_BACKOFF, XCR_DONE};
struct xredisclusterroute {
    xredisclusterrouteconfig Config;
    xredisclusterrouteresult Result;
    xfuture* Future; xbytesview* Arguments; unsigned char *Host, *AttemptHost, *HostHeap;
    size_t Count, StorageBytes, HostCapacity; double Entered, Deadline, Due;
    unsigned Phase; bool NativeAsking;
};
static bool xredisRouteError(xerrkind Kind, cstr Text)
{xrtSetErrorInfo(Kind,"xrt.redis",Kind==XERR_STATE?XREDIS_CLIENT_STATE:XREDIS_CLIENT_ARGUMENT,Text);return false;}
static double xredisRouteDue(double Entered,int64 Timeout)
{return Timeout==0?Entered:nextafter(Entered+(double)(Timeout/1000)+(double)(Timeout%1000)*.001,INFINITY);}
static bool xredisRouteHost(xbytesview H)
{
    if(!H.Size || !xrtMemRangeValid(H.Data,H.Size) || (H.Size==1 && H.Data[0]=='?'))return false;
    for(size_t I=0;I<H.Size;++I){unsigned C=H.Data[I];if(C<=32 || C==127 || C=='/' || C=='\\' || C=='@' || C=='#')return false;}
    return true;
}
static bool xredisRouteObserve(xredisclusterroute* R)
{
    if(!R->Future || xrtFutureState(R->Future)!=XFUTURE_RESOLVED)return false;
    const xredisrequestresult* Q=xrtFutureValue(R->Future);if(!Q)return false;
    R->Result.Query=*Q;R->Result.RequestObserved=true;R->Result.UsedAsking=R->NativeAsking;
    if(R->NativeAsking){const xredisaskingresult* A=(const xredisaskingresult*)Q;R->Result.ControlReceived=A->ControlReceived;R->Result.Control=A->Control;}
    return true;
}
static void xredisRouteFinish(xredisclusterroute* R,xredisclusterroutestatus Status)
{
    if(R->Phase==XCR_DONE)return;
    R->Result.Status=Status;
    if(R->Future && xrtFutureState(R->Future)==XFUTURE_PENDING)xrtFutureCancel(R->Future);
    (void)xredisRouteObserve(R);R->Phase=XCR_DONE;R->Due=0;R->Config.GetPool=NULL;R->Config.Data=NULL;
}
static bool xredisRouteOutput(xredisclusterroute* R,const void* P,size_t Bytes)
{
    if(!R || !xrtMemRangeValid(P,Bytes) || xrtMemRangesOverlap(R,R?R->StorageBytes:0,P,Bytes) ||
        (R->HostHeap && xrtMemRangesOverlap(R->HostHeap,2*R->HostCapacity,P,Bytes)))return false;
    if(R->Future && xrtFutureState(R->Future)==XFUTURE_RESOLVED){
        const xredisrequestresult* Q=xrtFutureValue(R->Future);
        if(Q && (xrtMemRangesOverlap(Q,R->NativeAsking?sizeof(xredisaskingresult):sizeof(*Q),P,Bytes) ||
            (Q->Reply && !__xredisClusterReplyOutput(Q->Reply,P,Bytes))))return false;
        if(Q && R->NativeAsking){const xredisaskingresult* A=(const xredisaskingresult*)Q;
            if(A->Control.Reply && !__xredisClusterReplyOutput(A->Control.Reply,P,Bytes))return false;}
    }
    return true;
}
XREDIS_API bool xredisClusterRouteConfigInit(xredisclusterrouteconfig* C)
{
    if(!C || !xrtMemRangeValid(C,sizeof(*C)))return xredisRouteError(XERR_ARGUMENT,"invalid Cluster route config output");
    *C=(xredisclusterrouteconfig){sizeof(*C),8,1024,1024,1024u*1024u,1024,2u*1024u*1024u,5000,5,false,NULL,NULL};return true;
}
bool __xredisClusterRouteConfig(const xredisclusterrouteconfig* C)
{
    return C->Size==sizeof(*C) && C->GetPool && C->MaxRedirects<=1024 && C->MaxWaits<=65536 && C->MaxArguments && C->MaxArguments<=65536 &&
        C->MaxCommandBytes && C->MaxCommandBytes<=64u*1024u*1024u && C->MaxHostBytes && C->MaxHostBytes<=65536 &&
        C->MaxStorageBytes && C->MaxStorageBytes<=128u*1024u*1024u && C->Timeout>=0 && C->Timeout<=86400000 && C->RetryDelay>0 && C->RetryDelay<=60000;
}
size_t __xredisClusterRouteBaseBytes(void){return sizeof(xredisclusterroute);}
static xredisadmission xredisRouteCreate(const xredisclustertopology* T,xbytesview Origin,uint16 Slot,
    const xbytesview* A,size_t Count,const xredisclusterrouteconfig* Config,xredisclusterroute** Out,double Entered,xredisclusterroutestatus* Failure,
    size_t Prefix,ptr* Context)
{
    if(Failure)*Failure=XREDIS_CLUSTER_ROUTE_MEMORY;
    xredisclusterrouteconfig C;
    if(!T || !Out || !__xredisClusterTopologyOutput(T,Out,sizeof(*Out)) || Slot>=XREDIS_CLUSTER_SLOTS ||
        !Config || !xrtMemRangeValid(Config,sizeof(*Config)) || xrtMemRangesOverlap(Config,sizeof(*Config),Out,sizeof(*Out))){
        xredisRouteError(XERR_ARGUMENT,"invalid Cluster route topology/config/output");return XREDIS_ADMISSION_ERROR;}
    C=*Config;
    if(!__xredisClusterRouteConfig(&C) || !Count || Count>C.MaxArguments || !xrtMemRangeValid(A,Count*sizeof(*A)) ||
        xrtMemRangesOverlap(A,Count*sizeof(*A),Out,sizeof(*Out)) || !xrtMemRangeValid(Origin.Data,Origin.Size) ||
        xrtMemRangesOverlap(Origin.Data,Origin.Size,Out,sizeof(*Out))){xredisRouteError(XERR_ARGUMENT,"invalid Cluster route bounds/input");return XREDIS_ADMISSION_ERROR;}
    size_t Wire=0,Payload=0;if(!xredisCommandSize(A,Count,&Wire) || Wire>C.MaxCommandBytes){xredisRouteError(XERR_RANGE,"Cluster route command limit");return XREDIS_ADMISSION_ERROR;}
    for(size_t I=0;I<Count;++I){
        if(xrtMemRangesOverlap(A[I].Data,A[I].Size,Out,sizeof(*Out)) || A[I].Size>C.MaxCommandBytes-Payload){xredisRouteError(XERR_ARGUMENT,"Cluster route payload/output overlap");return XREDIS_ADMISSION_ERROR;}
        Payload+=A[I].Size;
    }
    xredisclusternode N={0};xredisclusterstatus Found=xredisClusterTopologyLookup(T,Slot,&N);
    if(Found<0)return XREDIS_ADMISSION_ERROR;
    xredisclustertopologyinfo TI={0};TI.Size=sizeof(TI);if(!xredisClusterTopologyInfo(T,&TI))return XREDIS_ADMISSION_ERROR;
    xbytesview Host=N.Endpoint==XREDIS_CLUSTER_ENDPOINT_SOURCE?Origin:N.Host;
    uint16 Port=TI.Format==XREDIS_CLUSTER_LEGACY_SLOTS?N.Port:C.Tls?N.TlsPort:N.Port;
    bool Eligible=Found==XREDIS_CLUSTER_VALUE && N.Role==XREDIS_CLUSTER_MASTER && (N.Health==XREDIS_CLUSTER_ONLINE || N.Health==XREDIS_CLUSTER_UNREPORTED) &&
        N.Endpoint!=XREDIS_CLUSTER_ENDPOINT_UNKNOWN && Port && Host.Size<=C.MaxHostBytes && xredisRouteHost(Host);
    size_t HostCapacity=Eligible?Host.Size:1;
    size_t Bytes=sizeof(xredisclusterroute)+Count*sizeof(*A)+Payload+2*HostCapacity;
    if(Bytes>C.MaxStorageBytes){if(Failure)*Failure=XREDIS_CLUSTER_ROUTE_LIMIT;xredisRouteError(XERR_RANGE,"Cluster route storage limit");return XREDIS_ADMISSION_ERROR;}
    unsigned char* Block=xrtCalloc(1,Prefix+Bytes);if(!Block)return XREDIS_ADMISSION_ERROR;
    xredisclusterroute* R=(xredisclusterroute*)(Block+Prefix);if(Prefix)C.Data=Block;
    R->Config=C;R->Entered=Entered;R->Deadline=xredisRouteDue(Entered,C.Timeout);R->StorageBytes=Bytes;R->Count=Count;R->Arguments=(xbytesview*)(R+1);
    unsigned char* P=(unsigned char*)(R->Arguments+Count);
    for(size_t I=0;I<Count;++I){R->Arguments[I]=(xbytesview){P,A[I].Size};if(A[I].Size)memcpy(P,A[I].Data,A[I].Size);P+=A[I].Size;}
    R->Host=P;R->HostCapacity=HostCapacity;R->AttemptHost=P+HostCapacity;R->Result.Size=sizeof(R->Result);R->Result.StorageBytes=Bytes;R->Result.Address.Slot=Slot;R->Result.Address.Tls=C.Tls;R->Result.Admission=XREDIS_ADMISSION_ACCEPTED;
    if(!Eligible)xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_NO_ROUTE);
    else {memcpy(R->Host,Host.Data,Host.Size);R->Result.Address.Host=(xbytesview){R->Host,Host.Size};R->Result.Address.Port=Port;R->Due=R->Deadline;}
    if(R->Phase!=XCR_DONE && xrtTimer()>=R->Deadline)xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_TIMEOUT);
    if(Context)*Context=Block;
    *Out=R;return XREDIS_ADMISSION_ACCEPTED;
}
xredisadmission __xredisClusterRouteCreate(const xredisclustertopology* T,xbytesview Origin,uint16 Slot,
    const xbytesview* A,size_t Count,const xredisclusterrouteconfig* C,xredisclusterroute** Out,double Entered,xredisclusterroutestatus* Failure)
{return xredisRouteCreate(T,Origin,Slot,A,Count,C,Out,Entered,Failure,0,NULL);}
xredisadmission __xredisClusterRouteCreateContext(const xredisclustertopology* T,xbytesview Origin,uint16 Slot,
    const xbytesview* A,size_t Count,const xredisclusterrouteconfig* C,size_t Prefix,ptr* Context,xredisclusterroute** Out,double Entered)
{
    if(!Prefix || Prefix>128u*1024u*1024u || Prefix%_Alignof(max_align_t) || !Context){xredisRouteError(XERR_ARGUMENT,"invalid Cluster route context prefix");return XREDIS_ADMISSION_ERROR;}
    return xredisRouteCreate(T,Origin,Slot,A,Count,C,Out,Entered,NULL,Prefix,Context);
}
XREDIS_API xredisadmission xredisClusterRouteCreate(const xredisclustertopology* T,xbytesview Origin,uint16 Slot,
    const xbytesview* A,size_t Count,const xredisclusterrouteconfig* Config,xredisclusterroute** Out)
{return __xredisClusterRouteCreate(T,Origin,Slot,A,Count,Config,Out,xrtTimer(),NULL);}
xredisadmission __xredisClusterRouteRetryMissing(xredisclusterroute* R,const xredisclustertopology* T,
    xbytesview Origin,const xredisclusterrouteconfig* C,xredisclusterroute** Out,xredisclusterroutestatus* Failure)
{
    if(!R || R->Result.Status!=XREDIS_CLUSTER_ROUTE_NO_ROUTE || R->Result.Attempts){xredisRouteError(XERR_STATE,"only unsubmitted missing routes can restart");return XREDIS_ADMISSION_ERROR;}
    return __xredisClusterRouteCreate(T,Origin,R->Result.Address.Slot,R->Arguments,R->Count,C,Out,R->Entered,Failure);
}
static void xredisRouteWait(xredisclusterroute* R)
{
    if(R->Result.Waits>=R->Config.MaxWaits){xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_OVERLOADED);return;}
    ++R->Result.Waits;R->Due=fmin(R->Deadline,xredisRouteDue(xrtTimer(),R->Config.RetryDelay));R->Phase=XCR_BACKOFF;
}
static void xredisRouteResolve(xredisclusterroute* R)
{
    double Remaining=(R->Deadline-xrtTimer())*1000.;int64 Timeout=Remaining<=0?0:Remaining>=86400000.?86400000:(int64)ceil(Remaining);
    xredispool* Pool=NULL;++R->Result.ProviderCalls;
    R->Result.Provider=R->Config.GetPool(&R->Result.Address,Timeout,R->Config.Data,&Pool);
    if(xrtTimer()>=R->Deadline){if(R->Result.Provider==XREDIS_CLUSTER_POOL_READY && Pool)xredisPoolRelease(Pool);xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_TIMEOUT);return;}
    if(R->Result.Provider!=XREDIS_CLUSTER_POOL_READY){
        if(R->Result.Provider==XREDIS_CLUSTER_POOL_WAIT)xredisRouteWait(R);
        else xredisRouteFinish(R,R->Result.Provider==XREDIS_CLUSTER_POOL_DENIED?XREDIS_CLUSTER_ROUTE_POLICY:
            R->Result.Provider==XREDIS_CLUSTER_POOL_CLOSED?XREDIS_CLUSTER_ROUTE_CLOSED:
            R->Result.Provider==XREDIS_CLUSTER_POOL_MEMORY?XREDIS_CLUSTER_ROUTE_MEMORY:XREDIS_CLUSTER_ROUTE_PROVIDER_ERROR);
        return;
    }
    if(!Pool){xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_PROVIDER_ERROR);return;}
    bool Asking=R->Result.Address.Reason==XREDIS_CLUSTER_ASK;xfuture* F=NULL;
    R->Result.AdmissionObserved=true;
    R->Result.Admission=Asking?__xredisSharedPoolAsking(Pool,R->Arguments,R->Count,R->Config.Timeout,&F,R->Entered,false):
        __xredisSharedPoolCall(Pool,R->Arguments,R->Count,R->Config.Timeout,&F,R->Entered,false);
    xredisPoolRelease(Pool);
    if(R->Result.Admission==XREDIS_ADMISSION_ACCEPTED){
        xrtFutureDestroy(R->Future);R->Future=F;R->NativeAsking=Asking;R->Result.UsedAsking=Asking;
        R->Result.AttemptAddress=R->Result.Address;
        memcpy(R->AttemptHost,R->Host,R->Result.Address.Host.Size);
        R->Result.AttemptAddress.Host.Data=R->AttemptHost;
        ++R->Result.Attempts;R->Phase=XCR_NATIVE;R->Due=R->Deadline;
        if(xrtTimer()>=R->Deadline)xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_TIMEOUT);
    }else if(xrtTimer()>=R->Deadline)xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_TIMEOUT);
    else if(R->Result.Admission==XREDIS_ADMISSION_OVERLOADED)xredisRouteWait(R);
    else xredisRouteFinish(R,R->Result.Admission==XREDIS_ADMISSION_CLOSED?XREDIS_CLUSTER_ROUTE_CLOSED:XREDIS_CLUSTER_ROUTE_ADMISSION_ERROR);
}
static void xredisRouteReply(xredisclusterroute* R)
{
    if(xrtFutureState(R->Future)!=XFUTURE_RESOLVED || !xrtFutureValue(R->Future)){xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_PROTOCOL);return;}
    const xredisrequestresult* Q=xrtFutureValue(R->Future);
    if(R->NativeAsking){const xredisaskingresult* A=(const xredisaskingresult*)Q;
        if(!A->ControlReceived || A->Control.Status!=XREDIS_REQUEST_OK){xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_COMPLETE);return;}}
    if(Q->Status!=XREDIS_REQUEST_SERVER_ERROR || Q->Delivery!=XREDIS_REPLIED || !Q->Reply){xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_COMPLETE);return;}
    xredisclusterredirect E;xredisclusterstatus S=xredisClusterRedirectRead(Q->Reply,R->Config.MaxHostBytes,&E);
    if(S==XREDIS_CLUSTER_ABSENT){xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_COMPLETE);return;}
    if(S!=XREDIS_CLUSTER_VALUE || E.Slot!=R->Result.Address.Slot){xredisRouteFinish(R,S==XREDIS_CLUSTER_LIMIT?XREDIS_CLUSTER_ROUTE_LIMIT:XREDIS_CLUSTER_ROUTE_PROTOCOL);return;}
    if(E.Kind==XREDIS_CLUSTER_MOVED)++R->Result.Moved;else ++R->Result.Asks;
    if(R->Result.Redirects>=R->Config.MaxRedirects){xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_LIMIT);return;}
    if(E.Endpoint==XREDIS_CLUSTER_ENDPOINT_UNKNOWN || (E.Endpoint==XREDIS_CLUSTER_ENDPOINT_HOST && !xredisRouteHost(E.Host))){xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_POLICY);return;}
    if(E.Endpoint==XREDIS_CLUSTER_ENDPOINT_HOST){
        if(E.Host.Size>R->HostCapacity){
            size_t Bytes=2*R->Config.MaxHostBytes;
            if(Bytes>R->Config.MaxStorageBytes-R->StorageBytes){xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_LIMIT);return;}
            unsigned char* Host=xrtMalloc(Bytes);if(!Host){xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_MEMORY);return;}
            memcpy(Host,R->Host,R->Result.Address.Host.Size);
            memcpy(Host+R->Config.MaxHostBytes,R->AttemptHost,R->Result.AttemptAddress.Host.Size);
            R->HostHeap=Host;R->Host=Host;R->AttemptHost=Host+R->Config.MaxHostBytes;R->HostCapacity=R->Config.MaxHostBytes;
            R->Result.StorageBytes=R->StorageBytes+Bytes;
            R->Result.Address.Host.Data=R->Host;R->Result.AttemptAddress.Host.Data=R->AttemptHost;
        }
        memmove(R->Host,E.Host.Data,E.Host.Size);R->Result.Address.Host.Size=E.Host.Size;
    }
    R->Result.Address.Port=E.Port;R->Result.Address.Reason=E.Kind;++R->Result.Redirects;R->Phase=XCR_RESOLVE;
}
XREDIS_API xredisclusterroutestep xredisClusterRouteStep(xredisclusterroute* R)
{
    if(!R){xredisRouteError(XERR_ARGUMENT,"Cluster route is NULL");return XREDIS_CLUSTER_ROUTE_STEP_ERROR;}
    if(R->Phase==XCR_DONE)return XREDIS_CLUSTER_ROUTE_DONE;
    if(xrtTimer()>=R->Deadline){xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_TIMEOUT);return XREDIS_CLUSTER_ROUTE_DONE;}
    if(R->Phase==XCR_BACKOFF){if(xrtTimer()<R->Due)return XREDIS_CLUSTER_ROUTE_WAIT;R->Phase=XCR_RESOLVE;}
    else if(R->Phase==XCR_NATIVE){if(xrtFutureState(R->Future)==XFUTURE_PENDING)return XREDIS_CLUSTER_ROUTE_WAIT;xredisRouteReply(R);}
    else xredisRouteResolve(R);
    return R->Phase==XCR_DONE?XREDIS_CLUSTER_ROUTE_DONE:R->Phase==XCR_BACKOFF ||
        (R->Phase==XCR_NATIVE && xrtFutureState(R->Future)==XFUTURE_PENDING)?XREDIS_CLUSTER_ROUTE_WAIT:XREDIS_CLUSTER_ROUTE_MORE;
}
XREDIS_API xfuture* xredisClusterRoutePending(xredisclusterroute* R)
{if(!R){xredisRouteError(XERR_ARGUMENT,"Cluster route is NULL");return NULL;}return R->Phase==XCR_NATIVE?xrtFutureRef(R->Future):NULL;}
XREDIS_API double xredisClusterRouteNextWake(xredisclusterroute* R)
{if(!R){xredisRouteError(XERR_ARGUMENT,"Cluster route is NULL");return 0;}return R->Phase==XCR_DONE?0:(R->Phase==XCR_RESOLVE || (R->Phase==XCR_NATIVE && xrtFutureState(R->Future)!=XFUTURE_PENDING))?fmin(R->Deadline,xrtTimer()):R->Phase==XCR_BACKOFF?R->Due:R->Deadline;}
XREDIS_API bool xredisClusterRouteResult(xredisclusterroute* R,xredisclusterrouteresult* Out)
{if(!xredisRouteOutput(R,Out,sizeof(*Out)) || Out->Size!=sizeof(*Out))return xredisRouteError(XERR_ARGUMENT,"invalid Cluster route result output");*Out=R->Result;return true;}
XREDIS_API xfuture* xredisClusterRouteResultFuture(xredisclusterroute* R)
{if(!R){xredisRouteError(XERR_ARGUMENT,"Cluster route is NULL");return NULL;}return R->Phase==XCR_DONE && R->Future?xrtFutureRef(R->Future):NULL;}
XREDIS_API bool xredisClusterRouteCancel(xredisclusterroute* R)
{if(!R)return xredisRouteError(XERR_ARGUMENT,"Cluster route is NULL");xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_CANCELLED);return true;}
XREDIS_API void xredisClusterRouteDestroy(xredisclusterroute* R)
{if(!R)return;__xredisClusterRouteUnit(R);xrtFree(R);}
void __xredisClusterRouteUnit(xredisclusterroute* R)
{if(!R)return;if(R->Phase!=XCR_DONE)xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_CANCELLED);xrtFutureDestroy(R->Future);xrtFree(R->HostHeap);}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/cluster_nodes/cluster_nodes.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_NODES)
#include <math.h>
#include <stdlib.h>
#include <string.h>
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_NODES)
typedef struct xredisclusternodepool xredisclusternodepool;
struct xredisclusternodepool {
    xredisclusternodes* Cache;xredisclusternodepool *Next,*FreeNext;
    xredisclusteraddress Address;uint64 Hash;
    xredispool* Pool;xfuture* Closed;xfuturewatch Watch;
    bool Opening,Retiring,ObservedClosed;
};
struct xredisclusternodes {
    xatomic32 References,Admission;xmutex Gate;xnetengine* Engine;uint64 Affinity;
    xredisclusternodesconfig Config;xredisclusternodesinfo Info;
    xpromise* Promise;xfuture* Closed;
    xredisclusternodepool **Buckets,*Slots,*Free;
    size_t BucketCount,StorageBytes;bool Finishing,Scanning;
};
static bool xredisNodesError(xerrkind K,cstr S)
{xrtSetErrorInfo(K,"xrt.redis",K==XERR_STATE?XREDIS_CLIENT_STATE:XREDIS_CLIENT_ARGUMENT,S);return false;}
static void xredisNodesRef(xredisclusternodes* C){xrtAtomic32FetchAdd(&C->References,1,XMEMORY_RELAXED);}
XREDIS_API bool xredisClusterNodesRetain(xredisclusternodes* C)
{if(!C)return xredisNodesError(XERR_ARGUMENT,"Cluster node cache is NULL");xredisNodesRef(C);return true;}
XREDIS_API void xredisClusterNodesRelease(xredisclusternodes* C)
{if(!C || xrtAtomic32FetchSub(&C->References,1,XMEMORY_ACQ_REL)!=1)return;xrtPromiseDestroy(C->Promise);xrtFutureDestroy(C->Closed);xrtMutexUnit(&C->Gate);xrtFree(C);}
static bool xredisNodesOutput(xredisclusternodes* C,const void* P,size_t N)
{return C && xrtMemRangeValid(P,N) && !xrtMemRangesOverlap(C,C->StorageBytes,P,N);}
static bool xredisNodesAddress(const xredisclusteraddress* A,size_t Max)
{
    if(!xrtMemRangeValid(A,sizeof(*A)) || !A->Host.Size || A->Host.Size>Max || !xrtMemRangeValid(A->Host.Data,A->Host.Size) || !A->Port || A->Slot>=XREDIS_CLUSTER_SLOTS ||
        (A->Reason!=0 && A->Reason!=XREDIS_CLUSTER_MOVED && A->Reason!=XREDIS_CLUSTER_ASK) || (A->Host.Size==1 && A->Host.Data[0]=='?'))return false;
    for(size_t I=0;I<A->Host.Size;++I){unsigned X=A->Host.Data[I];if(X<=32 || X==127 || X=='/' || X=='\\' || X=='@' || X=='#')return false;}return true;
}
static uint64 xredisNodesHash(const xredisclusteraddress* A)
{uint64 H=14695981039346656037ull;for(size_t I=0;I<A->Host.Size;++I){H^=A->Host.Data[I];H*=1099511628211ull;}H^=A->Port;H*=1099511628211ull;H^=A->Tls;return H;}
static xredisclusternodepool* xredisNodesFind(xredisclusternodes* C,const xredisclusteraddress* A,uint64 H)
{for(xredisclusternodepool* N=C->Buckets[H&(C->BucketCount-1)];N;N=N->Next)if(N->Hash==H && N->Address.Port==A->Port && N->Address.Tls==A->Tls && N->Address.Host.Size==A->Host.Size && !memcmp(N->Address.Host.Data,A->Host.Data,A->Host.Size))return N;return NULL;}
#define XREDIS_NODES_SEALED 0x80000000u
#define XREDIS_NODES_USERS 0x7fffffffu
/* The seal bit and admitted caller count share one modification order. A
 * successful increment protects policy/data through End; Close atomically
 * prevents every later admission. Saturation/contended retries transfer no
 * lifetime and never call policy. MaxLookups<=65536 leaves the seal bit free. */
static xredisclusterpoolstatus xredisNodesBegin(xredisclusternodes* C)
{
    uint32 S=xrtAtomic32Load(&C->Admission,XMEMORY_ACQUIRE);
    for(unsigned I=0;I<32;++I){
        if(S&XREDIS_NODES_SEALED)return XREDIS_CLUSTER_POOL_CLOSED;
        if((S&XREDIS_NODES_USERS)>=C->Config.MaxLookups)break;
        if(xrtAtomic32CompareExchange(&C->Admission,&S,S+1,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE))return XREDIS_CLUSTER_POOL_READY;
    }
    xrtMutexLock(&C->Gate);xredisclusterpoolstatus Result=C->Info.State==XREDIS_CLUSTER_NODES_OPEN?XREDIS_CLUSTER_POOL_WAIT:XREDIS_CLUSTER_POOL_CLOSED;
    if(Result==XREDIS_CLUSTER_POOL_WAIT)++C->Info.Waits;
    xrtMutexUnlock(&C->Gate);return Result;
}
static void xredisNodesFinish(xredisclusternodes* C)
{
    xrtMutexLock(&C->Gate);
    if(C->Info.State!=XREDIS_CLUSTER_NODES_CLOSING || C->Info.Nodes || (xrtAtomic32Load(&C->Admission,XMEMORY_ACQUIRE)&XREDIS_NODES_USERS) || C->Scanning || C->Finishing){xrtMutexUnlock(&C->Gate);return;}
    C->Finishing=true;xnetengine* E=C->Engine;C->Engine=NULL;xredispoolconnectdrop Drop=C->Config.Drop;ptr Data=C->Config.Data;
    C->Config.Drop=NULL;C->Config.Data=NULL;C->Config.Connect=NULL;C->Config.Authorize=NULL;xpromise* P=C->Promise;C->Promise=NULL;xrtMutexUnlock(&C->Gate);
    if(Drop)Drop(Data);
    xrtNetEngineUnpin(E);
    xrtMutexLock(&C->Gate);C->Info.State=XREDIS_CLUSTER_NODES_CLOSED;xrtMutexUnlock(&C->Gate);
    xrtPromiseResolve(P,NULL);xrtPromiseDestroy(P);xredisClusterNodesRelease(C); /* active lifetime */
}
static void xredisNodesEnd(xredisclusternodes* C)
{
    uint32 Previous=xrtAtomic32FetchSub(&C->Admission,1,XMEMORY_ACQ_REL);
    if(Previous==(XREDIS_NODES_SEALED|1u))xredisNodesFinish(C);
}
static void xredisNodesEndLocked(xredisclusternodes* C)
{
    uint32 Previous=xrtAtomic32FetchSub(&C->Admission,1,XMEMORY_ACQ_REL);
    xrtMutexUnlock(&C->Gate);
    if(Previous==(XREDIS_NODES_SEALED|1u))xredisNodesFinish(C);
}
/* Gate held; caller releases the detached pool/Future outside it. The Watch
 * release right is already retired (or was never transferred on READY). */
static void xredisNodesUnlink(xredisclusternodepool* N,xredispool** P,xfuture** F)
{
    xredisclusternodes* C=N->Cache;xredisclusternodepool** Link=&C->Buckets[N->Hash&(C->BucketCount-1)];while(*Link!=N)Link=&(*Link)->Next;*Link=N->Next;
    *P=N->Pool;*F=N->Closed;N->Pool=NULL;N->Closed=NULL;N->Next=NULL;N->FreeNext=C->Free;C->Free=N;--C->Info.Nodes;if(*P)++C->Info.Retired;
}
static void xredisNodesNotify(ptr Data){(void)Data;}
static void xredisNodesReleased(ptr Data)
{
    xredisclusternodepool* N=Data;xredisclusternodes* C=N->Cache;xredispool* P=NULL;xfuture* F=NULL;
    xrtMutexLock(&C->Gate);N->ObservedClosed=true;if(!N->Opening)xredisNodesUnlink(N,&P,&F);xrtMutexUnlock(&C->Gate);
    xrtFutureDestroy(F);xredisPoolRelease(P);xredisNodesFinish(C);xredisClusterNodesRelease(C); /* watch lifetime; no Node access after unlock */
}
static bool xredisNodesConnect(xnetengine* E,size_t Slot,uint64 Attempt,int64 Timeout,ptr Data,xredisclient** Client)
{xredisclusternodepool* N=Data;xredisclusternodes* C=N->Cache;return C->Config.Connect(&N->Address,E,Slot,Attempt,Timeout,C->Config.Data,Client);}
XREDIS_API bool xredisClusterNodesConfigInit(xredisclusternodesconfig* C)
{
    if(!xrtMemRangeValid(C,sizeof(*C)))return xredisNodesError(XERR_ARGUMENT,"invalid Cluster nodes config output");
    *C=(xredisclusternodesconfig){0};C->Size=sizeof(*C);C->MaxNodes=64;C->MaxHostBytes=1024;C->MaxStorageBytes=1024u*1024u;C->ConnectionsPerNode=1;C->MaxConnections=64;C->MaxLookups=1024;
    C->Protocol=XREDIS_RESP3;C->MaxConnecting=4;C->AttemptTimeout=5000;C->RetryMin=100;C->RetryMax=10000;return xredisPoolConfigInit(&C->Pool);
}
static bool xredisNodesConfig(const xredisclusternodesconfig* C)
{
    const xredispoolconfig* P=&C->Pool;
    return C->Size==sizeof(*C) && C->Authorize && C->Connect && C->MaxNodes && C->MaxNodes<=65536 && C->MaxHostBytes && C->MaxHostBytes<=65536 &&
        C->MaxStorageBytes && C->MaxStorageBytes<=128u*1024u*1024u && C->MaxLookups && C->MaxLookups<=65536 && C->ConnectionsPerNode && C->MaxConnections && C->MaxConnections<=65536 &&
        C->ConnectionsPerNode<=C->MaxConnections/C->MaxNodes && (C->Protocol==XREDIS_RESP2 || C->Protocol==XREDIS_RESP3) && C->MaxConnecting && C->MaxConnecting<=65536 &&
        C->AttemptTimeout>0 && C->AttemptTimeout<=60000 && C->RetryMin>0 && C->RetryMin<=C->RetryMax && C->RetryMax<=60000 &&
        P->Size==sizeof(*P) && P->MaxPending && P->MaxPending<=65536 && P->MaxRequests && P->MaxRequests<=UINT64_MAX/C->MaxNodes && P->MaxCommandBytes && P->MaxCommandBytes<=UINT64_MAX/C->MaxNodes &&
        P->LeaseTimeout>0 && P->LeaseTimeout<=86400000 && P->ReturnTimeout>0 && P->ReturnTimeout<=60000 && P->WorkItems && P->WorkItems<=65536;
}
XREDIS_API xredisadmission xredisClusterNodesCreate(xnetengine* E,uint64 Affinity,const xredisclusternodesconfig* Config,xredisclusternodes** Out)
{
    if(!xrtMemRangeValid(Config,sizeof(*Config)) || !xrtMemRangeValid(Out,sizeof(*Out)) || xrtMemRangesOverlap(Config,sizeof(*Config),Out,sizeof(*Out)) || !xredisNodesConfig(Config)){
        xredisNodesError(XERR_ARGUMENT,"invalid Cluster nodes config/output");return XREDIS_ADMISSION_ERROR;}
    xredisclusternodesconfig K=*Config;size_t Buckets=1;while(Buckets<2*K.MaxNodes)Buckets*=2;
    size_t Base=sizeof(xredisclusternodes)+Buckets*sizeof(xredisclusternodepool*),Each=sizeof(xredisclusternodepool)+K.MaxHostBytes;
    if(Base>K.MaxStorageBytes || K.MaxNodes>(K.MaxStorageBytes-Base)/Each){xredisNodesError(XERR_RANGE,"Cluster nodes storage limit");return XREDIS_ADMISSION_ERROR;}
    size_t Bytes=Base+K.MaxNodes*Each;
    if(!xrtNetEnginePin(E))return XREDIS_ADMISSION_ERROR;
    xredisclusternodes* C=xrtCalloc(1,Bytes);if(!C || !xrtMutexInit(&C->Gate)){xrtFree(C);xrtNetEngineUnpin(E);return XREDIS_ADMISSION_ERROR;}
    xrtAtomic32Init(&C->References,1);xrtAtomic32Init(&C->Admission,0);C->StorageBytes=Bytes;C->Engine=E;C->Affinity=Affinity;C->Config=K;C->BucketCount=Buckets;
    if(!(C->Promise=xrtPromiseCreate(&C->Closed,NULL))){C->Engine=NULL;xrtNetEngineUnpin(E);xredisClusterNodesRelease(C);return XREDIS_ADMISSION_ERROR;}
    C->Buckets=(xredisclusternodepool**)(C+1);C->Slots=(xredisclusternodepool*)(C->Buckets+Buckets);unsigned char* Host=(unsigned char*)(C->Slots+K.MaxNodes);
    for(size_t I=0;I<K.MaxNodes;++I){xredisclusternodepool* N=&C->Slots[I];N->Cache=C;N->Address.Host.Data=Host+I*K.MaxHostBytes;N->FreeNext=C->Free;C->Free=N;}
    C->Info.Size=sizeof(C->Info);C->Info.Capacity=K.MaxNodes;C->Info.StorageBytes=Bytes;C->Info.ConnectionBound=K.MaxNodes*K.ConnectionsPerNode;
    C->Info.LookupBound=K.MaxLookups;C->Info.ConnectingBound=K.MaxNodes*(K.MaxConnecting<K.ConnectionsPerNode?K.MaxConnecting:K.ConnectionsPerNode);C->Info.RequestBound=(uint64)K.MaxNodes*K.Pool.MaxRequests;C->Info.CommandBytesBound=(uint64)K.MaxNodes*K.Pool.MaxCommandBytes;
    xredisNodesRef(C);*Out=C;return XREDIS_ADMISSION_ACCEPTED;
}
XREDIS_API xredisclusterpoolstatus xredisClusterNodesGetPool(const xredisclusteraddress* A,int64 Remaining,ptr Data,xredispool** Out)
{
    xredisclusternodes* C=Data;double Entered=xrtTimer();
    if(!C || !xredisNodesAddress(A,C->Config.MaxHostBytes) || !xredisNodesOutput(C,Out,sizeof(*Out)) || Remaining<0 || Remaining>86400000 ||
        xrtMemRangesOverlap(A,sizeof(*A),Out,sizeof(*Out)) || xrtMemRangesOverlap(A->Host.Data,A->Host.Size,Out,sizeof(*Out))){xredisNodesError(XERR_ARGUMENT,"invalid Cluster node lookup");return XREDIS_CLUSTER_POOL_ERROR;}
    xredisclusterpoolstatus Began=xredisNodesBegin(C);if(Began!=XREDIS_CLUSTER_POOL_READY)return Began;
    if(!Remaining){xrtMutexLock(&C->Gate);++C->Info.Waits;xredisNodesEndLocked(C);return XREDIS_CLUSTER_POOL_WAIT;}
    bool Allowed=C->Config.Authorize(A,C->Config.Data);uint64 H=xredisNodesHash(A);xrtMutexLock(&C->Gate);
    if(!Allowed){++C->Info.Denied;xredisNodesEndLocked(C);return XREDIS_CLUSTER_POOL_DENIED;}
    if(C->Info.State!=XREDIS_CLUSTER_NODES_OPEN){xredisNodesEndLocked(C);return XREDIS_CLUSTER_POOL_CLOSED;}
    if(xrtTimer()>=nextafter(Entered+(double)(Remaining/1000)+(double)(Remaining%1000)*.001,INFINITY)){++C->Info.Waits;xredisNodesEndLocked(C);return XREDIS_CLUSTER_POOL_WAIT;}
    xredisclusternodepool* N=xredisNodesFind(C,A,H);
    if(N){if(N->Opening || N->Retiring){++C->Info.Waits;xredisNodesEndLocked(C);return XREDIS_CLUSTER_POOL_WAIT;}
        xredisPoolRetain(N->Pool);*Out=N->Pool;++C->Info.Hits;xredisNodesEndLocked(C);return XREDIS_CLUSTER_POOL_READY;}
    if(!C->Free){++C->Info.Waits;xredisNodesEndLocked(C);return XREDIS_CLUSTER_POOL_WAIT;}
    N=C->Free;C->Free=N->FreeNext;N->FreeNext=NULL;N->Address.Host.Size=A->Host.Size;memcpy((ptr)N->Address.Host.Data,A->Host.Data,A->Host.Size);N->Address.Port=A->Port;N->Address.Tls=A->Tls;N->Address.Slot=0;N->Address.Reason=0;
    N->Hash=H;size_t Bucket=H&(C->BucketCount-1);N->Next=C->Buckets[Bucket];C->Buckets[Bucket]=N;N->Opening=true;N->Retiring=false;N->ObservedClosed=false;N->Pool=NULL;N->Closed=NULL;++C->Info.Nodes;++C->Info.Opening;++C->Info.Misses;
    xrtMutexUnlock(&C->Gate);
    if(!xrtFutureWatchInit(&N->Watch,xredisNodesNotify,xredisNodesReleased,N)){
        xredispool* P=NULL;xfuture* F=NULL;xrtMutexLock(&C->Gate);--C->Info.Opening;N->Opening=false;++C->Info.Failures;xredisNodesUnlink(N,&P,&F);xredisNodesEndLocked(C);return XREDIS_CLUSTER_POOL_MEMORY;}
    xredispoolconnectconfig Connect={sizeof(Connect),xredisNodesConnect,NULL,N,C->Config.Protocol,C->Config.MaxConnecting,C->Config.AttemptTimeout,C->Config.RetryMin,C->Config.RetryMax};
    xredispool* P=NULL;xredisNodesRef(C);xredisadmission Admission=xredisSharedPoolConnect(C->Engine,C->Affinity,C->Config.ConnectionsPerNode,&C->Config.Pool,&Connect,&P);
    if(Admission!=XREDIS_ADMISSION_ACCEPTED){xredispool* Drop=NULL;xfuture* F=NULL;xrtMutexLock(&C->Gate);--C->Info.Opening;N->Opening=false;++C->Info.Failures;xredisNodesUnlink(N,&Drop,&F);xrtMutexUnlock(&C->Gate);xredisClusterNodesRelease(C);xredisNodesEnd(C);return Admission==XREDIS_ADMISSION_CLOSED?XREDIS_CLUSTER_POOL_CLOSED:XREDIS_CLUSTER_POOL_MEMORY;}
    xfuture* Closed=xredisPoolClosed(P);xrtMutexLock(&C->Gate);N->Pool=P;N->Closed=Closed;++C->Info.Created;xrtMutexUnlock(&C->Gate);
    xfuturewatchresult W=xrtFutureWatchAdd(Closed,&N->Watch);
    /* Fresh private Watch/valid native Future: READY/PENDING are the only
     * possible outcomes. No allocation is performed by registration. */
    if(W==XFUTURE_WATCH_ERROR)abort();
    if(W==XFUTURE_WATCH_READY)xredisNodesReleased(N);
    xredispool* Drop=NULL;xfuture* F=NULL;xredisclusterpoolstatus Result=XREDIS_CLUSTER_POOL_READY;
    xrtMutexLock(&C->Gate);N->Opening=false;--C->Info.Opening;bool Closing=C->Info.State!=XREDIS_CLUSTER_NODES_OPEN || N->Retiring;bool Dead=N->ObservedClosed;
    if(Dead){xredisNodesUnlink(N,&Drop,&F);Result=Closing?XREDIS_CLUSTER_POOL_CLOSED:XREDIS_CLUSTER_POOL_WAIT;}
    else {if(Closing){N->Retiring=true;Result=XREDIS_CLUSTER_POOL_CLOSED;}else {xredisPoolRetain(P);*Out=P;}}
    if(Closing && !Dead)xredisPoolRetain(P);
    xrtMutexUnlock(&C->Gate);
    if(Closing && !Dead){xredisPoolClose(P);xredisPoolRelease(P);}xrtFutureDestroy(F);xredisPoolRelease(Drop);xredisNodesEnd(C);return Result;
}
XREDIS_API bool xredisClusterNodesRetire(xredisclusternodes* C,const xredisclusteraddress* A)
{
    if(!C || !xredisNodesAddress(A,C->Config.MaxHostBytes))return xredisNodesError(XERR_ARGUMENT,"invalid Cluster node retirement");
    uint64 H=xredisNodesHash(A);xredispool* P=NULL;xrtMutexLock(&C->Gate);xredisclusternodepool* N=xredisNodesFind(C,A,H);if(N){N->Retiring=true;if(N->Pool){P=N->Pool;xredisPoolRetain(P);}}xrtMutexUnlock(&C->Gate);
    if(P){xredisPoolClose(P);xredisPoolRelease(P);}return true;
}
XREDIS_API bool xredisClusterNodesInfo(xredisclusternodes* C,xredisclusternodesinfo* Out)
{if(!xredisNodesOutput(C,Out,sizeof(*Out)) || Out->Size!=sizeof(*Out))return xredisNodesError(XERR_ARGUMENT,"invalid Cluster node info output");xrtMutexLock(&C->Gate);*Out=C->Info;Out->APIs=(xrtAtomic32Load(&C->Admission,XMEMORY_ACQUIRE)&XREDIS_NODES_USERS)+(C->Scanning?1u:0u);xrtMutexUnlock(&C->Gate);return true;}
XREDIS_API bool xredisClusterNodesClose(xredisclusternodes* C)
{
    if(!C)return xredisNodesError(XERR_ARGUMENT,"Cluster node cache is NULL");
    xrtMutexLock(&C->Gate);if(C->Info.State!=XREDIS_CLUSTER_NODES_OPEN){xrtMutexUnlock(&C->Gate);return true;}xrtAtomic32FetchOr(&C->Admission,XREDIS_NODES_SEALED,XMEMORY_ACQ_REL);C->Info.State=XREDIS_CLUSTER_NODES_CLOSING;C->Scanning=true;xrtMutexUnlock(&C->Gate);
    for(size_t I=0;I<C->Config.MaxNodes;++I){xredispool* P=NULL;xrtMutexLock(&C->Gate);xredisclusternodepool* N=&C->Slots[I];N->Retiring=true;if(N->Pool){P=N->Pool;xredisPoolRetain(P);}xrtMutexUnlock(&C->Gate);if(P){xredisPoolClose(P);xredisPoolRelease(P);}}
    xrtMutexLock(&C->Gate);C->Scanning=false;xrtMutexUnlock(&C->Gate);xredisNodesFinish(C);return true;
}
XREDIS_API xfuture* xredisClusterNodesClosed(xredisclusternodes* C)
{if(!C){xredisNodesError(XERR_ARGUMENT,"Cluster node cache is NULL");return NULL;}return xrtFutureRef(C->Closed);}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/cluster_refresh/cluster_refresh.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_REFRESH)
#include <math.h>
#include <string.h>
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_REFRESH)
enum {XRF_RESOLVE,XRF_NATIVE,XRF_BACKOFF};
struct xredisclustermap {xatomic32 References;xredisclustermapinfo Info;size_t Bytes;};
typedef struct xredisrefreshwork {
    xredisclusterrefreshresult Result;xfuture* Future;xpromise* Promise;
    double Entered,Deadline,Due;size_t Seed,AttemptSeed,ProviderSeed;unsigned Phase;bool Done;
} xredisrefreshwork;
struct xredisclusterrefresh {
    xatomic32 References;xmutex Gate;xredisclusterrefreshconfig Config;
    xredisclusternodes* Nodes;xredisclusteraddress *Seeds,*Bootstrap;size_t Count,BootCount,Cursor,Bytes,Buckets;
    uint32* Plan;uint16* Hash;unsigned char* SeedHosts;
    xredisclusterrefreshinfo Info;xredisrefreshwork* Work;xredisclustermap* Current;
    xfuture *Last,*Closed;xpromise* Promise;bool Finishing;
};
static bool xredisRefreshError(xerrkind Kind,cstr Text)
{xrtSetErrorInfo(Kind,"xrt.redis",Kind==XERR_STATE?XREDIS_CLIENT_STATE:XREDIS_CLIENT_ARGUMENT,Text);return false;}
static double xredisRefreshDeadline(double E,int64 T)
{return T==0?E:nextafter(E+(double)(T/1000)+(double)(T%1000)*.001,INFINITY);}
static bool xredisRefreshHost(xbytesview H)
{if(!H.Size || !xrtMemRangeValid(H.Data,H.Size) || (H.Size==1 && H.Data[0]=='?'))return false;for(size_t I=0;I<H.Size;++I){unsigned X=H.Data[I];if(X<=32 || X==127 || X=='/' || X=='\\' || X=='@' || X=='#')return false;}return true;}
XREDIS_API bool xredisClusterMapRetain(xredisclustermap* M)
{if(!M)return xredisRefreshError(XERR_ARGUMENT,"Cluster map is NULL");xrtAtomic32FetchAdd(&M->References,1,XMEMORY_RELAXED);return true;}
XREDIS_API void xredisClusterMapRelease(xredisclustermap* M)
{if(!M || xrtAtomic32FetchSub(&M->References,1,XMEMORY_ACQ_REL)!=1)return;xredisClusterTopologyRelease(M->Info.Topology);xrtFree(M);}
bool __xredisClusterMapOutput(const xredisclustermap* M,const void* P,size_t N)
{return xrtMemRangeValid(P,N) && (!M || (!xrtMemRangesOverlap(M,M->Bytes,P,N) && __xredisClusterTopologyOutput(M->Info.Topology,P,N)));}
XREDIS_API bool xredisClusterMapInfo(const xredisclustermap* M,xredisclustermapinfo* Out)
{if(!M || !__xredisClusterMapOutput(M,Out,sizeof(*Out)) || Out->Size!=sizeof(*Out))return xredisRefreshError(XERR_ARGUMENT,"invalid Cluster map info output");*Out=M->Info;return true;}
static bool xredisRefreshOutput(xredisclusterrefresh* C,const void* P,size_t N)
{return C && xrtMemRangeValid(P,N) && !xrtMemRangesOverlap(C,C->Bytes,P,N) && __xredisClusterMapOutput(C->Current,P,N);}
XREDIS_API bool xredisClusterRefreshRetain(xredisclusterrefresh* C)
{if(!C)return xredisRefreshError(XERR_ARGUMENT,"Cluster refresh is NULL");xrtAtomic32FetchAdd(&C->References,1,XMEMORY_RELAXED);return true;}
XREDIS_API void xredisClusterRefreshRelease(xredisclusterrefresh* C)
{if(!C || xrtAtomic32FetchSub(&C->References,1,XMEMORY_ACQ_REL)!=1)return;xrtPromiseDestroy(C->Promise);xrtFutureDestroy(C->Closed);xrtMutexUnit(&C->Gate);xrtFree(C);}
static void xredisRefreshPayloadFree(ptr Value,ptr Data)
{(void)Data;xredisrefreshwork* W=Value;xrtFutureDestroy(W->Result.Native);xredisClusterMapRelease(W->Result.Map);xrtFree(W);}
static void xredisRefreshMaybeClose(xredisclusterrefresh* C)
{
    xrtMutexLock(&C->Gate);if(!C->Info.Closing || C->Info.Busy || C->Work || C->Finishing){xrtMutexUnlock(&C->Gate);return;}
    C->Finishing=true;xredisclusternodes* N=C->Nodes;C->Nodes=NULL;xredisclustermap* M=C->Current;C->Current=NULL;xfuture* Last=C->Last;C->Last=NULL;xpromise* P=C->Promise;C->Promise=NULL;xrtMutexUnlock(&C->Gate);
    xredisClusterNodesRelease(N);xredisClusterMapRelease(M);xrtFutureDestroy(Last);
    xrtMutexLock(&C->Gate);C->Info.Closed=true;xrtMutexUnlock(&C->Gate);xrtPromiseResolve(P,NULL);xrtPromiseDestroy(P);xredisClusterRefreshRelease(C);
}
static void xredisRefreshStop(xredisrefreshwork* W,xredisclusterrefreshstatus S)
{W->Result.Status=S;W->Done=true;}
/* Busy keeps Work/cache/seeds stable while policy/native calls run outside Gate. */
static void xredisRefreshPublish(xredisclusterrefresh* C,xredisrefreshwork* W)
{
    if(W->Result.Native && xrtFutureState(W->Result.Native)==XFUTURE_PENDING)xrtFutureCancel(W->Result.Native);
    if(W->Result.Native && xrtFutureState(W->Result.Native)==XFUTURE_RESOLVED){const xredisrequestresult* Q=xrtFutureValue(W->Result.Native);if(Q){W->Result.Query=*Q;W->Result.QueryObserved=true;}}
    xredisclustermap* OldMap=NULL;xfuture* OldLast=NULL;xredisclustermap* Unpublished=NULL;
    xrtMutexLock(&C->Gate);
    if(C->Info.Closing)W->Result.Status=XREDIS_REFRESH_CLOSED;
    if(W->Result.Status==XREDIS_REFRESH_OK){
        if(C->Info.Generation==UINT64_MAX)W->Result.Status=XREDIS_REFRESH_LIMIT;
        else {xredisclustermap* M=W->Result.Map;M->Info.Generation=++C->Info.Generation;M->Info.Epoch=W->Result.Epoch;W->Result.Generation=M->Info.Generation;W->Result.Published=true;
            W->Result.StaleAtPublish=W->Result.Epoch!=C->Info.DesiredEpoch;if(W->Result.StaleAtPublish)W->Result.Status=XREDIS_REFRESH_STALE;
            OldMap=C->Current;C->Current=M;xredisClusterMapRetain(M);C->Info.PublishedEpoch=W->Result.Epoch;C->Info.Dirty=C->Info.PublishedEpoch!=C->Info.DesiredEpoch;}
    }
    if(!W->Result.Published){Unpublished=W->Result.Map;W->Result.Map=NULL;}
    size_t Last=W->Result.ProviderCalls?W->ProviderSeed:W->Seed;
    xredisclusteraddress A=C->Seeds[W->Result.Attempts?W->AttemptSeed:Last];unsigned char* Host=(unsigned char*)(W+1);memcpy(Host,A.Host.Data,A.Host.Size);W->Result.Origin=A;W->Result.Origin.Host=(xbytesview){Host,A.Host.Size};
    C->Cursor=W->Result.Published?W->AttemptSeed:(Last+1)%C->Count;
    C->Work=NULL;C->Info.Active=false;++C->Info.Completed;C->Info.LastStatus=W->Result.Status;OldLast=C->Last;C->Last=W->Future;W->Future=NULL;xpromise* P=W->Promise;W->Promise=NULL;xrtMutexUnlock(&C->Gate);
    xredisClusterMapRelease(Unpublished);xredisClusterMapRelease(OldMap);xrtFutureDestroy(OldLast);xrtPromiseResolveOwned(P,W,xredisRefreshPayloadFree,NULL);xrtPromiseDestroy(P);
    xrtMutexLock(&C->Gate);C->Info.Busy=false;xrtMutexUnlock(&C->Gate);xredisRefreshMaybeClose(C);
}
XREDIS_API bool xredisClusterRefreshConfigInit(xredisclusterrefreshconfig* K)
{
    if(!xrtMemRangeValid(K,sizeof(*K)))return xredisRefreshError(XERR_ARGUMENT,"invalid Cluster refresh config output");
    *K=(xredisclusterrefreshconfig){sizeof(*K),16,1024,1024u*1024u,32u*1024u*1024u,8,1024,5000,5,XREDIS_CLUSTER_SHARDS,{0}};
    if(!xredisClusterLimitsInit(&K->Limits))return false;
    K->Limits.RequireFullCoverage=true;return true;
}
XREDIS_API xredisadmission xredisClusterRefreshCreate(xredisclusternodes* Nodes,const xredisclusteraddress* A,size_t Count,const xredisclusterrefreshconfig* Config,xredisclusterrefresh** Out)
{
    if(!Nodes || !xrtMemRangeValid(Config,sizeof(*Config)) || !xrtMemRangeValid(Out,sizeof(*Out)) || xrtMemRangesOverlap(Config,sizeof(*Config),Out,sizeof(*Out))){xredisRefreshError(XERR_ARGUMENT,"invalid Cluster refresh config/output");return XREDIS_ADMISSION_ERROR;}
    xredisclusterrefreshconfig K=*Config;size_t Overhead=2*(sizeof(xredisrefreshwork)+K.MaxHostBytes);
    if(K.Size!=sizeof(K) || !Count || !K.MaxSeeds || K.MaxSeeds>1024 || Count>K.MaxSeeds || !K.MaxHostBytes || K.MaxHostBytes>65536 || !K.MaxControlBytes || K.MaxControlBytes>128u*1024u*1024u ||
        K.MaxSnapshotBytes<sizeof(xredisclustermap) || K.MaxSnapshotBytes>128u*1024u*1024u || !K.MaxAttempts || K.MaxAttempts>1024 || K.MaxWaits>65536 || K.Timeout<0 || K.Timeout>86400000 || K.RetryDelay<=0 || K.RetryDelay>60000 ||
        (K.Format!=XREDIS_CLUSTER_SHARDS && K.Format!=XREDIS_CLUSTER_LEGACY_SLOTS) || K.Limits.Size!=sizeof(K.Limits) || K.Limits.MaxNodes>65535 || K.Limits.MaxShards>XREDIS_CLUSTER_SLOTS || K.Limits.MaxRanges>XREDIS_CLUSTER_SLOTS || !xrtMemRangeValid(A,Count*sizeof(*A)) || xrtMemRangesOverlap(A,Count*sizeof(*A),Out,sizeof(*Out))){xredisRefreshError(XERR_ARGUMENT,"invalid Cluster refresh bounds");return XREDIS_ADMISSION_ERROR;}
    size_t Buckets=4;while(Buckets<4*K.MaxSeeds)Buckets*=2;
    size_t Base=sizeof(xredisclusterrefresh)+(Count+2*K.MaxSeeds+1)*sizeof(*A)+(2*K.MaxSeeds+1)*sizeof(uint32)+Buckets*sizeof(uint16)+(K.MaxSeeds+1)*K.MaxHostBytes,Bytes=Base;
    if(Base>K.MaxControlBytes || Overhead>K.MaxControlBytes-Base){xredisRefreshError(XERR_RANGE,"Cluster refresh control limit");return XREDIS_ADMISSION_ERROR;}
    for(size_t I=0;I<Count;++I){if(!A[I].Port || A[I].Slot || A[I].Reason || A[I].Host.Size>K.MaxHostBytes || !xredisRefreshHost(A[I].Host) || xrtMemRangesOverlap(A[I].Host.Data,A[I].Host.Size,Out,sizeof(*Out)) || A[I].Host.Size>K.MaxControlBytes-Bytes-Overhead){xredisRefreshError(XERR_ARGUMENT,"invalid Cluster refresh seed/storage");return XREDIS_ADMISSION_ERROR;}Bytes+=A[I].Host.Size;}
    xredisclusterrefresh* C=xrtCalloc(1,Bytes);if(!C || !xrtMutexInit(&C->Gate)){xrtFree(C);return XREDIS_ADMISSION_ERROR;}xrtAtomic32Init(&C->References,1);C->Config=K;C->Bytes=Bytes;C->Count=C->BootCount=Count;C->Buckets=Buckets;C->Bootstrap=(xredisclusteraddress*)(C+1);C->Seeds=C->Bootstrap+Count;C->Plan=(uint32*)(C->Seeds+2*K.MaxSeeds+1);C->Hash=(uint16*)(C->Plan+2*K.MaxSeeds+1);C->SeedHosts=(unsigned char*)(C->Hash+Buckets);
    if(!(C->Promise=xrtPromiseCreate(&C->Closed,NULL))){xredisClusterRefreshRelease(C);return XREDIS_ADMISSION_ERROR;}
    unsigned char* Host=C->SeedHosts+(K.MaxSeeds+1)*K.MaxHostBytes;for(size_t I=0;I<Count;++I){C->Bootstrap[I]=A[I];memcpy(Host,A[I].Host.Data,A[I].Host.Size);C->Bootstrap[I].Host.Data=Host;C->Seeds[I]=C->Bootstrap[I];Host+=A[I].Host.Size;}
    C->Nodes=Nodes;xredisClusterNodesRetain(Nodes);C->Info.Size=sizeof(C->Info);C->Info.DesiredEpoch=1;C->Info.Dirty=true;C->Info.Seeds=Count;C->Info.ControlBytes=Bytes+Overhead;xredisClusterRefreshRetain(C);*Out=C;return XREDIS_ADMISSION_ACCEPTED;
}
XREDIS_API xredisadmission xredisClusterRefreshStart(xredisclusterrefresh* C,bool Force,xfuture** Out)
{
    double E=xrtTimer();if(!C){xredisRefreshError(XERR_ARGUMENT,"Cluster refresh is NULL");return XREDIS_ADMISSION_ERROR;}xrtMutexLock(&C->Gate);
    if(!xredisRefreshOutput(C,Out,sizeof(*Out))){xrtMutexUnlock(&C->Gate);xredisRefreshError(XERR_ARGUMENT,"invalid refresh Future output");return XREDIS_ADMISSION_ERROR;}
    if(C->Info.Closing){xrtMutexUnlock(&C->Gate);return XREDIS_ADMISSION_CLOSED;}
    if(C->Work){*Out=xrtFutureRef(C->Work->Future);++C->Info.Coalesced;xrtMutexUnlock(&C->Gate);return XREDIS_ADMISSION_ACCEPTED;}
    if(C->Info.Busy){xrtMutexUnlock(&C->Gate);return XREDIS_ADMISSION_OVERLOADED;}
    if(!Force && !C->Info.Dirty && C->Last){*Out=xrtFutureRef(C->Last);++C->Info.CacheHits;xrtMutexUnlock(&C->Gate);return XREDIS_ADMISSION_ACCEPTED;}
    C->Info.Busy=true;xrtMutexUnlock(&C->Gate);
    xredisrefreshwork* W=xrtCalloc(1,sizeof(*W)+C->Config.MaxHostBytes);
    if(W)W->Promise=xrtPromiseCreate(&W->Future,NULL);
    xrtMutexLock(&C->Gate);bool Closing=C->Info.Closing,Advance=Force && !C->Info.Dirty,Limit=Advance && C->Info.DesiredEpoch==UINT64_MAX;
    if(Closing || Limit || !W || !W->Promise){xrtMutexUnlock(&C->Gate);if(W){xrtPromiseDestroy(W->Promise);xrtFutureDestroy(W->Future);xrtFree(W);}xrtMutexLock(&C->Gate);C->Info.Busy=false;xrtMutexUnlock(&C->Gate);xredisRefreshMaybeClose(C);if(Limit)xredisRefreshError(XERR_RANGE,"Cluster refresh epoch limit");return Closing?XREDIS_ADMISSION_CLOSED:XREDIS_ADMISSION_ERROR;}
    if(Advance)++C->Info.DesiredEpoch;
    C->Info.Dirty=true;W->Entered=E;W->Deadline=xredisRefreshDeadline(E,C->Config.Timeout);W->Due=W->Deadline;W->Seed=C->Cursor;W->Result.Size=sizeof(W->Result);W->Result.Epoch=C->Info.DesiredEpoch;C->Work=W;C->Info.Active=true;C->Info.Busy=false;++C->Info.Started;*Out=xrtFutureRef(W->Future);xrtMutexUnlock(&C->Gate);return XREDIS_ADMISSION_ACCEPTED;
}
static bool xredisRefreshSeedEqual(const xredisclusteraddress* A,const xredisclusteraddress* B)
{return A->Port==B->Port && A->Tls==B->Tls && A->Host.Size==B->Host.Size && !memcmp(A->Host.Data,B->Host.Data,A->Host.Size);}
static bool xredisRefreshCandidate(const xredisclustermapinfo* M,size_t Index,size_t Max,xredisclusteraddress* A)
{
    xredisclusternode N={0};if(!xredisClusterTopologyNode(M->Topology,Index,&N))return false;
    xredisclustertopologyinfo T={0};T.Size=sizeof(T);if(!xredisClusterTopologyInfo(M->Topology,&T))return false;
    *A=(xredisclusteraddress){N.Endpoint==XREDIS_CLUSTER_ENDPOINT_SOURCE?M->Origin.Host:N.Host,T.Format==XREDIS_CLUSTER_LEGACY_SLOTS?N.Port:M->Origin.Tls?N.TlsPort:N.Port,0,M->Origin.Tls,0};
    return N.Role==XREDIS_CLUSTER_MASTER && (N.Health==XREDIS_CLUSTER_ONLINE || N.Health==XREDIS_CLUSTER_UNREPORTED) && N.Endpoint!=XREDIS_CLUSTER_ENDPOINT_UNKNOWN && A->Port && A->Host.Size<=Max && xredisRefreshHost(A->Host);
}
static xredisclusteraddress xredisRefreshPlanned(xredisclusterrefresh* C,const xredisclustermapinfo* M,uint32 Code)
{xredisclusteraddress A={0};if(Code&0x80000000u)A=C->Bootstrap[Code&0x7fffffffu];else if(Code&0x40000000u){A=M->Origin;A.Slot=0;A.Reason=0;}else (void)xredisRefreshCandidate(M,Code,C->Config.MaxHostBytes,&A);return A;}
static bool xredisRefreshPlanAdd(xredisclusterrefresh* C,const xredisclustermapinfo* M,uint32 Code,const xredisclusteraddress* A,size_t* Count)
{
    uint64 H=14695981039346656037ull;for(size_t I=0;I<A->Host.Size;++I){H^=A->Host.Data[I];H*=1099511628211ull;}H^=A->Port;H*=1099511628211ull;H^=A->Tls;size_t Bucket=(size_t)H&(C->Buckets-1);
    while(C->Hash[Bucket]){xredisclusteraddress B=xredisRefreshPlanned(C,M,C->Plan[C->Hash[Bucket]-1]);if(xredisRefreshSeedEqual(A,&B))return false;Bucket=(Bucket+1)&(C->Buckets-1);}
    C->Plan[*Count]=Code;C->Hash[Bucket]=(uint16)(*Count+1);++*Count;return true;
}
XREDIS_API xredisadmission xredisClusterRefreshDiscover(xredisclusterrefresh* C,const xredisclustermap* Map)
{
    if(!C || !Map){xredisRefreshError(XERR_ARGUMENT,"invalid Cluster seed discovery");return XREDIS_ADMISSION_ERROR;}
    xrtMutexLock(&C->Gate);if(C->Info.Closing){xrtMutexUnlock(&C->Gate);return XREDIS_ADMISSION_CLOSED;}if(C->Info.Busy || C->Work){xrtMutexUnlock(&C->Gate);return XREDIS_ADMISSION_OVERLOADED;}C->Info.Busy=true;xrtMutexUnlock(&C->Gate);
    xredisclustermapinfo M={0};M.Size=sizeof(M);xredisClusterMapInfo(Map,&M);xredisclustertopologyinfo T={0};T.Size=sizeof(T);xredisClusterTopologyInfo(M.Topology,&T);memset(C->Hash,0,C->Buckets*sizeof(*C->Hash));size_t Count=0;
    for(size_t I=0;I<T.Nodes;++I){xredisclusteraddress A;if(xredisRefreshCandidate(&M,I,C->Config.MaxHostBytes,&A) && xredisRefreshSeedEqual(&A,&M.Origin)){xredisRefreshPlanAdd(C,&M,(uint32)I,&A,&Count);break;}}
    for(size_t I=0;I<T.Nodes && Count<C->Config.MaxSeeds;++I){xredisclusteraddress A;if(xredisRefreshCandidate(&M,I,C->Config.MaxHostBytes,&A))xredisRefreshPlanAdd(C,&M,(uint32)I,&A,&Count);}
    size_t Discovered=Count;if(M.Origin.Port && M.Origin.Host.Size<=C->Config.MaxHostBytes && xredisRefreshHost(M.Origin.Host))xredisRefreshPlanAdd(C,&M,0x40000000u,&M.Origin,&Count);for(size_t I=0;I<C->BootCount;++I)xredisRefreshPlanAdd(C,&M,0x80000000u|(uint32)I,&C->Bootstrap[I],&Count);
    xrtMutexLock(&C->Gate);bool Closing=C->Info.Closing;xrtMutexUnlock(&C->Gate);
    if(!Closing){unsigned char* Host=C->SeedHosts;for(size_t I=0;I<Count;++I){xredisclusteraddress A=xredisRefreshPlanned(C,&M,C->Plan[I]);C->Seeds[I]=A;if(I<Discovered || C->Plan[I]==0x40000000u){memcpy(Host,A.Host.Data,A.Host.Size);C->Seeds[I].Host.Data=Host;Host+=A.Host.Size;}}}
    xrtMutexLock(&C->Gate);Closing=C->Info.Closing;if(!Closing){C->Count=Count;C->Cursor=0;C->Info.Seeds=Count;}C->Info.Busy=false;xrtMutexUnlock(&C->Gate);xredisRefreshMaybeClose(C);return Closing?XREDIS_ADMISSION_CLOSED:XREDIS_ADMISSION_ACCEPTED;
}
XREDIS_API bool xredisClusterRefreshInvalidate(xredisclusterrefresh* C)
{
    if(!C)return xredisRefreshError(XERR_ARGUMENT,"Cluster refresh is NULL");
    xrtMutexLock(&C->Gate);
    if(C->Info.Closing || C->Info.DesiredEpoch==UINT64_MAX){xrtMutexUnlock(&C->Gate);return xredisRefreshError(XERR_STATE,"Cluster refresh is sealed or epoch exhausted");}
    ++C->Info.DesiredEpoch;C->Info.Dirty=true;xrtMutexUnlock(&C->Gate);return true;
}
static void xredisRefreshWait(xredisclusterrefresh* C,xredisrefreshwork* W)
{if(W->Result.Waits>=C->Config.MaxWaits){xredisRefreshStop(W,XREDIS_REFRESH_OVERLOADED);return;}++W->Result.Waits;W->Due=fmin(W->Deadline,xredisRefreshDeadline(xrtTimer(),C->Config.RetryDelay));W->Phase=XRF_BACKOFF;}
static void xredisRefreshResolve(xredisclusterrefresh* C,xredisrefreshwork* W)
{
    double Ms=(W->Deadline-xrtTimer())*1000.;int64 Remaining=Ms<=0?0:Ms>=86400000.?86400000:(int64)ceil(Ms);xredispool* P=NULL;W->ProviderSeed=W->Seed;++W->Result.ProviderCalls;
    W->Result.Provider=xredisClusterNodesGetPool(&C->Seeds[W->Seed],Remaining,C->Nodes,&P);
    xrtMutexLock(&C->Gate);bool Closing=C->Info.Closing;xrtMutexUnlock(&C->Gate);
    if(Closing || xrtTimer()>=W->Deadline){if(P)xredisPoolRelease(P);xredisRefreshStop(W,Closing?XREDIS_REFRESH_CLOSED:XREDIS_REFRESH_TIMEOUT);return;}
    if(W->Result.Provider!=XREDIS_CLUSTER_POOL_READY){
        if(W->Result.Provider==XREDIS_CLUSTER_POOL_WAIT){W->Seed=(W->Seed+1)%C->Count;xredisRefreshWait(C,W);}
        else xredisRefreshStop(W,W->Result.Provider==XREDIS_CLUSTER_POOL_DENIED?XREDIS_REFRESH_POLICY:W->Result.Provider==XREDIS_CLUSTER_POOL_CLOSED?XREDIS_REFRESH_CLOSED:W->Result.Provider==XREDIS_CLUSTER_POOL_MEMORY?XREDIS_REFRESH_MEMORY:XREDIS_REFRESH_PROTOCOL);
        return;
    }
    xbytesview Args[]={XRT_BYTES_LITERAL("CLUSTER"),C->Config.Format==XREDIS_CLUSTER_SHARDS?(xbytesview)XRT_BYTES_LITERAL("SHARDS"):(xbytesview)XRT_BYTES_LITERAL("SLOTS")};xfuture* F=NULL;
    W->Result.Admission=__xredisSharedPoolCall(P,Args,2,C->Config.Timeout,&F,W->Entered,false);xredisPoolRelease(P);
    if(W->Result.Admission==XREDIS_ADMISSION_ACCEPTED){xrtFutureDestroy(W->Result.Native);W->Result.Native=F;W->AttemptSeed=W->Seed;++W->Result.Attempts;W->Phase=XRF_NATIVE;W->Due=W->Deadline;}
    else if(W->Result.Admission==XREDIS_ADMISSION_OVERLOADED){W->Seed=(W->Seed+1)%C->Count;xredisRefreshWait(C,W);}
    else xredisRefreshStop(W,W->Result.Admission==XREDIS_ADMISSION_CLOSED?XREDIS_REFRESH_CLOSED:XREDIS_REFRESH_MEMORY);
}
static void xredisRefreshRead(xredisclusterrefresh* C,xredisrefreshwork* W)
{
    const xredisrequestresult* Q=xrtFutureValue(W->Result.Native);if(!Q){xredisRefreshStop(W,XREDIS_REFRESH_PROTOCOL);return;}
    if(Q->Status!=XREDIS_REQUEST_OK){
        if(Q->Status==XREDIS_REQUEST_SERVER_ERROR){xredisRefreshStop(W,XREDIS_REFRESH_SERVER);return;}
        if(W->Result.Attempts>=C->Config.MaxAttempts){xredisRefreshStop(W,XREDIS_REFRESH_TRANSPORT);return;}
        W->Seed=(W->Seed+1)%C->Count;xredisRefreshWait(C,W);return;
    }
    xredisclusteraddress A=C->Seeds[W->Seed];size_t Bytes=sizeof(xredisclustermap)+A.Host.Size;
    if(Bytes>C->Config.MaxSnapshotBytes){xredisRefreshStop(W,XREDIS_REFRESH_LIMIT);return;}
    xredisclusterlimits K=C->Config.Limits;if(K.MaxOwnedBytes>C->Config.MaxSnapshotBytes-Bytes)K.MaxOwnedBytes=C->Config.MaxSnapshotBytes-Bytes;xredisclustertopology* T=NULL;
    W->Result.Parse=xredisClusterTopologyCreate(Q->Reply,C->Config.Format,&K,&T);
    if(W->Result.Parse!=XREDIS_CLUSTER_VALUE){xredisRefreshStop(W,W->Result.Parse==XREDIS_CLUSTER_MEMORY?XREDIS_REFRESH_MEMORY:W->Result.Parse==XREDIS_CLUSTER_LIMIT?XREDIS_REFRESH_LIMIT:XREDIS_REFRESH_PROTOCOL);return;}
    xredisclustermap* M=xrtCalloc(1,Bytes);if(!M){xredisClusterTopologyRelease(T);xredisRefreshStop(W,XREDIS_REFRESH_MEMORY);return;}
    xrtAtomic32Init(&M->References,1);M->Bytes=Bytes;M->Info.Size=sizeof(M->Info);M->Info.Topology=T;M->Info.Origin=A;unsigned char* Host=(unsigned char*)(M+1);memcpy(Host,A.Host.Data,A.Host.Size);M->Info.Origin.Host=(xbytesview){Host,A.Host.Size};
    xredisclustertopologyinfo Info={0};Info.Size=sizeof(Info);xredisClusterTopologyInfo(T,&Info);M->Info.StorageBytes=Bytes+Info.StorageBytes+Info.RetainedReplyBytes;W->Result.Map=M;xredisRefreshStop(W,XREDIS_REFRESH_OK);
}
XREDIS_API xredisclusterrefreshstep xredisClusterRefreshStep(xredisclusterrefresh* C)
{
    if(!C){xredisRefreshError(XERR_ARGUMENT,"Cluster refresh is NULL");return XREDIS_REFRESH_STEP_ERROR;}xrtMutexLock(&C->Gate);
    if(C->Info.Busy){xrtMutexUnlock(&C->Gate);return XREDIS_REFRESH_BUSY;}xredisrefreshwork* W=C->Work;
    if(!W){xrtMutexUnlock(&C->Gate);return XREDIS_REFRESH_IDLE;}C->Info.Busy=true;bool Closing=C->Info.Closing;xrtMutexUnlock(&C->Gate);
    if(Closing)xredisRefreshStop(W,XREDIS_REFRESH_CLOSED);
    else if(xrtTimer()>=W->Deadline)xredisRefreshStop(W,XREDIS_REFRESH_TIMEOUT);
    else if(W->Phase==XRF_BACKOFF){if(xrtTimer()>=W->Due)W->Phase=XRF_RESOLVE;}
    else if(W->Phase==XRF_NATIVE){if(xrtFutureState(W->Result.Native)!=XFUTURE_PENDING)xredisRefreshRead(C,W);}
    else xredisRefreshResolve(C,W);
    if((!W->Done || W->Result.Status==XREDIS_REFRESH_OK) && xrtTimer()>=W->Deadline)xredisRefreshStop(W,XREDIS_REFRESH_TIMEOUT);
    xrtMutexLock(&C->Gate);Closing=C->Info.Closing;if(Closing)xredisRefreshStop(W,XREDIS_REFRESH_CLOSED);
    if(W->Done){xrtMutexUnlock(&C->Gate);xredisRefreshPublish(C,W);return XREDIS_REFRESH_DONE;}
    xredisclusterrefreshstep Result=W->Phase==XRF_BACKOFF || (W->Phase==XRF_NATIVE && xrtFutureState(W->Result.Native)==XFUTURE_PENDING)?XREDIS_REFRESH_WAIT:XREDIS_REFRESH_MORE;
    C->Info.Busy=false;xrtMutexUnlock(&C->Gate);return Result;
}
XREDIS_API xfuture* xredisClusterRefreshPending(xredisclusterrefresh* C)
{if(!C){xredisRefreshError(XERR_ARGUMENT,"Cluster refresh is NULL");return NULL;}xrtMutexLock(&C->Gate);xredisrefreshwork* W=C->Work;xfuture* F=!C->Info.Busy && W && W->Phase==XRF_NATIVE?xrtFutureRef(W->Result.Native):NULL;xrtMutexUnlock(&C->Gate);return F;}
XREDIS_API double xredisClusterRefreshNextWake(xredisclusterrefresh* C)
{if(!C){xredisRefreshError(XERR_ARGUMENT,"Cluster refresh is NULL");return 0;}xrtMutexLock(&C->Gate);xredisrefreshwork* W=C->Work;double D=!W?0:C->Info.Busy?xrtTimer():W->Phase==XRF_BACKOFF?W->Due:W->Phase==XRF_RESOLVE || (W->Result.Native && xrtFutureState(W->Result.Native)!=XFUTURE_PENDING)?fmin(W->Deadline,xrtTimer()):W->Deadline;xrtMutexUnlock(&C->Gate);return D;}
XREDIS_API xredisclusterstatus xredisClusterRefreshSnapshot(xredisclusterrefresh* C,xredisclustermap** Out,bool* Stale)
{
    if(!C){xredisRefreshError(XERR_ARGUMENT,"Cluster refresh is NULL");return XREDIS_CLUSTER_ERROR;}xrtMutexLock(&C->Gate);
    if(!xredisRefreshOutput(C,Out,sizeof(*Out)) || !xredisRefreshOutput(C,Stale,sizeof(*Stale)) || xrtMemRangesOverlap(Out,sizeof(*Out),Stale,sizeof(*Stale))){xrtMutexUnlock(&C->Gate);xredisRefreshError(XERR_ARGUMENT,"invalid Cluster snapshot outputs");return XREDIS_CLUSTER_ERROR;}
    if(!C->Current){xrtMutexUnlock(&C->Gate);return XREDIS_CLUSTER_ABSENT;}xredisClusterMapRetain(C->Current);*Out=C->Current;*Stale=C->Info.Dirty;xrtMutexUnlock(&C->Gate);return XREDIS_CLUSTER_VALUE;
}
XREDIS_API bool xredisClusterRefreshInfo(xredisclusterrefresh* C,xredisclusterrefreshinfo* Out)
{if(!C)return xredisRefreshError(XERR_ARGUMENT,"Cluster refresh is NULL");
    xrtMutexLock(&C->Gate);if(!xredisRefreshOutput(C,Out,sizeof(*Out)) || Out->Size!=sizeof(*Out)){xrtMutexUnlock(&C->Gate);return xredisRefreshError(XERR_ARGUMENT,"invalid refresh info output");}*Out=C->Info;xrtMutexUnlock(&C->Gate);return true;}
XREDIS_API bool xredisClusterRefreshClose(xredisclusterrefresh* C)
{
    if(!C)return xredisRefreshError(XERR_ARGUMENT,"Cluster refresh is NULL");
    xrtMutexLock(&C->Gate);C->Info.Closing=true;
    if(C->Info.Busy){xrtMutexUnlock(&C->Gate);return true;}xredisrefreshwork* W=C->Work;if(W)C->Info.Busy=true;xrtMutexUnlock(&C->Gate);
    if(W){xredisRefreshStop(W,XREDIS_REFRESH_CLOSED);xredisRefreshPublish(C,W);}else xredisRefreshMaybeClose(C);return true;
}
XREDIS_API xfuture* xredisClusterRefreshClosed(xredisclusterrefresh* C)
{if(!C){xredisRefreshError(XERR_ARGUMENT,"Cluster refresh is NULL");return NULL;}return xrtFutureRef(C->Closed);}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/cluster_client/cluster_client.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_CLIENT)
#include <math.h>
#include <string.h>
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_CLIENT)
typedef struct xredisclusteroperation xredisclusteroperation;
typedef struct xredisclusterwatch {
    xfuturewatch Watch;xfuture* Future;xredisclusterclient* Client;
    xredisclusteroperation* Operation;bool Active;
} xredisclusterwatch;
struct xredisclusteroperation {
    xredisclusterclientresult Result;xredisclusterclient* Client;
    xredisclusteroperation *Next,*Previous,*ReadyNext,*ReadyPrevious,*MapNext,*MapPrevious;
    xredisclusterroute* Route;xredisclusterwatch Watch;
    xfuture* Future;xpromise* Promise;xcancel* Token;xcancelwatch* Cancel;
    xbytesview* Arguments;size_t Count,Wire,Reservation,Heap;
    int64 Timeout;uint16 Slot;double Entered,Deadline,Due,Started;
    size_t Moved;bool Queued,WaitingMap,MissingTried,Stopping,Publishing;
};
#define XREDIS_CLUSTER_OPERATION_PREFIX ((sizeof(xredisclusteroperation)+_Alignof(max_align_t)-1)/_Alignof(max_align_t)*_Alignof(max_align_t))
struct xredisclusterclient {
    xatomic32 References;xmutex Gate;xnetengine* Engine;xnetworker* Worker;xnetpost Post;
    xredisclusterclientconfig Config;xredisclusterclientinfo Info;
    xredisclusternodes* Nodes;xredisclusterrefresh* Refresh;xredisclustermap* Map;
    xredisclusteroperation *All,*ReadyHead,*ReadyTail,*MapHead,**Heap;size_t HeapCount,Bytes;
    xredisclusterwatch Metadata;xfuture* RefreshFuture;
    xpromise *ReadyPromise,*ClosePromise;xfuture *ReadyFuture,*ClosedFuture;
    xredisclusterclientready *ReadyValue,*CloseValue;
    uint64 Timer,InvalidatedGeneration;double TimerDue,RefreshDue;int64 Backoff;
    bool Posted,Finished,Invalidation;
};
static void xredisClusterWork(xnetworker*,ptr);
static void xredisClusterStop(xredisclusterclient*,xredisclusterclientstatus);
static bool xredisClusterClientError(xerrkind K,cstr T)
{xrtSetErrorInfo(K,"xrt.redis",K==XERR_STATE?XREDIS_CLIENT_STATE:XREDIS_CLIENT_ARGUMENT,T);return false;}
static double xredisClusterDeadline(double E,int64 T)
{return T==0?E:nextafter(E+(double)(T/1000)+(double)(T%1000)*.001,INFINITY);}
XREDIS_API bool xredisClusterClientRetain(xredisclusterclient* C)
{if(!C)return xredisClusterClientError(XERR_ARGUMENT,"Cluster client is NULL");xrtAtomic32FetchAdd(&C->References,1,XMEMORY_RELAXED);return true;}
XREDIS_API void xredisClusterClientRelease(xredisclusterclient* C)
{
    if(!C || xrtAtomic32FetchSub(&C->References,1,XMEMORY_ACQ_REL)!=1)return;
    xrtPromiseDestroy(C->ReadyPromise);xrtPromiseDestroy(C->ClosePromise);xrtFutureDestroy(C->ReadyFuture);xrtFutureDestroy(C->ClosedFuture);
    xrtFree(C->ReadyValue);xrtFree(C->CloseValue);xrtMutexUnit(&C->Gate);xrtFree(C);
}
static void xredisClusterWakeLocked(xredisclusterclient* C)
{
    if(C->Finished || C->Posted)return;
    C->Posted=true;xredisClusterClientRetain(C);
    if(!xrtNetPost(C->Worker,&C->Post,xredisClusterWork,C)){C->Posted=false;xredisClusterClientRelease(C);}
}
static void xredisClusterWake(xredisclusterclient* C)
{xrtMutexLock(&C->Gate);xredisClusterWakeLocked(C);xrtMutexUnlock(&C->Gate);}
static void xredisClusterQueueLocked(xredisclusterclient* C,xredisclusteroperation* O)
{
    if(O->Queued || O->Publishing)return;
    O->Queued=true;O->ReadyPrevious=C->ReadyTail;O->ReadyNext=NULL;
    if(C->ReadyTail)C->ReadyTail->ReadyNext=O;else C->ReadyHead=O;C->ReadyTail=O;xredisClusterWakeLocked(C);
}
static void xredisClusterUnqueueLocked(xredisclusterclient* C,xredisclusteroperation* O)
{
    if(!O->Queued)return;
    if(O->ReadyPrevious)O->ReadyPrevious->ReadyNext=O->ReadyNext;else C->ReadyHead=O->ReadyNext;
    if(O->ReadyNext)O->ReadyNext->ReadyPrevious=O->ReadyPrevious;else C->ReadyTail=O->ReadyPrevious;
    O->Queued=false;O->ReadyNext=O->ReadyPrevious=NULL;
}
static void xredisClusterWatchNotify(ptr Data){(void)Data;}
static void xredisClusterWatchDrop(ptr Data)
{
    xredisclusterwatch* W=Data;xredisclusterclient* C=W->Client;
    xrtMutexLock(&C->Gate);W->Active=false;if(W->Operation)xredisClusterQueueLocked(C,W->Operation);else xredisClusterWakeLocked(C);xrtMutexUnlock(&C->Gate);
    xredisClusterClientRelease(C);
}
static bool xredisClusterWatchActive(xredisclusterwatch* W)
{xredisclusterclient* C=W->Client;xrtMutexLock(&C->Gate);bool Active=W->Active;xrtMutexUnlock(&C->Gate);return Active;}
static bool xredisClusterWatchStop(xredisclusterwatch* W)
{
    if(!W->Future)return true;
    if(xredisClusterWatchActive(W)){
        (void)xrtFutureWatchDetach(W->Future,&W->Watch);
        if(xredisClusterWatchActive(W))return false;
    }
    xrtFutureDestroy(W->Future);W->Future=NULL;return true;
}
/* Supplied Future reference always transfers, including failed registration. */
static bool xredisClusterWatchStart(xredisclusterwatch* W,xfuture* F)
{
    xredisclusterclient* C=W->Client;W->Future=F;
    if(!F || !xrtFutureWatchInit(&W->Watch,xredisClusterWatchNotify,xredisClusterWatchDrop,W))return false;
    xredisClusterClientRetain(C);xrtMutexLock(&C->Gate);W->Active=true;xrtMutexUnlock(&C->Gate);
    xfuturewatchresult R=xrtFutureWatchAdd(F,&W->Watch);
    if(R!=XFUTURE_WATCH_PENDING){xrtMutexLock(&C->Gate);W->Active=false;if(R==XFUTURE_WATCH_READY){if(W->Operation)xredisClusterQueueLocked(C,W->Operation);else xredisClusterWakeLocked(C);}xrtMutexUnlock(&C->Gate);xredisClusterClientRelease(C);}
    return R!=XFUTURE_WATCH_ERROR;
}
static void xredisClusterHeapSwap(xredisclusterclient* C,size_t A,size_t B)
{xredisclusteroperation* O=C->Heap[A];C->Heap[A]=C->Heap[B];C->Heap[B]=O;C->Heap[A]->Heap=A;O->Heap=B;}
static void xredisClusterHeapFix(xredisclusterclient* C,size_t I)
{
    while(I && C->Heap[I]->Due<C->Heap[(I-1)/2]->Due){size_t P=(I-1)/2;xredisClusterHeapSwap(C,I,P);I=P;}
    for(;;){size_t L=I*2+1;if(L>=C->HeapCount)break;size_t R=L+1,J=R<C->HeapCount && C->Heap[R]->Due<C->Heap[L]->Due?R:L;if(C->Heap[I]->Due<=C->Heap[J]->Due)break;xredisClusterHeapSwap(C,I,J);I=J;}
}
static void xredisClusterHeapRemove(xredisclusterclient* C,xredisclusteroperation* O)
{
    if(O->Heap==SIZE_MAX)return;
    size_t I=O->Heap;O->Heap=SIZE_MAX;--C->HeapCount;
    if(I<C->HeapCount){C->Heap[I]=C->Heap[C->HeapCount];C->Heap[I]->Heap=I;xredisClusterHeapFix(C,I);}
}
static void xredisClusterHeapSet(xredisclusterclient* C,xredisclusteroperation* O,double Due)
{O->Due=Due;if(O->Heap==SIZE_MAX){O->Heap=C->HeapCount;C->Heap[C->HeapCount++]=O;}xredisClusterHeapFix(C,O->Heap);}
static void xredisClusterMapWaitRemove(xredisclusterclient* C,xredisclusteroperation* O)
{
    if(!O->WaitingMap)return;
    if(O->MapPrevious)O->MapPrevious->MapNext=O->MapNext;else C->MapHead=O->MapNext;
    if(O->MapNext)O->MapNext->MapPrevious=O->MapPrevious;
    O->MapNext=O->MapPrevious=NULL;O->WaitingMap=false;
}
static void xredisClusterMapWait(xredisclusterclient* C,xredisclusteroperation* O)
{
    if(!O->WaitingMap){O->MapNext=C->MapHead;if(C->MapHead)C->MapHead->MapPrevious=O;C->MapHead=O;O->WaitingMap=true;}
    xredisClusterHeapSet(C,O,O->Deadline);
}
static bool xredisClusterClosing(xredisclusterclient* C)
{xrtMutexLock(&C->Gate);bool V=C->Info.Closing;xrtMutexUnlock(&C->Gate);return V;}
static xredisclusterpoolstatus xredisClusterProvider(const xredisclusteraddress* A,int64 T,ptr Data,xredispool** Out)
{
    xredisclusteroperation* O=Data;xredisclusterclient* C=O->Client;xredispool* P=NULL;
    if(xredisClusterClosing(C))return XREDIS_CLUSTER_POOL_CLOSED;
    xredisclusterpoolstatus S=xredisClusterNodesGetPool(A,T,C->Nodes,&P);
    if(xredisClusterClosing(C)){if(P)xredisPoolRelease(P);return XREDIS_CLUSTER_POOL_CLOSED;}
    if(S==XREDIS_CLUSTER_POOL_READY)*Out=P;
    return S;
}
static xredisclusterrouteconfig xredisClusterRouteConfigFor(xredisclusterclient* C,xredisclusteroperation* O,bool Tls)
{xredisclusterrouteconfig K=C->Config.Route;K.GetPool=xredisClusterProvider;K.Data=O;K.Tls=Tls;if(O)K.Timeout=O->Timeout;return K;}
static void xredisClusterInvalidateGeneration(xredisclusterclient* C,uint64 Generation)
{
    xrtMutexLock(&C->Gate);bool Needed=Generation==C->Info.Generation && Generation!=C->InvalidatedGeneration;
    if(Needed){C->InvalidatedGeneration=Generation;C->Invalidation=true;C->Info.Dirty=true;xredisClusterWakeLocked(C);}xrtMutexUnlock(&C->Gate);
}
static void xredisClusterOperationFree(ptr Value,ptr Data)
{
    (void)Data;xredisclusteroperation* O=Value;
    if(O->Route==(xredisclusterroute*)((unsigned char*)O+XREDIS_CLUSTER_OPERATION_PREFIX))__xredisClusterRouteUnit(O->Route);else xredisClusterRouteDestroy(O->Route);
    xrtFutureDestroy(O->Result.Native);xrtFree(O->Arguments);xrtFree(O);
}
static void xredisClusterCancelled(ptr Data)
{xredisclusteroperation* O=Data;xredisclusterclient* C=O->Client;xrtMutexLock(&C->Gate);xredisClusterQueueLocked(C,O);xrtMutexUnlock(&C->Gate);}
static void xredisClusterComplete(xredisclusterclient* C,xredisclusteroperation* O)
{
    if(!xredisClusterWatchStop(&O->Watch)){xredisClusterHeapRemove(C,O);return;}
    xredisClusterMapWaitRemove(C,O);xredisClusterHeapRemove(C,O);
    if(O->Route){O->Result.Route.Size=sizeof(O->Result.Route);O->Result.RouteObserved=xredisClusterRouteResult(O->Route,&O->Result.Route);O->Result.Native=xredisClusterRouteResultFuture(O->Route);}
    O->Result.TotalSeconds=fmax(0,xrtTimer()-O->Entered);O->Result.QueueSeconds=fmax(0,(O->Started?O->Started:xrtTimer())-O->Entered);
    xrtMutexLock(&C->Gate);O->Publishing=true;xredisClusterUnqueueLocked(C,O);
    if(O->Previous)O->Previous->Next=O->Next;else C->All=O->Next;if(O->Next)O->Next->Previous=O->Previous;
    --C->Info.Requests;C->Info.CommandBytes-=O->Wire;C->Info.StorageBytes-=O->Reservation;++C->Info.Completed;O->Result.RefreshStatus=C->Info.LastRefresh;O->Result.RefreshObserved=C->Info.RefreshCompleted!=0;xrtMutexUnlock(&C->Gate);
    xrtCancelUnwatch(O->Cancel);O->Cancel=NULL;xrtCancelDestroy(O->Token);O->Token=NULL;
    xrtFutureDestroy(O->Future);O->Future=NULL;xpromise* P=O->Promise;O->Promise=NULL;O->Client=NULL;O->Watch.Client=NULL;xrtFree(O->Arguments);O->Arguments=NULL;
    if(!xrtPromiseResolveOwned(P,O,xredisClusterOperationFree,NULL))xredisClusterOperationFree(O,NULL);
    xrtPromiseDestroy(P);
}
static void xredisClusterStopOperation(xredisclusterclient* C,xredisclusteroperation* O,xredisclusterclientstatus Status)
{if(!O->Stopping){O->Stopping=true;O->Result.Status=Status;if(O->Route)xredisClusterRouteCancel(O->Route);}xredisClusterComplete(C,O);}
static void xredisClusterOperationStep(xredisclusterclient* C,xredisclusteroperation* O)
{
    xredisClusterHeapRemove(C,O);
    if(O->Stopping){xredisClusterComplete(C,O);return;}
    if(xredisClusterClosing(C)){xredisClusterStopOperation(C,O,C->Info.StopStatus);return;}
    if(xrtCancelRequested(O->Token)){xredisClusterStopOperation(C,O,XREDIS_CLUSTER_CLIENT_CANCELLED);return;}
    if(xrtTimer()>=O->Deadline){xredisClusterStopOperation(C,O,XREDIS_CLUSTER_CLIENT_TIMEOUT);return;}
    if(O->Watch.Future){if(xredisClusterWatchActive(&O->Watch) && xrtFutureState(O->Watch.Future)==XFUTURE_PENDING){xredisClusterHeapSet(C,O,O->Deadline);return;}if(!xredisClusterWatchStop(&O->Watch))return;}
    if(!O->Started)O->Started=xrtTimer();
    if(!O->Route || O->WaitingMap){
        if(!C->Map){xredisClusterMapWait(C,O);return;}
        xredisclustermapinfo M={0};M.Size=sizeof(M);xredisClusterMapInfo(C->Map,&M);
        if(O->WaitingMap && O->Route && M.Generation<=O->Result.Generation){xredisClusterMapWait(C,O);return;}
        xredisclusterrouteconfig K=xredisClusterRouteConfigFor(C,O,M.Origin.Tls);xredisclusterroute* R=NULL;
        xredisclusterroutestatus Failure=XREDIS_CLUSTER_ROUTE_MEMORY;
        xredisadmission A=O->Route?__xredisClusterRouteRetryMissing(O->Route,M.Topology,M.Origin.Host,&K,&R,&Failure):__xredisClusterRouteCreate(M.Topology,M.Origin.Host,O->Slot,O->Arguments,O->Count,&K,&R,O->Entered,&Failure);
        if(A!=XREDIS_ADMISSION_ACCEPTED){xredisClusterStopOperation(C,O,Failure==XREDIS_CLUSTER_ROUTE_LIMIT?XREDIS_CLUSTER_CLIENT_LIMIT:XREDIS_CLUSTER_CLIENT_MEMORY);return;}
        xredisClusterMapWaitRemove(C,O);
        if(O->Route==(xredisclusterroute*)((unsigned char*)O+XREDIS_CLUSTER_OPERATION_PREFIX))__xredisClusterRouteUnit(O->Route);else xredisClusterRouteDestroy(O->Route);
        O->Route=R;O->Result.Generation=M.Generation;xrtFree(O->Arguments);O->Arguments=NULL;
    }
    xredisclusterroutestep S=xredisClusterRouteStep(O->Route);xredisclusterrouteresult R={0};R.Size=sizeof(R);xredisClusterRouteResult(O->Route,&R);
    if(R.Moved>O->Moved){xrtMutexLock(&C->Gate);C->Info.Moved+=R.Moved-O->Moved;xrtMutexUnlock(&C->Gate);O->Moved=R.Moved;xredisClusterInvalidateGeneration(C,O->Result.Generation);}
    if(xredisClusterClosing(C)){xredisClusterStopOperation(C,O,C->Info.StopStatus);return;}
    if(xrtTimer()>=O->Deadline && (S!=XREDIS_CLUSTER_ROUTE_DONE || R.Status==XREDIS_CLUSTER_ROUTE_COMPLETE)){xredisClusterStopOperation(C,O,XREDIS_CLUSTER_CLIENT_TIMEOUT);return;}
    if(S==XREDIS_CLUSTER_ROUTE_DONE){
        if(R.Status==XREDIS_CLUSTER_ROUTE_NO_ROUTE && !O->MissingTried){O->MissingTried=true;xredisClusterInvalidateGeneration(C,O->Result.Generation);xredisClusterMapWait(C,O);return;}
        O->Stopping=true;O->Result.Status=R.Status==XREDIS_CLUSTER_ROUTE_TIMEOUT?XREDIS_CLUSTER_CLIENT_TIMEOUT:XREDIS_CLUSTER_CLIENT_COMPLETE;xredisClusterComplete(C,O);return;
    }
    if(S==XREDIS_CLUSTER_ROUTE_STEP_ERROR){xredisClusterStopOperation(C,O,XREDIS_CLUSTER_CLIENT_INTERNAL);return;}
    double Due=xredisClusterRouteNextWake(O->Route);xredisClusterHeapSet(C,O,Due);
    if(S==XREDIS_CLUSTER_ROUTE_MORE){xrtMutexLock(&C->Gate);xredisClusterQueueLocked(C,O);xrtMutexUnlock(&C->Gate);}
    else {xfuture* F=xredisClusterRoutePending(O->Route);if(F && !xredisClusterWatchStart(&O->Watch,F))xredisClusterStopOperation(C,O,XREDIS_CLUSTER_CLIENT_INTERNAL);}
}
static void xredisClusterValueFree(ptr V,ptr Data){(void)Data;xrtFree(V);}
static void xredisClusterReady(xredisclusterclient* C,xredisclusterclientstatus S,uint64 G,uint64 E)
{
    if(!C->ReadyPromise)return;
    xpromise* P=C->ReadyPromise;C->ReadyPromise=NULL;xredisclusterclientready* V=C->ReadyValue;C->ReadyValue=NULL;*V=(xredisclusterclientready){S,G,E};
    if(!xrtPromiseResolveOwned(P,V,xredisClusterValueFree,NULL))xrtFree(V);
    xrtPromiseDestroy(P);
}
static void xredisClusterDriveRefresh(xredisclusterclient* C)
{
    xrtMutexLock(&C->Gate);bool Invalidate=C->Invalidation;C->Invalidation=false;xrtMutexUnlock(&C->Gate);
    if(Invalidate)xredisClusterRefreshInvalidate(C->Refresh);
    xredisclusterrefreshinfo I={0};I.Size=sizeof(I);xredisClusterRefreshInfo(C->Refresh,&I);
    if(!C->RefreshFuture && I.Dirty && xrtTimer()>=C->RefreshDue){
        if(xredisClusterRefreshStart(C->Refresh,false,&C->RefreshFuture)!=XREDIS_ADMISSION_ACCEPTED){xredisClusterStop(C,XREDIS_CLUSTER_CLIENT_MEMORY);return;}
        xrtMutexLock(&C->Gate);++C->Info.Refreshes;xrtMutexUnlock(&C->Gate);
    }
    if(!C->RefreshFuture)return;
    if(C->Metadata.Future){if(xredisClusterWatchActive(&C->Metadata) && xrtFutureState(C->Metadata.Future)==XFUTURE_PENDING && xrtTimer()<xredisClusterRefreshNextWake(C->Refresh))return;if(!xredisClusterWatchStop(&C->Metadata))return;}
    xredisclusterrefreshstep S=xredisClusterRefreshStep(C->Refresh);
    if(S==XREDIS_REFRESH_DONE){
        const xredisclusterrefreshresult* R=xrtFutureValue(C->RefreshFuture);if(!R){xredisClusterStop(C,XREDIS_CLUSTER_CLIENT_INTERNAL);return;}
        xredisclustermap* Map=NULL;bool Dirty=true;if(R->Published)xredisClusterRefreshSnapshot(C->Refresh,&Map,&Dirty);
        if(Map)(void)xredisClusterRefreshDiscover(C->Refresh,Map);
        xredisclustermap* Old=NULL;xrtMutexLock(&C->Gate);C->Info.LastRefresh=R->Status;++C->Info.RefreshCompleted;
        if(Map){Old=C->Map;C->Map=Map;C->Info.Generation=R->Generation;C->Info.Epoch=R->Epoch;C->Info.Dirty=Dirty || C->Invalidation;C->Backoff=C->Config.RefreshMin;}
        else {++C->Info.RefreshFailures;C->Info.Dirty=true;C->Backoff=C->Backoff>C->Config.RefreshRetryMax/2?C->Config.RefreshRetryMax:C->Backoff*2;}
        C->RefreshDue=xredisClusterDeadline(xrtTimer(),C->Backoff);C->Info.NextRefresh=C->Info.Dirty?C->RefreshDue:0;xrtMutexUnlock(&C->Gate);xredisClusterMapRelease(Old);
        if(Map){xredisClusterReady(C,XREDIS_CLUSTER_CLIENT_COMPLETE,R->Generation,R->Epoch);xrtMutexLock(&C->Gate);for(xredisclusteroperation* O=C->MapHead;O;O=O->MapNext)xredisClusterQueueLocked(C,O);xrtMutexUnlock(&C->Gate);}
        xrtFutureDestroy(C->RefreshFuture);C->RefreshFuture=NULL;
    }else if(S==XREDIS_REFRESH_MORE)xredisClusterWake(C);
    else if(S==XREDIS_REFRESH_WAIT){xfuture* F=xredisClusterRefreshPending(C->Refresh);if(F && !xredisClusterWatchStart(&C->Metadata,F))xredisClusterStop(C,XREDIS_CLUSTER_CLIENT_INTERNAL);}
    else if(S==XREDIS_REFRESH_STEP_ERROR || S==XREDIS_REFRESH_BUSY)xredisClusterStop(C,XREDIS_CLUSTER_CLIENT_INTERNAL);
}
static void xredisClusterTimer(xnetworker* W,uint64 Id,xnetresult R,ptr Data)
{(void)W;xredisclusterclient* C=Data;if(C->Timer==Id)C->Timer=0;if(R!=XNET_RESULT_OK && R!=XNET_RESULT_CANCELLED)xredisClusterStop(C,XREDIS_CLUSTER_CLIENT_RESOURCE);xredisClusterWake(C);xredisClusterClientRelease(C);}
static bool xredisClusterArm(xredisclusterclient* C,double Due)
{
    if(C->Timer && C->TimerDue==Due)return true;
    if(C->Timer)xrtNetEngineTimerCancelCurrent(C->Engine,C->Timer);
    if(C->Timer)return false;
    if(Due==INFINITY)return true;
    double Ms=ceil(fmax(0,Due-xrtTimer())*1000);int64 Delay=Ms>=86400000?86400000:(int64)Ms;C->TimerDue=Due;xredisClusterClientRetain(C);
    C->Timer=xrtNetEngineAfter(C->Engine,xrtNetWorkerIndex(C->Worker),Delay,xredisClusterTimer,C);
    if(C->Timer)return true;
    xredisClusterClientRelease(C);return false;
}
static void xredisClusterFinish(xredisclusterclient* C)
{
    if(C->Timer || !xredisClusterWatchStop(&C->Metadata))return;
    xrtMutexLock(&C->Gate);if(C->Info.Requests || C->Info.APIs || C->Posted){xrtMutexUnlock(&C->Gate);return;}C->Finished=true;C->Info.Closed=true;xredisclustermap* M=C->Map;C->Map=NULL;xrtMutexUnlock(&C->Gate);
    xredisClusterReady(C,C->Info.StopStatus,0,0);xrtFutureDestroy(C->RefreshFuture);C->RefreshFuture=NULL;xredisClusterRefreshRelease(C->Refresh);C->Refresh=NULL;xredisClusterMapRelease(M);xredisClusterNodesRelease(C->Nodes);C->Nodes=NULL;
    xnetengine* E=C->Engine;C->Engine=NULL;C->Worker=NULL;xrtNetEngineUnpin(E);
    xpromise* P=C->ClosePromise;C->ClosePromise=NULL;xredisclusterclientready* V=C->CloseValue;C->CloseValue=NULL;*V=(xredisclusterclientready){C->Info.StopStatus,C->Info.Generation,C->Info.Epoch};
    if(!xrtPromiseResolveOwned(P,V,xredisClusterValueFree,NULL))xrtFree(V);
    xrtPromiseDestroy(P);xredisClusterClientRelease(C);
}
static void xredisClusterWork(xnetworker* W,ptr Data)
{
    (void)W;xredisclusterclient* C=Data;xrtMutexLock(&C->Gate);C->Posted=false;bool Closing=C->Info.Closing;xrtMutexUnlock(&C->Gate);
    if(Closing){xredisClusterRefreshClose(C->Refresh);(void)xredisClusterWatchStop(&C->Metadata);}else xredisClusterDriveRefresh(C);
    for(size_t N=0;N<C->Config.MaxSteps;++N){
        xrtMutexLock(&C->Gate);xredisclusteroperation* O=C->ReadyHead;if(O)xredisClusterUnqueueLocked(C,O);xrtMutexUnlock(&C->Gate);
        if(!O && C->HeapCount && C->Heap[0]->Due<=xrtTimer()){O=C->Heap[0];xredisClusterHeapRemove(C,O);}if(!O)break;xredisClusterOperationStep(C,O);
    }
    xrtMutexLock(&C->Gate);C->Info.HeapEntries=C->HeapCount;Closing=C->Info.Closing;bool Ready=C->ReadyHead!=NULL;xrtMutexUnlock(&C->Gate);
    if(Ready)xredisClusterWake(C);
    if(Closing){xredisClusterRefreshClose(C->Refresh);if(C->Timer)xrtNetEngineTimerCancelCurrent(C->Engine,C->Timer);xredisClusterFinish(C);}
    else {double Due=C->HeapCount?C->Heap[0]->Due:INFINITY;
        if(C->RefreshFuture)Due=fmin(Due,xredisClusterRefreshNextWake(C->Refresh));else {xrtMutexLock(&C->Gate);bool Dirty=C->Info.Dirty;xrtMutexUnlock(&C->Gate);if(Dirty)Due=fmin(Due,C->RefreshDue);}
        if(!xredisClusterArm(C,Due)){xredisClusterStop(C,XREDIS_CLUSTER_CLIENT_RESOURCE);}}
    xredisClusterClientRelease(C);
}
XREDIS_API bool xredisClusterClientConfigInit(xredisclusterclientconfig* K)
{
    if(!xrtMemRangeValid(K,sizeof(*K)))return xredisClusterClientError(XERR_ARGUMENT,"invalid Cluster client config");
    *K=(xredisclusterclientconfig){0};K->Size=sizeof(*K);K->MaxRequests=1024;K->MaxCommandBytes=16u*1024u*1024u;K->MaxStorageBytes=64u*1024u*1024u;K->MaxSteps=64;K->RefreshMin=100;K->RefreshRetryMax=10000;
    return xredisClusterRefreshConfigInit(&K->Refresh) && xredisClusterRouteConfigInit(&K->Route);
}
XREDIS_API xredisadmission xredisClusterClientCreate(xnetengine* E,uint64 Affinity,xredisclusternodes* Nodes,const xredisclusteraddress* Seeds,size_t Count,const xredisclusterclientconfig* Config,xredisclusterclient** Out)
{
    if(!E || !Nodes || !xrtMemRangeValid(Config,sizeof(*Config)) || !xrtMemRangeValid(Out,sizeof(*Out)) || xrtMemRangesOverlap(Config,sizeof(*Config),Out,sizeof(*Out))){xredisClusterClientError(XERR_ARGUMENT,"invalid Cluster client create");return XREDIS_ADMISSION_ERROR;}
    xredisclusterclientconfig K=*Config;xredisclusterrouteconfig R=K.Route;R.GetPool=xredisClusterProvider;
    if(K.Size!=sizeof(K) || !K.MaxRequests || K.MaxRequests>65536 || !K.MaxCommandBytes || K.MaxCommandBytes>1024u*1024u*1024u || !K.MaxStorageBytes || K.MaxStorageBytes>1024u*1024u*1024u || !K.MaxSteps || K.MaxSteps>4096 || K.RefreshMin<=0 || K.RefreshRetryMax<K.RefreshMin || K.RefreshRetryMax>60000 || K.Route.GetPool || K.Route.Data || K.Route.Tls || !__xredisClusterRouteConfig(&R)){
        xredisClusterClientError(XERR_ARGUMENT,"invalid Cluster client bounds/profile");return XREDIS_ADMISSION_ERROR;}
    if(!Count || Count>1024 || !xrtMemRangeValid(Seeds,Count*sizeof(*Seeds)) || xrtMemRangesOverlap(Seeds,Count*sizeof(*Seeds),Out,sizeof(*Out))){xredisClusterClientError(XERR_ARGUMENT,"invalid Cluster client seed/output");return XREDIS_ADMISSION_ERROR;}
    for(size_t I=0;I<Count;++I)if(xrtMemRangesOverlap(Seeds[I].Host.Data,Seeds[I].Host.Size,Out,sizeof(*Out))){xredisClusterClientError(XERR_ARGUMENT,"Cluster client output aliases seed");return XREDIS_ADMISSION_ERROR;}
    if(!xrtNetEnginePin(E))return XREDIS_ADMISSION_ERROR;
    size_t Bytes=sizeof(xredisclusterclient)+K.MaxRequests*sizeof(xredisclusteroperation*);xredisclusterclient* C=xrtCalloc(1,Bytes);
    if(!C || !xrtMutexInit(&C->Gate)){xrtFree(C);xrtNetEngineUnpin(E);return XREDIS_ADMISSION_ERROR;}xrtAtomic32Init(&C->References,1);C->Bytes=Bytes;C->Heap=(xredisclusteroperation**)(C+1);C->Config=K;
    C->ReadyValue=xrtMalloc(sizeof(*C->ReadyValue));C->CloseValue=xrtMalloc(sizeof(*C->CloseValue));C->ReadyPromise=xrtPromiseCreate(&C->ReadyFuture,NULL);C->ClosePromise=xrtPromiseCreate(&C->ClosedFuture,NULL);
    if(!C->ReadyValue || !C->CloseValue || !C->ReadyPromise || !C->ClosePromise || !xrtNetPostInit(&C->Post) || xredisClusterRefreshCreate(Nodes,Seeds,Count,&K.Refresh,&C->Refresh)!=XREDIS_ADMISSION_ACCEPTED){xrtNetEngineUnpin(E);xredisClusterClientRelease(C);return XREDIS_ADMISSION_ERROR;}
    C->Engine=E;C->Worker=xrtNetEngineWorker(E,(uint32)(Affinity%xrtNetEngineWorkerCount(E)));C->Nodes=Nodes;xredisClusterNodesRetain(Nodes);C->Metadata.Client=C;C->Info.Size=sizeof(C->Info);C->Info.Dirty=true;C->Backoff=K.RefreshMin;
    xredisClusterClientRetain(C);xrtMutexLock(&C->Gate);*Out=C;xredisClusterWakeLocked(C);xrtMutexUnlock(&C->Gate);return XREDIS_ADMISSION_ACCEPTED;
}
static bool xredisClusterClientOutput(xredisclusterclient* C,const void* P,size_t N)
{return C && xrtMemRangeValid(P,N) && !xrtMemRangesOverlap(C,C->Bytes,P,N);}
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND)
static xredisadmission xredisClusterClientCallAt(xredisclusterclient* C,uint16 Slot,const xbytesview* A,size_t Count,int64 Timeout,xfuture** Out,double E)
#else
XREDIS_API xredisadmission xredisClusterClientCall(xredisclusterclient* C,uint16 Slot,const xbytesview* A,size_t Count,int64 Timeout,xfuture** Out)
#endif
{
#if !defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND)
    double E=xrtTimer();
#endif
    if(!xredisClusterClientOutput(C,Out,sizeof(*Out)) || Slot>=XREDIS_CLUSTER_SLOTS || Timeout< -1 || Timeout>86400000 || !Count || Count>C->Config.Route.MaxArguments || !xrtMemRangeValid(A,Count*sizeof(*A)) || xrtMemRangesOverlap(A,Count*sizeof(*A),Out,sizeof(*Out))){xredisClusterClientError(XERR_ARGUMENT,"invalid Cluster call");return XREDIS_ADMISSION_ERROR;}
    if(Timeout==-1)Timeout=C->Config.Route.Timeout;
    size_t Wire=0,Payload=0;if(!xredisCommandSize(A,Count,&Wire) || Wire>C->Config.Route.MaxCommandBytes){xredisClusterClientError(XERR_RANGE,"Cluster call command limit");return XREDIS_ADMISSION_ERROR;}
    for(size_t I=0;I<Count;++I){if(xrtMemRangesOverlap(A[I].Data,A[I].Size,Out,sizeof(*Out))){xredisClusterClientError(XERR_ARGUMENT,"Cluster output aliases command");return XREDIS_ADMISSION_ERROR;}Payload+=A[I].Size;}
    size_t Inputs=Count*sizeof(*A)+Payload,Reservation=XREDIS_CLUSTER_OPERATION_PREFIX+2*(__xredisClusterRouteBaseBytes()+Inputs+4*C->Config.Route.MaxHostBytes);
    xrtMutexLock(&C->Gate);if(!__xredisClusterMapOutput(C->Map,Out,sizeof(*Out))){xrtMutexUnlock(&C->Gate);xredisClusterClientError(XERR_ARGUMENT,"Cluster output aliases published map");return XREDIS_ADMISSION_ERROR;}if(C->Info.Closing){xrtMutexUnlock(&C->Gate);return XREDIS_ADMISSION_CLOSED;}
    if(C->Info.Requests>=C->Config.MaxRequests || Wire>C->Config.MaxCommandBytes-C->Info.CommandBytes || Reservation>C->Config.MaxStorageBytes-C->Info.StorageBytes){++C->Info.Rejected;xrtMutexUnlock(&C->Gate);return XREDIS_ADMISSION_OVERLOADED;}
    ++C->Info.Requests;++C->Info.APIs;C->Info.CommandBytes+=Wire;C->Info.StorageBytes+=Reservation;xredisclustermap* Map=C->Map;if(Map)xredisClusterMapRetain(Map);xrtMutexUnlock(&C->Gate);
    xredisclusteroperation* O=NULL;xredisclusterroute* Route=NULL;uint64 Generation=0;
    if(Map){xredisclustermapinfo M={0};M.Size=sizeof(M);xredisClusterMapInfo(Map,&M);xredisclusterrouteconfig K=xredisClusterRouteConfigFor(C,NULL,M.Origin.Tls);K.Timeout=Timeout;ptr Context=NULL;
        if(__xredisClusterRouteCreateContext(M.Topology,M.Origin.Host,Slot,A,Count,&K,XREDIS_CLUSTER_OPERATION_PREFIX,&Context,&Route,E)==XREDIS_ADMISSION_ACCEPTED){O=Context;Generation=M.Generation;}}
    else O=xrtCalloc(1,sizeof(*O));
    bool OK=O!=NULL;
    if(O){O->Client=C;O->Watch.Client=C;O->Watch.Operation=O;O->Heap=SIZE_MAX;O->Entered=E;O->Deadline=xredisClusterDeadline(E,Timeout);O->Slot=Slot;O->Timeout=Timeout;O->Count=Count;O->Wire=Wire;O->Reservation=Reservation;O->Result.Size=sizeof(O->Result);
        O->Route=Route;O->Result.Generation=Generation;
        O->Promise=xrtPromiseCreate(&O->Future,NULL);if(O->Promise)O->Token=xrtPromiseCancelToken(O->Promise);if(O->Token)O->Cancel=xrtCancelWatch(O->Token,xredisClusterCancelled,O);OK=O->Promise && O->Token && O->Cancel;
        if(OK && !Map){O->Arguments=xrtMalloc(Inputs);OK=O->Arguments!=NULL;if(OK){unsigned char* P=(unsigned char*)(O->Arguments+Count);for(size_t I=0;I<Count;++I){O->Arguments[I]=(xbytesview){P,A[I].Size};if(A[I].Size)memcpy(P,A[I].Data,A[I].Size);P+=A[I].Size;}}}
    }
    xredisClusterMapRelease(Map);xrtMutexLock(&C->Gate);--C->Info.APIs;bool Closing=C->Info.Closing;
    if(OK && !Closing){O->Next=C->All;if(C->All)C->All->Previous=O;C->All=O;++C->Info.Accepted;*Out=xrtFutureRef(O->Future);xredisClusterQueueLocked(C,O);xrtMutexUnlock(&C->Gate);return XREDIS_ADMISSION_ACCEPTED;}
    --C->Info.Requests;C->Info.CommandBytes-=Wire;C->Info.StorageBytes-=Reservation;if(Closing)xredisClusterWakeLocked(C);xrtMutexUnlock(&C->Gate);
    if(O){xrtCancelUnwatch(O->Cancel);xrtCancelDestroy(O->Token);xrtPromiseDestroy(O->Promise);xrtFutureDestroy(O->Future);xredisClusterOperationFree(O,NULL);}return Closing?XREDIS_ADMISSION_CLOSED:XREDIS_ADMISSION_ERROR;
}
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND)
XREDIS_API xredisadmission xredisClusterClientCall(xredisclusterclient* C,uint16 Slot,const xbytesview* A,size_t Count,int64 Timeout,xfuture** Out)
{return xredisClusterClientCallAt(C,Slot,A,Count,Timeout,Out,xrtTimer());}
XREDIS_API xredisclustercommandadmission xredisClusterClientCallCommand(xredisclusterclient* C,const xrediscommandcatalog* Catalogue,const xbytesview* A,size_t Count,uint16 KeylessSlot,int64 Timeout,xfuture** Out)
{
    double Entered=xrtTimer();
    if(!xredisClusterClientOutput(C,Out,sizeof(*Out)) || !__xredisCommandCatalogOutput(Catalogue,Out,sizeof(*Out)) ||
        (KeylessSlot>=XREDIS_CLUSTER_SLOTS && KeylessSlot!=XREDIS_CLUSTER_COMMAND_NO_SLOT) || Timeout< -1 || Timeout>86400000){xredisClusterClientError(XERR_ARGUMENT,"invalid metadata-routed Cluster call");return XREDIS_CLUSTER_COMMAND_ERROR;}
    if(Count>C->Config.Route.MaxArguments)return XREDIS_CLUSTER_COMMAND_LIMIT;
    if(!xrtMemRangeValid(A,Count*sizeof(*A)) || xrtMemRangesOverlap(A,Count*sizeof(*A),Out,sizeof(*Out))){xredisClusterClientError(XERR_ARGUMENT,"Cluster Future aliases argument descriptors");return XREDIS_CLUSTER_COMMAND_ERROR;}
    for(size_t I=0;I<Count;++I)if(xrtMemRangesOverlap(A[I].Data,A[I].Size,Out,sizeof(*Out))){xredisClusterClientError(XERR_ARGUMENT,"Cluster Future aliases argument data");return XREDIS_CLUSTER_COMMAND_ERROR;}
    xrediscommandkeyresult Keys={0};Keys.Size=sizeof(Keys);xrediskeysstatus Status=xredisCommandCatalogEvaluate(Catalogue,A,Count,NULL,0,&Keys);
    switch(Status){
    case XREDIS_KEYS_OK:break;
    case XREDIS_KEYS_NONE:if(KeylessSlot==XREDIS_CLUSTER_COMMAND_NO_SLOT)return XREDIS_CLUSTER_COMMAND_KEYLESS;break;
    case XREDIS_KEYS_UNKNOWN:return XREDIS_CLUSTER_COMMAND_UNKNOWN;
    case XREDIS_KEYS_UNSUPPORTED:return XREDIS_CLUSTER_COMMAND_UNSUPPORTED;
    case XREDIS_KEYS_CROSS_SLOT:return XREDIS_CLUSTER_COMMAND_CROSS_SLOT;
    case XREDIS_KEYS_ARGUMENTS:return XREDIS_CLUSTER_COMMAND_ARGUMENTS;
    case XREDIS_KEYS_LIMIT:return XREDIS_CLUSTER_COMMAND_LIMIT;
    default:return XREDIS_CLUSTER_COMMAND_ERROR;
    }
    xredisadmission Admission=xredisClusterClientCallAt(C,Keys.HasSlot?Keys.Slot:KeylessSlot,A,Count,Timeout,Out,Entered);
    switch(Admission){case XREDIS_ADMISSION_ACCEPTED:return XREDIS_CLUSTER_COMMAND_ACCEPTED;case XREDIS_ADMISSION_OVERLOADED:return XREDIS_CLUSTER_COMMAND_OVERLOADED;case XREDIS_ADMISSION_CLOSED:return XREDIS_CLUSTER_COMMAND_CLOSED;default:return XREDIS_CLUSTER_COMMAND_ERROR;}
}
#endif
XREDIS_API xfuture* xredisClusterClientReady(xredisclusterclient* C)
{if(!C){xredisClusterClientError(XERR_ARGUMENT,"Cluster client is NULL");return NULL;}return xrtFutureRef(C->ReadyFuture);}
XREDIS_API xfuture* xredisClusterClientClosed(xredisclusterclient* C)
{if(!C){xredisClusterClientError(XERR_ARGUMENT,"Cluster client is NULL");return NULL;}return xrtFutureRef(C->ClosedFuture);}
XREDIS_API bool xredisClusterClientInfo(xredisclusterclient* C,xredisclusterclientinfo* Out)
{if(!xredisClusterClientOutput(C,Out,sizeof(*Out)))return xredisClusterClientError(XERR_ARGUMENT,"invalid Cluster client info");xrtMutexLock(&C->Gate);if(!__xredisClusterMapOutput(C->Map,Out,sizeof(*Out)) || Out->Size!=sizeof(*Out)){xrtMutexUnlock(&C->Gate);return xredisClusterClientError(XERR_ARGUMENT,"invalid Cluster client info output");}*Out=C->Info;xrtMutexUnlock(&C->Gate);return true;}
XREDIS_API xredisclusterstatus xredisClusterClientSnapshot(xredisclusterclient* C,xredisclustermap** Out,bool* Stale)
{
    if(!xredisClusterClientOutput(C,Out,sizeof(*Out)) || !xredisClusterClientOutput(C,Stale,sizeof(*Stale)) || xrtMemRangesOverlap(Out,sizeof(*Out),Stale,sizeof(*Stale)))return xredisClusterClientError(XERR_ARGUMENT,"invalid Cluster client snapshot"),XREDIS_CLUSTER_ERROR;
    xrtMutexLock(&C->Gate);if(!__xredisClusterMapOutput(C->Map,Out,sizeof(*Out)) || !__xredisClusterMapOutput(C->Map,Stale,sizeof(*Stale))){xrtMutexUnlock(&C->Gate);xredisClusterClientError(XERR_ARGUMENT,"Cluster snapshot output aliases map");return XREDIS_CLUSTER_ERROR;}if(!C->Map){xrtMutexUnlock(&C->Gate);return XREDIS_CLUSTER_ABSENT;}xredisClusterMapRetain(C->Map);*Out=C->Map;*Stale=C->Info.Dirty;xrtMutexUnlock(&C->Gate);return XREDIS_CLUSTER_VALUE;
}
XREDIS_API bool xredisClusterClientInvalidate(xredisclusterclient* C)
{if(!C)return xredisClusterClientError(XERR_ARGUMENT,"Cluster client is NULL");xrtMutexLock(&C->Gate);if(C->Info.Closing){xrtMutexUnlock(&C->Gate);return false;}C->Invalidation=true;C->Info.Dirty=true;xredisClusterWakeLocked(C);xrtMutexUnlock(&C->Gate);return true;}
static void xredisClusterStop(xredisclusterclient* C,xredisclusterclientstatus Status)
{xrtMutexLock(&C->Gate);if(!C->Info.Closing){C->Info.Closing=true;C->Info.StopStatus=Status;}for(xredisclusteroperation* O=C->All;O;O=O->Next)xredisClusterQueueLocked(C,O);xredisClusterWakeLocked(C);xrtMutexUnlock(&C->Gate);}
XREDIS_API bool xredisClusterClientClose(xredisclusterclient* C)
{if(!C)return xredisClusterClientError(XERR_ARGUMENT,"Cluster client is NULL");xredisClusterStop(C,XREDIS_CLUSTER_CLIENT_CLOSED);return true;}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/command_keys/command_keys.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_COMMAND_KEYS)
#include <stdlib.h>
#include <string.h>
#if defined(XREDIS_FEATURE_XREDIS_COMMAND_KEYS)
enum {XRK_INDEX=1,XRK_KEYWORD,XRK_RANGE,XRK_KEYNUM};
typedef struct xrediskeyspec {
    xbytesview Keyword;int64 Index,Last,Step,Limit,Number,First;
    uint32 Flags;unsigned Begin,Find;bool Unsupported;
} xrediskeyspec;
typedef struct xrediskeycommand {
    xbytesview Name;int64 Arity,First,Last,Step;size_t Spec,Count;
    bool Movable,Modern,Children,Unsupported;
} xrediskeycommand;
struct xrediscommandcatalog {
    xatomic32 References;xredisreply* Reply;xrediscommandkeylimits Limits;
    xrediscommandcataloginfo Info;size_t Buckets;
    xrediskeycommand* Commands;xrediskeyspec* Specs;uint32* Hash;
};
typedef struct xrediskeysbuild {
    xredisreply* Reply;const xrediscommandkeylimits* Limits;xrediscommandcatalog* Out;
    size_t Commands,Specs,Depth;
} xrediskeysbuild;
static bool xredisKeysError(cstr Text)
{xrtSetErrorInfo(XERR_ARGUMENT,"xrt.redis",XREDIS_REPLY_ARGUMENT,Text);return false;}
static unsigned xredisKeysLower(unsigned C){return C>='A' && C<='Z'?C+32:C;}
static bool xredisKeysEqual(xbytesview A,xbytesview B)
{if(A.Size!=B.Size)return false;for(size_t I=0;I<A.Size;++I)if(xredisKeysLower(A.Data[I])!=xredisKeysLower(B.Data[I]))return false;return true;}
static bool xredisKeysWord(xbytesview A,cstr B)
{return xredisKeysEqual(A,(xbytesview){(cbytes)B,strlen(B)});}
static bool xredisKeysText(xredisreplyvalue V)
{return (V.Type==XREDIS_REPLY_SIMPLE_STRING || V.Type==XREDIS_REPLY_BULK_STRING) && !(V.Flags&XREDIS_REPLY_NULL_VALUE);}
static bool xredisKeysArray(xredisreplyvalue V)
{return V.Type==XREDIS_REPLY_ARRAY && !(V.Flags&XREDIS_REPLY_NULL_VALUE);}
static bool xredisKeysSequence(xredisreplyvalue V)
{return (xredisKeysArray(V) || V.Type==XREDIS_REPLY_SET) && !(V.Flags&XREDIS_REPLY_NULL_VALUE);}
static bool xredisKeysNull(xredisreplyvalue V)
{return V.Type==XREDIS_REPLY_NULL || (V.Type==XREDIS_REPLY_BULK_STRING && (V.Flags&XREDIS_REPLY_NULL_VALUE));}
static xredisreplyvalue xredisKeysGet(const xredisreply* R,size_t Index)
{xredisreplyvalue V={0};xredisReplyGet(R,Index,&V);return V;}
static bool xredisKeysFields(const xredisreply* R,xredisreplyvalue V,const cstr* Names,size_t Count,size_t* Fields)
{
    if((V.Type!=XREDIS_REPLY_MAP && !xredisKeysArray(V)) || V.ChildCount%2)return false;
    for(size_t I=0;I<Count;++I)Fields[I]=XREDIS_REPLY_NONE;
    for(size_t I=V.FirstChild;I!=XREDIS_REPLY_NONE;){xredisreplyvalue K=xredisKeysGet(R,I),D=xredisKeysGet(R,K.Next);if(!xredisKeysText(K))return false;
        for(size_t J=0;J<Count;++J)if(K.Data.Size==strlen(Names[J]) && !memcmp(K.Data.Data,Names[J],K.Data.Size)){if(Fields[J]!=XREDIS_REPLY_NONE)return false;Fields[J]=K.Next;break;}
        I=D.Next;}
    return true;
}
static bool xredisKeysInteger(const xredisreply* R,size_t Index,int64* N)
{if(Index==XREDIS_REPLY_NONE)return false;xredisreplyvalue V=xredisKeysGet(R,Index);if(V.Type!=XREDIS_REPLY_INTEGER)return false;*N=V.Integer;return true;}
static bool xredisKeysFlags(const xredisreply* R,xredisreplyvalue V,xrediskeyspec* S,xrediskeycommand* C)
{
    if(!xredisKeysSequence(V))return false;
    static const cstr Words[]={"RO","RW","OW","RM","access","update","insert","delete","not_key","variable_flags"};
    for(size_t I=V.FirstChild;I!=XREDIS_REPLY_NONE;){xredisreplyvalue W=xredisKeysGet(R,I);if(!xredisKeysText(W))return false;
        if(C){if(xredisKeysWord(W.Data,"movablekeys"))C->Movable=true;}
        else {size_t J=0;for(;J<sizeof(Words)/sizeof(*Words);++J)if(xredisKeysWord(W.Data,Words[J])){S->Flags|=1u<<J;break;}
            if(J==sizeof(Words)/sizeof(*Words))S->Unsupported=true;}
        I=W.Next;
    }
    return true;
}
static bool xredisKeysRule(const xredisreply* R,size_t Index,bool Begin,xrediskeyspec* S)
{
    static const cstr Names[]={"type","spec"};size_t F[2];if(!xredisKeysFields(R,xredisKeysGet(R,Index),Names,2,F) || F[0]==XREDIS_REPLY_NONE || F[1]==XREDIS_REPLY_NONE)return false;
    xredisreplyvalue Type=xredisKeysGet(R,F[0]),Spec=xredisKeysGet(R,F[1]);if(!xredisKeysText(Type))return false;
    if(Begin && xredisKeysWord(Type.Data,"index")){static const cstr N[]={"index"};size_t A[1];S->Begin=XRK_INDEX;
        return xredisKeysFields(R,Spec,N,1,A) && xredisKeysInteger(R,A[0],&S->Index) && S->Index>=0;}
    if(Begin && xredisKeysWord(Type.Data,"keyword")){static const cstr N[]={"keyword","startfrom"};size_t A[2];S->Begin=XRK_KEYWORD;
        if(!xredisKeysFields(R,Spec,N,2,A) || A[0]==XREDIS_REPLY_NONE || !xredisKeysInteger(R,A[1],&S->Index))return false;
        xredisreplyvalue K=xredisKeysGet(R,A[0]);if(!xredisKeysText(K) || !K.Data.Size)return false;S->Keyword=K.Data;return true;}
    if(!Begin && xredisKeysWord(Type.Data,"range")){static const cstr N[]={"lastkey","keystep","limit"};size_t A[3];S->Find=XRK_RANGE;
        return xredisKeysFields(R,Spec,N,3,A) && xredisKeysInteger(R,A[0],&S->Last) && xredisKeysInteger(R,A[1],&S->Step) && xredisKeysInteger(R,A[2],&S->Limit) && S->Step>0 && S->Limit>=0;}
    if(!Begin && xredisKeysWord(Type.Data,"keynum")){static const cstr N[]={"keynumidx","firstkey","keystep"};size_t A[3];S->Find=XRK_KEYNUM;
        return xredisKeysFields(R,Spec,N,3,A) && xredisKeysInteger(R,A[0],&S->Number) && xredisKeysInteger(R,A[1],&S->First) && xredisKeysInteger(R,A[2],&S->Step) && S->Number>=0 && S->First>=0 && S->Step>0;}
    S->Unsupported=true;return true; /* An unfamiliar rule's spec stays opaque. */
}
static xrediskeysstatus xredisKeysSpecs(xrediskeysbuild* B,xredisreplyvalue V,xrediskeycommand* C)
{
    if(!xredisKeysSequence(V))return XREDIS_KEYS_MALFORMED;
    if(V.ChildCount>B->Limits->MaxSpecifications-B->Specs)return XREDIS_KEYS_LIMIT;
    C->Spec=B->Specs;C->Count=V.ChildCount;static const cstr N[]={"flags","begin_search","find_keys","notes"};
    for(size_t I=V.FirstChild;I!=XREDIS_REPLY_NONE;){xredisreplyvalue X=xredisKeysGet(B->Reply,I);size_t F[4];xrediskeyspec S={0};
        if(!xredisKeysFields(B->Reply,X,N,4,F) || F[0]==XREDIS_REPLY_NONE || F[1]==XREDIS_REPLY_NONE || F[2]==XREDIS_REPLY_NONE ||
            !xredisKeysFlags(B->Reply,xredisKeysGet(B->Reply,F[0]),&S,NULL) || !xredisKeysRule(B->Reply,F[1],true,&S) || !xredisKeysRule(B->Reply,F[2],false,&S) ||
            (F[3]!=XREDIS_REPLY_NONE && !xredisKeysText(xredisKeysGet(B->Reply,F[3]))))return XREDIS_KEYS_MALFORMED;
        C->Unsupported|=S.Unsupported;if(B->Out)B->Out->Specs[B->Specs]=S;++B->Specs;I=X.Next;
    }
    return XREDIS_KEYS_OK;
}
static xrediskeysstatus xredisKeysCommands(xrediskeysbuild* B,xredisreplyvalue V,size_t Level,xbytesview Parent)
{
    if(!xredisKeysSequence(V))return XREDIS_KEYS_MALFORMED;
    if(Level>B->Limits->MaxDepth && V.ChildCount)return XREDIS_KEYS_LIMIT;
    for(size_t I=V.FirstChild;I!=XREDIS_REPLY_NONE;){xredisreplyvalue X=xredisKeysGet(B->Reply,I);I=X.Next;if(xredisKeysNull(X))continue;
        if(!xredisKeysArray(X) || X.ChildCount<6)return XREDIS_KEYS_MALFORMED;
        if(B->Commands>=B->Limits->MaxCommands)return XREDIS_KEYS_LIMIT;
        size_t F[10],J=0;for(size_t K=X.FirstChild;K!=XREDIS_REPLY_NONE && J<10;){F[J++]=K;K=xredisKeysGet(B->Reply,K).Next;}
        xredisreplyvalue Name=xredisKeysGet(B->Reply,F[0]);if(!xredisKeysText(Name) || !Name.Data.Size)return XREDIS_KEYS_MALFORMED;
        if(Name.Data.Size>B->Limits->MaxNameBytes)return XREDIS_KEYS_LIMIT;
        size_t Depth=1;bool Part=false;for(size_t K=0;K<Name.Data.Size;++K){unsigned Ch=Name.Data.Data[K];if(Ch=='|'){if(!Part)return XREDIS_KEYS_MALFORMED;Part=false;++Depth;}else {if(Ch<=32 || Ch==127)return XREDIS_KEYS_MALFORMED;Part=true;}}
        if(!Part)return XREDIS_KEYS_MALFORMED;
        if(Depth>B->Limits->MaxDepth)return XREDIS_KEYS_LIMIT;
        if(Depth>B->Depth)B->Depth=Depth;
        if(Parent.Size){if(Name.Data.Size<=Parent.Size+1 || Name.Data.Data[Parent.Size]!='|' || !xredisKeysEqual((xbytesview){Name.Data.Data,Parent.Size},Parent) || memchr(Name.Data.Data+Parent.Size+1,'|',Name.Data.Size-Parent.Size-1))return XREDIS_KEYS_MALFORMED;}
        xrediskeycommand C={0};C.Name=Name.Data;
        if(!xredisKeysInteger(B->Reply,F[1],&C.Arity) || !C.Arity || C.Arity==INT64_MIN || !xredisKeysFlags(B->Reply,xredisKeysGet(B->Reply,F[2]),NULL,&C) ||
            !xredisKeysInteger(B->Reply,F[3],&C.First) || !xredisKeysInteger(B->Reply,F[4],&C.Last) || !xredisKeysInteger(B->Reply,F[5],&C.Step) ||
            C.First<0 || (C.First && C.Step<=0) || (!C.First && (C.Last || C.Step)))return XREDIS_KEYS_MALFORMED;
        size_t At=B->Commands++;if(J>8){C.Modern=true;xrediskeysstatus S=xredisKeysSpecs(B,xredisKeysGet(B->Reply,F[8]),&C);if(S!=XREDIS_KEYS_OK)return S;}
        if(J>9){xredisreplyvalue Children=xredisKeysGet(B->Reply,F[9]);if(!xredisKeysSequence(Children))return XREDIS_KEYS_MALFORMED;C.Children=Children.ChildCount!=0;
            xrediskeysstatus S=xredisKeysCommands(B,Children,Level+1,C.Name);if(S!=XREDIS_KEYS_OK)return S;}
        if(B->Out)B->Out->Commands[At]=C;
    }
    return XREDIS_KEYS_OK;
}
static uint64 xredisKeysHash(uint64 H,xbytesview V)
{for(size_t I=0;I<V.Size;++I){H^=xredisKeysLower(V.Data[I]);H*=1099511628211ull;}return H;}
static bool xredisKeysCharge(size_t* Used,size_t N,size_t Limit)
{if(N>Limit-*Used)return false;*Used+=N;return true;}
static bool xredisKeysOutput(const xrediscommandcatalog* C,const void* P,size_t N)
{return C && __xredisClusterReplyOutput(C->Reply,P,N) && !xrtMemRangesOverlap(C,C->Info.StorageBytes,P,N);}
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND)
bool __xredisCommandCatalogOutput(const xrediscommandcatalog* C,const void* P,size_t N)
{return xredisKeysOutput(C,P,N);}
#endif
XREDIS_API bool xredisCommandKeyLimitsInit(xrediscommandkeylimits* K)
{
    if(!xrtMemRangeValid(K,sizeof(*K)))return xredisKeysError("invalid command key limits output");
    *K=(xrediscommandkeylimits){sizeof(*K),8192,32768,256,16,1048576,16u*1024u*1024u,64u*1024u*1024u,65536,64u*1024u*1024u,65536,64u*1024u*1024u,1048576};return true;
}
XREDIS_API xrediskeysstatus xredisCommandCatalogCreate(xredisreply* R,const xrediscommandkeylimits* Config,xrediscommandcatalog** Out)
{
    if(!__xredisClusterReplyOutput(R,Out,sizeof(*Out)) || (Config && (!__xredisClusterReplyOutput(R,Config,sizeof(*Config)) || Config->Size!=sizeof(*Config) || xrtMemRangesOverlap(Config,sizeof(*Config),Out,sizeof(*Out)))))return xredisKeysError("invalid command catalogue arguments"),XREDIS_KEYS_ERROR;
    xrediscommandkeylimits K;if(Config)K=*Config;else xredisCommandKeyLimitsInit(&K);
    if(K.MaxCommands>65535 || K.MaxSpecifications>262144 || K.MaxNameBytes>4096 || K.MaxDepth>16 || K.MaxArguments>1048576 || K.MaxKeys>1048576 || K.MaxOwnedBytes>1073741824 || K.MaxPayloadBytes>1073741824 || K.MaxArgumentBytes>1073741824 || K.MaxKeyBytes>1073741824 || K.MaxSearchWork>1073741824)return xredisKeysError("invalid command key bounds"),XREDIS_KEYS_ERROR;
    xredisreplyinfo I={0};I.Size=sizeof(I);xredisReplyInfo(R,&I);if(I.Nodes>K.MaxReplyNodes || I.PayloadBytes>K.MaxPayloadBytes)return XREDIS_KEYS_LIMIT;
    xredisreplyvalue Root=xredisKeysGet(R,I.Root);if(Root.Type==XREDIS_REPLY_SIMPLE_ERROR || Root.Type==XREDIS_REPLY_BULK_ERROR)return XREDIS_KEYS_SERVER_ERROR;
    xrediskeysbuild B={0};B.Reply=R;B.Limits=&K;xrediskeysstatus S=xredisKeysCommands(&B,Root,1,(xbytesview){0});if(S!=XREDIS_KEYS_OK)return S;
    size_t Buckets=2;while(Buckets<2*B.Commands)Buckets*=2;
    size_t Bytes=sizeof(xrediscommandcatalog)+B.Commands*sizeof(xrediskeycommand)+B.Specs*sizeof(xrediskeyspec)+Buckets*sizeof(uint32);
    if(Bytes>K.MaxOwnedBytes || I.ReservedBytes>K.MaxOwnedBytes-Bytes)return XREDIS_KEYS_LIMIT;
    xrediscommandcatalog* C=xrtCalloc(1,Bytes);if(!C)return XREDIS_KEYS_MEMORY;
    C->Commands=(xrediskeycommand*)(C+1);C->Specs=(xrediskeyspec*)(C->Commands+B.Commands);C->Hash=(uint32*)(C->Specs+B.Specs);C->Buckets=Buckets;C->Limits=K;
    C->Info=(xrediscommandcataloginfo){sizeof(C->Info),B.Commands,B.Specs,B.Depth,Bytes,I.ReservedBytes};
    xrediskeysbuild Fill={0};Fill.Reply=R;Fill.Limits=&K;Fill.Out=C;S=xredisKeysCommands(&Fill,Root,1,(xbytesview){0});
    size_t Work=0;
    if(S==XREDIS_KEYS_OK)for(size_t N=0;N<B.Commands;++N){xbytesview Name=C->Commands[N].Name;if(!xredisKeysCharge(&Work,Name.Size+1,K.MaxSearchWork)){S=XREDIS_KEYS_LIMIT;break;}size_t H=(size_t)xredisKeysHash(14695981039346656037ull,Name)&(Buckets-1);
        while(C->Hash[H]){if(!xredisKeysCharge(&Work,Name.Size+1,K.MaxSearchWork)){S=XREDIS_KEYS_LIMIT;break;}if(xredisKeysEqual(Name,C->Commands[C->Hash[H]-1].Name)){S=XREDIS_KEYS_MALFORMED;break;}H=(H+1)&(Buckets-1);}if(S!=XREDIS_KEYS_OK)break;C->Hash[H]=(uint32)N+1;}
    if(S!=XREDIS_KEYS_OK || !xredisReplyRetain(R)){xrtFree(C);return S!=XREDIS_KEYS_OK?S:XREDIS_KEYS_ERROR;}
    C->Reply=R;xrtAtomic32Init(&C->References,1);*Out=C;return XREDIS_KEYS_OK;
}
XREDIS_API bool xredisCommandCatalogRetain(xrediscommandcatalog* C)
{
    if(!C)return xredisKeysError("command catalogue is NULL");
    uint32 N=xrtAtomic32Load(&C->References,XMEMORY_RELAXED);
    for(;;){if(N==UINT32_MAX){xrtSetErrorInfo(XERR_RANGE,"xrt.redis",XREDIS_REPLY_LIMIT,"command catalogue reference overflow");return false;}if(xrtAtomic32CompareExchange(&C->References,&N,N+1,XMEMORY_RELAXED,XMEMORY_RELAXED))return true;}
}
XREDIS_API void xredisCommandCatalogRelease(xrediscommandcatalog* C)
{if(C && xrtAtomic32FetchSub(&C->References,1,XMEMORY_ACQ_REL)==1){xredisReplyRelease(C->Reply);xrtFree(C);}}
XREDIS_API bool xredisCommandCatalogInfo(const xrediscommandcatalog* C,xrediscommandcataloginfo* I)
{if(!xredisKeysOutput(C,I,sizeof(*I)) || I->Size!=sizeof(*I))return xredisKeysError("invalid command catalogue info output");*I=C->Info;return true;}
typedef struct xrediskeyseval {
    const xrediscommandcatalog* Catalog;const xbytesview* Arguments;size_t Count;
    xrediscommandkey* Output;size_t Keys,Bytes,Work;uint16 Slot;
    bool HasSlot,Cross,Variable,Fill;
} xrediskeyseval;
static bool xredisKeysInvocation(xbytesview Name,const xbytesview* A,size_t Count)
{
    size_t At=0;for(size_t I=0;I<Count;++I){size_t End=At;while(End<Name.Size && Name.Data[End]!='|')++End;
        if(!xredisKeysEqual((xbytesview){Name.Data+At,End-At},A[I]))return false;
        if(I+1==Count)return End==Name.Size;
        if(End==Name.Size)return false;
        At=End+1;}
    return false;
}
static xrediskeysstatus xredisKeysLookup(xrediskeyseval* E,size_t* Command)
{
    const xrediscommandcatalog* C=E->Catalog;uint64 Hash=14695981039346656037ull;size_t Bytes=0,Best=SIZE_MAX;
    for(size_t I=0;I<E->Count && I<C->Info.LookupDepth;++I){xbytesview A=E->Arguments[I];
        if((I && Bytes==C->Limits.MaxNameBytes) || A.Size>C->Limits.MaxNameBytes-Bytes-(I!=0))break;
        if(!xredisKeysCharge(&E->Work,A.Size+1,C->Limits.MaxSearchWork))return XREDIS_KEYS_LIMIT;
        if(I){Hash^='|';Hash*=1099511628211ull;++Bytes;}Bytes+=A.Size;Hash=xredisKeysHash(Hash,A);
        size_t Bucket=(size_t)Hash&(C->Buckets-1);
        while(C->Hash[Bucket]){size_t At=C->Hash[Bucket]-1;
            if(!xredisKeysCharge(&E->Work,Bytes+2*C->Commands[At].Name.Size+1,C->Limits.MaxSearchWork))return XREDIS_KEYS_LIMIT;
            if(xredisKeysInvocation(C->Commands[At].Name,E->Arguments,I+1)){Best=At;break;}Bucket=(Bucket+1)&(C->Buckets-1);}
        if(Best!=SIZE_MAX && !C->Commands[Best].Children)break;
    }
    if(Best==SIZE_MAX)return XREDIS_KEYS_UNKNOWN;
    *Command=Best;return XREDIS_KEYS_OK;
}
static bool xredisKeysRelative(int64 Index,size_t Count,size_t* Out)
{
    if(Index>=0){if((uint64)Index>=Count)return false;*Out=(size_t)Index;return true;}
    uint64 Back=(uint64)(-(Index+1))+1;if(Back>Count)return false;*Out=Count-(size_t)Back;return true;
}
static bool xredisKeysNumber(xbytesview V,uint64* Out)
{
    if(!V.Size || V.Size>20 || (V.Size>1 && V.Data[0]=='0'))return false;
    uint64 N=0;for(size_t I=0;I<V.Size;++I){unsigned Ch=V.Data[I];if(Ch<'0' || Ch>'9' || N>(UINT64_MAX-(Ch-'0'))/10)return false;N=N*10+Ch-'0';}*Out=N;return true;
}
static xrediskeysstatus xredisKeysAdd(xrediskeyseval* E,size_t At,uint32 Flags)
{
    xbytesview A=E->Arguments[At];const xrediscommandkeylimits* K=&E->Catalog->Limits;
    if(E->Keys>=K->MaxKeys || A.Size>K->MaxKeyBytes-E->Bytes)return XREDIS_KEYS_LIMIT;
    E->Bytes+=A.Size;
    if(E->Fill)E->Output[E->Keys]=(xrediscommandkey){At,Flags};
    else {uint16 Slot=xredisClusterSlotRaw(A);if(E->HasSlot && E->Slot!=Slot)E->Cross=true;else E->Slot=Slot;E->HasSlot=true;}
    ++E->Keys;E->Variable|=(Flags&XREDIS_KEY_VARIABLE)!=0;return XREDIS_KEYS_OK;
}
static xrediskeysstatus xredisKeysExtract(xrediskeyseval* E,const xrediskeyspec* S)
{
    size_t Begin=0;
    if(S->Begin==XRK_INDEX){if((uint64)S->Index>=E->Count)return XREDIS_KEYS_ARGUMENTS;Begin=(size_t)S->Index;}
    else {size_t At;if(!xredisKeysRelative(S->Index,E->Count,&At))return XREDIS_KEYS_OK;bool Found=false;
        for(;;){xbytesview A=E->Arguments[At];size_t Cost=1+(A.Size==S->Keyword.Size?A.Size:0);
            if(!xredisKeysCharge(&E->Work,Cost,E->Catalog->Limits.MaxSearchWork))return XREDIS_KEYS_LIMIT;
            if(xredisKeysEqual(A,S->Keyword)){Begin=At+1;Found=true;break;}
            if(S->Index<0){if(!At)break;--At;}else if(++At==E->Count)break;
        }
        if(!Found)return XREDIS_KEYS_OK;
        if(Begin>=E->Count)return XREDIS_KEYS_ARGUMENTS;
    }
    size_t First=Begin,Last=0;uint64 Step=(uint64)S->Step;
    if(S->Find==XRK_RANGE){
        if(S->Last==-1 && S->Limit>1){size_t Keys=(size_t)((E->Count-Begin)/(uint64)S->Limit);if(!Keys)return XREDIS_KEYS_OK;Last=Begin+Keys-1;}
        else if(S->Last<0){if(!xredisKeysRelative(S->Last,E->Count,&Last))return XREDIS_KEYS_ARGUMENTS;}
        else {if((uint64)S->Last>=E->Count-Begin)return XREDIS_KEYS_ARGUMENTS;Last=Begin+(size_t)S->Last;}
        if(Last<First)return XREDIS_KEYS_OK;
    }else {
        if((uint64)S->Number>=E->Count-Begin)return XREDIS_KEYS_ARGUMENTS;
        uint64 N=0;
        if(!xredisKeysNumber(E->Arguments[Begin+(size_t)S->Number],&N) || N>E->Count)return XREDIS_KEYS_ARGUMENTS;
        if(!N)return XREDIS_KEYS_OK;
        if((uint64)S->First>=E->Count-Begin)return XREDIS_KEYS_ARGUMENTS;
        First=Begin+(size_t)S->First;
        if(N-1>(E->Count-1-First)/Step)return XREDIS_KEYS_ARGUMENTS;
        Last=First+(size_t)((N-1)*Step);
    }
    for(size_t At=First;;){xrediskeysstatus Status=xredisKeysAdd(E,At,S->Flags);if(Status!=XREDIS_KEYS_OK)return Status;if(Step>Last-At)break;At+=(size_t)Step;}
    return XREDIS_KEYS_OK;
}
static xrediskeysstatus xredisKeysEvaluatePass(xrediskeyseval* E,const xrediskeycommand* Command)
{
    if(Command->Modern){for(size_t I=0;I<Command->Count;++I){xrediskeysstatus S=xredisKeysExtract(E,&E->Catalog->Specs[Command->Spec+I]);if(S!=XREDIS_KEYS_OK)return S;}}
    else if(Command->First){xrediskeyspec S={0};S.Begin=XRK_INDEX;S.Find=XRK_RANGE;S.Index=Command->First;S.Step=Command->Step;S.Last=Command->Last;
        /* Legacy positive lastkey is absolute, unlike a modern relative range. */
        if(S.Last>=0){if(S.Last<S.Index)return XREDIS_KEYS_ARGUMENTS;S.Last-=S.Index;}
        return xredisKeysExtract(E,&S);}
    return XREDIS_KEYS_OK;
}
XREDIS_API xrediskeysstatus xredisCommandCatalogEvaluate(const xrediscommandcatalog* C,const xbytesview* A,size_t Count,xrediscommandkey* Keys,size_t Capacity,xrediscommandkeyresult* Out)
{
    if(!xredisKeysOutput(C,Out,sizeof(*Out)) || Out->Size!=sizeof(*Out))return xredisKeysError("invalid key extraction result output"),XREDIS_KEYS_ERROR;
    if(Count>C->Limits.MaxArguments || Capacity>C->Limits.MaxKeys)return XREDIS_KEYS_LIMIT;
    size_t KeyBytes=Capacity*sizeof(*Keys);
    if((!Keys && Capacity) || (KeyBytes && (!xredisKeysOutput(C,Keys,KeyBytes) || xrtMemRangesOverlap(Keys,KeyBytes,Out,sizeof(*Out)))) || !xrtMemRangeValid(A,Count*sizeof(*A)) ||
        xrtMemRangesOverlap(A,Count*sizeof(*A),Out,sizeof(*Out)) || (KeyBytes && xrtMemRangesOverlap(A,Count*sizeof(*A),Keys,KeyBytes)))return xredisKeysError("invalid key extraction spans"),XREDIS_KEYS_ERROR;
    size_t Bytes=0;for(size_t I=0;I<Count;++I){if(!xrtMemRangeValid(A[I].Data,A[I].Size) || xrtMemRangesOverlap(A[I].Data,A[I].Size,Out,sizeof(*Out)) || (KeyBytes && xrtMemRangesOverlap(A[I].Data,A[I].Size,Keys,KeyBytes)))return xredisKeysError("invalid key extraction argument/alias"),XREDIS_KEYS_ERROR;
        if(!xredisKeysCharge(&Bytes,A[I].Size,C->Limits.MaxArgumentBytes))return XREDIS_KEYS_LIMIT;}
    if(!Count)return XREDIS_KEYS_ARGUMENTS;
    xrediskeyseval E={0};E.Catalog=C;E.Arguments=A;E.Count=Count;size_t At=0;xrediskeysstatus Status=xredisKeysLookup(&E,&At);if(Status!=XREDIS_KEYS_OK)return Status;
    const xrediskeycommand* Command=&C->Commands[At];
    if((Command->Arity>0 && (uint64)Command->Arity!=Count) || (Command->Arity<0 && (uint64)(-Command->Arity)>Count))return XREDIS_KEYS_ARGUMENTS;
    if(Command->Children)return XREDIS_KEYS_UNKNOWN;
    if(Command->Unsupported || ((!Command->Modern || !Command->Count) && Command->Movable) || (Command->Modern && !Command->Count && Command->First))return XREDIS_KEYS_UNSUPPORTED;
    Status=xredisKeysEvaluatePass(&E,Command);if(Status!=XREDIS_KEYS_OK)return Status;
    if(E.Cross)return XREDIS_KEYS_CROSS_SLOT;
    if(Keys && E.Keys>Capacity)return XREDIS_KEYS_LIMIT;
    xrediscommandkeyresult Result={sizeof(Result),At,E.Keys,E.Slot,E.HasSlot,!Command->Modern,E.Variable};
    if(Keys && E.Keys){xrediskeyseval Fill={0};Fill.Catalog=C;Fill.Arguments=A;Fill.Count=Count;Fill.Output=Keys;Fill.Fill=true;
        Status=xredisKeysEvaluatePass(&Fill,Command);if(Status!=XREDIS_KEYS_OK)abort();}
    *Out=Result;return E.Keys?XREDIS_KEYS_OK:XREDIS_KEYS_NONE;
}
#endif
#endif

#endif
