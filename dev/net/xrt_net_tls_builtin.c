#if defined(XRT_TLS_BUILTIN)

#include "xrt_net_tls_builtin.h"
#include "../crypto/sha256.h"
#include "../crypto/aes.h"
#include "../crypto/chacha20.h"
#include "../crypto/poly1305.h"
#include "../crypto/x25519.h"
#include "../mempool.h"
#include <string.h>

static bool g_builtin_initialized = false;

static uint8_t zeros[32] = {0};
static uint8_t zeros_sha256_digest[32] = {
	0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14, 0x9a, 0xfb, 0xf4,
	0xc8, 0x99, 0x6f, 0xb9, 0x24, 0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b,
	0x93, 0x4c, 0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
};

static inline uint16_t load_be16(const uint8_t* buf)
{
	return (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);
}

static inline uint32_t load_be32(const uint8_t* buf)
{
	return (uint32_t)((uint32_t)buf[0] << 24 | (uint32_t)buf[1] << 16 |
	                 (uint32_t)buf[2] << 8 | buf[3]);
}

static inline void store_be16(uint8_t* buf, uint16_t val)
{
	buf[0] = (val >> 8) & 0xff;
	buf[1] = val & 0xff;
}

static inline void store_be32(uint8_t* buf, uint32_t val)
{
	buf[0] = (val >> 24) & 0xff;
	buf[1] = (val >> 16) & 0xff;
	buf[2] = (val >> 8) & 0xff;
	buf[3] = val & 0xff;
}

static void sha256_hmac(const uint8_t* key, size_t key_len, const uint8_t* msg,
		     size_t msg_len, uint8_t* out)
{
	uint8_t kpad[64];
	uint8_t inner_hash[32];
	uint8_t outer_hash[32];
	size_t i;

	for ( i = 0; i < 64; i++ ) {
		kpad[i] = i < key_len ? key[i] ^ 0x36 : 0x36;
	}

	xrt_sha256(kpad, 64, inner_hash);

	uint8_t tmp[64 + msg_len];
	memcpy(tmp, kpad, 64);
	memcpy(tmp + 64, msg, msg_len);
	xrt_sha256(tmp, 64 + msg_len, inner_hash);

	for ( i = 0; i < 64; i++ ) {
		kpad[i] = i < key_len ? key[i] ^ 0x5c : 0x5c;
	}

	memcpy(tmp, kpad, 64);
	memcpy(tmp + 64, inner_hash, 32);
	xrt_sha256(tmp, 96, outer_hash);

	memcpy(out, outer_hash, 32);
}

static void hkdf_extract(const uint8_t* salt, size_t salt_len, const uint8_t* ikm,
		      size_t ikm_len, uint8_t* prk)
{
	uint8_t zeros_key[32];
	memset(zeros_key, 0, 32);
	sha256_hmac(zeros_key, 32, salt, salt_len, prk);
}

static void hkdf_expand(const uint8_t* prk, size_t prk_len, const uint8_t* info,
		     size_t info_len, uint8_t* okm, size_t okm_len)
{
	uint8_t t[64];
	uint8_t prev[32] = {0};
	size_t t_len = 0;
	size_t i;

	for ( i = 1; t_len < okm_len; i++ ) {
		uint8_t ctr = (uint8_t)i;
		memcpy(t, prev, 32);
		memcpy(t + 32, &ctr, 1);
		memcpy(t + 33, info, info_len);
		sha256_hmac(prk, prk_len, t, 32 + 1 + info_len, t + 32);
		memcpy(prev, t + 32, 32);
		memcpy(okm + t_len, t + 32, 32);
		t_len += 32;
	}
}

static void derive_secret(const uint8_t* secret, const uint8_t* label,
		     size_t label_len, uint8_t* out)
{
	uint8_t h[32];
	uint8_t zeros_hash[32];
	uint8_t tmp[64];
	xrt_sha256(zeros, 32, zeros_hash);
	memcpy(tmp, zeros_hash, 32);
	memcpy(tmp + 32, label, label_len);
	memcpy(tmp + 32 + label_len, secret, 32);
	xrt_sha256(tmp, 32 + label_len + 32, h);
	hkdf_extract(h, 32, zeros, 0, out);
}

static void derive_traffic_keys(xrt_tls_builtin_ctx* ctx, const uint8_t* secret)
{
	uint8_t client_server_traffic[12];
	memcpy(client_server_traffic, "c hs traffic", 12);
	derive_secret(secret, client_server_traffic, 12,
		   ctx->enc.handshake_secret);

	memcpy(client_server_traffic, "s hs traffic", 12);
	derive_secret(ctx->enc.handshake_secret, client_server_traffic, 12,
		   ctx->enc.server_write_key);

	uint8_t key[32];
	memcpy(key, ctx->enc.server_write_key, 32);
	memcpy(client_server_traffic, "key", 3);
	hkdf_expand(ctx->enc.server_write_key, 32, client_server_traffic, 3, key, 32);

	memcpy(ctx->enc.server_write_key, key, 32);
	memcpy(ctx->enc.server_write_iv, key + 16, 12);

	memcpy(client_server_traffic, "c ap traffic", 12);
	derive_secret(ctx->enc.handshake_secret, client_server_traffic, 12,
		   ctx->enc.client_write_key);

	memcpy(key, ctx->enc.client_write_key, 32);
	memcpy(client_server_traffic, "key", 3);
	hkdf_expand(ctx->enc.client_write_key, 32, client_server_traffic, 3, key, 32);

	memcpy(ctx->enc.client_write_key, key, 32);
	memcpy(ctx->enc.client_write_iv, key + 16, 12);
}

static xrt_tls_result builtin_init()
{
	g_builtin_initialized = true;
	return XRT_TLS_OK;
}

static void builtin_cleanup()
{
	g_builtin_initialized = false;
}

static xrt_tls_ctx* builtin_ctx_create(const xrt_tls_config* config, bool is_server)
{
	xrt_tls_ctx* ctx = (xrt_tls_ctx*)xrtCalloc(1, sizeof(xrt_tls_ctx));
	if ( !ctx ) {
		return NULL;
	}

	xrt_tls_builtin_ctx* bctx =
		(xrt_tls_builtin_ctx*)xrtCalloc(1, sizeof(xrt_tls_builtin_ctx));
	if ( !bctx ) {
		xrtFree(ctx);
		return NULL;
	}

	bctx->state = is_server ? XRT_TLS_BUILTIN_STATE_SERVER_START :
				     XRT_TLS_BUILTIN_STATE_CLIENT_START;
	bctx->skip_verification = config ? config->verify_peer : 0;

	if ( config && config->host_name ) {
		strncpy(bctx->hostname, config->host_name, sizeof(bctx->hostname) - 1);
	}

	if ( !xrt_net_buffer_init(&bctx->send, 8192) ) {
		goto error;
	}

	if ( config && config->cert_file ) {
		FILE* f = fopen(config->cert_file, "rb");
		if ( f ) {
			fseek(f, 0, SEEK_END);
			long size = ftell(f);
			fseek(f, 0, SEEK_SET);
			bctx->cert_der.data = (char*)xrtMalloc(size);
			if ( bctx->cert_der.data ) {
				bctx->cert_der.size = size;
				fread(bctx->cert_der.data, 1, size, f);
			}
			fclose(f);
		}
	}

	if ( config && config->key_file ) {
		FILE* f = fopen(config->key_file, "rb");
		if ( f ) {
			fseek(f, 0, SEEK_END);
			long size = ftell(f);
			fseek(f, 0, SEEK_SET);
			if ( size == 32 ) {
				fread(bctx->ec_key, 1, 32, f);
			}
			fclose(f);
		}
	}

	if ( config && config->ca_file ) {
		FILE* f = fopen(config->ca_file, "rb");
		if ( f ) {
			fseek(f, 0, SEEK_END);
			long size = ftell(f);
			fseek(f, 0, SEEK_SET);
			bctx->ca_der.data = (char*)xrtMalloc(size);
			if ( bctx->ca_der.data ) {
				bctx->ca_der.size = size;
				fread(bctx->ca_der.data, 1, size, f);
			}
			fclose(f);
		}
	}

	ctx->backend_ctx = bctx;
	ctx->backend = XRT_TLS_BACKEND_MBEDTLS;
	ctx->is_server = is_server;
	ctx->handshake_done = false;

	return ctx;

error:
	xrtFree(bctx);
	xrtFree(ctx);
	return NULL;
}

static void builtin_ctx_destroy(xrt_tls_ctx* ctx)
{
	if ( !ctx ) {
		return;
	}

	xrt_tls_builtin_ctx* bctx = (xrt_tls_builtin_ctx*)ctx->backend_ctx;
	if ( bctx ) {
		xrt_net_buffer_free(&bctx->send);
		xrt_net_buffer_free(&bctx->cert_der);
		xrt_net_buffer_free(&bctx->ca_der);
		xrtFree(bctx);
	}

	xrtFree(ctx);
}

static void builtin_encrypt_record(xrt_tls_ctx* ctx, uint8_t type,
			       const uint8_t* data, size_t len)
{
	xrt_tls_builtin_ctx* bctx = (xrt_tls_builtin_ctx*)ctx->backend_ctx;
	xrt_tls_builtin_enc* enc = &bctx->enc;

	uint8_t* key = ctx->is_server ? enc->client_write_key : enc->server_write_key;
	uint8_t* iv = ctx->is_server ? enc->client_write_iv : enc->server_write_iv;
	uint32_t seq = ctx->is_server ? enc->cseq : enc->sseq;

	uint8_t nonce[12];
	memcpy(nonce, iv, 12);
	nonce[8] ^= (seq >> 24) & 0xff;
	nonce[9] ^= (seq >> 16) & 0xff;
	nonce[10] ^= (seq >> 8) & 0xff;
	nonce[11] ^= seq & 0xff;

	size_t total_len = 5 + len + 16;
	uint8_t* record = (uint8_t*)xrtMalloc(total_len);
	if ( !record ) {
		return;
	}

	record[0] = type;
	store_be16(record + 1, 0x0303);
	store_be16(record + 3, (uint16_t)(len + 16));

#if defined(XRT_ENABLE_CHACHA20)
	xrt_chacha20_poly1305_encrypt(record + 5, key, nonce, record, 5,
					   data, len);
#else
	xrt_aes_gcm_encrypt(record + 5, record, len + 1, key, 16, nonce, 12);
#endif

	xrt_net_buffer_append(&bctx->send, (char*)record, total_len);
	xrtFree(record);

	if ( ctx->is_server ) {
		enc->cseq++;
	} else {
		enc->sseq++;
	}
}

static void builtin_send_client_hello(xrt_tls_ctx* ctx)
{
	xrt_tls_builtin_ctx* bctx = (xrt_tls_builtin_ctx*)ctx->backend_ctx;

	uint8_t hello[512];
	size_t pos = 0;

	xrt_random(bctx->random, 32);

	memcpy(hello + pos, "\x01\x00\x03\x03", 4);
	pos += 4;

	memcpy(hello + pos, bctx->session_id, 32);
	pos += 32;

	memcpy(hello + pos, "\x00\x02\x13\x01", 4);
	pos += 4;

	memcpy(hello + pos, "\x00\x05\x13\x03\x13\x02", 5);
	pos += 5;

	memcpy(hello + pos, "\x00\x2b\x00\x01\x00", 5);
	pos += 5;

	memcpy(hello + pos, "\x00\x33\x00\x26", 4);
	pos += 4;

	memcpy(hello + pos, "\x00\x24\x00\x1d", 4);
	pos += 4;

	memcpy(hello + pos, "\x00\x20", 2);
	pos += 2;

	uint8_t pubkey[32];
	xrt_x25519_generate_keypair(bctx->ec_key, pubkey);
	memcpy(hello + pos, "\x00\x1d", 2);
	pos += 2;
	memcpy(hello + pos, pubkey, 32);
	pos += 32;

	memcpy(hello + pos, "\x00\x00", 2);
	pos += 2;

	memcpy(hello + pos, "\x00\x01", 2);
	pos += 2;

	builtin_encrypt_record(ctx, XRT_TLS_BUILTIN_HANDSHAKE, hello, pos);
}

static void builtin_send_server_hello(xrt_tls_ctx* ctx)
{
	xrt_tls_builtin_ctx* bctx = (xrt_tls_builtin_ctx*)ctx->backend_ctx;

	uint8_t hello[512];
	size_t pos = 0;

	xrt_random(bctx->random, 32);

	memcpy(hello + pos, "\x02\x00\x03\x03", 4);
	pos += 4;

	memcpy(hello + pos, "\x00\x00", 2);
	pos += 2;

	memcpy(hello + pos, "\x00\x02\x13\x01", 4);
	pos += 4;

	memcpy(hello + pos, "\x00\x05\x13\x03\x13\x02", 5);
	pos += 5;

	memcpy(hello + pos, "\x00\x2b\x00\x01\x00", 5);
	pos += 5;

	memcpy(hello + pos, "\x00\x33\x00\x24", 4);
	pos += 4;

	memcpy(hello + pos, "\x00\x1d\x00\x20", 4);
	pos += 4;

	memcpy(hello + pos, "\x00\x1d", 2);
	pos += 2;

	uint8_t pubkey[32];
	xrt_x25519_generate_keypair(bctx->ec_key, pubkey);
	memcpy(hello + pos, pubkey, 32);
	pos += 32;

	builtin_encrypt_record(ctx, XRT_TLS_BUILTIN_HANDSHAKE, hello, pos);
}

static xrt_tls_result builtin_ctx_handshake(xrt_tls_ctx* ctx)
{
	if ( !ctx ) {
		return XRT_TLS_ERROR;
	}

	xrt_tls_builtin_ctx* bctx = (xrt_tls_builtin_ctx*)ctx->backend_ctx;

	if ( bctx->send.size > 0 ) {
		if ( ctx->conn ) {
			size_t sent = 0;
			xrt_net_socket_send(ctx->conn, bctx->send.data, bctx->send.size,
					    &sent);
			xrt_net_buffer_consume(&bctx->send, sent);
		}
		return XRT_TLS_WANT_WRITE;
	}

	switch ( bctx->state ) {
		case XRT_TLS_BUILTIN_STATE_CLIENT_START:
			builtin_send_client_hello(ctx);
			bctx->state = XRT_TLS_BUILTIN_STATE_CLIENT_WAIT_SH;
			return XRT_TLS_WANT_WRITE;

		case XRT_TLS_BUILTIN_STATE_SERVER_START:
			bctx->state = XRT_TLS_BUILTIN_STATE_SERVER_NEGOTIATED;
			builtin_send_server_hello(ctx);
			return XRT_TLS_WANT_WRITE;

		case XRT_TLS_BUILTIN_STATE_SERVER_NEGOTIATED:
		case XRT_TLS_BUILTIN_STATE_CLIENT_CONNECTED:
			ctx->handshake_done = true;
			return XRT_TLS_OK;

		default:
			return XRT_TLS_OK;
	}
}

static xrt_tls_result builtin_ctx_read(xrt_tls_ctx* ctx, char* buf, size_t len,
				    size_t* read_len)
{
	if ( !ctx || !buf || len == 0 ) {
		return XRT_TLS_ERROR;
	}

	if ( !ctx->handshake_done ) {
		return XRT_TLS_HANDSHAKE_PENDING;
	}

	size_t received = 0;
	xrt_net_result result =
		xrt_net_socket_recv(ctx->conn, buf, len, &received);
	if ( result != XRT_NET_OK ) {
		return XRT_TLS_ERROR;
	}

	if ( read_len ) {
		*read_len = received;
	}

	return XRT_TLS_OK;
}

static xrt_tls_result builtin_ctx_write(xrt_tls_ctx* ctx, const char* buf,
				     size_t len, size_t* write_len)
{
	if ( !ctx || !buf || len == 0 ) {
		return XRT_TLS_ERROR;
	}

	if ( !ctx->handshake_done ) {
		return XRT_TLS_HANDSHAKE_PENDING;
	}

	builtin_encrypt_record(ctx, XRT_TLS_BUILTIN_APP_DATA, (const uint8_t*)buf, len);

	if ( write_len ) {
		*write_len = len;
	}

	return XRT_TLS_OK;
}

static xrt_tls_result builtin_ctx_close(xrt_tls_ctx* ctx)
{
	if ( !ctx ) {
		return XRT_TLS_ERROR;
	}

	return XRT_TLS_OK;
}

static const xrt_tls_backend_ops builtin_ops = {
	.init = builtin_init,
	.cleanup = builtin_cleanup,
	.ctx_create = builtin_ctx_create,
	.ctx_destroy = builtin_ctx_destroy,
	.ctx_handshake = builtin_ctx_handshake,
	.ctx_read = builtin_ctx_read,
	.ctx_write = builtin_ctx_write,
	.ctx_close = builtin_ctx_close,
};

xrt_tls_backend_impl xrt_tls_builtin_backend = {
	.name = "builtin",
	.ops = &builtin_ops,
};

#endif
