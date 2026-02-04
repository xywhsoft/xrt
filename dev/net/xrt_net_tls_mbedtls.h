#ifndef XRT_NET_TLS_MBEDTLS_H
#define XRT_NET_TLS_MBEDTLS_H

#include "xrt_net_tls.h"

#if defined(XRT_TLS_MBEDTLS)

#include <mbedtls/ssl.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/debug.h>
#include <mbedtls/error.h>

typedef struct xrt_mbedtls_ctx {
	mbedtls_ssl_context ssl;
	mbedtls_ssl_config conf;
	mbedtls_ctr_drbg_context ctr_drbg;
	mbedtls_entropy_context entropy;
} xrt_mbedtls_ctx;

xrt_tls_backend_impl xrt_tls_mbedtls_backend;

#endif

#endif
