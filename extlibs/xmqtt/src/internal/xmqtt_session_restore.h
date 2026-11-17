#ifndef XMQTT_INTERNAL_SESSION_RESTORE_H
#define XMQTT_INTERNAL_SESSION_RESTORE_H
#include "xmqtt_internal.h"
#include "xmqtt_session_state.h"
#if defined(XMQTT_FEATURE_XMQTT_SESSION_IMAGE) || defined(XMQTT_FEATURE_XMQTT_STORE_SESSION)
/* Sole unpublished Session owner. Parsed is NULL for native decode directly
 * into the owned packet (image), or one complete native-decoded PUBLISH whose
 * views all borrow Wire (store import). No public descriptor adoption API. */
XMQTT_INTERNAL xmqttsessionstatus xmqttInternalSessionRestorePacket(xmqttsession* Session,
    xbytesview Wire,size_t Properties,uint8 Stage,uint8 Flags,uint8 Reason,const xmqttpacket* Parsed);
XMQTT_INTERNAL void xmqttInternalSessionRestoreExpiry(xmqttsession* Session,
    double Now,bool HasState,bool Infinite,uint64 RemainingMilliseconds);
#endif
#endif
