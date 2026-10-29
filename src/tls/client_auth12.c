#include "../internal/xrt_tls_client_auth.h"

#if defined(XRT_FEATURE_TLS_CLIENT_AUTH)

static bool __xrtTlsClientAuth12PlainSize(size_t Size, size_t* pSize)
{
	size_t records = Size / XTLS_RECORD_PLAINTEXT_MAX + (Size % XTLS_RECORD_PLAINTEXT_MAX != 0);
	if (!Size || records > (SIZE_MAX - Size) / XTLS_RECORD_HEADER_SIZE) return false;
	*pSize = Size + records * XTLS_RECORD_HEADER_SIZE;
	return true;
}

static bool __xrtTlsClientAuth12PayloadSize(const xtlsclientstate* pState, size_t* pSize)
{
	size_t exchange = xrtTls12ClientKeyExchangeSize((xbytesview) { pState->PublicKey, pState->PublicKeySize });
	if (!exchange) return false;
	*pSize = __xrtTlsClientAuthCertificateSize(pState) + xrtTlsHandshakeSize(exchange);
	if (pState->Auth->Selected) *pSize += 8u + pState->Auth->SignatureCapacity;
	return true;
}

xtlsresult __xrtTlsClientAuth12Writable(xtlssession* pSession, const xtlsclientstate* pState)
{
	const xtlslimits* limits = xrtTlsContextLimits(pSession->Context);
	const xtlscipherinfo* cipher = xrtTlsCipherInfo(pState->Cipher);
	size_t payload, flight;
	if (!limits || !cipher || !__xrtTlsClientAuth12PayloadSize(pState, &payload) ||
		!__xrtTlsClientAuth12PlainSize(payload, &flight)) return XTLS_ERROR;
	flight += xrtTlsRecordSize(1u) + XTLS_RECORD_HEADER_SIZE + cipher->ExplicitNonceSize +
		XTLS_HANDSHAKE_HEADER_SIZE + XTLS12_FINISHED_SIZE + cipher->TagSize;
	if (flight > limits->SendLimit) {
		(void)__xrtTlsClientError(XERR_RANGE, XTLS_ERROR_LIMIT, "queue-tls12-client-auth",
			"TLS send limit cannot hold the bounded client authentication flight");
		return XTLS_ERROR;
	}
	size_t queued = xrtTlsSessionSendSize(pSession);
	return queued > limits->SendLimit || flight > limits->SendLimit - queued ? XTLS_AGAIN : XTLS_OK;
}

/* Prepare every record with local keys and queue the entire flight once. The
 * signature capacity is admitted before any signer invocation. */
xtlsresult __xrtTlsClientAuth12Flight(xtlssession* pSession, xtlsclientstate* pState,
	const xtlshandshake* pMessage)
{
	if (pMessage->Type != XTLS_HANDSHAKE_SERVER_HELLO_DONE || pMessage->Body.Size)
		return __xrtTlsClientProtocol(pSession, XTLS_ERROR_HANDSHAKE,
			"process-tls12-server-hello-done", "TLS 1.2 client expected an empty ServerHelloDone");
	xtlsresult writable = __xrtTlsClientAuth12Writable(pSession, pState);
	if (writable == XTLS_AGAIN) return XTLS_AGAIN;
	if (writable != XTLS_OK || !__xrtTlsClientAuthRequire(pState)) return __xrtTlsClientAuthFailure(pSession);

	xtlsclientauthstate* auth = pState->Auth;
	xtlstranscript next = pState->Transcript;
	xtls12keymaterial material;
	xtlsrecordkey write_key, read_key;
	uint8 digest[XTLS_CLIENT_SECRET_MAX_SIZE];
	uint8 verify[XTLS12_FINISHED_SIZE];
	uint8 finished[XTLS_HANDSHAKE_HEADER_SIZE + XTLS12_FINISHED_SIZE];
	bytes plain = NULL, wire = NULL;
	size_t capacity = 0, plain_size = 0, wire_size = 0;
	bool moved = false;
	xtlsresult result = XTLS_ERROR;
	memset(&material, 0, sizeof(material));
	memset(&write_key, 0, sizeof(write_key));
	memset(&read_key, 0, sizeof(read_key));
	memset(digest, 0, sizeof(digest));
	memset(verify, 0, sizeof(verify));
	memset(finished, 0, sizeof(finished));

	if (!__xrtTlsClientAuth12PayloadSize(pState, &capacity)) goto cleanup;
	plain = (bytes)xrtMalloc(capacity);
	if (!plain) {
		(void)__xrtTlsClientCause("build-tls12-client-auth", "TLS client authentication plaintext allocation failed");
		goto cleanup;
	}
	size_t certificate = __xrtTlsClientAuthCertificateSize(pState);
	if (!__xrtTlsClientAuthCertificateEncode(pState, plain, capacity)) goto cleanup;
	size_t exchange_body = xrtTls12ClientKeyExchangeSize((xbytesview) { pState->PublicKey, pState->PublicKeySize });
	size_t exchange = xrtTlsHandshakeSize(exchange_body);
	if (!xrtTls12ClientKeyExchangeEncode((xbytesview) { pState->PublicKey, pState->PublicKeySize },
		plain + certificate + XTLS_HANDSHAKE_HEADER_SIZE, exchange_body) ||
		!xrtTlsHandshakeEncode(XTLS_HANDSHAKE_CLIENT_KEY_EXCHANGE,
			(xbytesview) { plain + certificate + XTLS_HANDSHAKE_HEADER_SIZE, exchange_body },
			plain + certificate, capacity - certificate)) goto cleanup;
	plain_size = certificate + exchange;
	xbytesview done = { pMessage->Body.Data - XTLS_HANDSHAKE_HEADER_SIZE, pMessage->EncodedSize };
	if (!__xrtTlsTranscriptUpdate(&next, done) ||
		!__xrtTlsTranscriptUpdate(&next, (xbytesview) { plain, plain_size }) ||
		!__xrtTlsTranscriptDigest(&next, digest, pState->HashSize) ||
		!__xrtTls12KeyMaterial(pState->Cipher, (xbytesview) { pState->Shared, pState->SharedSize },
			(xbytesview) { digest, pState->HashSize },
			(xbytesview) { pState->Random, XTLS12_RANDOM_SIZE },
			(xbytesview) { pState->ServerRandom, XTLS12_RANDOM_SIZE }, &material)) goto cleanup;
	if (!__xrtTlsRecordKeyInit(&write_key, XTLS_VERSION_12, pState->Cipher,
		(xbytesview) { material.ClientKey, material.KeySize }, (xbytesview) { material.ClientIv, material.IvSize }) ||
		!__xrtTlsRecordKeyInit(&read_key, XTLS_VERSION_12, pState->Cipher,
			(xbytesview) { material.ServerKey, material.KeySize }, (xbytesview) { material.ServerIv, material.IvSize })) goto cleanup;

	if (auth->Selected) {
		if (done.Size > auth->Config.MaxTranscriptBytes - auth->TranscriptSize ||
			plain_size > auth->Config.MaxTranscriptBytes - auth->TranscriptSize - done.Size) {
			(void)__xrtTlsClientError(XERR_RANGE, XTLS_ERROR_LIMIT, "build-tls12-client-auth",
				"TLS 1.2 signing transcript exceeds its limit");
			goto cleanup;
		}
		size_t size = auth->TranscriptSize + done.Size + plain_size;
		if (!__xrtTlsClientAuthTranscriptReserve12(auth, size)) goto cleanup;
		memcpy(auth->Transcript + auth->TranscriptSize, done.Data, done.Size);
		memcpy(auth->Transcript + auth->TranscriptSize + done.Size, plain, plain_size);
		size_t proof;
		if (!__xrtTlsClientAuthVerifyEncode(pState, (xbytesview) { auth->Transcript, size },
			plain + plain_size, capacity - plain_size, &proof) ||
			!__xrtTlsTranscriptUpdate(&next, (xbytesview) { plain + plain_size, proof })) goto cleanup;
		plain_size += proof;
	}
	if (!__xrtTlsTranscriptDigest(&next, digest, pState->HashSize) ||
		!__xrtTls12Finished(material.Hash, (xbytesview) { material.Master, sizeof(material.Master) }, false,
			(xbytesview) { digest, pState->HashSize }, verify, sizeof(verify)) ||
		!xrtTlsHandshakeEncode(XTLS_HANDSHAKE_FINISHED, (xbytesview) { verify, sizeof(verify) },
			finished, sizeof(finished)) ||
		!__xrtTlsTranscriptUpdate(&next, (xbytesview) { finished, sizeof(finished) })) goto cleanup;

	size_t plain_wire;
	if (!__xrtTlsClientAuth12PlainSize(plain_size, &plain_wire)) goto cleanup;
	size_t ccs = xrtTlsRecordSize(1u);
	size_t protected_finished = __xrtTlsRecordSealSize(&write_key, sizeof(finished), 0);
	wire_size = plain_wire + ccs + protected_finished;
	wire = (bytes)xrtMalloc(wire_size);
	if (!wire) {
		(void)__xrtTlsClientCause("build-tls12-client-auth", "TLS client authentication flight allocation failed");
		goto cleanup;
	}
	size_t offset = 0, encoded = 0;
	while (offset < plain_size) {
		size_t chunk = plain_size - offset;
		if (chunk > XTLS_RECORD_PLAINTEXT_MAX) chunk = XTLS_RECORD_PLAINTEXT_MAX;
		size_t record = xrtTlsRecordSize(chunk);
		if (!xrtTlsRecordEncode(XTLS_RECORD_HANDSHAKE, XTLS_VERSION_12,
			(xbytesview) { plain + offset, chunk }, wire + encoded, record)) goto cleanup;
		offset += chunk;
		encoded += record;
	}
	uint8 change = 1u;
	size_t written;
	if (!xrtTlsRecordEncode(XTLS_RECORD_CHANGE_CIPHER_SPEC, XTLS_VERSION_12,
		(xbytesview) { &change, 1u }, wire + encoded, ccs) ||
		!__xrtTlsRecordSeal(&write_key, XTLS_RECORD_HANDSHAKE,
			(xbytesview) { finished, sizeof(finished) }, 0,
			wire + encoded + ccs, protected_finished, &written) || written != protected_finished) goto cleanup;
	result = __xrtTlsSessionSendTake(pSession, wire, wire_size);
	if (result != XTLS_OK) {
		if (result == XTLS_AGAIN) (void)__xrtTlsClientError(XERR_STATE, XTLS_ERROR_STATE,
			"queue-tls12-client-auth", "TLS client output capacity changed during signing");
		goto cleanup;
	}
	moved = true;
	__xrtTlsRecordKeyClear(&pSession->WriteKey);
	pSession->WriteKey = write_key;
	memset(&write_key, 0, sizeof(write_key));
	__xrtTlsRecordKeyClear(&pState->PendingReadKey);
	pState->PendingReadKey = read_key;
	memset(&read_key, 0, sizeof(read_key));
	__xrtTlsTranscriptClear(&pState->Transcript);
	pState->Transcript = next;
	memset(&next, 0, sizeof(next));
	memcpy(pState->Master, material.Master, sizeof(pState->Master));
	pState->Step = XTLS_CLIENT_WAIT_CHANGE_CIPHER_SPEC;
	auth->Queued = auth->Selected;
	__xrtTlsClientAuthTranscriptClear(auth);
	xrtSecureZero(pState->PrivateKey, pState->PrivateKeyCapacity);
	xrtSecureZero(pState->Shared, sizeof(pState->Shared));
	result = __xrtTlsClientWait(pSession, true) ? XTLS_OK : XTLS_ERROR;

cleanup:
	if (!moved && wire) { xrtSecureZero(wire, wire_size); xrtFree(wire); }
	if (plain) { xrtSecureZero(plain, capacity); xrtFree(plain); }
	__xrtTlsRecordKeyClear(&write_key);
	__xrtTlsRecordKeyClear(&read_key);
	__xrtTlsTranscriptClear(&next);
	xrtSecureZero(&material, sizeof(material));
	xrtSecureZero(digest, sizeof(digest));
	xrtSecureZero(verify, sizeof(verify));
	xrtSecureZero(finished, sizeof(finished));
	return result == XTLS_OK ? XTLS_OK : __xrtTlsClientAuthFailure(pSession);
}
#endif
