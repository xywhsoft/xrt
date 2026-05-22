#if defined(XRT_TLS_MBEDTLS)

#include "xrt_net_tls_mbedtls.h"
#include "../mempool.h"

static void mbedtls_debug(void* ctx, int level, const char* file, int line, const char* str)
{
	(void)ctx;
	(void)level;
	(void)file;
	(void)line;
}

static xrt_tls_result mbedtls_init()
{
	mbedtls_debug_set_threshold(0);
	return XRT_TLS_OK;
}

static void mbedtls_cleanup()
{
}

static int mbedtls_net_send(void* ctx, const unsigned char* buf, size_t len)
{
	xrt_net_connection* conn = (xrt_net_connection*)ctx;
	size_t sent = 0;
	xrt_net_result result = xrt_net_socket_send(conn, (const char*)buf, len, &sent);
	if ( result == XRT_NET_OK ) {
		return (int)sent;
	} else if ( result == XRT_NET_AGAIN ) {
		return MBEDTLS_ERR_SSL_WANT_WRITE;
	}
	return MBEDTLS_ERR_NET_SEND_FAILED;
}

static int mbedtls_net_recv(void* ctx, unsigned char* buf, size_t len)
{
	xrt_net_connection* conn = (xrt_net_connection*)ctx;
	size_t received = 0;
	xrt_net_result result = xrt_net_socket_recv(conn, (char*)buf, len, &received);
	if ( result == XRT_NET_OK ) {
		return (int)received;
	} else if ( result == XRT_NET_AGAIN ) {
		return MBEDTLS_ERR_SSL_WANT_READ;
	} else if ( result == XRT_NET_CLOSED ) {
		return MBEDTLS_ERR_NET_CONN_RESET;
	}
	return MBEDTLS_ERR_NET_RECV_FAILED;
}

static xrt_tls_ctx* mbedtls_ctx_create(const xrt_tls_config* config, bool is_server)
{
	xrt_tls_ctx* ctx = (xrt_tls_ctx*)xrtCalloc(1, sizeof(xrt_tls_ctx));
	if ( !ctx ) {
		return NULL;
	}

	xrt_mbedtls_ctx* mctx = (xrt_mbedtls_ctx*)xrtCalloc(1, sizeof(xrt_mbedtls_ctx));
	if ( !mctx ) {
		xrtFree(ctx);
		return NULL;
	}

	mbedtls_ssl_init(&mctx->ssl);
	mbedtls_ssl_config_init(&mctx->conf);
	mbedtls_ctr_drbg_init(&mctx->ctr_drbg);
	mbedtls_entropy_init(&mctx->entropy);

	const char* pers = "xrt_net_tls";
	int ret = mbedtls_ctr_drbg_seed(&mctx->ctr_drbg, mbedtls_entropy_func, &mctx->entropy,
		(const unsigned char*)pers, strlen(pers));
	if ( ret != 0 ) {
		goto error;
	}

	ret = mbedtls_ssl_config_defaults(&mctx->conf,
		is_server ? MBEDTLS_SSL_IS_SERVER : MBEDTLS_SSL_IS_CLIENT,
		MBEDTLS_SSL_TRANSPORT_STREAM,
		MBEDTLS_SSL_PRESET_DEFAULT);
	if ( ret != 0 ) {
		goto error;
	}

	mbedtls_ssl_conf_rng(&mctx->conf, mbedtls_ctr_drbg_random, &mctx->ctr_drbg);

	mbedtls_ssl_conf_authmode(&mctx->conf, config->verify_peer ? MBEDTLS_SSL_VERIFY_REQUIRED : MBEDTLS_SSL_VERIFY_NONE);

	if ( config->ca_file ) {
		ret = mbedtls_x509_crt_parse_file(&mctx->conf.ca_chain, config->ca_file);
		if ( ret != 0 ) {
			goto error;
		}
	}

	if ( is_server && config->cert_file && config->key_file ) {
		ret = mbedtls_x509_crt_parse_file(&mctx->conf.own_cert, config->cert_file);
		if ( ret != 0 ) {
			goto error;
		}

		ret = mbedtls_pk_parse_keyfile(&mctx->conf.pkey, config->key_file, NULL);
		if ( ret != 0 ) {
			goto error;
		}
	}

	mbedtls_ssl_conf_min_version(&mctx->conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
	mbedtls_ssl_conf_max_version(&mctx->conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_4);

	ret = mbedtls_ssl_setup(&mctx->ssl, &mctx->conf);
	if ( ret != 0 ) {
		goto error;
	}

	if ( !is_server && config->host_name ) {
		mbedtls_ssl_set_hostname(&mctx->ssl, config->host_name);
	}

	ctx->backend_ctx = mctx;
	return ctx;

error:
	mbedtls_ssl_free(&mctx->ssl);
	mbedtls_ssl_config_free(&mctx->conf);
	mbedtls_ctr_drbg_free(&mctx->ctr_drbg);
	mbedtls_entropy_free(&mctx->entropy);
	xrtFree(mctx);
	xrtFree(ctx);
	return NULL;
}

static void mbedtls_ctx_destroy(xrt_tls_ctx* ctx)
{
	if ( !ctx ) {
		return;
	}

	xrt_mbedtls_ctx* mctx = (xrt_mbedtls_ctx*)ctx->backend_ctx;
	if ( mctx ) {
		mbedtls_ssl_free(&mctx->ssl);
		mbedtls_ssl_config_free(&mctx->conf);
		mbedtls_ctr_drbg_free(&mctx->ctr_drbg);
		mbedtls_entropy_free(&mctx->entropy);
		xrtFree(mctx);
	}
}

static xrt_tls_result mbedtls_ctx_handshake(xrt_tls_ctx* ctx)
{
	if ( !ctx ) {
		return XRT_TLS_ERROR;
	}

	xrt_mbedtls_ctx* mctx = (xrt_mbedtls_ctx*)ctx->backend_ctx;
	if ( !mctx ) {
		return XRT_TLS_ERROR;
	}

	mbedtls_ssl_set_bio(&mctx->ssl, ctx->conn, mbedtls_net_send, mbedtls_net_recv, NULL);

	int ret = mbedtls_ssl_handshake(&mctx->ssl);
	if ( ret == 0 ) {
		return XRT_TLS_OK;
	} else if ( ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE ) {
		return ret == MBEDTLS_ERR_SSL_WANT_READ ? XRT_TLS_WANT_READ : XRT_TLS_WANT_WRITE;
	}

	return XRT_TLS_ERROR;
}

static xrt_tls_result mbedtls_ctx_read(xrt_tls_ctx* ctx, char* buf, size_t len, size_t* read_len)
{
	if ( !ctx || !buf || len == 0 ) {
		return XRT_TLS_ERROR;
	}

	xrt_mbedtls_ctx* mctx = (xrt_mbedtls_ctx*)ctx->backend_ctx;
	if ( !mctx ) {
		return XRT_TLS_ERROR;
	}

	int ret = mbedtls_ssl_read(&mctx->ssl, (unsigned char*)buf, len);
	if ( ret > 0 ) {
		if ( read_len ) {
			*read_len = (size_t)ret;
		}
		return XRT_TLS_OK;
	} else if ( ret == 0 ) {
		return XRT_TLS_ERROR;
	} else if ( ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE ) {
		return ret == MBEDTLS_ERR_SSL_WANT_READ ? XRT_TLS_WANT_READ : XRT_TLS_WANT_WRITE;
	}

	return XRT_TLS_ERROR;
}

static xrt_tls_result mbedtls_ctx_write(xrt_tls_ctx* ctx, const char* buf, size_t len, size_t* write_len)
{
	if ( !ctx || !buf || len == 0 ) {
		return XRT_TLS_ERROR;
	}

	xrt_mbedtls_ctx* mctx = (xrt_mbedtls_ctx*)ctx->backend_ctx;
	if ( !mctx ) {
		return XRT_TLS_ERROR;
	}

	int ret = mbedtls_ssl_write(&mctx->ssl, (const unsigned char*)buf, len);
	if ( ret > 0 ) {
		if ( write_len ) {
			*write_len = (size_t)ret;
		}
		return XRT_TLS_OK;
	} else if ( ret == 0 ) {
		return XRT_TLS_ERROR;
	} else if ( ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE ) {
		return ret == MBEDTLS_ERR_SSL_WANT_READ ? XRT_TLS_WANT_READ : XRT_TLS_WANT_WRITE;
	}

	return XRT_TLS_ERROR;
}

static xrt_tls_result mbedtls_ctx_close(xrt_tls_ctx* ctx)
{
	if ( !ctx ) {
		return XRT_TLS_ERROR;
	}

	xrt_mbedtls_ctx* mctx = (xrt_mbedtls_ctx*)ctx->backend_ctx;
	if ( !mctx ) {
		return XRT_TLS_ERROR;
	}

	mbedtls_ssl_close_notify(&mctx->ssl);
	return XRT_TLS_OK;
}

static const xrt_tls_backend_ops mbedtls_ops = {
	.init = mbedtls_init,
	.cleanup = mbedtls_cleanup,
	.ctx_create = mbedtls_ctx_create,
	.ctx_destroy = mbedtls_ctx_destroy,
	.ctx_handshake = mbedtls_ctx_handshake,
	.ctx_read = mbedtls_ctx_read,
	.ctx_write = mbedtls_ctx_write,
	.ctx_close = mbedtls_ctx_close,
};

xrt_tls_backend_impl xrt_tls_mbedtls_backend = {
	.name = "mbedTLS",
	.ops = &mbedtls_ops,
};

#endif
