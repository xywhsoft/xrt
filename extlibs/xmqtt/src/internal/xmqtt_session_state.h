#ifndef XMQTT_INTERNAL_SESSION_STATE_H
#define XMQTT_INTERNAL_SESSION_STATE_H
#include <xmqtt/session.h>
#include <xrt/atomic.h>
#include <xrt/sync.h>
#if defined(XMQTT_FEATURE_XMQTT_SESSION)
enum {XMQTT_SESSION_ID_WORDS=2048,XMQTT_SESSION_ID_BYTES=8192,XMQTT_SESSION_RX_BYTES=16384};
struct xmqttsessionpacket {
    xatomic32 Refs;xmqttsession* Owner;
    xmqttsessionpacket *Next,*Previous,*HashNext,*ReadyNext,*ReadyPrevious;
    size_t Allocation,Bytes;uint16 Identifier;uint8 Qos,Reason,PubrecReason,ReadyKind,SelectedType;
    bool Active,Sent,Attempted,ReleaseSent,PubrecReceived,Need,Retry;
    xmqttsessionstage Stage;unsigned char Duplicate,Release[4];
    xmqttpacket Packet;xmqttproperty Properties[];
};
struct xmqttsession {
    xatomic32 Refs;xmutex Gate;xmqttsessionconfig Config;xmqttnegotiated Peer;
    size_t BaseBytes,LiveBytes,Objects,Active,RxCount,Prepared,Buckets;
    xmqttsessionpacket **Hash,*Head,*Tail,*ReadyHead[2],*ReadyTail[2],*Selected;
    unsigned char *Ids,*Receive;uint16 Next,Quota,InitialQuota;uint64 Generation,Selection;
    /* Fractional seconds allow restart rebasing without subtracting a fraction
     * from an arbitrarily large monotonic clock. Native negotiated integer
     * intervals remain exactly representable. No public layout depends on it. */
    bool Connected,HasState,Persistent,InfiniteExpiry;
#if defined(XMQTT_FEATURE_XMQTT_SESSION_IMAGE) || defined(XMQTT_FEATURE_XMQTT_STORE_SESSION)
    double Expiry;
#else
    uint32 Expiry;
#endif
    double LastNow,ExpiresAt,ClosedAt;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    const void* Driver;const xmqttcodecconfig* DriverCodec;
#endif
    unsigned char Data[];
};
/* Counts/identity have already passed the Session configuration bounds. Shared
 * by normal creation and image preflight so fixed storage uses one formula. */
static inline size_t xmqttSessionStorageSize(const xmqttsessionconfig* C,size_t AssignedCapacity,size_t* Buckets)
{
    size_t B=0;if(C->MaxOutgoing){B=2;while(B<C->MaxOutgoing*2u)B*=2u;}
    if(Buckets)*Buckets=B;
    return sizeof(xmqttsession)+B*sizeof(xmqttsessionpacket*)+(C->MaxOutgoing?XMQTT_SESSION_ID_BYTES:0u)+(C->MaxIncoming?XMQTT_SESSION_RX_BYTES:0u)+(AssignedCapacity?AssignedCapacity:C->ClientId.Size);
}
#endif
#endif
