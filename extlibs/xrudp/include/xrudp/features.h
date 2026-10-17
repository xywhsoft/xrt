/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XRUDP_FEATURES_H
#define XRUDP_FEATURES_H

/* rudp_crypto 及其直接依赖。 */
#if defined(XRUDP_MODULE_ALL) || defined(XRUDP_MODULE_RUDP_CRYPTO)
#ifndef XRUDP_FEATURE_RUDP_CRYPTO
#define XRUDP_FEATURE_RUDP_CRYPTO
#endif
#ifndef XRUDP_MODULE_RUDP_WIRE
#define XRUDP_MODULE_RUDP_WIRE
#endif
#ifndef XRT_MODULE_CRYPTO_CHACHA20_POLY1305
#define XRT_MODULE_CRYPTO_CHACHA20_POLY1305
#endif
#endif

/* rudp_wire 及其直接依赖。 */
#if defined(XRUDP_MODULE_ALL) || defined(XRUDP_MODULE_RUDP_WIRE)
#ifndef XRUDP_FEATURE_RUDP_WIRE
#define XRUDP_FEATURE_RUDP_WIRE
#endif
#endif

#endif /* XRUDP_FEATURES_H */
