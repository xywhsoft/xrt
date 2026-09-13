/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XPOP3_FEATURES_H
#define XPOP3_FEATURES_H

/* pop3_message 及其直接依赖。 */
#if defined(XPOP3_MODULE_ALL) || defined(XPOP3_MODULE_POP3_MESSAGE)
#ifndef XPOP3_FEATURE_POP3_MESSAGE
#define XPOP3_FEATURE_POP3_MESSAGE
#endif
#ifndef XPOP3_MODULE_POP3_CLIENT
#define XPOP3_MODULE_POP3_CLIENT
#endif
#ifndef XMAIL_MODULE_MAIL_TREE
#define XMAIL_MODULE_MAIL_TREE
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#endif

/* pop3_auth 及其直接依赖。 */
#if defined(XPOP3_MODULE_ALL) || defined(XPOP3_MODULE_POP3_AUTH)
#ifndef XPOP3_FEATURE_POP3_AUTH
#define XPOP3_FEATURE_POP3_AUTH
#endif
#ifndef XPOP3_MODULE_POP3_CLIENT
#define XPOP3_MODULE_POP3_CLIENT
#endif
#ifndef XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_CODEC_BASE64
#endif
#endif

/* pop3_client_tls 及其直接依赖。 */
#if defined(XPOP3_MODULE_ALL) || defined(XPOP3_MODULE_POP3_CLIENT_TLS)
#ifndef XPOP3_FEATURE_POP3_CLIENT_TLS
#define XPOP3_FEATURE_POP3_CLIENT_TLS
#endif
#ifndef XPOP3_MODULE_POP3_CLIENT
#define XPOP3_MODULE_POP3_CLIENT
#endif
#ifndef XMAIL_MODULE_MAIL_NET_TLS
#define XMAIL_MODULE_MAIL_NET_TLS
#endif
#endif

/* pop3_client 及其直接依赖。 */
#if defined(XPOP3_MODULE_ALL) || defined(XPOP3_MODULE_POP3_CLIENT)
#ifndef XPOP3_FEATURE_POP3_CLIENT
#define XPOP3_FEATURE_POP3_CLIENT
#endif
#ifndef XPOP3_MODULE_POP3
#define XPOP3_MODULE_POP3
#endif
#ifndef XMAIL_MODULE_MAIL_NET
#define XMAIL_MODULE_MAIL_NET
#endif
#endif

/* pop3 及其直接依赖。 */
#if defined(XPOP3_MODULE_ALL) || defined(XPOP3_MODULE_POP3)
#ifndef XPOP3_FEATURE_POP3
#define XPOP3_FEATURE_POP3
#endif
#ifndef XMAIL_MODULE_MAIL_WIRE
#define XMAIL_MODULE_MAIL_WIRE
#endif
#endif

#endif /* XPOP3_FEATURES_H */
