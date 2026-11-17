#ifndef XMQTT_STORE_SESSION_H
#define XMQTT_STORE_SESSION_H
#include <xmqtt/store_state.h>
#include <xmqtt/session.h>
#if defined(XMQTT_FEATURE_XMQTT_STORE_SESSION) && (!defined(XMQTT_FEATURE_XMQTT_STORE_STATE) || !defined(XMQTT_FEATURE_XMQTT_SESSION))
#error "MQTT store Session import requires recovered state and Session"
#endif
#if defined(XMQTT_FEATURE_XMQTT_STORE_SESSION)
typedef struct xmqttstoresessionconfig {
    size_t Size;xbytesview Binding;uint64 Generation;
    double MonotonicTime;uint64 WallMilliseconds;size_t MaxScratchBytes;
} xmqttstoresessionconfig;
XRT_EXTERN_C_BEGIN
/* Defaults:16KiB temporary property scratch, zero clocks/Generation and empty
 * Binding. Set the exact broker/principal/slot and storage generation, finite
 * nonnegative monotonic time and trusted wall milliseconds<=INT64_MAX. */
XMQTT_API bool xmqttStoreSessionConfigInit(xmqttstoresessionconfig* Config);
/* Construct one independent disconnected Session from an immutable, bound
 * recovered view. Version/ClientId/Binding/Generation must match exactly.
 * No input is retained; State can be destroyed after return. Incoming QoS2
 * identities and canonical outgoing id/order/PUBLISH-or-PUBREL/attempt metadata
 * survive. Connection generation starts at0, selection/windows/aliases/old
 * Futures are not restored; normal authenticated Open/CONNACK is still required.
 * Session does not hold Inbox payloads or acknowledge application completion.
 * Caller must preserve/use the separate recovered Inbox until durable ACK.
 * Additional wall time since recovery consumes closed finite expiry; rollback
 * is STATE_ERROR. Unknown disconnect time at a live cut retains the candidate
 * until a fresh broker decision. Expiry during import publishes an empty Session
 * with RESET, after validating all outgoing packets under the new codec/caps.
 * Already empty protocol state publishes DONE. Unbound empty journals cannot
 * supply an identity and return STATE_ERROR; use normal Session creation first.
 * SessionConfig.MaxBytes bounds the new owner's requested allocations. Caller
 * State storage is additional; Config.MaxScratchBytes separately bounds reusable
 * temporary property descriptors. Zero scratch permits property-free packets.
 * One native parse and one retained wire copy per outgoing packet, descriptor
 * rebasing instead of a second parse, no whole-image serialization/CRC/buffer.
 * Inputs borrow only for the call; stable State lifetime is required, and all
 * outputs must be disjoint from inputs/config views/every State-owned allocation.
 * DONE/RESET/LIMIT restore ambient errors quietly. Native decoder limit notices
 * may occur before being discarded. Checked error-slot failure returns ERROR
 * without publishing Session; all partial allocations are released. This is
 * passive import, not live durable Client/ACK/Inbox/lease/commit integration.
 * Join/destroy the previous Client/transport before replacing its owner; this
 * function cannot fence a still-live old connection. */
XMQTT_API xmqttsessionstatus xmqttStoreSessionRestore(const xmqttstorestate* State,
    const xmqttsessionconfig* SessionConfig,const xmqttstoresessionconfig* Config,xmqttsession** Session);
/* Explicit MQTT5 assigned-id recovery: SessionConfig.ClientId must be empty.
 * Copy the already bound identity only after exact slot/generation verification.
 * Query SessionClientId before the next CONNECT; no implicit identity adoption
 * is performed by ordinary Restore. Other contracts and limits are identical. */
XMQTT_API xmqttsessionstatus xmqttStoreSessionRestoreAssigned(const xmqttstorestate* State,
    const xmqttsessionconfig* SessionConfig,const xmqttstoresessionconfig* Config,xmqttsession** Session);
XRT_EXTERN_C_END
#endif
#endif
