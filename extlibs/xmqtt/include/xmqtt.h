#ifndef XMQTT_H
#define XMQTT_H
#include <xmqtt/features.h>
#if defined(XMQTT_FEATURE_XMQTT) && !defined(XMQTT_FEATURE_XMQTT_WIRE)
#error "XMQTT_FEATURE_XMQTT requires XMQTT_FEATURE_XMQTT_WIRE"
#endif
#if defined(XMQTT_FEATURE_XMQTT) && !defined(XMQTT_FEATURE_XMQTT_PROPERTIES)
#error "XMQTT_FEATURE_XMQTT requires XMQTT_FEATURE_XMQTT_PROPERTIES"
#endif
#include <xmqtt/wire.h>
#include <xmqtt/properties.h>
#endif
