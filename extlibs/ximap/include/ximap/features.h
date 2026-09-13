/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XIMAP_FEATURES_H
#define XIMAP_FEATURES_H

/* imap_compress 及其直接依赖。 */
#if defined(XIMAP_MODULE_ALL) || defined(XIMAP_MODULE_IMAP_COMPRESS)
#ifndef XIMAP_FEATURE_IMAP_COMPRESS
#define XIMAP_FEATURE_IMAP_COMPRESS
#endif
#ifndef XIMAP_MODULE_IMAP_CLIENT
#define XIMAP_MODULE_IMAP_CLIENT
#endif
#ifndef XMAIL_MODULE_MAIL_NET_DEFLATE
#define XMAIL_MODULE_MAIL_NET_DEFLATE
#endif
#endif

/* imap_append 及其直接依赖。 */
#if defined(XIMAP_MODULE_ALL) || defined(XIMAP_MODULE_IMAP_APPEND)
#ifndef XIMAP_FEATURE_IMAP_APPEND
#define XIMAP_FEATURE_IMAP_APPEND
#endif
#ifndef XIMAP_MODULE_IMAP_CLIENT
#define XIMAP_MODULE_IMAP_CLIENT
#endif
#endif

/* imap_message 及其直接依赖。 */
#if defined(XIMAP_MODULE_ALL) || defined(XIMAP_MODULE_IMAP_MESSAGE)
#ifndef XIMAP_FEATURE_IMAP_MESSAGE
#define XIMAP_FEATURE_IMAP_MESSAGE
#endif
#ifndef XIMAP_MODULE_IMAP_COMMAND
#define XIMAP_MODULE_IMAP_COMMAND
#endif
#ifndef XIMAP_MODULE_IMAP_DATA
#define XIMAP_MODULE_IMAP_DATA
#endif
#ifndef XMAIL_MODULE_MAIL_TREE
#define XMAIL_MODULE_MAIL_TREE
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#endif

/* imap_command 及其直接依赖。 */
#if defined(XIMAP_MODULE_ALL) || defined(XIMAP_MODULE_IMAP_COMMAND)
#ifndef XIMAP_FEATURE_IMAP_COMMAND
#define XIMAP_FEATURE_IMAP_COMMAND
#endif
#ifndef XIMAP_MODULE_IMAP_CLIENT
#define XIMAP_MODULE_IMAP_CLIENT
#endif
#endif

/* imap_auth 及其直接依赖。 */
#if defined(XIMAP_MODULE_ALL) || defined(XIMAP_MODULE_IMAP_AUTH)
#ifndef XIMAP_FEATURE_IMAP_AUTH
#define XIMAP_FEATURE_IMAP_AUTH
#endif
#ifndef XIMAP_MODULE_IMAP_CLIENT
#define XIMAP_MODULE_IMAP_CLIENT
#endif
#ifndef XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_CODEC_BASE64
#endif
#endif

/* imap_client_tls 及其直接依赖。 */
#if defined(XIMAP_MODULE_ALL) || defined(XIMAP_MODULE_IMAP_CLIENT_TLS)
#ifndef XIMAP_FEATURE_IMAP_CLIENT_TLS
#define XIMAP_FEATURE_IMAP_CLIENT_TLS
#endif
#ifndef XIMAP_MODULE_IMAP_CLIENT
#define XIMAP_MODULE_IMAP_CLIENT
#endif
#ifndef XMAIL_MODULE_MAIL_NET_TLS
#define XMAIL_MODULE_MAIL_NET_TLS
#endif
#endif

/* imap_client 及其直接依赖。 */
#if defined(XIMAP_MODULE_ALL) || defined(XIMAP_MODULE_IMAP_CLIENT)
#ifndef XIMAP_FEATURE_IMAP_CLIENT
#define XIMAP_FEATURE_IMAP_CLIENT
#endif
#ifndef XIMAP_MODULE_IMAP
#define XIMAP_MODULE_IMAP
#endif
#ifndef XMAIL_MODULE_MAIL_NET
#define XMAIL_MODULE_MAIL_NET
#endif
#endif

/* imap_body 及其直接依赖。 */
#if defined(XIMAP_MODULE_ALL) || defined(XIMAP_MODULE_IMAP_BODY)
#ifndef XIMAP_FEATURE_IMAP_BODY
#define XIMAP_FEATURE_IMAP_BODY
#endif
#ifndef XIMAP_MODULE_IMAP_DATA
#define XIMAP_MODULE_IMAP_DATA
#endif
#endif

/* imap_data 及其直接依赖。 */
#if defined(XIMAP_MODULE_ALL) || defined(XIMAP_MODULE_IMAP_DATA)
#ifndef XIMAP_FEATURE_IMAP_DATA
#define XIMAP_FEATURE_IMAP_DATA
#endif
#ifndef XIMAP_MODULE_IMAP
#define XIMAP_MODULE_IMAP
#endif
#endif

/* imap 及其直接依赖。 */
#if defined(XIMAP_MODULE_ALL) || defined(XIMAP_MODULE_IMAP)
#ifndef XIMAP_FEATURE_IMAP
#define XIMAP_FEATURE_IMAP
#endif
#ifndef XMAIL_MODULE_MAIL_WIRE
#define XMAIL_MODULE_MAIL_WIRE
#endif
#endif

#endif /* XIMAP_FEATURES_H */
