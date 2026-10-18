#ifndef XRUDP_COOKIE_H
#define XRUDP_COOKIE_H

#include <xrudp/features.h>
#include <xrudp/wire.h>

#if defined(XRUDP_FEATURE_RUDP_COOKIE)

#define XRUDP_COOKIE_SIZE 40u
#define XRUDP_COOKIE_MAX_AGE_US UINT64_C(30000000)

XRT_EXTERN_C_BEGIN

/* Stateless address-return validation, NOT peer identity or a one-use grant.
 * Key is a 32-byte server secret generated securely per endpoint generation.
 * Scope is a nonzero 16-byte listener/session domain. HelloHash is the 32-byte
 * digest of the canonical client probe, including connection IDs and target.
 * Time uses the server's monotonic microseconds; tokens expire at most 30s
 * after issuance. The driver enforces amplification/CPU/rate limits separately.
 * No allocation, random generation, logging or I/O. Failure preserves output. */
XRUDP_API xrudpresult xrtRudpCookieCreate(const void* pKey,
	const void* pScope, const void* pHelloHash, const xrudpaddress* pAddress,
	uint64_t iNowUs, void* pCookie, size_t iCapacity);
/* PreviousKey may be NULL, or the immediately previous endpoint secret during
 * a bounded rotation grace period. iMaxAgeUs=0 selects the fixed maximum.
 * Authentication is checked before reporting an expired/future timestamp.
 * Keys and caller policy remain unchanged on every result. */
XRUDP_API xrudpresult xrtRudpCookieVerify(const void* pCurrentKey,
	const void* pPreviousKey, const void* pScope, const void* pHelloHash,
	const xrudpaddress* pAddress, uint64_t iNowUs, uint64_t iMaxAgeUs,
	const void* pCookie, size_t iCookieSize);

XRT_EXTERN_C_END

#endif
#endif
