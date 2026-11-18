#ifndef XMQTT_INTERNAL_STORE_PACKET_H
#define XMQTT_INTERNAL_STORE_PACKET_H
#include <xmqtt/store_delta.h>
#include "xmqtt_internal.h"
#if defined(XMQTT_FEATURE_XMQTT_STORE_DELTA)
/* Validate one bounded stored PUBLISH. The envelope already checked the span. */
XMQTT_INTERNAL xmqttstoredeltastatus xmqttInternalStorePacket(const xmqttstoredeltaconfig* Config,
    const xmqttstoredelta* Delta,xmqttproperty* Properties,size_t Capacity);
#endif
#endif
