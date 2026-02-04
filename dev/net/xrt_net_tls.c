#include "xrt_net_tls.h"
#include "xrt_net_tls_mbedtls.h"
#include "xrt_net_tls_openssl.h"
#include "xrt_net_tls_wolfssl.h"
#include "xrt_net_tls_builtin.h"

static bool g_tls_initialized = false;
static xrt_tls_backend_impl* g_backends[5] = { NULL, NULL, NULL, NULL, NULL };

xrt_tls_backend_impl* xrt_tls_backend_get(xrt_tls_backend backend)
{
	switch ( backend ) {
		case XRT_TLS_BACKEND_MBEDTLS:
#if defined(XRT_TLS_MBEDTLS)
			return &xrt_tls_mbedtls_backend;
#else
			return NULL;
#endif
		case XRT_TLS_BACKEND_OPENSSL:
#if defined(XRT_TLS_OPENSSL)
			return &xrt_tls_openssl_backend;
#else
			return NULL;
#endif
		case XRT_TLS_BACKEND_WOLFSSL:
#if defined(XRT_TLS_WOLFSSL)
			return &xrt_tls_wolfssl_backend;
#else
			return NULL;
#endif
		case XRT_TLS_BACKEND_BUILTIN:
#if defined(XRT_TLS_BUILTIN)
			return &xrt_tls_builtin_backend;
#else
			return NULL;
#endif
		default:
			return NULL;
	}
}

xrt_tls_result xrt_tls_init()
{
	if ( g_tls_initialized ) {
		return XRT_TLS_OK;
	}

#if defined(XRT_TLS_MBEDTLS)
	if ( xrt_tls_mbedtls_backend.ops->init() != XRT_TLS_OK ) {
		return XRT_TLS_ERROR;
	}
#endif

#if defined(XRT_TLS_OPENSSL)
	if ( xrt_tls_openssl_backend.ops->init() != XRT_TLS_OK ) {
		return XRT_TLS_ERROR;
	}
#endif

#if defined(XRT_TLS_WOLFSSL)
	if ( xrt_tls_wolfssl_backend.ops->init() != XRT_TLS_OK ) {
		return XRT_TLS_ERROR;
	}
#endif

#if defined(XRT_TLS_BUILTIN)
	if ( xrt_tls_builtin_backend.ops->init() != XRT_TLS_OK ) {
		return XRT_TLS_ERROR;
	}
#endif

	g_tls_initialized = true;
	return XRT_TLS_OK;
}

void xrt_tls_cleanup()
{
	if ( !g_tls_initialized ) {
		return;
	}

#if defined(XRT_TLS_MBEDTLS)
	xrt_tls_mbedtls_backend.ops->cleanup();
#endif

#if defined(XRT_TLS_OPENSSL)
	xrt_tls_openssl_backend.ops->cleanup();
#endif

#if defined(XRT_TLS_WOLFSSL)
	xrt_tls_wolfssl_backend.ops->cleanup();
#endif

#if defined(XRT_TLS_BUILTIN)
	xrt_tls_builtin_backend.ops->cleanup();
#endif

	g_tls_initialized = false;
}

xrt_tls_ctx* xrt_tls_ctx_create(const xrt_tls_config* config, bool is_server)
{
	if ( !config || config->backend == XRT_TLS_BACKEND_NONE ) {
		return NULL;
	}

	xrt_tls_backend_impl* backend = xrt_tls_backend_get(config->backend);
	if ( !backend ) {
		return NULL;
	}

	xrt_tls_ctx* ctx = (xrt_tls_ctx*)xrtCalloc(1, sizeof(xrt_tls_ctx));
	if ( !ctx ) {
		return NULL;
	}

	ctx->backend_ctx = backend->ops->ctx_create(config, is_server);
	if ( !ctx->backend_ctx ) {
		xrtFree(ctx);
		return NULL;
	}

	ctx->backend = config->backend;
	ctx->is_server = is_server;
	ctx->handshake_done = false;

	return ctx;
}

void xrt_tls_ctx_destroy(xrt_tls_ctx* ctx)
{
	if ( !ctx ) {
		return;
	}

	xrt_tls_backend_impl* backend = xrt_tls_backend_get(ctx->backend);
	if ( backend && backend->ops->ctx_destroy ) {
		backend->ops->ctx_destroy(ctx);
	}

	xrtFree(ctx);
}

xrt_tls_result xrt_tls_handshake(xrt_tls_ctx* ctx)
{
	if ( !ctx ) {
		return XRT_TLS_ERROR;
	}

	xrt_tls_backend_impl* backend = xrt_tls_backend_get(ctx->backend);
	if ( !backend || !backend->ops->ctx_handshake ) {
		return XRT_TLS_ERROR;
	}

	xrt_tls_result result = backend->ops->ctx_handshake(ctx);
	if ( result == XRT_TLS_OK ) {
		ctx->handshake_done = true;
	}

	return result;
}

xrt_tls_result xrt_tls_read(xrt_tls_ctx* ctx, char* buf, size_t len, size_t* read_len)
{
	if ( !ctx || !buf || len == 0 ) {
		return XRT_TLS_ERROR;
	}

	xrt_tls_backend_impl* backend = xrt_tls_backend_get(ctx->backend);
	if ( !backend || !backend->ops->ctx_read ) {
		return XRT_TLS_ERROR;
	}

	return backend->ops->ctx_read(ctx, buf, len, read_len);
}

xrt_tls_result xrt_tls_write(xrt_tls_ctx* ctx, const char* buf, size_t len, size_t* write_len)
{
	if ( !ctx || !buf || len == 0 ) {
		return XRT_TLS_ERROR;
	}

	xrt_tls_backend_impl* backend = xrt_tls_backend_get(ctx->backend);
	if ( !backend || !backend->ops->ctx_write ) {
		return XRT_TLS_ERROR;
	}

	return backend->ops->ctx_write(ctx, buf, len, write_len);
}

xrt_tls_result xrt_tls_close(xrt_tls_ctx* ctx)
{
	if ( !ctx ) {
		return XRT_TLS_ERROR;
	}

	xrt_tls_backend_impl* backend = xrt_tls_backend_get(ctx->backend);
	if ( !backend || !backend->ops->ctx_close ) {
		return XRT_TLS_ERROR;
	}

	return backend->ops->ctx_close(ctx);
}

bool xrt_tls_is_handshake_done(xrt_tls_ctx* ctx)
{
	return ctx ? ctx->handshake_done : false;
}

xrt_tls_backend xrt_tls_get_backend(xrt_tls_ctx* ctx)
{
	return ctx ? ctx->backend : XRT_TLS_BACKEND_NONE;
}
