#ifndef XREDIS_H
#define XREDIS_H

#include <xredis/features.h>

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_COMMAND)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_COMMAND"
#endif

#include <xredis/command.h>

#endif
