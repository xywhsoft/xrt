/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XTP_FEATURES_H
#define XTP_FEATURES_H

/* xtp_xrudp 及其直接依赖。 */
#if defined(XTP_MODULE_ALL) || defined(XTP_MODULE_XTP_XRUDP)
#ifndef XTP_FEATURE_XTP_XRUDP
#define XTP_FEATURE_XTP_XRUDP
#endif
#ifndef XTP_MODULE_XTP_TRANSPORT
#define XTP_MODULE_XTP_TRANSPORT
#endif
#ifndef XRUDP_MODULE_RUDP_SESSION
#define XRUDP_MODULE_RUDP_SESSION
#endif
#endif

/* xtp_native 及其直接依赖。 */
#if defined(XTP_MODULE_ALL) || defined(XTP_MODULE_XTP_NATIVE)
#ifndef XTP_FEATURE_XTP_NATIVE
#define XTP_FEATURE_XTP_NATIVE
#endif
#ifndef XTP_MODULE_XTP_TCP
#define XTP_MODULE_XTP_TCP
#endif
#ifndef XTP_MODULE_XTP_TLS
#define XTP_MODULE_XTP_TLS
#endif
#ifndef XTP_MODULE_XTP_WS
#define XTP_MODULE_XTP_WS
#endif
#ifndef XRT_MODULE_NET_TCP_RETIRE
#define XRT_MODULE_NET_TCP_RETIRE
#endif
#ifndef XRT_MODULE_TLS_STREAM_RETIRE
#define XRT_MODULE_TLS_STREAM_RETIRE
#endif
#ifndef XRT_MODULE_THREAD
#define XRT_MODULE_THREAD
#endif
#endif

/* xtp_ws 及其直接依赖。 */
#if defined(XTP_MODULE_ALL) || defined(XTP_MODULE_XTP_WS)
#ifndef XTP_FEATURE_XTP_WS
#define XTP_FEATURE_XTP_WS
#endif
#ifndef XTP_MODULE_XTP_TRANSPORT
#define XTP_MODULE_XTP_TRANSPORT
#endif
#ifndef XWS_MODULE_WEBSOCKET_CONNECTION
#define XWS_MODULE_WEBSOCKET_CONNECTION
#endif
#endif

/* xtp_tls 及其直接依赖。 */
#if defined(XTP_MODULE_ALL) || defined(XTP_MODULE_XTP_TLS)
#ifndef XTP_FEATURE_XTP_TLS
#define XTP_FEATURE_XTP_TLS
#endif
#ifndef XTP_MODULE_XTP_TRANSPORT
#define XTP_MODULE_XTP_TRANSPORT
#endif
#ifndef XRT_MODULE_TLS_STREAM
#define XRT_MODULE_TLS_STREAM
#endif
#ifndef XRT_MODULE_TLS_CLIENT_VERIFY
#define XRT_MODULE_TLS_CLIENT_VERIFY
#endif
#ifndef XRT_MODULE_TLS_IDENTITY_ED25519
#define XRT_MODULE_TLS_IDENTITY_ED25519
#endif
#ifndef XRT_MODULE_TLS_IDENTITY_P256
#define XRT_MODULE_TLS_IDENTITY_P256
#endif
#ifndef XRT_MODULE_TLS_RECORD_CHACHA
#define XRT_MODULE_TLS_RECORD_CHACHA
#endif
#ifndef XRT_MODULE_TLS_RECORD_AES
#define XRT_MODULE_TLS_RECORD_AES
#endif
#ifndef XRT_MODULE_TLS_SCHEDULE_SHA256
#define XRT_MODULE_TLS_SCHEDULE_SHA256
#endif
#ifndef XRT_MODULE_TLS_KEY_EXCHANGE_X25519
#define XRT_MODULE_TLS_KEY_EXCHANGE_X25519
#endif
#endif

/* xtp_tcp 及其直接依赖。 */
#if defined(XTP_MODULE_ALL) || defined(XTP_MODULE_XTP_TCP)
#ifndef XTP_FEATURE_XTP_TCP
#define XTP_FEATURE_XTP_TCP
#endif
#ifndef XTP_MODULE_XTP_TRANSPORT
#define XTP_MODULE_XTP_TRANSPORT
#endif
#ifndef XRT_MODULE_NET_TCP
#define XRT_MODULE_NET_TCP
#endif
#endif

/* xtp 及其直接依赖。 */
#if defined(XTP_MODULE_ALL) || defined(XTP_MODULE_XTP)
#ifndef XTP_FEATURE_XTP
#define XTP_FEATURE_XTP
#endif
#ifndef XTP_MODULE_XTP_CODEC
#define XTP_MODULE_XTP_CODEC
#endif
#ifndef XTP_MODULE_XTP_READER
#define XTP_MODULE_XTP_READER
#endif
#ifndef XTP_MODULE_XTP_SESSION
#define XTP_MODULE_XTP_SESSION
#endif
#ifndef XTP_MODULE_XTP_TRANSPORT
#define XTP_MODULE_XTP_TRANSPORT
#endif
#ifndef XTP_MODULE_XTP_DELIVERY
#define XTP_MODULE_XTP_DELIVERY
#endif
#endif

/* xtp_delivery 及其直接依赖。 */
#if defined(XTP_MODULE_ALL) || defined(XTP_MODULE_XTP_DELIVERY)
#ifndef XTP_FEATURE_XTP_DELIVERY
#define XTP_FEATURE_XTP_DELIVERY
#endif
#ifndef XTP_MODULE_XTP_SESSION
#define XTP_MODULE_XTP_SESSION
#endif
#endif

/* xtp_transport 及其直接依赖。 */
#if defined(XTP_MODULE_ALL) || defined(XTP_MODULE_XTP_TRANSPORT)
#ifndef XTP_FEATURE_XTP_TRANSPORT
#define XTP_FEATURE_XTP_TRANSPORT
#endif
#ifndef XTP_MODULE_XTP_SESSION
#define XTP_MODULE_XTP_SESSION
#endif
#endif

/* xtp_session 及其直接依赖。 */
#if defined(XTP_MODULE_ALL) || defined(XTP_MODULE_XTP_SESSION)
#ifndef XTP_FEATURE_XTP_SESSION
#define XTP_FEATURE_XTP_SESSION
#endif
#ifndef XTP_MODULE_XTP_READER
#define XTP_MODULE_XTP_READER
#endif
#ifndef XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_RANDOM_SECURE
#endif
#endif

/* xtp_reader 及其直接依赖。 */
#if defined(XTP_MODULE_ALL) || defined(XTP_MODULE_XTP_READER)
#ifndef XTP_FEATURE_XTP_READER
#define XTP_FEATURE_XTP_READER
#endif
#ifndef XTP_MODULE_XTP_CODEC
#define XTP_MODULE_XTP_CODEC
#endif
#endif

/* xtp_codec 及其直接依赖。 */
#if defined(XTP_MODULE_ALL) || defined(XTP_MODULE_XTP_CODEC)
#ifndef XTP_FEATURE_XTP_CODEC
#define XTP_FEATURE_XTP_CODEC
#endif
#endif

#endif /* XTP_FEATURES_H */
