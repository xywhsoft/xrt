#ifndef XRT_NET_TLS_H
#define XRT_NET_TLS_H

#include "xrt_net_types.h"

typedef enum {
	XRT_TLS_BACKEND_NONE = 0,
	XRT_TLS_BACKEND_MBEDTLS = 1,
	XRT_TLS_BACKEND_OPENSSL = 2,
	XRT_TLS_BACKEND_WOLFSSL = 3,
	XRT_TLS_BACKEND_BUILTIN = 4,
} xrt_tls_backend;

typedef enum {
	XRT_TLS_OK = 0,
	XRT_TLS_ERROR = -1,
	XRT_TLS_WANT_READ = -2,
	XRT_TLS_WANT_WRITE = -3,
	XRT_TLS_HANDSHAKE_PENDING = -4,
} xrt_tls_result;

typedef struct xrt_tls_config {
	xrt_tls_backend backend;
	const char* cert_file;
	const char* key_file;
	const char* ca_file;
	bool verify_peer;
	bool verify_host;
	const char* host_name;
} xrt_tls_config;

typedef struct xrt_tls_ctx {
	void* backend_ctx;
	xrt_tls_backend backend;
	bool is_server;
	bool handshake_done;
	xrt_net_connection* conn;
} xrt_tls_ctx;

typedef struct xrt_tls_backend_ops {
	xrt_tls_result (*init)();
	void (*cleanup)();
	xrt_tls_ctx* (*ctx_create)(const xrt_tls_config* config, bool is_server);
	void (*ctx_destroy)(xrt_tls_ctx* ctx);
	xrt_tls_result (*ctx_handshake)(xrt_tls_ctx* ctx);
	xrt_tls_result (*ctx_read)(xrt_tls_ctx* ctx, char* buf, size_t len, size_t* read_len);
	xrt_tls_result (*ctx_write)(xrt_tls_ctx* ctx, const char* buf, size_t len, size_t* write_len);
	xrt_tls_result (*ctx_close)(xrt_tls_ctx* ctx);
} xrt_tls_backend_ops;

typedef struct xrt_tls_backend_impl {
	const char* name;
	const xrt_tls_backend_ops* ops;
} xrt_tls_backend_impl;

xrt_tls_backend_impl* xrt_tls_backend_get(xrt_tls_backend backend);

xrt_tls_result xrt_tls_init();
void xrt_tls_cleanup();

xrt_tls_ctx* xrt_tls_ctx_create(const xrt_tls_config* config, bool is_server);
void xrt_tls_ctx_destroy(xrt_tls_ctx* ctx);

xrt_tls_result xrt_tls_handshake(xrt_tls_ctx* ctx);
xrt_tls_result xrt_tls_read(xrt_tls_ctx* ctx, char* buf, size_t len, size_t* read_len);
xrt_tls_result xrt_tls_write(xrt_tls_ctx* ctx, const char* buf, size_t len, size_t* write_len);
xrt_tls_result xrt_tls_close(xrt_tls_ctx* ctx);

bool xrt_tls_is_handshake_done(xrt_tls_ctx* ctx);
xrt_tls_backend xrt_tls_get_backend(xrt_tls_ctx* ctx);

static inline const char* xrt_tls_result_str(xrt_tls_result result)
{
	switch ( result ) {
		case XRT_TLS_OK: return "OK";
		case XRT_TLS_ERROR: return "ERROR";
		case XRT_TLS_WANT_READ: return "WANT_READ";
		case XRT_TLS_WANT_WRITE: return "WANT_WRITE";
		case XRT_TLS_HANDSHAKE_PENDING: return "HANDSHAKE_PENDING";
		default: return "UNKNOWN";
	}
}

#endif
