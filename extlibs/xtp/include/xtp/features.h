/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XTP_FEATURES_H
#define XTP_FEATURES_H

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
