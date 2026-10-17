#include <xrudp.h>
#include <xrt/memory.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
	fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; \
} } while (0)

static size_t allocations;
static void* fail_alloc(void* context, size_t size)
{ (void)context; (void)size; ++allocations; return NULL; }
static void* fail_realloc(void* context, void* data, size_t size)
{ (void)context; (void)data; (void)size; ++allocations; return NULL; }
static void no_free(void* context, void* data)
{ (void)context; (void)data; ++allocations; }

int main(void)
{
	xallocator allocator = {NULL, fail_alloc, fail_realloc, no_free};
	unsigned char key[32], nextKey[32], scope[16], hash[32];
	unsigned char cookie[XRUDP_COOKIE_SIZE], saved[XRUDP_COOKIE_SIZE], output[XRUDP_COOKIE_SIZE];
	xrudpaddress address = {0}, altered;
	uint64_t now = UINT64_C(0x0102030405060708);
	/* Python standard-library hmac/hashlib oracle, canonical address encoding;
	 * all keys and hashes here are synthetic fixture bytes. */
	static const unsigned char golden[XRUDP_COOKIE_SIZE] = {
		0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
		0x8b,0xeb,0x29,0xe2,0x22,0xb0,0xed,0x73,
		0xbd,0xb3,0x26,0x15,0x4d,0xd5,0xf7,0xb3,
		0x5a,0x16,0x23,0xcd,0xea,0x34,0x45,0xdb,
		0xdc,0x36,0xdf,0xe8,0x89,0x2b,0xb1,0x73
	};
	CHECK(xrtSetAllocator(&allocator));
	for (size_t i = 0; i < 32; ++i) { key[i] = (unsigned char)i; nextKey[i] = (unsigned char)(i + 1); hash[i] = (unsigned char)(0xa0 + i); }
	for (size_t i = 0; i < 16; ++i) scope[i] = (unsigned char)i;
	address.Family = 4; address.Port = 0x1234;
	address.Address[0] = 127; address.Address[3] = 1;
	CHECK(xrtRudpCookieCreate(key, scope, hash, &address, now, cookie, sizeof(cookie)) == XRUDP_OK);
	CHECK(memcmp(cookie, golden, sizeof(golden)) == 0);
	memcpy(saved, cookie, sizeof(saved));
	CHECK(xrtRudpCookieVerify(key, NULL, scope, hash, &address, now, 0, cookie, sizeof(cookie)) == XRUDP_OK);
	for (size_t i = 0; i < sizeof(cookie); ++i) {
		for (unsigned int bit = 0; bit < 8; ++bit) {
			cookie[i] ^= (unsigned char)(1u << bit);
			CHECK(xrtRudpCookieVerify(key, NULL, scope, hash, &address, now, 0, cookie, sizeof(cookie)) == XRUDP_AUTH_FAILED);
			cookie[i] ^= (unsigned char)(1u << bit);
		}
		CHECK(xrtRudpCookieVerify(key, NULL, scope, hash, &address, now, 0, cookie, i) == XRUDP_MALFORMED);
	}
	CHECK(memcmp(cookie, saved, sizeof(cookie)) == 0);
	CHECK(xrtRudpCookieVerify(key, NULL, scope, hash, &address, now + XRUDP_COOKIE_MAX_AGE_US,
		0, cookie, sizeof(cookie)) == XRUDP_OK);
	CHECK(xrtRudpCookieVerify(key, NULL, scope, hash, &address, now + XRUDP_COOKIE_MAX_AGE_US + 1,
		0, cookie, sizeof(cookie)) == XRUDP_TIMEOUT);
	CHECK(xrtRudpCookieVerify(key, NULL, scope, hash, &address, now - 1, 0, cookie, sizeof(cookie)) == XRUDP_TIMEOUT);
	CHECK(xrtRudpCookieVerify(key, NULL, scope, hash, &address, now + 6, 5, cookie, sizeof(cookie)) == XRUDP_TIMEOUT);
	CHECK(xrtRudpCookieVerify(key, NULL, scope, hash, &address, now, XRUDP_COOKIE_MAX_AGE_US + 1,
		cookie, sizeof(cookie)) == XRUDP_ARGUMENT);
	CHECK(xrtRudpCookieVerify(nextKey, NULL, scope, hash, &address, now, 0, cookie, sizeof(cookie)) == XRUDP_AUTH_FAILED);
	CHECK(xrtRudpCookieVerify(nextKey, key, scope, hash, &address, now, 0, cookie, sizeof(cookie)) == XRUDP_OK);
	CHECK(xrtRudpCookieVerify(key, nextKey, scope, hash, &address, now, 0, cookie, sizeof(cookie)) == XRUDP_OK);
	scope[0] ^= 1;
	CHECK(xrtRudpCookieVerify(key, NULL, scope, hash, &address, now, 0, cookie, sizeof(cookie)) == XRUDP_AUTH_FAILED);
	scope[0] ^= 1; hash[0] ^= 1;
	CHECK(xrtRudpCookieVerify(key, NULL, scope, hash, &address, now, 0, cookie, sizeof(cookie)) == XRUDP_AUTH_FAILED);
	hash[0] ^= 1; altered = address; ++altered.Port;
	CHECK(xrtRudpCookieVerify(key, NULL, scope, hash, &altered, now, 0, cookie, sizeof(cookie)) == XRUDP_AUTH_FAILED);
	altered = address; ++altered.Address[3];
	CHECK(xrtRudpCookieVerify(key, NULL, scope, hash, &altered, now, 0, cookie, sizeof(cookie)) == XRUDP_AUTH_FAILED);
	memset(output, 0xa5, sizeof(output));
	CHECK(xrtRudpCookieCreate(key, scope, hash, &address, now, output, sizeof(output) - 1) == XRUDP_LIMIT);
	for (size_t i = 0; i < sizeof(output); ++i) CHECK(output[i] == 0xa5);
	altered = address; altered.Address[15] = 1;
	CHECK(xrtRudpCookieCreate(key, scope, hash, &altered, now, output, sizeof(output)) == XRUDP_ARGUMENT);
	altered = address; altered.Zone = 1;
	CHECK(xrtRudpCookieCreate(key, scope, hash, &altered, now, output, sizeof(output)) == XRUDP_ARGUMENT);
	altered = address; altered.Address[0] = 224;
	CHECK(xrtRudpCookieCreate(key, scope, hash, &altered, now, output, sizeof(output)) == XRUDP_ARGUMENT);
	memset(&address, 0, sizeof(address)); address.Family = 6; address.Port = 1234;
	address.Address[0] = 0xfe; address.Address[1] = 0x80; address.Address[15] = 1; address.Zone = 3;
	CHECK(xrtRudpCookieCreate(key, scope, hash, &address, UINT64_MAX - 10, cookie, sizeof(cookie)) == XRUDP_OK);
	CHECK(xrtRudpCookieVerify(key, NULL, scope, hash, &address, UINT64_MAX, 0, cookie, sizeof(cookie)) == XRUDP_OK);
	CHECK(xrtRudpCookieVerify(key, NULL, scope, hash, &address, UINT64_MAX, 5, cookie, sizeof(cookie)) == XRUDP_TIMEOUT);
	++address.Zone;
	CHECK(xrtRudpCookieVerify(key, NULL, scope, hash, &address, UINT64_MAX, 0, cookie, sizeof(cookie)) == XRUDP_AUTH_FAILED);
	address.Address[0] = 0xff;
	CHECK(xrtRudpCookieCreate(key, scope, hash, &address, now, output, sizeof(output)) == XRUDP_ARGUMENT);
	CHECK(allocations == 0);
	puts("xrudp cookie: authentication, address/session binding, expiry, rotation, no allocations passed");
	return 0;
}
