#ifndef XRT_NET_TLS_WOLFSSL_H
#define XRT_NET_TLS_WOLFSSL_H

#include "xrt_net_tls.h"

#if defined(XRT_TLS_WOLFSSL)

#include <wolfssl/ssl.h>
#include <wolfssl/openssl/ssl.h>

typedef struct xrt_wolfssl_ctx {
	WOLFSSL* ssl;
	WOLFSSL_CTX* ssl_ctx;
} xrt_wolfssl_ctx;

xrt_tls_backend_impl xrt_tls_wolfssl_backend;

#endif

#endif
