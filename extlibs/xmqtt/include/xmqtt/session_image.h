#ifndef XMQTT_SESSION_IMAGE_H
#define XMQTT_SESSION_IMAGE_H
#include <xmqtt/session.h>
#if defined(XMQTT_FEATURE_XMQTT_SESSION_IMAGE) && (!defined(XMQTT_FEATURE_XMQTT_SESSION) || !defined(XRT_FEATURE_CRC32))
#error "MQTT Session images require Session and CRC32"
#endif
#if defined(XMQTT_FEATURE_XMQTT_SESSION_IMAGE)
#define XMQTT_SESSION_IMAGE_BINDING_MAX 1024u
typedef struct xmqttsessionimageconfig {
    size_t Size;xbytesview Binding;double MonotonicTime;
    uint64 WallMilliseconds;size_t MaxBytes;
} xmqttsessionimageconfig;
XRT_EXTERN_C_BEGIN
/* Defaults:16MiB image cap, zero clocks, empty Binding. Supply1..1024 opaque
 * binding bytes identifying ONE broker/security principal and a nonnegative
 * finite monotonic clock plus trusted UTC milliseconds <=INT64_MAX. Binding
 * comparison prevents accidental retargeting; CRC detects corruption, not
 * malicious modification/authentication/rollback. Protect storage separately. */
XMQTT_API bool xmqttSessionImageConfigInit(xmqttsessionimageconfig* Config);
/* Stable sole protocol owner, with no attached Client claim. Connected passive
 * sessions are allowed. Stop other mutators; ordinary stable reference readers
 * and releases retain their normal contract. Images contain active canonical
 * outgoing QoS1/2, committed incoming QoS2 ids and bound ClientId; never TLS/auth
 * secrets, prepared/invisible receives, completed external references or Inbox
 * payloads. This is a checkpoint component, not a durable Client/store.
 * Successful Size/Write allocate nothing and preserve ambient error. Size
 * walks active metadata; Write also copies wire once and scans the image for
 * CRC. Incoming ids are emitted in order from the existing bounded map.
 * All caller outputs must be disjoint from inputs and library-owned storage.
 * Failures/LIMIT preserve outputs/buffer; LIMIT is normal capacity refusal.
 * Separate Size and Write each take a fresh coherent cut under Session's gate;
 * Size is not a reservation if the caller changes state between them. */
XMQTT_API xmqttsessionstatus xmqttSessionImageSize(xmqttsession* Session,
    const xmqttsessionimageconfig* Config,size_t* Bytes);
XMQTT_API xmqttsessionstatus xmqttSessionImageWrite(xmqttsession* Session,
    const xmqttsessionimageconfig* Config,ptr Buffer,size_t Capacity,size_t* Bytes);
/* Validate bounded version1 big-endian format/CRC before constructing a new
 * quota-limited Session, then validate every embedded packet with native MQTT
 * codecs. Require exact Binding, Version and ClientId. Caller image/config
 * borrows only during the call; success owns one new disconnected Session.
 * Original monotonic epochs/leases/Futures are not reused. Caller still joins
 * the old transport; images do not close or retire it. Reconnect still
 * authenticates and negotiates fresh CONNACK/SessionPresent before replay.
 * Closed finite expiry is rounded DOWN to milliseconds, charged for downtime,
 * and rebased without adding life. Backward wall time is STATE_ERROR. An expired
 * image publishes an empty Session with RESET; DONE publishes retained/empty
 * state otherwise. A live-at-cut image has unknown actual disconnect time:
 * retain locally until a fresh broker decision, even after long downtime.
 * ERROR/STATE_ERROR/PROTOCOL_ERROR/LIMIT leave Session output unchanged and
 * release every partial allocation. Restored bytes/objects obey SessionConfig,
 * image bytes obey Config.MaxBytes; caller image storage remains additional.
 * CRC-valid state does not prove receive payload durability: commit payload,
 * protocol state and acknowledgements in one future store transaction. */
XMQTT_API xmqttsessionstatus xmqttSessionImageRestore(const xmqttsessionconfig* SessionConfig,
    const xmqttsessionimageconfig* Config,xbytesview Image,xmqttsession** Session);
/* Explicit MQTT5 server-assigned identity recovery. SessionConfig.Version must
 * be5 and ClientId empty; copy the bound, valid ClientId from the image after
 * exact Binding validation. Query xmqttSessionClientId on success before the
 * new CONNECT. Ordinary Restore never implicitly adopts another identity.
 * Bind a storage slot as well as broker/principal when the application owns
 * multiple Sessions. Remaining limits, clocks, output and ownership are equal. */
XMQTT_API xmqttsessionstatus xmqttSessionImageRestoreAssigned(const xmqttsessionconfig* SessionConfig,
    const xmqttsessionimageconfig* Config,xbytesview Image,xmqttsession** Session);
XRT_EXTERN_C_END
#endif
#endif
