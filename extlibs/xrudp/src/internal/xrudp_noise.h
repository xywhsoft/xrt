#ifndef XRUDP_NOISE_INTERNAL_H
#define XRUDP_NOISE_INTERNAL_H

#include <xrudp/noise.h>
#include <xrt/crypto.h>
#include <xrt/random.h>
#include <xrt/memory.h>
#include <string.h>

struct xrudpnoise {
	xrudprole Role;
	unsigned int Index;
	bool HasPin, Trusted, Taken;
	xrudpnoisetrustproc Trust;
	void* TrustContext;
	unsigned char StaticPrivate[32], StaticPublic[32];
	unsigned char EphemeralPrivate[32], EphemeralPublic[32];
	unsigned char RemoteStatic[32], RemoteEphemeral[32], Pin[32];
	unsigned char Hash[32], Chain[32], CipherKey[32];
	uint64_t Nonce;
	unsigned char SendKey[32], ReceiveKey[32];
	unsigned char PeerParameters[XRUDP_NOISE_PARAMETERS_MAX];
	size_t PeerParameterSize;
};

/* Internal vector-test seam. NULL ephemeral uses the OS secure random source.
 * Not a public/XS symbol; production callers must use xrtRudpNoiseCreate. */
xrudpresult xrudp__noise_create(const xrudpnoiseconfig* config,
	const void* deterministicEphemeral, xrudpnoise** output);

#endif
