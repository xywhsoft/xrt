#include "../internal/xrt_tls_client_auth.h"

#if defined(XRT_FEATURE_TLS_CLIENT_AUTH)

static bool __xrtTlsClientAuthError(xerrkind Kind, cstr Message)
{
	return __xrtTlsClientError(Kind, XTLS_ERROR_IDENTITY, "tls-client-auth", Message);
}

XRT_API void xrtTlsClientAuthConfigInitV1(xtlsclientauthconfigv1* pConfig)
{
	if (!pConfig) {
		(void)__xrtTlsClientAuthError(XERR_ARGUMENT, "TLS client auth config is null");
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->Size = sizeof(*pConfig);
	pConfig->MaxCertificates = XTLS_CLIENT_AUTH_CERTIFICATES_DEFAULT;
	pConfig->MaxCertificateBytes = XTLS_CLIENT_AUTH_CERTIFICATE_BYTES_DEFAULT;
	pConfig->MaxTranscriptBytes = XTLS_CLIENT_AUTH_TRANSCRIPT_BYTES_DEFAULT;
	pConfig->RequireCertificate = true;
}

void __xrtTlsClientAuthTranscriptClear(xtlsclientauthstate* pAuth)
{
	if (!pAuth) return;
	if (pAuth->Transcript) {
		xrtSecureZero(pAuth->Transcript, pAuth->TranscriptCapacity);
		xrtFree(pAuth->Transcript);
	}
	pAuth->Transcript = NULL;
	pAuth->TranscriptSize = pAuth->TranscriptCapacity = 0;
}

void __xrtTlsClientAuthClear(xtlsclientauthstate* pAuth)
{
	if (!pAuth) return;
	__xrtTlsClientAuthTranscriptClear(pAuth);
	xrtTlsIdentityRelease(pAuth->Identity);
	xrtSecureZero(pAuth, sizeof(*pAuth));
	xrtFree(pAuth);
}

bool __xrtTlsClientAuthOwnershipLeaf(const xtlsclientauthstate* pAuth)
{
	return !pAuth || __xrtTlsIdentityOwnershipLeaf(pAuth->Identity);
}

xtlsresult __xrtTlsClientAuthFailure(xtlssession* pSession)
{
	const xerror* error = xrtGetError();
	return error && xrtErrorKind(error) == XERR_PERMISSION ?
		__xrtTlsSessionFailAlert(pSession, XTLS_ALERT_HANDSHAKE_FAILURE) : __xrtTlsSessionFail(pSession);
}

bool __xrtTlsClientAuthTranscriptReserve12(xtlsclientauthstate* pAuth, size_t iSize)
{
	if (iSize > pAuth->Config.MaxTranscriptBytes)
		return __xrtTlsClientAuthError(XERR_RANGE, "TLS 1.2 client signing transcript exceeds its limit");
	if (iSize <= pAuth->TranscriptCapacity) return true;
	size_t capacity = pAuth->TranscriptCapacity ? pAuth->TranscriptCapacity : 1024u;
	if (capacity > pAuth->Config.MaxTranscriptBytes) capacity = pAuth->Config.MaxTranscriptBytes;
	while (capacity < iSize) {
		if (capacity > pAuth->Config.MaxTranscriptBytes / 2u) {
			capacity = pAuth->Config.MaxTranscriptBytes;
			break;
		}
		capacity *= 2u;
	}
	bytes data = (bytes)xrtRealloc(pAuth->Transcript, capacity);
	if (!data) return __xrtTlsClientCause("tls-client-auth", "TLS client signing transcript allocation failed");
	pAuth->Transcript = data;
	pAuth->TranscriptCapacity = capacity;
	return true;
}

bool __xrtTlsClientAuthTranscriptStart12(xtlsclientstate* pState, xbytesview ServerHello)
{
	xtlsclientauthstate* auth = pState->Auth;
	if (!auth) return true;
	if (ServerHello.Size > SIZE_MAX - pState->ClientHelloSize)
		return __xrtTlsClientAuthError(XERR_RANGE, "TLS 1.2 client signing transcript size overflow");
	if (!__xrtTlsClientAuthTranscriptReserve12(auth, pState->ClientHelloSize + ServerHello.Size)) return false;
	memcpy(auth->Transcript, pState->ClientHello, pState->ClientHelloSize);
	memcpy(auth->Transcript + pState->ClientHelloSize, ServerHello.Data, ServerHello.Size);
	auth->TranscriptSize = pState->ClientHelloSize + ServerHello.Size;
	return true;
}

bool __xrtTlsClientAuthTranscriptAppend12(xtlsclientstate* pState, xbytesview Message)
{
	xtlsclientauthstate* auth = pState->Auth;
	if (!auth || pState->Version != XTLS_VERSION_12) return true;
	if (Message.Size > SIZE_MAX - auth->TranscriptSize)
		return __xrtTlsClientAuthError(XERR_RANGE, "TLS 1.2 client signing transcript size overflow");
	if (!__xrtTlsClientAuthTranscriptReserve12(auth, auth->TranscriptSize + Message.Size)) return false;
	memcpy(auth->Transcript + auth->TranscriptSize, Message.Data, Message.Size);
	auth->TranscriptSize += Message.Size;
	return true;
}

bool __xrtTlsClientAuthRequire(const xtlsclientstate* pState)
{
	return !pState->Auth || !pState->Auth->Config.RequireCertificate ||
		(pState->CertificateRequested && pState->Auth->Selected) ||
		__xrtTlsClientAuthError(XERR_PERMISSION, "TLS server did not request a compatible local certificate");
}

/* Certificate signature schemes are independent of CertificateVerify schemes.
 * The second PSS form is resolved against an available issuer SPKI below. */
static uint16 __xrtTlsClientAuthCertificateScheme(const xx509signature* signature)
{
	uint16 hash = signature->Hash == X509_HASH_SHA256 ? 4u :
		signature->Hash == X509_HASH_SHA384 ? 5u : signature->Hash == X509_HASH_SHA512 ? 6u :
		signature->Hash == X509_HASH_SHA1 ? 2u : 0u;
	if (signature->Type == X509_SIGNATURE_ED25519) return XTLS_SIGNATURE_ED25519;
	if (signature->Type == X509_SIGNATURE_ED448) return XTLS_SIGNATURE_ED448;
	if (!hash) return 0;
	if (signature->Type == X509_SIGNATURE_RSA_PKCS1) return (uint16)((hash << 8u) | 1u);
	if (signature->Type == X509_SIGNATURE_ECDSA) return (uint16)((hash << 8u) | 3u);
	if (signature->Type == X509_SIGNATURE_RSA_PSS && hash >= 4u &&
		signature->MaskHash == signature->Hash && signature->Trailer == 1u &&
		signature->SaltSize == (hash == 4u ? 32u : hash == 5u ? 48u : 64u)) return (uint16)(0x0804u + hash - 4u);
	return 0;
}

static bool __xrtTlsClientAuthLeafUsage(const xx509cert* leaf)
{
	static const uint8 ClientAuth[] = { 0x2Bu, 6u, 1u, 5u, 5u, 7u, 3u, 2u };
	static const uint8 AnyUsage[] = { 0x55u, 0x1Du, 0x25u, 0u };
	uint16 usage;
	xx509result result = xrtX509KeyUsage(leaf, &usage);
	if (result == X509_ERROR) return false;
	if (result == X509_VALUE && !(usage & X509_USAGE_DIGITAL_SIGNATURE))
		return __xrtTlsClientAuthError(XERR_PERMISSION, "TLS client certificate does not permit signing");
	xx509oidcursor cursor;
	result = xrtX509ExtendedKeyUsage(leaf, &cursor);
	if (result == X509_ERROR) return false;
	if (result == X509_DONE) return true;
	xbytesview oid;
	bool allowed = false;
	while ((result = xrtX509OidRead(&cursor, &oid)) == X509_VALUE) {
		if ((oid.Size == sizeof(ClientAuth) && !memcmp(oid.Data, ClientAuth, oid.Size)) ||
			(oid.Size == sizeof(AnyUsage) && !memcmp(oid.Data, AnyUsage, oid.Size))) allowed = true;
	}
	return result != X509_ERROR && (allowed ||
		__xrtTlsClientAuthError(XERR_PERMISSION, "TLS client certificate does not permit client authentication"));
}

XRT_API xtlssession* xrtTlsClientCreateAuthV1(const xtlsclientconfig* pSession,
	const xtlsclientauthconfigv1* pAuth, xnetbufpool* pPool)
{
	if (!pAuth || pAuth->Size != sizeof(*pAuth) || !pAuth->Identity ||
		!pAuth->MaxCertificates || pAuth->MaxCertificates > XTLS_CLIENT_AUTH_CERTIFICATES_MAX ||
		!pAuth->MaxCertificateBytes || pAuth->MaxCertificateBytes > XTLS_CLIENT_AUTH_BYTES_MAX ||
		!pAuth->MaxTranscriptBytes || pAuth->MaxTranscriptBytes > XTLS_CLIENT_AUTH_BYTES_MAX) {
		(void)__xrtTlsClientAuthError(XERR_ARGUMENT, "TLS client authentication config or limits are invalid");
		return NULL;
	}
	xtlsclientconfig config;
	if (pSession) config = *pSession; else xrtTlsClientConfigInit(&config);
	if (config.Resume || config.ResumeOnly) {
		(void)__xrtTlsClientAuthError(XERR_UNSUPPORTED, "TLS client auth requires a full certificate handshake");
		return NULL;
	}
	/* Do not publish credentials which lose their local-identity binding. */
	config.ResumeLimit = 0;
	xtlssession* session = xrtTlsClientCreate(&config, pPool);
	if (!session) return NULL;
	xtlsclientstate* state = (xtlsclientstate*)__xrtTlsSessionRoleData(session);
	size_t count = xrtTlsIdentityCertificateCount(pAuth->Identity);
	if (!count || count > pAuth->MaxCertificates) {
		(void)__xrtTlsClientAuthError(XERR_RANGE, "TLS client certificate count exceeds its limit");
		goto failed;
	}
	size_t size = sizeof(xtlsclientauthstate) + count *
		(sizeof(xtlscertificateentry) + sizeof(xtlsclientauthcertificate));
	state->Auth = (xtlsclientauthstate*)xrtCalloc(1, size);
	if (!state->Auth) {
		(void)__xrtTlsClientCause("tls-client-auth", "TLS client authentication state allocation failed");
		goto failed;
	}
	xtlsclientauthstate* auth = state->Auth;
	auth->Config = *pAuth;
	auth->Identity = xrtTlsIdentityRetain(pAuth->Identity);
	if (!auth->Identity) goto failed;
	auth->Config.Identity = auth->Identity;
	auth->Entries = (xtlscertificateentry*)(auth + 1);
	auth->Certificates = (xtlsclientauthcertificate*)(auth->Entries + count);
	auth->CertificateCount = count;
	size_t bytes = 0;
	for (size_t i = 0; i < count; i++) {
		xx509cert cert;
		xx509signature signature;
		if (!xrtTlsIdentityCertificate(auth->Identity, i, &auth->Entries[i].Data)) goto failed;
		if (auth->Entries[i].Data.Size > pAuth->MaxCertificateBytes - bytes) {
			(void)__xrtTlsClientAuthError(XERR_RANGE, "TLS client certificate bytes exceed their limit");
			goto failed;
		}
		bytes += auth->Entries[i].Data.Size;
		if (!xrtX509Parse(auth->Entries[i].Data.Data, auth->Entries[i].Data.Size, &cert)) goto failed;
		if (!i) auth->Leaf = cert;
		auth->Certificates[i].Issuer = cert.Issuer;
		xx509result parsed = xrtX509SignatureParse(&cert.SignatureAlgorithm, &signature);
		if (parsed == X509_ERROR) goto failed;
		if (parsed == X509_VALUE) auth->Certificates[i].Signature = __xrtTlsClientAuthCertificateScheme(&signature);
		if (parsed == X509_VALUE && signature.Type == X509_SIGNATURE_RSA_PSS) {
			auth->Certificates[i].Alternate = auth->Certificates[i].Signature ?
				(uint16)(auth->Certificates[i].Signature + 5u) : 0;
			if (i + 1u < count) {
				xbytesview issuer_der;
				xx509cert issuer;
				xx509pubkey key;
				if (!xrtTlsIdentityCertificate(auth->Identity, i + 1u, &issuer_der) ||
					!xrtX509Parse(issuer_der.Data, issuer_der.Size, &issuer) ||
					!xrtX509PublicKey(&issuer, &key)) goto failed;
				if (key.Type == X509_KEY_RSA_PSS) auth->Certificates[i].Signature = auth->Certificates[i].Alternate;
				auth->Certificates[i].Alternate = 0;
			}
		}
		/* Only an actually self-signed terminal anchor gets the TLS exemption. */
		if (i + 1u == count) {
			xx509result equal = xrtX509NameEqual(cert.Issuer, cert.Subject);
			if (equal == X509_ERROR) goto failed;
			if (equal == X509_VALUE) {
				xerror* previous = xrtTakeError();
				auth->Certificates[i].Anchor = xrtX509CertificateVerify(&cert, &cert);
				if (!auth->Certificates[i].Anchor && xrtErrorIs(xrtGetError(), XERR_MEMORY)) {
					xrtErrorFree(previous);
					goto failed;
				}
				xrtClearError();
				if (previous) __xrtErrorSetOwned(previous);
			}
		}
	}
	if (!__xrtTlsClientAuthLeafUsage(&auth->Leaf)) goto failed;
	xx509pubkey key;
	if (!xrtTlsIdentityPublicKey(auth->Identity, &key)) goto failed;
	switch (key.Type) {
		case X509_KEY_RSA: case X509_KEY_RSA_PSS: auth->SignatureCapacity = key.Modulus.Size; break;
		case X509_KEY_ED25519: auth->SignatureCapacity = 64u; break;
		case X509_KEY_ED448: auth->SignatureCapacity = 114u; break;
		case X509_KEY_EC:
			auth->SignatureCapacity = key.Curve == X509_CURVE_P256 ? 72u :
				key.Curve == X509_CURVE_P384 ? 104u : key.Curve == X509_CURVE_P521 ? 141u : 0u;
			break;
		default: break;
	}
	if (!auth->SignatureCapacity || auth->SignatureCapacity > UINT16_MAX) {
		(void)__xrtTlsClientAuthError(XERR_UNSUPPORTED, "TLS client signing key has no bounded signature encoding");
		goto failed;
	}
	xbytesview empty = { NULL, 0 };
	size_t body12 = xrtTlsCertificateSize(XTLS_VERSION_12, empty, auth->Entries, count);
	size_t body13 = xrtTlsCertificateSize(XTLS_VERSION_13, empty, auth->Entries, count);
	if (!body12 || !body13) goto failed;
	auth->Certificate12 = xrtTlsHandshakeSize(body12);
	auth->Certificate13 = xrtTlsHandshakeSize(body13);
	const xtlslimits* limits = xrtTlsContextLimits(session->Context);
	if (!auth->Certificate12 || !auth->Certificate13 ||
		(state->Offer12 && auth->Certificate12 > limits->HandshakeLimit) ||
		(state->Offer13 && auth->Certificate13 > limits->HandshakeLimit) ||
		8u + auth->SignatureCapacity > limits->HandshakeLimit) {
		(void)__xrtTlsClientAuthError(XERR_RANGE, "TLS client Certificate exceeds the handshake limit");
		goto failed;
	}
	return session;
failed:
	{
		xerror* primary = xrtTakeError();
		xrtTlsSessionDestroy(session);
		xrtClearError();
		if (primary) __xrtErrorSetOwned(primary);
	}
	return NULL;
}

XRT_API bool xrtTlsClientCertificateQueuedV1(const xtlssession* pSession)
{
	const xtlsclientauthstate* auth = __xrtTlsClientAuthStateOf(pSession);
	return auth && auth->Queued;
}

size_t __xrtTlsClientAuthCertificateSize(const xtlsclientstate* pState)
{
	if (!pState->CertificateRequested) return 0;
	if (!pState->Auth || !pState->Auth->Selected) return pState->Version == XTLS_VERSION_13 ?
		XTLS_CLIENT_EMPTY_CERTIFICATE13_SIZE : XTLS_CLIENT_EMPTY_CERTIFICATE12_SIZE;
	return pState->Version == XTLS_VERSION_13 ? pState->Auth->Certificate13 : pState->Auth->Certificate12;
}

bool __xrtTlsClientAuthCertificateEncode(const xtlsclientstate* pState, void* pOutput, size_t iCapacity)
{
	const xtlsclientauthstate* auth = pState->Auth;
	bool selected = auth && auth->Selected;
	size_t size = __xrtTlsClientAuthCertificateSize(pState);
	if (!size) return true;
	xbytesview empty = { NULL, 0 };
	return iCapacity >= size && xrtTlsCertificateEncode(pState->Version, empty,
		selected ? auth->Entries : NULL, selected ? auth->CertificateCount : 0,
		(bytes)pOutput + XTLS_HANDSHAKE_HEADER_SIZE, size - XTLS_HANDSHAKE_HEADER_SIZE) &&
		xrtTlsHandshakeEncode(XTLS_HANDSHAKE_CERTIFICATE,
			(xbytesview) { (bytes)pOutput + XTLS_HANDSHAKE_HEADER_SIZE, size - XTLS_HANDSHAKE_HEADER_SIZE },
			pOutput, iCapacity);
}

bool __xrtTlsClientAuthVerifyEncode(const xtlsclientstate* pState, xbytesview Content,
	void* pOutput, size_t iCapacity, size_t* pSize)
{
	const xtlsclientauthstate* auth = pState->Auth;
	if (!auth || !auth->Selected || iCapacity < 8u + auth->SignatureCapacity)
		return __xrtTlsClientAuthError(XERR_RANGE, "TLS client CertificateVerify output is too small");
	size_t size;
	xerror* previous = xrtTakeError();
	bool signed_message = xrtTlsIdentitySign(auth->Identity, pState->Version, auth->Signature, Content,
		(bytes)pOutput + 8u, auth->SignatureCapacity, &size);
	xerror* failure = xrtTakeError();
	if (!signed_message || failure) {
		xrtErrorFree(previous);
		if (failure) __xrtErrorSetOwned(failure);
		else (void)__xrtTlsClientAuthError(XERR_INTERNAL, "TLS client signer failed without a diagnostic");
		return false;
	}
	if (previous) __xrtErrorSetOwned(previous);
	xtlscertificateverify verify = { (uint16)auth->Signature, { (bytes)pOutput + 8u, size } };
	if (!xrtTlsCertificateVerifyEncode(&verify, (bytes)pOutput + 4u, iCapacity - 4u) ||
		!xrtTlsHandshakeEncode(XTLS_HANDSHAKE_CERTIFICATE_VERIFY,
			(xbytesview) { (bytes)pOutput + 4u, size + 4u }, pOutput, iCapacity)) return false;
	*pSize = size + 8u;
	return true;
}

bool __xrtTlsClientAuthRecordsSize13(const xtlsrecordkey* pKey, size_t iPlain, size_t* pSize)
{
	size_t size = 0;
	while (iPlain) {
		size_t chunk = iPlain < XTLS_RECORD_PLAINTEXT_MAX ? iPlain : XTLS_RECORD_PLAINTEXT_MAX;
		size_t record = __xrtTlsRecordSealSize(pKey, chunk, 0);
		if (!record) return false;
		if (record > SIZE_MAX - size)
			return __xrtTlsClientAuthError(XERR_RANGE, "TLS client authentication record size overflow");
		size += record;
		iPlain -= chunk;
	}
	*pSize = size;
	return size != 0 || __xrtTlsClientAuthError(XERR_RANGE, "TLS client authentication flight is empty");
}

xtlsresult __xrtTlsClientAuthQueue13(xtlssession* pSession, xbytesview Plain)
{
	size_t size;
	if (!__xrtTlsClientAuthRecordsSize13(&pSession->WriteKey, Plain.Size, &size)) return XTLS_ERROR;
	bytes wire = (bytes)xrtMalloc(size);
	if (!wire) {
		(void)__xrtTlsClientCause("tls-client-auth", "TLS client authentication flight allocation failed");
		return XTLS_ERROR;
	}
	xtlsrecordkey key = pSession->WriteKey;
	size_t offset = 0, written = 0;
	while (offset < Plain.Size) {
		size_t chunk = Plain.Size - offset;
		if (chunk > XTLS_RECORD_PLAINTEXT_MAX) chunk = XTLS_RECORD_PLAINTEXT_MAX;
		size_t record;
		if (!__xrtTlsRecordSeal(&key, XTLS_RECORD_HANDSHAKE,
			(xbytesview) { Plain.Data + offset, chunk }, 0, wire + written, size - written, &record)) {
			__xrtTlsRecordKeyClear(&key);
			xrtSecureZero(wire, size);
			xrtFree(wire);
			return XTLS_ERROR;
		}
		offset += chunk;
		written += record;
	}
	__xrtTlsRecordKeyClear(&key);
	xtlsresult result = __xrtTlsSessionSendTake(pSession, wire, written);
	if (result != XTLS_OK) {
		xrtSecureZero(wire, size);
		xrtFree(wire);
	}
	return result;
}

#endif
