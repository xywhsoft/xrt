/* Actual encoder, DER/PEM and Core allocator. Observe the extension's real
 * buffer retirement and volatile zeroing; no encoder or allocator is replaced. */
#define XACME_MODULE_ACME_CSR
#include <xacme/features.h>
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#include "../../src/acme/xacme_jose.c"
#include "../test.h"
#include "../acme/key_fixtures.h"

static bool exportWatching;
static xbytesview exportSecrets[6];
static size_t exportSecretCount, exportDirtyBuffers, exportStackErases;

static bool exportContainsSecret(const void* data, size_t size)
{
    const uint8* bytes = data;
    for (size_t s = 0; s < exportSecretCount; ++s) {
        xbytesview secret = exportSecrets[s];
        if (secret.Size < 16u || secret.Size > size) continue;
        for (size_t i = 0; i <= size - secret.Size; ++i)
            if (memcmp(bytes + i, secret.Data, secret.Size) == 0) return true;
    }
    return false;
}
static void exportBufferUnit(xbuffer* buffer)
{
    if (exportWatching && buffer && buffer->Data &&
        exportContainsSecret(buffer->Data, buffer->Size)) exportDirtyBuffers++;
    xrtBufferUnit(buffer);
}
static void exportZero(ptr data, size_t size)
{
    bool private_area = exportWatching && data && exportContainsSecret(data, size);
    xrtSecureZero(data, size);
    if (private_area) {
        uint8 nonzero = 0;
        for (size_t i = 0; i < size; ++i) nonzero |= ((const uint8*)data)[i];
        testRequire(nonzero == 0u, "export private area not zero after actual volatile clear");
        exportStackErases++;
    }
}
#define xrtBufferUnit exportBufferUnit
#define xrtSecureZero exportZero
#include "../../src/acme/xacme_csr.c"
#undef xrtBufferUnit
#undef xrtSecureZero

static void exportSelectSecrets(const xacmecertkey* key)
{
    exportSecretCount = 0u;
    if (key->Kind == XACME_CERT_KEY_ES256) {
        exportSecrets[exportSecretCount++] = (xbytesview){ key->Ec.Private, sizeof(key->Ec.Private) };
    } else {
        exportSecrets[exportSecretCount++] = (xbytesview){ key->Rsa.PrivateExponent, key->Rsa.PrivateExponentSize };
        exportSecrets[exportSecretCount++] = (xbytesview){ key->Rsa.Prime1, key->Rsa.Prime1Size };
        exportSecrets[exportSecretCount++] = (xbytesview){ key->Rsa.Prime2, key->Rsa.Prime2Size };
        exportSecrets[exportSecretCount++] = (xbytesview){ key->Rsa.Exponent1, key->Rsa.Exponent1Size };
        exportSecrets[exportSecretCount++] = (xbytesview){ key->Rsa.Exponent2, key->Rsa.Exponent2Size };
        exportSecrets[exportSecretCount++] = (xbytesview){ key->Rsa.Coefficient, key->Rsa.CoefficientSize };
    }
}
static size_t exportCase(cstr name, const xacmecertkey* key, bool dedicated, uint64 budget)
{
    xmemdebugsnapshot before, after;
    xacmecertkey parsed, original = *key;
    str pem;
    size_t failures = 0;
    exportSelectSecrets(key); exportDirtyBuffers = exportStackErases = 0u;
    xrtClearError(); xrtMemDebugSnapshot(&before);
    if (budget != UINT64_MAX) testRequire(xrtMemDebugFailAfter(budget), "export allocation fault arm");
    exportWatching = true;
    pem = dedicated ? xacmeKeyPemWrite(&key->Ec) : xacmeCertKeyPemWrite(key);
    exportWatching = false;
    xrtMemDebugSnapshot(&after);
    bool injected = budget != UINT64_MAX && xrtMemDebugFailTriggered();
    if (budget != UINT64_MAX) xrtMemDebugFailClear();
    printf("[key-export] %s budget=%llu success=%d allocations=%llu dirty-buffers=%zu private-erases=%zu\n",
        name, (unsigned long long)budget, pem != NULL,
        (unsigned long long)(after.AllocCount - before.AllocCount), exportDirtyBuffers, exportStackErases);
    /* All intermediate heap copies (including Core DER scratch and moved
     * realloc blocks) are excluded by the actual allocation count, not only
     * by watching the extension's final BufferUnit calls. */
    if (exportDirtyBuffers != 0u || after.AllocCount - before.AllocCount > 1u ||
        exportStackErases != 1u) failures++;
    testRequire(memcmp(key, &original, sizeof(original)) == 0, "export changed borrowed key");
    if (budget == 0u) {
        testRequire(pem == NULL && injected && xrtErrorKind(xrtGetError()) == XERR_MEMORY,
            "output allocation failure must preserve MEMORY");
    } else {
        testRequire(pem != NULL && !injected, "only the final PEM text may allocate");
        testRequire(xacmeCertKeyReadPem(pem, strlen(pem), &parsed) && parsed.Kind == key->Kind,
            "actual exported PEM must roundtrip");
        if (key->Kind == XACME_CERT_KEY_ES256)
            testRequire(memcmp(&parsed.Ec, &key->Ec, sizeof(key->Ec)) == 0, "EC export identity");
        else testRequire(parsed.Rsa.ModulusSize == key->Rsa.ModulusSize &&
            parsed.Rsa.PrivateExponentSize == key->Rsa.PrivateExponentSize &&
            memcmp(parsed.Rsa.Modulus, key->Rsa.Modulus, key->Rsa.ModulusSize) == 0 &&
            memcmp(parsed.Rsa.PrivateExponent, key->Rsa.PrivateExponent, key->Rsa.PrivateExponentSize) == 0,
            "RSA export identity");
        xacmeCertKeyUnit(&parsed); xrtSecureZero(pem, strlen(pem)); xrtFree(pem);
    }
    xrtSecureZero(&original, sizeof(original)); xrtClearError();
    xrtMemDebugSnapshot(&after);
    testRequire(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes &&
        after.InvalidFreeCount == before.InvalidFreeCount && after.DoubleFreeCount == before.DoubleFreeCount,
        "export leaked logical storage or freed invalid memory");
    return failures;
}
static size_t exportInvalidCase(const xacmecertkey* source, unsigned field, bool extreme)
{
    xacmecertkey key = *source, original;
    size_t* sizes[] = {&key.Rsa.ModulusSize, &key.Rsa.ExponentSize, &key.Rsa.PrivateExponentSize,
        &key.Rsa.Prime1Size, &key.Rsa.Prime2Size, &key.Rsa.Exponent1Size,
        &key.Rsa.Exponent2Size, &key.Rsa.CoefficientSize};
    size_t capacities[] = {sizeof(key.Rsa.Modulus), sizeof(key.Rsa.Exponent), sizeof(key.Rsa.PrivateExponent),
        sizeof(key.Rsa.Prime1), sizeof(key.Rsa.Prime2), sizeof(key.Rsa.Exponent1),
        sizeof(key.Rsa.Exponent2), sizeof(key.Rsa.Coefficient)};
    if (field < 8u) *sizes[field] = extreme ? SIZE_MAX : capacities[field] + 1u;
    else key.Kind = (xacmecertkeykind)2;
    original = key;
    xrtClearError();
    str pem = xacmeCertKeyPemWrite(&key);
    bool rejected = pem == NULL && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT &&
        xrtErrorDomain(xrtGetError()) && strcmp(xrtErrorDomain(xrtGetError()), "xrt.acme.csr") == 0;
    printf("[key-export-invalid] field=%u extreme=%d rejected=%d\n", field, extreme, rejected);
    testRequire(memcmp(&key, &original, sizeof(key)) == 0, "invalid export changed borrowed key");
    if (pem) { xrtSecureZero(pem, strlen(pem)); xrtFree(pem); }
    xacmeCertKeyUnit(&key); xacmeCertKeyUnit(&original); xrtClearError();
    return rejected ? 0u : 1u;
}
int main(void)
{
    xacmecertkey ec = {0}, rsa, minimal, maximum;
    size_t failures = 0;
    for (size_t i = 0; i < sizeof(ec.Ec.Private); ++i) ec.Ec.Private[i] = (uint8)(i + 1u);
    testRequire(xacmeEs256FromPrivate(&ec.Ec), "export EC fixture");
    testRequire(xacmeCertKeyReadPem(testCsrRsaPem, strlen(testCsrRsaPem), &rsa), "export RSA fixture");
    minimal = rsa;
    minimal.Rsa.Prime1Size = minimal.Rsa.Prime2Size = minimal.Rsa.Exponent1Size =
        minimal.Rsa.Exponent2Size = minimal.Rsa.CoefficientSize = 0u;
    {
        xacmecertkey padded = rsa, partial = rsa;
        str canonical = xacmeCertKeyPemWrite(&rsa), shortened = xacmeCertKeyPemWrite(&minimal);
        testRequire(canonical && shortened && strcmp(canonical, testCsrRsaPem) == 0,
            "RSA export differs from external PKCS8 fixed vector");
        #define PAD_COMPONENT(field) do { memmove(padded.Rsa.field + 2u, padded.Rsa.field, padded.Rsa.field##Size); \
            memset(padded.Rsa.field, 0, 2u); padded.Rsa.field##Size += 2u; } while (0)
        PAD_COMPONENT(Modulus); PAD_COMPONENT(Exponent); PAD_COMPONENT(PrivateExponent);
        PAD_COMPONENT(Prime1); PAD_COMPONENT(Prime2); PAD_COMPONENT(Exponent1);
        PAD_COMPONENT(Exponent2); PAD_COMPONENT(Coefficient);
        #undef PAD_COMPONENT
        partial.Rsa.Prime2Size = 0u;
        str padded_pem = xacmeCertKeyPemWrite(&padded), partial_pem = xacmeCertKeyPemWrite(&partial);
        testRequire(padded_pem && partial_pem && strcmp(padded_pem, canonical) == 0 &&
            strcmp(partial_pem, shortened) == 0, "leading-zero normalization or legacy CRT fallback changed");
        xrtSecureZero(canonical, strlen(canonical)); xrtFree(canonical);
        xrtSecureZero(shortened, strlen(shortened)); xrtFree(shortened);
        xrtSecureZero(padded_pem, strlen(padded_pem)); xrtFree(padded_pem);
        xrtSecureZero(partial_pem, strlen(partial_pem)); xrtFree(partial_pem);
        xacmeCertKeyUnit(&padded); xacmeCertKeyUnit(&partial);
    }
    memset(&maximum, 0, sizeof(maximum)); maximum.Kind = XACME_CERT_KEY_RSA;
    #define MAX_COMPONENT(field, byte) do { memset(maximum.Rsa.field, byte, sizeof(maximum.Rsa.field)); \
        maximum.Rsa.field##Size = sizeof(maximum.Rsa.field); } while (0)
    MAX_COMPONENT(Modulus, 0xd1); MAX_COMPONENT(Exponent, 0x01); MAX_COMPONENT(PrivateExponent, 0xd3);
    MAX_COMPONENT(Prime1, 0xd4); MAX_COMPONENT(Prime2, 0xd5); MAX_COMPONENT(Exponent1, 0xd6);
    MAX_COMPONENT(Exponent2, 0xd7); MAX_COMPONENT(Coefficient, 0xd8);
    #undef MAX_COMPONENT
    const xacmecertkey* keys[] = {&ec, &ec, &rsa, &minimal, &maximum};
    cstr names[] = {"EC dedicated", "EC generic", "RSA CRT", "RSA n/e/d", "RSA field capacities"};
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        failures += exportCase(names[i], keys[i], i == 0u, UINT64_MAX);
        failures += exportCase(names[i], keys[i], i == 0u, 0u);
        failures += exportCase(names[i], keys[i], i == 0u, 1u);
    }
    /* Synthetic bounded integers check DER sign/length transitions, not RSA
     * mathematics. The independent key oracle uses the actual CRT fixture. */
    static const size_t lengths[] = {32,63,64,120,126,127,128,129,254,255,256,257,1023,1024};
    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
        xacmecertkey edge = minimal;
        char name[64];
        memset(edge.Rsa.Modulus, 0x83, sizeof(edge.Rsa.Modulus));
        memset(edge.Rsa.PrivateExponent, 0xa5, sizeof(edge.Rsa.PrivateExponent));
        edge.Rsa.ModulusSize = edge.Rsa.PrivateExponentSize = lengths[i];
        snprintf(name, sizeof(name), "RSA DER length %zu", lengths[i]);
        failures += exportCase(name, &edge, false, UINT64_MAX);
        xacmeCertKeyUnit(&edge);
    }
    for (unsigned i = 0; i < 8u; ++i) {
        failures += exportInvalidCase(&rsa, i, false);
        failures += exportInvalidCase(&rsa, i, true);
    }
    failures += exportInvalidCase(&rsa, 8u, false);
    xacmeCertKeyUnit(&ec); xacmeCertKeyUnit(&rsa); xacmeCertKeyUnit(&minimal); xacmeCertKeyUnit(&maximum);
    testMemoryDebugDrain("key export probe leaked logical allocations");
    testRequire(failures == 0u, "private key encoder retained heap scratch or uncleared private storage");
    puts("[PASS] private key export: one output allocation, no heap scratch, erased local DER, unchanged inputs");
    return 0;
}
