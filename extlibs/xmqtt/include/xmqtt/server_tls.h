#ifndef XMQTT_SERVER_TLS_H
#define XMQTT_SERVER_TLS_H
#include <xmqtt/server.h>
#include <xrt/tls_stream.h>

#if defined(XMQTT_FEATURE_XMQTT_SERVER_TLS)
#if !defined(XMQTT_FEATURE_XMQTT_SERVER) || !defined(XRT_FEATURE_TLS_SERVER) || !defined(XRT_FEATURE_TLS_STREAM_RETIRE)
#error "MQTT TLS broker requires the MQTT broker, XRT TLS server and TLS stream retirement"
#endif
XRT_EXTERN_C_BEGIN

/* In an XRT TCP Accept callback, start TLS then read MQTT on the same worker.
 * True consumes the raw stream reference. False leaves it with the listener.
 * The application supplies TLS identity and policy and keeps their borrowed
 * callback contexts alive through broker WaitFor. TLS plaintext capacity
 * must accommodate Codec.MaxPacketBytes plus one maximum TLS record.
 * MQTT's CONNECT deadline includes the TLS handshake. */
XMQTT_API bool xmqttServerAttachTls(xmqttserver* Server, xnetstream* Stream,
	const xtlsserverconfig* Tls, const xtlsstreamconfig* TlsStream);

XRT_EXTERN_C_END
#endif
#endif
