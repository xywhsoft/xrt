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

#include <xredis/command.h>
#include <xredis/reader.h>
#include <xredis/reply.h>
#include <xredis/client.h>
#include <xredis/dial.h>

#endif
