#if defined(XRT_TLS_WOLFSSL)

#include "xrt_net_tls_wolfssl.h"
#include "../mempool.h"

static xrt_tls_result wolfssl_init()
{
	wolfSSL_Init();
	return XRT_TLS_OK;
}

static void wolfssl_cleanup()
{
	wolfSSL_Cleanup();
}

static int wolfssl_net_send(WOLFSSL* ssl, char* buf, int sz, void* ctx)
{
	xrt_net_connection* conn = (xrt_net_connection*)ctx;
	size_t sent = 0;
	xrt_net_result result = xrt_net_socket_send(conn, buf, sz, &sent);
	if ( result == XRT_NET_OK ) {
		return (int)sent;
	} else if ( result == XRT_NET_AGAIN ) {
		return WOLFSSL_ERROR_WANT_WRITE;
	}
	return WOLFSSL_FATAL_ERROR;
}

static int wolfssl_net_recv(WOLFSSL* ssl, char* buf, int sz, void* ctx)
{
	xrt_net_connection* conn = (xrt_net_connection*)ctx;
	size_t received = 0;
	xrt_net_result result = xrt_net_socket_recv(conn, buf, sz, &received);
	if ( result == XRT_NET_OK ) {
		return (int)received;
	} else if ( result == XRT_NET_AGAIN ) {
		return WOLFSSL_ERROR_WANT_READ;
	} else if ( result == XRT_NET_CLOSED ) {
		return WOLFSSL_SOCKET_ERROR_E;
	}
	return WOLFSSL_FATAL_ERROR;
}

static xrt_tls_ctx* wolfssl_ctx_create(const xrt_tls_config* config, bool is_server)
{
	xrt_tls_ctx* ctx = (xrt_tls_ctx*)xrtCalloc(1, sizeof(xrt_tls_ctx));
	if ( !ctx ) {
		return NULL;
	}

	xrt_wolfssl_ctx* wctx = (xrt_wolfssl_ctx*)xrtCalloc(1, sizeof(xrt_wolfssl_ctx));
	if ( !wctx ) {
		xrtFree(ctx);
		return NULL;
	}

	int method;
	if ( is_server ) {
		method = wolfTLSv1_3_server_method();
	} else {
		method = wolfTLSv1_3_client_method();
	}

	wctx->ssl_ctx = wolfSSL_CTX_new(method);
	if ( !wctx->ssl_ctx ) {
		goto error;
	}

	if ( is_server && config->cert_file && config->key_file ) {
		if ( wolfSSL_CTX_use_certificate_file(wctx->ssl_ctx, config->cert_file, SSL_FILETYPE_PEM) != SSL_SUCCESS ) {
			goto error;
		}

		if ( wolfSSL_CTX_use_PrivateKey_file(wctx->ssl_ctx, config->key_file, SSL_FILETYPE_PEM) != SSL_SUCCESS ) {
			goto error;
		}
	}

	if ( config->ca_file ) {
		if ( wolfSSL_CTX_load_verify_locations(wctx->ssl_ctx, config->ca_file, NULL) != SSL_SUCCESS ) {
			goto error;
		}
	}

	if ( config->verify_peer ) {
		wolfSSL_CTX_set_verify(wctx->ssl_ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
	} else {
		wolfSSL_CTX_set_verify(wctx->ssl_ctx, SSL_VERIFY_NONE, NULL);
	}

	wctx->ssl = wolfSSL_new(wctx->ssl_ctx);
	if ( !wctx->ssl ) {
		goto error;
	}

	wolfSSL_SetIORecv(wctx->ssl, wolfssl_net_recv);
	wolfSSL_SetIOSend(wctx->ssl, wolfssl_net_send);

	if ( !is_server && config->host_name ) {
		wolfSSL_UseSNI(wctx->ssl, WOLFSSL_SNI_HOST_NAME, config->host_name, strlen(config->host_name));
	}

	ctx->backend_ctx = wctx;
	return ctx;

error:
	if ( wctx->ssl ) {
		wolfSSL_free(wctx->ssl);
	}
	if ( wctx->ssl_ctx ) {
		wolfSSL_CTX_free(wctx->ssl_ctx);
	}
	xrtFree(wctx);
	xrtFree(ctx);
	return NULL;
}

static void wolfssl_ctx_destroy(xrt_tls_ctx* ctx)
{
	if ( !ctx ) {
		return;
	}

	xrt_wolfssl_ctx* wctx = (xrt_wolfssl_ctx*)ctx->backend_ctx;
	if ( wctx ) {
		if ( wctx->ssl ) {
			wolfSSL_free(wctx->ssl);
		}
		if ( wctx->ssl_ctx ) {
			wolfSSL_CTX_free(wctx->ssl_ctx);
		}
		xrtFree(wctx);
	}
}

static xrt_tls_result wolfssl_ctx_handshake(xrt_tls_ctx* ctx)
{
	if ( !ctx ) {
		return XRT_TLS_ERROR;
	}

	xrt_wolfssl_ctx* wctx = (xrt_wolfssl_ctx*)ctx->backend_ctx;
	if ( !wctx || !wctx->ssl ) {
		return XRT_TLS_ERROR;
	}

	wolfSSL_SetIOReadCtx(wctx->ssl, ctx->conn);
	wolfSSL_SetIOWriteCtx(wctx->ssl, ctx->conn);

	int ret = wolfSSL_connect(wctx->ssl);
	if ( ret == SSL_SUCCESS ) {
		return XRT_TLS_OK;
	}

	int err = wolfSSL_get_error(wctx->ssl, ret);
	if ( err == WOLFSSL_ERROR_WANT_READ || err == WOLFSSL_ERROR_WANT_WRITE ) {
		return err == WOLFSSL_ERROR_WANT_READ ? XRT_TLS_WANT_READ : XRT_TLS_WANT_WRITE;
	}

	return XRT_TLS_ERROR;
}

static xrt_tls_result wolfssl_ctx_read(xrt_tls_ctx* ctx, char* buf, size_t len, size_t* read_len)
{
	if ( !ctx || !buf || len == 0 ) {
		return XRT_TLS_ERROR;
	}

	xrt_wolfssl_ctx* wctx = (xrt_wolfssl_ctx*)ctx->backend_ctx;
	if ( !wctx || !wctx->ssl ) {
		return XRT_TLS_ERROR;
	}

	int ret = wolfSSL_read(wctx->ssl, buf, (int)len);
	if ( ret > 0 ) {
		if ( read_len ) {
			*read_len = (size_t)ret;
		}
		return XRT_TLS_OK;
	}

	int err = wolfSSL_get_error(wctx->ssl, ret);
	if ( err == WOLFSSL_ERROR_WANT_READ || err == WOLFSSL_ERROR_WANT_WRITE ) {
		return err == WOLFSSL_ERROR_WANT_READ ? XRT_TLS_WANT_READ : XRT_TLS_WANT_WRITE;
	}

	return XRT_TLS_ERROR;
}

static xrt_tls_result wolfssl_ctx_write(xrt_tls_ctx* ctx, const char* buf, size_t len, size_t* write_len)
{
	if ( !ctx || !buf || len == 0 ) {
		return XRT_TLS_ERROR;
	}

	xrt_wolfssl_ctx* wctx = (xrt_wolfssl_ctx*)ctx->backend_ctx;
	if ( !wctx || !wctx->ssl ) {
		return XRT_TLS_ERROR;
	}

	int ret = wolfSSL_write(wctx->ssl, buf, (int)len);
	if ( ret > 0 ) {
		if ( write_len ) {
			*write_len = (size_t)ret;
		}
		return XRT_TLS_OK;
	}

	int err = wolfSSL_get_error(wctx->ssl, ret);
	if ( err == WOLFSSL_ERROR_WANT_READ || err == WOLFSSL_ERROR_WANT_WRITE ) {
		return err == WOLFSSL_ERROR_WANT_READ ? XRT_TLS_WANT_READ : XRT_TLS_WANT_WRITE;
	}

	return XRT_TLS_ERROR;
}

static xrt_tls_result wolfssl_ctx_close(xrt_tls_ctx* ctx)
{
	if ( !ctx ) {
		return XRT_TLS_ERROR;
	}

	xrt_wolfssl_ctx* wctx = (xrt_wolfssl_ctx*)ctx->backend_ctx;
	if ( !wctx || !wctx->ssl ) {
		return XRT_TLS_ERROR;
	}

	wolfSSL_shutdown(wctx->ssl);
	return XRT_TLS_OK;
}

static const xrt_tls_backend_ops wolfssl_ops = {
	.init = wolfssl_init,
	.cleanup = wolfssl_cleanup,
	.ctx_create = wolfssl_ctx_create,
	.ctx_destroy = wolfssl_ctx_destroy,
	.ctx_handshake = wolfssl_ctx_handshake,
	.ctx_read = wolfssl_ctx_read,
	.ctx_write = wolfssl_ctx_write,
	.ctx_close = wolfssl_ctx_close,
};

xrt_tls_backend_impl xrt_tls_wolfssl_backend = {
	.name = "WolfSSL",
	.ops = &wolfssl_ops,
};

#endif
