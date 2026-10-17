#include "../internal/xrudp_noise.h"

static bool xrudp__noise_hash(xrudpnoise* n, const void* data, size_t size)
{
	xsha256 hash;
	bool result;
	xrtSha256Init(&hash);
	result = xrtSha256Update(&hash, n->Hash, 32) &&
		xrtSha256Update(&hash, data, size) && xrtSha256Final(&hash, n->Hash);
	xrtSecureZero(&hash, sizeof(hash));
	return result;
}

static bool xrudp__noise_mix(xrudpnoise* n, const unsigned char privateKey[32],
	const unsigned char publicKey[32])
{
	unsigned char shared[32] = {0}, keys[64] = {0};
	bool result = xrtX25519Shared(privateKey, publicKey, shared) &&
		xrtHkdfSha256(n->Chain, 32, shared, 32, NULL, 0, keys, sizeof(keys));
	if (result) {
		memcpy(n->Chain, keys, 32);
		memcpy(n->CipherKey, keys + 32, 32);
		n->Nonce = 0;
	}
	xrtSecureZero(shared, sizeof(shared));
	xrtSecureZero(keys, sizeof(keys));
	return result;
}

static void xrudp__noise_nonce(unsigned char nonce[12], uint64_t number)
{
	memset(nonce, 0, 4);
	for (size_t i = 0; i < 8; ++i) nonce[i + 4] = (unsigned char)(number >> (i * 8));
}

static bool xrudp__noise_encrypt(xrudpnoise* n, const void* plain,
	size_t size, unsigned char* output)
{
	unsigned char nonce[12];
	if (n->Nonce == UINT64_MAX) return false;
	xrudp__noise_nonce(nonce, n->Nonce);
	if (!xrtChaCha20Poly1305Seal(n->CipherKey, nonce, n->Hash, 32,
		plain, size, output, size + XRUDP_TAG_SIZE) ||
		!xrudp__noise_hash(n, output, size + XRUDP_TAG_SIZE)) return false;
	++n->Nonce;
	return true;
}

static bool xrudp__noise_decrypt(xrudpnoise* n, const unsigned char* input,
	size_t size, void* output)
{
	unsigned char nonce[12];
	if (size < XRUDP_TAG_SIZE || n->Nonce == UINT64_MAX) return false;
	xrudp__noise_nonce(nonce, n->Nonce);
	if (!xrtChaCha20Poly1305TryOpen(n->CipherKey, nonce, n->Hash, 32,
		input, size, output, size - XRUDP_TAG_SIZE) ||
		!xrudp__noise_hash(n, input, size)) return false;
	++n->Nonce;
	return true;
}

static bool xrudp__noise_split(xrudpnoise* n)
{
	unsigned char keys[64] = {0};
	bool result = xrtHkdfSha256(n->Chain, 32, NULL, 0, NULL, 0, keys, sizeof(keys));
	if (result) {
		size_t sendOffset = n->Role == XRUDP_INITIATOR ? 0 : 32;
		memcpy(n->SendKey, keys + sendOffset, 32);
		memcpy(n->ReceiveKey, keys + 32 - sendOffset, 32);
		/* No more handshake crypto is needed. Keep only transcript/public
		 * identity/peer parameters and the once-transferable traffic keys. */
		xrtSecureZero(n->StaticPrivate, 32);
		xrtSecureZero(n->EphemeralPrivate, 32);
		xrtSecureZero(n->Chain, 32);
		xrtSecureZero(n->CipherKey, 32);
		n->Nonce = 0;
	}
	xrtSecureZero(keys, sizeof(keys));
	return result;
}

static bool xrudp__noise_authorize(xrudpnoise* n)
{
	if (n->HasPin && !xrtConstTimeEqual(n->RemoteStatic, n->Pin, 32)) return false;
	if (n->Trust != NULL && !n->Trust(n->RemoteStatic, n->TrustContext)) return false;
	n->Trusted = true;
	return true;
}

xrudpresult xrudp__noise_create(const xrudpnoiseconfig* config,
	const void* deterministicEphemeral, xrudpnoise** output)
{
	static const char protocol[] = "Noise_IK_25519_ChaChaPoly_SHA256";
	xrudpnoise* n;
	bool ok;
	if (config == NULL || output == NULL || config->StaticPrivate == NULL ||
		(config->Role != XRUDP_INITIATOR && config->Role != XRUDP_RESPONDER) ||
		(config->PeerPublic == NULL && (config->Role == XRUDP_INITIATOR || config->Trust == NULL)) ||
		(config->Prologue == NULL && config->PrologueSize != 0) ||
		config->PrologueSize > XRUDP_NOISE_PARAMETERS_MAX) return XRUDP_ARGUMENT;
	n = (xrudpnoise*)xrtMalloc(sizeof(*n));
	if (n == NULL) return XRUDP_NO_MEMORY;
	memset(n, 0, sizeof(*n));
	n->Role = config->Role;
	n->Trust = config->Trust;
	n->TrustContext = config->TrustContext;
	n->HasPin = config->PeerPublic != NULL;
	memcpy(n->StaticPrivate, config->StaticPrivate, 32);
	if (n->HasPin) memcpy(n->Pin, config->PeerPublic, 32);
	if (n->Role == XRUDP_INITIATOR) memcpy(n->RemoteStatic, n->Pin, 32);
	if (deterministicEphemeral != NULL) {
		memcpy(n->EphemeralPrivate, deterministicEphemeral, 32);
		ok = true;
	} else ok = xrtSecureRandom(n->EphemeralPrivate, 32);
	ok = ok && xrtX25519Public(n->StaticPrivate, n->StaticPublic) &&
		xrtX25519Public(n->EphemeralPrivate, n->EphemeralPublic);
	if (ok) {
		/* The fixed protocol name is exactly HASHLEN bytes; Noise pads shorter
		 * names and hashes longer ones. No terminator is part of the name. */
		if (sizeof(protocol) - 1 <= sizeof(n->Hash)) memcpy(n->Hash, protocol, sizeof(protocol) - 1);
		else ok = xrtSha256(protocol, sizeof(protocol) - 1, n->Hash);
		memcpy(n->Chain, n->Hash, 32);
		ok = ok && xrudp__noise_hash(n, config->Prologue, config->PrologueSize) &&
			xrudp__noise_hash(n, n->Role == XRUDP_INITIATOR ? n->RemoteStatic : n->StaticPublic, 32);
	}
	if (!ok) {
		xrtRudpNoiseDestroy(n);
		return XRUDP_IO_ERROR;
	}
	*output = n;
	return XRUDP_OK;
}

size_t xrtRudpNoiseAllocationSize(void) { return sizeof(xrudpnoise); }

xrudpresult xrtRudpNoiseCreate(const xrudpnoiseconfig* pConfig, xrudpnoise** ppNoise)
{
	return xrudp__noise_create(pConfig, NULL, ppNoise);
}

void xrtRudpNoiseDestroy(xrudpnoise* pNoise)
{
	if (pNoise != NULL) {
		xrtSecureZero(pNoise, sizeof(*pNoise));
		xrtFree(pNoise);
	}
}

xrudpresult xrtRudpNoiseInfo(const xrudpnoise* pNoise, xrudpnoiseinfo* pInfo)
{
	xrudpnoiseinfo info;
	if (pNoise == NULL || pInfo == NULL) return XRUDP_ARGUMENT;
	memset(&info, 0, sizeof(info));
	info.MessageIndex = pNoise->Index;
	info.Complete = pNoise->Index == 2;
	info.PeerVerified = pNoise->Trusted;
	info.KeysTaken = pNoise->Taken;
	if (pNoise->Trusted) memcpy(info.PeerPublic, pNoise->RemoteStatic, 32);
	if (info.Complete) {
		memcpy(info.HandshakeHash, pNoise->Hash, 32);
		info.PeerParameterSize = pNoise->PeerParameterSize;
	}
	*pInfo = info;
	return XRUDP_OK;
}

xrudpresult xrtRudpNoiseWrite(xrudpnoise* pNoise,
	const void* pParameters, size_t iParameterSize,
	void* pOutput, size_t iCapacity, size_t* pWritten)
{
	xrudpnoise candidate;
	unsigned char message[XRUDP_NOISE_MESSAGE_MAX];
	xrudpresult result = XRUDP_IO_ERROR;
	size_t prefix, total;
	bool ok;
	if (pWritten != NULL) *pWritten = 0;
	if (pNoise == NULL || pOutput == NULL || pWritten == NULL ||
		(pParameters == NULL && iParameterSize != 0)) return XRUDP_ARGUMENT;
	if (iParameterSize > XRUDP_NOISE_PARAMETERS_MAX) return XRUDP_LIMIT;
	if (pNoise->Index == 2) return XRUDP_DONE;
	if (!((pNoise->Role == XRUDP_INITIATOR && pNoise->Index == 0) ||
		(pNoise->Role == XRUDP_RESPONDER && pNoise->Index == 1))) return XRUDP_BACKPRESSURE;
	prefix = pNoise->Role == XRUDP_INITIATOR ? 80u : 32u;
	total = prefix + iParameterSize + XRUDP_TAG_SIZE;
	if (iCapacity < total) return XRUDP_LIMIT;
	candidate = *pNoise;
	memcpy(message, candidate.EphemeralPublic, 32);
	ok = xrudp__noise_hash(&candidate, message, 32);
	if (candidate.Role == XRUDP_INITIATOR) {
		ok = ok && xrudp__noise_mix(&candidate, candidate.EphemeralPrivate, candidate.RemoteStatic) &&
			xrudp__noise_encrypt(&candidate, candidate.StaticPublic, 32, message + 32) &&
			xrudp__noise_mix(&candidate, candidate.StaticPrivate, candidate.RemoteStatic);
	} else {
		ok = ok && xrudp__noise_mix(&candidate, candidate.EphemeralPrivate, candidate.RemoteEphemeral) &&
			xrudp__noise_mix(&candidate, candidate.EphemeralPrivate, candidate.RemoteStatic);
	}
	ok = ok && xrudp__noise_encrypt(&candidate, pParameters, iParameterSize, message + prefix);
	if (ok && candidate.Role == XRUDP_RESPONDER) ok = xrudp__noise_split(&candidate);
	if (ok) {
		++candidate.Index;
		memmove(pOutput, message, total);
		*pNoise = candidate;
		*pWritten = total;
		result = XRUDP_OK;
	}
	xrtSecureZero(&candidate, sizeof(candidate));
	xrtSecureZero(message, sizeof(message));
	return result;
}

xrudpresult xrtRudpNoiseRead(xrudpnoise* pNoise,
	const void* pMessage, size_t iMessageSize)
{
	const unsigned char* message = (const unsigned char*)pMessage;
	xrudpnoise candidate;
	xrudpresult result = XRUDP_AUTH_FAILED;
	size_t prefix;
	bool ok;
	if (pNoise == NULL || pMessage == NULL) return XRUDP_ARGUMENT;
	if (pNoise->Index == 2) return XRUDP_DONE;
	if (!((pNoise->Role == XRUDP_INITIATOR && pNoise->Index == 1) ||
		(pNoise->Role == XRUDP_RESPONDER && pNoise->Index == 0))) return XRUDP_BACKPRESSURE;
	prefix = pNoise->Role == XRUDP_INITIATOR ? 32u : 80u;
	if (iMessageSize < prefix + XRUDP_TAG_SIZE) return XRUDP_TRUNCATED;
	if (iMessageSize > prefix + XRUDP_TAG_SIZE + XRUDP_NOISE_PARAMETERS_MAX) return XRUDP_LIMIT;
	candidate = *pNoise;
	memcpy(candidate.RemoteEphemeral, message, 32);
	ok = xrudp__noise_hash(&candidate, message, 32);
	if (candidate.Role == XRUDP_INITIATOR) {
		ok = ok && xrudp__noise_mix(&candidate, candidate.EphemeralPrivate, candidate.RemoteEphemeral) &&
			xrudp__noise_mix(&candidate, candidate.StaticPrivate, candidate.RemoteEphemeral);
	} else {
		ok = ok && xrudp__noise_mix(&candidate, candidate.StaticPrivate, candidate.RemoteEphemeral) &&
			xrudp__noise_decrypt(&candidate, message + 32, 48, candidate.RemoteStatic) &&
			xrudp__noise_mix(&candidate, candidate.StaticPrivate, candidate.RemoteStatic);
	}
	candidate.PeerParameterSize = iMessageSize - prefix - XRUDP_TAG_SIZE;
	ok = ok && xrudp__noise_decrypt(&candidate, message + prefix, iMessageSize - prefix,
		candidate.PeerParameters);
	if (ok && !xrudp__noise_authorize(&candidate)) ok = false;
	if (ok && candidate.Role == XRUDP_INITIATOR) ok = xrudp__noise_split(&candidate);
	if (ok) {
		++candidate.Index;
		*pNoise = candidate;
		result = XRUDP_OK;
	}
	xrtSecureZero(&candidate, sizeof(candidate));
	return result;
}

xrudpresult xrtRudpNoisePeerParameters(const xrudpnoise* pNoise,
	void* pOutput, size_t iCapacity, size_t* pWritten)
{
	if (pWritten != NULL) *pWritten = 0;
	if (pNoise == NULL || pWritten == NULL || (pOutput == NULL && iCapacity != 0))
		return XRUDP_ARGUMENT;
	if (pNoise->Index != 2 || !pNoise->Trusted) return XRUDP_BACKPRESSURE;
	if (iCapacity < pNoise->PeerParameterSize) return XRUDP_LIMIT;
	if (pNoise->PeerParameterSize != 0)
		memmove(pOutput, pNoise->PeerParameters, pNoise->PeerParameterSize);
	*pWritten = pNoise->PeerParameterSize;
	return XRUDP_OK;
}

xrudpresult xrtRudpNoiseCreateCipher(xrudpnoise* pNoise,
	const void* pLocalId, const void* pPeerId, uint64_t iPacketLimit,
	xrudpcipher** ppCipher)
{
	xrudpresult result;
	if (pNoise == NULL || ppCipher == NULL) return XRUDP_ARGUMENT;
	if (pNoise->Index != 2 || !pNoise->Trusted) return XRUDP_BACKPRESSURE;
	if (pNoise->Taken) return XRUDP_DONE;
	result = xrtRudpCipherCreate(pNoise->SendKey, pNoise->ReceiveKey,
		pLocalId, pPeerId, iPacketLimit, ppCipher);
	if (result == XRUDP_OK) {
		pNoise->Taken = true;
		xrtSecureZero(pNoise->SendKey, 32);
		xrtSecureZero(pNoise->ReceiveKey, 32);
	}
	return result;
}
