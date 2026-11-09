#ifndef XMQTT_CLIENT_TLS_RESUME_H
#define XMQTT_CLIENT_TLS_RESUME_H
#include <xmqtt/client_tls.h>
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS_RESUME) && !defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
#error "MQTT TLS resume handoff requires the TLS client"
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS_RESUME)
/* Thread-safe observation, independent of MQTT CONNACK/authentication readiness.
 * Established/Resumed describe the authenticated TLS handshake and remain valid
 * after close. Enabled requires native ResumeLimit>0 and no ClientAuth (the
 * native mutual-authentication client currently disables ticket publication).
 * One latest TLS1.3 credential is cached. Captured counts objects taken from
 * the native queue; Evicted counts replaced cache entries and Rejected counts
 * local byte-budget refusals. Native queue drops/OOM are outside these counters.
 * CachedBytes counts ticket/secret/name/ALPN/identity bytes plus peer-chain DER;
 * it is charged to MaxPrivateBytes while cached. Native opaque metadata and its
 * bounded queue retain their separate Core limits. Counters saturate.
 * No public layout depends on optional module selections. Size is output. */
typedef struct xmqttclienttlsresumeinfo {
    size_t Size,CachedBytes;uint64 Captured,Evicted,Rejected;
    bool Enabled,Established,Resumed;
} xmqttclienttlsresumeinfo;
XRT_EXTERN_C_BEGIN
XMQTT_API bool xmqttClientTlsResumeInfo(xmqttclient* Client,xmqttclienttlsresumeinfo* Info);
/* Transfers the cached native reference to the caller without allocation/copy.
 * Any thread may call with a stable borrowed Client, including Config.Notify
 * on its Worker or after ClientWait. False/empty preserves Resume and ambient
 * error; invalid arguments/non-TLS clients report an error without output change.
 * Notify coalesces ticket changes in existing Client work, outside native TLS
 * callbacks. Take refunds the local cached-byte charge; the caller now owns
 * credential storage/budget and must xrtTlsResumeRelease it. It may outlive
 * Client/Engine and be supplied as another factory's Session.Resume. That factory
 * retains it before DNS. Keep Core code resident through its final release.
 * This is a TLS credential, not MQTT session state or permission to replay any
 * request. Each connection still performs CONNECT/CONNACK/authentication. There
 * is no automatic reconnect, persistence or 0-RTT. Native ResumeOnly controls
 * whether TLS may fall back to a full handshake; identity/ALPN/expiry validation
 * and certificate policy remain native. Credentials/secrets must not be logged. */
XMQTT_API bool xmqttClientTlsTakeResume(xmqttclient* Client,xtlsresume** Resume);
XRT_EXTERN_C_END
#endif
#endif
