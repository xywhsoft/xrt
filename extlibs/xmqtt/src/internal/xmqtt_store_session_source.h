#ifndef XMQTT_INTERNAL_STORE_SESSION_SOURCE_H
#define XMQTT_INTERNAL_STORE_SESSION_SOURCE_H
#include <xmqtt/store_state.h>
#include "xmqtt_internal.h"
#if defined(XMQTT_FEATURE_XMQTT_STORE_SESSION)
/* Immutable source: check expected slot and all State-owned output aliases. */
XMQTT_INTERNAL xmqttstorestatus xmqttInternalStoreSessionSource(const xmqttstorestate* State,
    xbytesview Binding,uint64 Generation,void* Output,size_t Bytes,uint64* RecoveryWall);
#endif
#endif
