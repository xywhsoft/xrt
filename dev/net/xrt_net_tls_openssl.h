#ifndef XRT_NET_TLS_OPENSSL_H
#define XRT_NET_TLS_OPENSSL_H

#include "xrt_net_tls.h"

#if defined(XRT_TLS_OPENSSL)

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>

typedef struct xrt_openssl_ctx {
	SSL* ssl;
	SSL_CTX* ssl_ctx;
} xrt_openssl_ctx;

xrt_tls_backend_impl xrt_tls_openssl_backend;

#endif

#endif
