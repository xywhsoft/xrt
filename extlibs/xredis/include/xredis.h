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

#endif
