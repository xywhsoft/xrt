#include <xrudp/cookie.h>
#include <xrt/crypto.h>
#include <xrt/memory.h>
#include <string.h>

static bool xrudp__cookie_nonzero(const unsigned char* p, size_t size)
{
	unsigned char any = 0;
	for (size_t i = 0; i < size; ++i) any |= p[i];
	return any != 0;
}

static void xrudp__cookie_put32(unsigned char* p, uint32_t n)
{
	for (size_t i = 0; i < 4; ++i) p[3 - i] = (unsigned char)(n >> (i * 8));
}

static bool xrudp__cookie_address(const xrudpaddress* address, unsigned char wire[23])
{
	if (address == NULL || address->Port == 0) return false;
	if (address->Family == 4) {
		if (address->Zone != 0 || xrudp__cookie_nonzero(address->Address + 4, 12) ||
			address->Address[0] == 0 || address->Address[0] >= 224) return false;
	} else if (address->Family == 6) {
		if (!xrudp__cookie_nonzero(address->Address, 16) || address->Address[0] == 0xff)
			return false;
	} else return false;
	wire[0] = address->Family;
	wire[1] = (unsigned char)(address->Port >> 8);
	wire[2] = (unsigned char)address->Port;
	xrudp__cookie_put32(wire + 3, address->Zone);
	memcpy(wire + 7, address->Address, 16);
	return true;
}

static bool xrudp__cookie_mac(const void* key, const void* scope,
	const void* helloHash, const unsigned char address[23],
	const unsigned char timestamp[8], unsigned char mac[32])
{
	static const char domain[] = "XRUDP001/address-cookie";
	xhmacsha256 state;
	bool ok = xrtHmacSha256Init(&state, key, 32) &&
		xrtHmacSha256Update(&state, domain, sizeof(domain) - 1) &&
		xrtHmacSha256Update(&state, scope, 16) &&
		xrtHmacSha256Update(&state, helloHash, 32) &&
		xrtHmacSha256Update(&state, address, 23) &&
		xrtHmacSha256Update(&state, timestamp, 8) &&
		xrtHmacSha256Final(&state, mac);
	xrtSecureZero(&state, sizeof(state));
	return ok;
}

xrudpresult xrtRudpCookieCreate(const void* pKey,
	const void* pScope, const void* pHelloHash, const xrudpaddress* pAddress,
	uint64_t iNowUs, void* pCookie, size_t iCapacity)
{
	unsigned char wire[23], cookie[XRUDP_COOKIE_SIZE];
	bool ok;
	if (pKey == NULL || pScope == NULL || pHelloHash == NULL || pCookie == NULL ||
		!xrudp__cookie_nonzero((const unsigned char*)pScope, 16) ||
		!xrudp__cookie_address(pAddress, wire)) return XRUDP_ARGUMENT;
	if (iCapacity < XRUDP_COOKIE_SIZE) return XRUDP_LIMIT;
	for (size_t i = 0; i < 8; ++i) cookie[7 - i] = (unsigned char)(iNowUs >> (i * 8));
	ok = xrudp__cookie_mac(pKey, pScope, pHelloHash, wire, cookie, cookie + 8);
	if (ok) memmove(pCookie, cookie, sizeof(cookie));
	xrtSecureZero(cookie, sizeof(cookie));
	return ok ? XRUDP_OK : XRUDP_IO_ERROR;
}

xrudpresult xrtRudpCookieVerify(const void* pCurrentKey,
	const void* pPreviousKey, const void* pScope, const void* pHelloHash,
	const xrudpaddress* pAddress, uint64_t iNowUs, uint64_t iMaxAgeUs,
	const void* pCookie, size_t iCookieSize)
{
	const unsigned char* cookie = (const unsigned char*)pCookie;
	unsigned char wire[23], current[32] = {0}, previous[32] = {0};
	uint64_t issued = 0;
	bool ok, valid, currentValid, previousValid;
	if (pCurrentKey == NULL || pScope == NULL || pHelloHash == NULL || cookie == NULL ||
		iMaxAgeUs > XRUDP_COOKIE_MAX_AGE_US ||
		!xrudp__cookie_nonzero((const unsigned char*)pScope, 16) ||
		!xrudp__cookie_address(pAddress, wire)) return XRUDP_ARGUMENT;
	if (iCookieSize != XRUDP_COOKIE_SIZE) return XRUDP_MALFORMED;
	/* Always calculate both candidates, without a key-ID/timing oracle. */
	ok = xrudp__cookie_mac(pCurrentKey, pScope, pHelloHash, wire, cookie, current);
	ok = xrudp__cookie_mac(pPreviousKey != NULL ? pPreviousKey : pCurrentKey,
		pScope, pHelloHash, wire, cookie, previous) && ok;
	currentValid = xrtConstTimeEqual(current, cookie + 8, 32);
	previousValid = xrtConstTimeEqual(previous, cookie + 8, 32);
	valid = currentValid || previousValid;
	xrtSecureZero(current, sizeof(current));
	xrtSecureZero(previous, sizeof(previous));
	if (!ok) return XRUDP_IO_ERROR;
	if (!valid) return XRUDP_AUTH_FAILED;
	for (size_t i = 0; i < 8; ++i) issued = (issued << 8) | cookie[i];
	if (iMaxAgeUs == 0) iMaxAgeUs = XRUDP_COOKIE_MAX_AGE_US;
	if (issued > iNowUs || iNowUs - issued > iMaxAgeUs) return XRUDP_TIMEOUT;
	return XRUDP_OK;
}
