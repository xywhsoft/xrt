#if defined(XRT_TLS_OPENSSL)

#include "xrt_net_tls_openssl.h"
#include "../mempool.h"

static xrt_tls_result openssl_init()
{
	SSL_library_init();
	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();
	return XRT_TLS_OK;
}

static void openssl_cleanup()
{
	ERR_free_strings();
	EVP_cleanup();
}

static xrt_tls_ctx* openssl_ctx_create(const xrt_tls_config* config, bool is_server)
{
	xrt_tls_ctx* ctx = (xrt_tls_ctx*)xrtCalloc(1, sizeof(xrt_tls_ctx));
	if ( !ctx ) {
		return NULL;
	}

	xrt_openssl_ctx* octx = (xrt_openssl_ctx*)xrtCalloc(1, sizeof(xrt_openssl_ctx));
	if ( !octx ) {
		xrtFree(ctx);
		return NULL;
	}

	const SSL_METHOD* method;
	if ( is_server ) {
		method = TLS_server_method();
	} else {
		method = TLS_client_method();
	}

	octx->ssl_ctx = SSL_CTX_new(method);
	if ( !octx->ssl_ctx ) {
		goto error;
	}

	SSL_CTX_set_min_proto_version(octx->ssl_ctx, TLS1_3_VERSION);
	SSL_CTX_set_max_proto_version(octx->ssl_ctx, TLS1_3_VERSION);

	if ( is_server && config->cert_file && config->key_file ) {
		if ( SSL_CTX_use_certificate_file(octx->ssl_ctx, config->cert_file, SSL_FILETYPE_PEM) != 1 ) {
			goto error;
		}

		if ( SSL_CTX_use_PrivateKey_file(octx->ssl_ctx, config->key_file, SSL_FILETYPE_PEM) != 1 ) {
			goto error;
		}

		if ( SSL_CTX_check_private_key(octx->ssl_ctx) != 1 ) {
			goto error;
		}
	}

	if ( config->ca_file ) {
		if ( SSL_CTX_load_verify_locations(octx->ssl_ctx, config->ca_file, NULL) != 1 ) {
			goto error;
		}
	}

	if ( config->verify_peer ) {
		SSL_CTX_set_verify(octx->ssl_ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
	} else {
		SSL_CTX_set_verify(octx->ssl_ctx, SSL_VERIFY_NONE, NULL);
	}

	octx->ssl = SSL_new(octx->ssl_ctx);
	if ( !octx->ssl ) {
		goto error;
	}

	if ( !is_server && config->host_name ) {
		SSL_set_tlsext_host_name(octx->ssl, config->host_name);
	}

	ctx->backend_ctx = octx;
	return ctx;

error:
	if ( octx->ssl ) {
		SSL_free(octx->ssl);
	}
	if ( octx->ssl_ctx ) {
		SSL_CTX_free(octx->ssl_ctx);
	}
	xrtFree(octx);
	xrtFree(ctx);
	return NULL;
}

static void openssl_ctx_destroy(xrt_tls_ctx* ctx)
{
	if ( !ctx ) {
		return;
	}

	xrt_openssl_ctx* octx = (xrt_openssl_ctx*)ctx->backend_ctx;
	if ( octx ) {
		if ( octx->ssl ) {
			SSL_free(octx->ssl);
		}
		if ( octx->ssl_ctx ) {
			SSL_CTX_free(octx->ssl_ctx);
		}
		xrtFree(octx);
	}
}

static xrt_tls_result openssl_ctx_handshake(xrt_tls_ctx* ctx)
{
	if ( !ctx ) {
		return XRT_TLS_ERROR;
	}

	xrt_openssl_ctx* octx = (xrt_openssl_ctx*)ctx->backend_ctx;
	if ( !octx || !octx->ssl ) {
		return XRT_TLS_ERROR;
	}

	int ret = SSL_do_handshake(octx->ssl);
	if ( ret == 1 ) {
		return XRT_TLS_OK;
	}

	int err = SSL_get_error(octx->ssl, ret);
	if ( err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE ) {
		return err == SSL_ERROR_WANT_READ ? XRT_TLS_WANT_READ : XRT_TLS_WANT_WRITE;
	}

	return XRT_TLS_ERROR;
}

static xrt_tls_result openssl_ctx_read(xrt_tls_ctx* ctx, char* buf, size_t len, size_t* read_len)
{
	if ( !ctx || !buf || len == 0 ) {
		return XRT_TLS_ERROR;
	}

	xrt_openssl_ctx* octx = (xrt_openssl_ctx*)ctx->backend_ctx;
	if ( !octx || !octx->ssl ) {
		return XRT_TLS_ERROR;
	}

	int ret = SSL_read(octx->ssl, buf, (int)len);
	if ( ret > 0 ) {
		if ( read_len ) {
			*read_len = (size_t)ret;
		}
		return XRT_TLS_OK;
	}

	int err = SSL_get_error(octx->ssl, ret);
	if ( err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE ) {
		return err == SSL_ERROR_WANT_READ ? XRT_TLS_WANT_READ : XRT_TLS_WANT_WRITE;
	}

	return XRT_TLS_ERROR;
}

static xrt_tls_result openssl_ctx_write(xrt_tls_ctx* ctx, const char* buf, size_t len, size_t* write_len)
{
	if ( !ctx || !buf || len == 0 ) {
		return XRT_TLS_ERROR;
	}

	xrt_openssl_ctx* octx = (xrt_openssl_ctx*)ctx->backend_ctx;
	if ( !octx || !octx->ssl ) {
		return XRT_TLS_ERROR;
	}

	int ret = SSL_write(octx->ssl, buf, (int)len);
	if ( ret > 0 ) {
		if ( write_len ) {
			*write_len = (size_t)ret;
		}
		return XRT_TLS_OK;
	}

	int err = SSL_get_error(octx->ssl, ret);
	if ( err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE ) {
		return err == SSL_ERROR_WANT_READ ? XRT_TLS_WANT_READ : XRT_TLS_WANT_WRITE;
	}

	return XRT_TLS_ERROR;
}

static xrt_tls_result openssl_ctx_close(xrt_tls_ctx* ctx)
{
	if ( !ctx ) {
		return XRT_TLS_ERROR;
	}

	xrt_openssl_ctx* octx = (xrt_openssl_ctx*)ctx->backend_ctx;
	if ( !octx || !octx->ssl ) {
		return XRT_TLS_ERROR;
	}

	SSL_shutdown(octx->ssl);
	return XRT_TLS_OK;
}

static const xrt_tls_backend_ops openssl_ops = {
	.init = openssl_init,
	.cleanup = openssl_cleanup,
	.ctx_create = openssl_ctx_create,
	.ctx_destroy = openssl_ctx_destroy,
	.ctx_handshake = openssl_ctx_handshake,
	.ctx_read = openssl_ctx_read,
	.ctx_write = openssl_ctx_write,
	.ctx_close = openssl_ctx_close,
};

xrt_tls_backend_impl xrt_tls_openssl_backend = {
	.name = "OpenSSL",
	.ops = &openssl_ops,
};

#endif
