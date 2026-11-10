#ifndef XMQTT_SESSION_CLIENT_INTERNAL_H
#define XMQTT_SESSION_CLIENT_INTERNAL_H
#include <xmqtt/session.h>
#include "xmqtt_internal.h"
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
/* Library-only exclusive driver. No callback, runtime or public operation-code
 * interface. All mutable entry points share the Session gate; public mutations
 * refuse while claimed. Immutable getters/ref operations remain available. */
XRT_EXTERN_C_BEGIN
XMQTT_INTERNAL bool xmqttInternalSessionClaim(xmqttsession* S,const void* Owner,xmqttversion Version,
    const xmqttpacket* Connect,const xmqttcodecconfig* Codec,double Now,size_t* Count,size_t* Bytes);
XMQTT_INTERNAL void xmqttInternalSessionUnclaim(xmqttsession* S,const void* Owner);
XMQTT_INTERNAL uint16 xmqttInternalSessionIdentifier(xmqttsession* S,const void* Owner,
    const unsigned char* Used,uint16 Requested,uint16 Previous);
XMQTT_INTERNAL xmqttsessionstatus xmqttInternalSessionVisit(xmqttsession* S,const void* Owner,
    const xmqttsessionpacket* Previous,const xmqttsessionpacket** Packet);
XMQTT_INTERNAL xmqttsessionstatus xmqttInternalSessionOpen(xmqttsession* S,const void* Owner,
    const xmqttpacket* Connect,const xmqttpacket* Connack,double Now,xmqttnegotiated* Negotiated);
XMQTT_INTERNAL xmqttsessionstatus xmqttInternalSessionEnd(xmqttsession* S,const void* Owner,const xmqttpacket* Disconnect,double Now);
XMQTT_INTERNAL xmqttsessionstatus xmqttInternalSessionPublish(xmqttsession* S,const void* Owner,
    const xmqttpacket* Publish,const xmqttsessionpacket** Packet);
/* One immutable copy, physically reserved but not linked to protocol state.
 * The exclusive driver activates it only when selecting its first output. */
XMQTT_INTERNAL xmqttsessionstatus xmqttInternalSessionSnapshot(xmqttsession* S,const void* Owner,
    const xmqttpacket* Publish,const xmqttsessionpacket** Packet);
XMQTT_INTERNAL xmqttsessionstatus xmqttInternalSessionSelect(xmqttsession* S,const void* Owner,
    const xmqttsessionpacket* Packet,xmqttsessionframe* Frame);
XMQTT_INTERNAL xmqttsessionstatus xmqttInternalSessionSent(xmqttsession* S,const void* Owner,const xmqttsessionframe* Frame);
XMQTT_INTERNAL xmqttsessionstatus xmqttInternalSessionAcknowledge(xmqttsession* S,const void* Owner,const xmqttpacket* Ack);
/* Withdraw only a driver-owned, new-on-this-connection PUBLISH for which the
 * Client has accepted zero native bytes. Imported obligations cannot use this. */
XMQTT_INTERNAL xmqttsessionstatus xmqttInternalSessionWithdraw(xmqttsession* S,const void* Owner,const xmqttsessionpacket* Packet);
XMQTT_INTERNAL xmqttsessionstatus xmqttInternalSessionReceive(xmqttsession* S,const void* Owner,uint16 Id,unsigned Operation,uint8* Reason);
XMQTT_INTERNAL unsigned xmqttInternalSessionReceiveState(xmqttsession* S,const void* Owner,uint16 Id);
XRT_EXTERN_C_END
#endif
#endif
