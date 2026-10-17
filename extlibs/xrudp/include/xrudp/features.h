/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XRUDP_FEATURES_H
#define XRUDP_FEATURES_H

/* rudp_sent 及其直接依赖。 */
#if defined(XRUDP_MODULE_ALL) || defined(XRUDP_MODULE_RUDP_SENT)
#ifndef XRUDP_FEATURE_RUDP_SENT
#define XRUDP_FEATURE_RUDP_SENT
#endif
#ifndef XRUDP_MODULE_RUDP_ACK
#define XRUDP_MODULE_RUDP_ACK
#endif
#endif

/* rudp_timing 及其直接依赖。 */
#if defined(XRUDP_MODULE_ALL) || defined(XRUDP_MODULE_RUDP_TIMING)
#ifndef XRUDP_FEATURE_RUDP_TIMING
#define XRUDP_FEATURE_RUDP_TIMING
#endif
#ifndef XRUDP_MODULE_RUDP_WIRE
#define XRUDP_MODULE_RUDP_WIRE
#endif
#endif

/* rudp_ack 及其直接依赖。 */
#if defined(XRUDP_MODULE_ALL) || defined(XRUDP_MODULE_RUDP_ACK)
#ifndef XRUDP_FEATURE_RUDP_ACK
#define XRUDP_FEATURE_RUDP_ACK
#endif
#ifndef XRUDP_MODULE_RUDP_WIRE
#define XRUDP_MODULE_RUDP_WIRE
#endif
#endif

/* rudp_cookie 及其直接依赖。 */
#if defined(XRUDP_MODULE_ALL) || defined(XRUDP_MODULE_RUDP_COOKIE)
#ifndef XRUDP_FEATURE_RUDP_COOKIE
#define XRUDP_FEATURE_RUDP_COOKIE
#endif
#ifndef XRUDP_MODULE_RUDP_WIRE
#define XRUDP_MODULE_RUDP_WIRE
#endif
#ifndef XRT_MODULE_CRYPTO_HMAC_SHA256
#define XRT_MODULE_CRYPTO_HMAC_SHA256
#endif
#endif

/* rudp_noise 及其直接依赖。 */
#if defined(XRUDP_MODULE_ALL) || defined(XRUDP_MODULE_RUDP_NOISE)
#ifndef XRUDP_FEATURE_RUDP_NOISE
#define XRUDP_FEATURE_RUDP_NOISE
#endif
#ifndef XRUDP_MODULE_RUDP_CRYPTO
#define XRUDP_MODULE_RUDP_CRYPTO
#endif
#ifndef XRT_MODULE_CRYPTO_X25519
#define XRT_MODULE_CRYPTO_X25519
#endif
#ifndef XRT_MODULE_CRYPTO_HKDF_SHA256
#define XRT_MODULE_CRYPTO_HKDF_SHA256
#endif
#ifndef XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_RANDOM_SECURE
#endif
#endif

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
