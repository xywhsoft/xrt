/* Observe real decoder allocations immediately before the CSR source frees
 * them. Core allocation/decoding/freeing and all parsing remain unchanged. */
#define XACME_MODULE_ACME_CSR
#include <xacme/features.h>
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#include "../../src/acme/xacme_jose.c"
#include "../test.h"
#include "../acme/key_fixtures.h"

static struct { ptr address; size_t size; } keyProbeRaw[8];
static size_t keyProbeCount, keyProbeUncleared;

static bytes keyProbeDecode(const xpemblock* block, size_t* size)
{
    bytes result = xrtPemDecodeNew(block, size);
    if (result) {
        testRequire(keyProbeCount < sizeof(keyProbeRaw) / sizeof(keyProbeRaw[0]), "key decoder probe overflow");
        keyProbeRaw[keyProbeCount].address = result;
        keyProbeRaw[keyProbeCount++].size = *size;
    }
    return result;
}

static void keyProbeFree(ptr address)
{
    for (size_t i = 0; i < keyProbeCount; ++i) {
        if (address && address == keyProbeRaw[i].address) {
            const uint8* data = address;
            uint8 nonzero = 0;
            for (size_t n = 0; n < keyProbeRaw[i].size; ++n) nonzero |= data[n];
            if (nonzero) keyProbeUncleared++;
            keyProbeRaw[i].address = NULL;
        }
    }
    xrtFree(address);
}

#undef xrtFree
#define xrtFree keyProbeFree
#define xrtPemDecodeNew keyProbeDecode
#include "../../src/acme/xacme_csr.c"
#undef xrtPemDecodeNew
#undef xrtFree

static size_t keyProbeCase(cstr label, cstr pem, bool ec_only, bool expected)
{
    xacmees256key ec;
    xacmecertkey cert;
    bool result;
    memset(keyProbeRaw, 0, sizeof(keyProbeRaw));
    keyProbeCount = keyProbeUncleared = 0;
    memset(&ec, 0, sizeof(ec)); memset(&cert, 0, sizeof(cert));
    xrtClearError();
    result = ec_only ? xacmeKeyPemRead(pem, strlen(pem), &ec) :
        xacmeCertKeyReadPem(pem, strlen(pem), &cert);
    testRequire(result == expected, "key retirement fixture changed parse outcome");
    testRequire(keyProbeCount != 0u, "key retirement fixture did not decode any private DER");
    for (size_t i = 0; i < keyProbeCount; ++i)
        testRequire(keyProbeRaw[i].address == NULL, "key retirement leaked a decoder allocation");
    printf("[key-wipe] %s decoded=%zu uncleared=%zu\n", label, keyProbeCount, keyProbeUncleared);
    xrtSecureZero(&ec, sizeof(ec)); xacmeCertKeyUnit(&cert); xrtClearError();
    return keyProbeUncleared;
}

static str keyProbeChangeOid(cstr pem, const uint8* oid, size_t oid_size)
{
    xpemblock block;
    bytes der;
    size_t size;
    str changed = NULL;
    testRequire(xrtPemFind(pem, strlen(pem), "PRIVATE KEY", &block), "key probe missing PKCS8");
    der = xrtPemDecodeNew(&block, &size);
    testRequire(der != NULL, "key probe decode failed");
    for (size_t i = 0; i + oid_size <= size; ++i) {
        if (memcmp(der + i, oid, oid_size) == 0) {
            der[i + oid_size - 1u]++;
            changed = xrtPemEncodeNew("PRIVATE KEY", der, size);
            break;
        }
    }
    xrtSecureZero(der, size); xrtFree(der);
    testRequire(changed != NULL, "key probe OID mutation failed");
    return changed;
}

static str keyProbePkcs1(bool malformed)
{
    xpemblock block;
    bytes der;
    size_t size;
    xdercursor cursor;
    xdervalue value;
    xbytesview private_der;
    str pem;
    testRequire(xrtPemFind(testCsrRsaPem, strlen(testCsrRsaPem), "PRIVATE KEY", &block), "key probe missing RSA fixture");
    der = xrtPemDecodeNew(&block, &size);
    testRequire(der != NULL && xrtDerInit(&cursor, der, size) &&
        xrtDerExpect(&cursor, XASN1_UNIVERSAL, XASN1_SEQUENCE, true, &value) &&
        xrtDerEnter(&value, &cursor) && xrtDerRead(&cursor, &value) == XDER_VALUE &&
        xrtDerRead(&cursor, &value) == XDER_VALUE && xrtDerRead(&cursor, &value) == XDER_VALUE &&
        xrtDerOctets(&value, &private_der), "key probe RSA fixture shape changed");
    if (malformed) ((uint8*)private_der.Data)[0] = 0x31u;
    pem = xrtPemEncodeNew("RSA PRIVATE KEY", private_der.Data, private_der.Size);
    xrtSecureZero(der, size); xrtFree(der);
    testRequire(pem != NULL, "key probe RSA fixture encoding failed");
    return pem;
}

int main(void)
{
    static const uint8 curve[] = {0x2a,0x86,0x48,0xce,0x3d,0x03,0x01,0x07};
    static const uint8 rsa[] = {0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x01,0x01};
    xacmees256key key = {{0},{0}};
    str ec, wrong_curve, pkcs1, invalid_pkcs1, wrong_algorithm;
    size_t failures = 0;
    key.Private[31] = 1u;
    testRequire(xacmeEs256FromPrivate(&key), "key probe EC fixture failed");
    ec = xacmeKeyPemWrite(&key);
    testRequire(ec != NULL, "key probe EC write failed");
    wrong_curve = keyProbeChangeOid(ec, curve, sizeof(curve));
    pkcs1 = keyProbePkcs1(false); invalid_pkcs1 = keyProbePkcs1(true);
    wrong_algorithm = keyProbeChangeOid(testCsrRsaPem, rsa, sizeof(rsa));
    failures += keyProbeCase("EC dedicated", ec, true, true);
    failures += keyProbeCase("EC generic", ec, false, true);
    failures += keyProbeCase("EC rejected fallback", wrong_curve, false, false);
    failures += keyProbeCase("RSA PKCS8", testCsrRsaPem, false, true);
    failures += keyProbeCase("RSA PKCS1", pkcs1, false, true);
    failures += keyProbeCase("RSA malformed PKCS1", invalid_pkcs1, false, false);
    failures += keyProbeCase("RSA unsupported algorithm", wrong_algorithm, false, false);
    xrtSecureZero(ec, strlen(ec)); xrtFree(ec);
    xrtSecureZero(wrong_curve, strlen(wrong_curve)); xrtFree(wrong_curve);
    xrtSecureZero(pkcs1, strlen(pkcs1)); xrtFree(pkcs1);
    xrtSecureZero(invalid_pkcs1, strlen(invalid_pkcs1)); xrtFree(invalid_pkcs1);
    xrtSecureZero(wrong_algorithm, strlen(wrong_algorithm)); xrtFree(wrong_algorithm);
    xrtSecureZero(&key, sizeof(key)); xrtClearError();
    testMemoryDebugDrain("key retirement probe leaked logical allocations");
    testRequire(failures == 0u, "private DER was freed before being securely cleared");
    printf("[PASS] private DER cleared before retirement on all seven reader paths\n");
    return 0;
}
