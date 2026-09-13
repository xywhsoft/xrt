/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XSMTP_FEATURES_H
#define XSMTP_FEATURES_H

/* smtp_auth 及其直接依赖。 */
#if defined(XSMTP_MODULE_ALL) || defined(XSMTP_MODULE_SMTP_AUTH)
#ifndef XSMTP_FEATURE_SMTP_AUTH
#define XSMTP_FEATURE_SMTP_AUTH
#endif
#ifndef XSMTP_MODULE_SMTP_CLIENT
#define XSMTP_MODULE_SMTP_CLIENT
#endif
#ifndef XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_CODEC_BASE64
#endif
#endif

/* smtp_submit 及其直接依赖。 */
#if defined(XSMTP_MODULE_ALL) || defined(XSMTP_MODULE_SMTP_SUBMIT)
#ifndef XSMTP_FEATURE_SMTP_SUBMIT
#define XSMTP_FEATURE_SMTP_SUBMIT
#endif
#ifndef XSMTP_MODULE_SMTP_CLIENT
#define XSMTP_MODULE_SMTP_CLIENT
#endif
#ifndef XMAIL_MODULE_MAIL_COMPOSE
#define XMAIL_MODULE_MAIL_COMPOSE
#endif
#endif

/* smtp_client_tls 及其直接依赖。 */
#if defined(XSMTP_MODULE_ALL) || defined(XSMTP_MODULE_SMTP_CLIENT_TLS)
#ifndef XSMTP_FEATURE_SMTP_CLIENT_TLS
#define XSMTP_FEATURE_SMTP_CLIENT_TLS
#endif
#ifndef XSMTP_MODULE_SMTP_CLIENT
#define XSMTP_MODULE_SMTP_CLIENT
#endif
#ifndef XMAIL_MODULE_MAIL_NET_TLS
#define XMAIL_MODULE_MAIL_NET_TLS
#endif
#endif

/* smtp_client 及其直接依赖。 */
#if defined(XSMTP_MODULE_ALL) || defined(XSMTP_MODULE_SMTP_CLIENT)
#ifndef XSMTP_FEATURE_SMTP_CLIENT
#define XSMTP_FEATURE_SMTP_CLIENT
#endif
#ifndef XSMTP_MODULE_SMTP
#define XSMTP_MODULE_SMTP
#endif
#ifndef XMAIL_MODULE_MAIL_NET
#define XMAIL_MODULE_MAIL_NET
#endif
#endif

/* smtp 及其直接依赖。 */
#if defined(XSMTP_MODULE_ALL) || defined(XSMTP_MODULE_SMTP)
#ifndef XSMTP_FEATURE_SMTP
#define XSMTP_FEATURE_SMTP
#endif
#ifndef XMAIL_MODULE_MAIL_WIRE
#define XMAIL_MODULE_MAIL_WIRE
#endif
#endif

#endif /* XSMTP_FEATURES_H */
