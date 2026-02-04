#ifndef XRT_NET_TLS_BUILTIN_H
#define XRT_NET_TLS_BUILTIN_H

#include "xrt_net_tls.h"

#if defined(XRT_TLS_BUILTIN)

typedef enum {
	XRT_TLS_BUILTIN_STATE_CLIENT_START,
	XRT_TLS_BUILTIN_STATE_CLIENT_WAIT_SH,
	XRT_TLS_BUILTIN_STATE_CLIENT_WAIT_EE,
	XRT_TLS_BUILTIN_STATE_CLIENT_WAIT_CERT,
	XRT_TLS_BUILTIN_STATE_CLIENT_WAIT_CV,
	XRT_TLS_BUILTIN_STATE_CLIENT_WAIT_FINISH,
	XRT_TLS_BUILTIN_STATE_CLIENT_CONNECTED,

	XRT_TLS_BUILTIN_STATE_SERVER_START,
	XRT_TLS_BUILTIN_STATE_SERVER_NEGOTIATED,
	XRT_TLS_BUILTIN_STATE_SERVER_CONNECTED,
} xrt_tls_builtin_state;

#define XRT_TLS_BUILTIN_CHANGE_CIPHER 20
#define XRT_TLS_BUILTIN_ALERT 21
#define XRT_TLS_BUILTIN_HANDSHAKE 22
#define XRT_TLS_BUILTIN_APP_DATA 23

#define XRT_TLS_BUILTIN_CLIENT_HELLO 1
#define XRT_TLS_BUILTIN_SERVER_HELLO 2
#define XRT_TLS_BUILTIN_ENCRYPTED_EXTENSIONS 8
#define XRT_TLS_BUILTIN_CERTIFICATE 11
#define XRT_TLS_BUILTIN_CERTIFICATE_REQUEST 13
#define XRT_TLS_BUILTIN_CERTIFICATE_VERIFY 15
#define XRT_TLS_BUILTIN_FINISHED 20

#define XRT_TLS_RECHDR_SIZE 5
#define XRT_TLS_MSGHDR_SIZE 4

typedef struct xrt_tls_builtin_enc {
	uint32_t sseq;
	uint32_t cseq;
	uint8_t handshake_secret[32];
	uint8_t server_write_key[32];
	uint8_t server_write_iv[12];
	uint8_t server_finished_key[32];
	uint8_t client_write_key[32];
	uint8_t client_write_iv[12];
	uint8_t client_finished_key[32];
} xrt_tls_builtin_enc;

typedef struct xrt_tls_builtin_ctx {
	xrt_tls_builtin_state state;

	xrt_net_buffer send;
	size_t recv_offset;
	size_t recv_len;
	uint8_t content_type;

	uint8_t random[32];
	uint8_t session_id[32];
	uint8_t x25519_cli[32];
	uint8_t x25519_sec[32];

	int skip_verification;
	xrt_net_buffer cert_der;
	xrt_net_buffer ca_der;
	uint8_t ec_key[32];
	char hostname[254];

	int is_ec_pubkey;
	uint8_t pubkey[544];
	size_t pubkeysz;
	uint8_t sighash[32];

	xrt_tls_builtin_enc enc;
} xrt_tls_builtin_ctx;

xrt_tls_backend_impl xrt_tls_builtin_backend;

#endif

#endif
