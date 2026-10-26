#ifndef XACME_TEST_KEY_IMPORT_CASES_H
#define XACME_TEST_KEY_IMPORT_CASES_H

enum {
    KEY_SEC1 = 1u, KEY_NO_CURVE = 2u, KEY_NO_PUBLIC = 4u,
    KEY_VERSION = 8u, KEY_CURVE = 16u, KEY_PUBLIC = 32u,
    KEY_PREFIX = 64u, KEY_ZERO = 128u, KEY_SHORT = 256u,
    KEY_DUP_CURVE = 512u, KEY_DUP_PUBLIC = 1024u,
    KEY_ORDER = 2048u, KEY_INNER_TAIL = 4096u, KEY_OUTER_TAIL = 8192u,
    KEY_ALGORITHM = 16384u, KEY_OUTER_CURVE = 32768u,
    KEY_OUTER_NO_CURVE = 65536u, KEY_OUTER_NULL_CURVE = 131072u,
    KEY_OUTER_ALG_TAIL = 262144u, KEY_V2 = 524288u,
    KEY_OUTER_PUBLIC = 1048576u, KEY_OUTER_PUBLIC_BAD = 2097152u,
    KEY_OUTER_PUBLIC_TAG = 4194304u, KEY_DUP_OUTER_PUBLIC = 8388608u,
    KEY_ATTRIBUTES = 16777216u, KEY_DUP_ATTRIBUTES = 33554432u,
    KEY_ATTRIBUTES_AFTER_PUBLIC = 67108864u, KEY_OUTER_VERSION = 134217728u,
    KEY_ATTRIBUTE_VALUE = 268435456u, KEY_ATTRIBUTE_BAD = 536870912u,
    KEY_ATTRIBUTE_UNSORTED = 1073741824u
};

static bool keyTestPublic(xbuffer* out, const uint8* point, bool outer, bool wrong_tag)
{
    xbuffer bits;
    bool ok;
    xrtBufferInit(&bits);
    if (outer && !wrong_tag) {
        ok = xrtBufferAppendByte(&bits, 0) &&
            xrtBufferAppend(&bits, (xbytesview){point, 65u}) &&
            xrtDerAppend(out, XASN1_CONTEXT, 1u, false, xrtBufferView(&bits));
    } else {
        ok = xrtDerAppendBitString(&bits, (xbytesview){point, 65u}, 0u) &&
            xrtDerAppend(out, XASN1_CONTEXT, 1u, true, xrtBufferView(&bits));
    }
    xrtBufferUnit(&bits);
    return ok;
}

/* Raw wrapping permits deliberate malformed children; the production DER
 * builder correctly refuses to wrap those, which would hide parser regressions. */
static bool keyTestSequence(xbuffer* out, xbytesview data)
{
    uint8 length[sizeof(size_t)];
    size_t count = 0, size = data.Size;
    bool ok = xrtBufferAppendByte(out, 0x30u);
    if (size < 128u) ok = ok && xrtBufferAppendByte(out, (uint8)size);
    else {
        while (size) { length[sizeof(length) - ++count] = (uint8)size; size >>= 8u; }
        ok = ok && xrtBufferAppendByte(out, (uint8)(0x80u | count)) &&
            xrtBufferAppend(out, (xbytesview){length + sizeof(length) - count, count});
    }
    return ok && xrtBufferAppend(out, data);
}

static bool keyTestAttributes(xbuffer* out, unsigned flags)
{
    xbuffer attributes, attribute, values;
    bool ok = true;
    xrtBufferInit(&attributes); xrtBufferInit(&attribute); xrtBufferInit(&values);
    if (flags & (KEY_ATTRIBUTE_VALUE | KEY_ATTRIBUTE_BAD | KEY_ATTRIBUTE_UNSORTED)) {
        size_t count = flags & KEY_ATTRIBUTE_UNSORTED ? 2u : 1u;
        for (size_t i = 0; ok && i < count; ++i) {
            attribute.Size = 0; values.Size = 0;
            ok = xrtDerAppendOid(&attribute, count == 2u && i == 0u ?
                XRT_STR_LITERAL("1.2.3.5") : XRT_STR_LITERAL("1.2.3.4"));
            if (!(flags & KEY_ATTRIBUTE_BAD))
                ok = ok && xrtDerAppend(&values, XASN1_UNIVERSAL, XASN1_UTF8_STRING, false, XRT_BYTES_LITERAL("metadata")) &&
                    xrtDerAppend(&attribute, XASN1_UNIVERSAL, XASN1_SET, true, xrtBufferView(&values));
            ok = ok && keyTestSequence(&attributes, xrtBufferView(&attribute));
        }
    }
    ok = ok && xrtDerAppend(out, XASN1_CONTEXT, 0u, true, xrtBufferView(&attributes));
    xrtBufferUnit(&attributes); xrtBufferUnit(&attribute); xrtBufferUnit(&values);
    return ok;
}

/* Independent ASN.1 fixtures assembled from a fixed external P-256 key.
 * Each flag changes one declared property; parsing/signing code is never mocked. */
static str keyTestPem(const xacmees256key* key, unsigned flags)
{
    xbuffer sec, inner, curve, alg, body, encoded;
    uint8 scalar[32], point[65];
    str pem = NULL;
    bool ok;
    memcpy(scalar, key->Private, sizeof(scalar));
    memcpy(point, key->Public, sizeof(point));
    if (flags & KEY_ZERO) memset(scalar, 0, sizeof(scalar));
    if (flags & KEY_PUBLIC) point[64] ^= 1u;
    if (flags & KEY_PREFIX) point[0] = 0x05u;
    xrtBufferInit(&sec); xrtBufferInit(&inner); xrtBufferInit(&curve);
    xrtBufferInit(&alg); xrtBufferInit(&body); xrtBufferInit(&encoded);
    ok = xrtDerAppendUInt64(&sec, flags & KEY_VERSION ? 2u : 1u) &&
        xrtDerAppend(&sec, XASN1_UNIVERSAL, XASN1_OCTET_STRING, false,
            (xbytesview){scalar, flags & KEY_SHORT ? 31u : 32u});
    if (ok && (flags & KEY_ORDER)) ok = keyTestPublic(&sec, point, false, false);
    if (ok && !(flags & KEY_NO_CURVE)) {
        ok = xrtDerAppendOid(&curve, flags & KEY_CURVE ?
            XRT_STR_LITERAL("1.3.132.0.10") : XRT_STR_LITERAL("1.2.840.10045.3.1.7")) &&
            xrtDerAppend(&sec, XASN1_CONTEXT, 0u, true, xrtBufferView(&curve));
        if (ok && (flags & KEY_DUP_CURVE))
            ok = xrtDerAppend(&sec, XASN1_CONTEXT, 0u, true, xrtBufferView(&curve));
    }
    if (ok && !(flags & (KEY_NO_PUBLIC | KEY_ORDER))) ok = keyTestPublic(&sec, point, false, false);
    if (ok && (flags & KEY_DUP_PUBLIC)) ok = keyTestPublic(&sec, point, false, false);
    if (ok && (flags & KEY_INNER_TAIL)) ok = xrtBufferAppendByte(&sec, 0xa0u);
    ok = ok && keyTestSequence(&inner, xrtBufferView(&sec));
    if (flags & KEY_SEC1) {
        ok = ok && xrtBufferAppend(&encoded, xrtBufferView(&inner));
    } else {
        ok = ok && xrtDerAppendOid(&alg, flags & KEY_ALGORITHM ?
            XRT_STR_LITERAL("1.3.132.1.12") : XRT_STR_LITERAL("1.2.840.10045.2.1"));
        if (ok && !(flags & KEY_OUTER_NO_CURVE)) {
            if (flags & KEY_OUTER_NULL_CURVE) ok = xrtDerAppendNull(&alg);
            else ok = xrtDerAppendOid(&alg, flags & KEY_OUTER_CURVE ?
                XRT_STR_LITERAL("1.3.132.0.34") : XRT_STR_LITERAL("1.2.840.10045.3.1.7"));
        }
        if (ok && (flags & KEY_OUTER_ALG_TAIL)) ok = xrtDerAppendNull(&alg);
        ok = ok && xrtDerAppendUInt64(&body, flags & KEY_OUTER_VERSION ? 2u : flags & KEY_V2 ? 1u : 0u) &&
            xrtDerAppend(&body, XASN1_UNIVERSAL, XASN1_SEQUENCE, true, xrtBufferView(&alg)) &&
            xrtDerAppend(&body, XASN1_UNIVERSAL, XASN1_OCTET_STRING, false, xrtBufferView(&inner));
        if (ok && (flags & KEY_ATTRIBUTES)) ok = keyTestAttributes(&body, flags);
        if (ok && (flags & KEY_DUP_ATTRIBUTES)) ok = xrtDerAppend(&body, XASN1_CONTEXT, 0u, true, (xbytesview){NULL, 0});
        if (flags & KEY_OUTER_PUBLIC_BAD) point[63] ^= 1u;
        if (ok && (flags & KEY_OUTER_PUBLIC)) ok = keyTestPublic(&body, point, true, (flags & KEY_OUTER_PUBLIC_TAG) != 0u);
        if (ok && (flags & KEY_DUP_OUTER_PUBLIC)) ok = keyTestPublic(&body, point, true, false);
        if (ok && (flags & KEY_ATTRIBUTES_AFTER_PUBLIC)) ok = xrtDerAppend(&body, XASN1_CONTEXT, 0u, true, (xbytesview){NULL, 0});
        ok = ok && keyTestSequence(&encoded, xrtBufferView(&body));
    }
    if (ok && (flags & KEY_OUTER_TAIL)) ok = xrtBufferAppendByte(&encoded, 0u);
    if (ok) pem = xrtPemEncodeNew(flags & KEY_SEC1 ? "EC PRIVATE KEY" : "PRIVATE KEY", encoded.Data, encoded.Size);
    xrtBufferUnit(&sec); xrtBufferUnit(&inner); xrtBufferUnit(&curve);
    xrtBufferUnit(&alg); xrtBufferUnit(&body); xrtBufferUnit(&encoded);
    xrtSecureZero(scalar, sizeof(scalar));
    return pem;
}

static void testEcKeyImportBoundaries(const xacmees256key* key)
{
    static const struct { const char* label; unsigned flags; bool valid; } cases[] = {
        {"PKCS8 P-256", 0u, true},
        {"SEC1 P-256", KEY_SEC1, true},
        {"PKCS8 derives omitted public point", KEY_NO_PUBLIC, true},
        {"PKCS8 supplies omitted inner curve", KEY_NO_CURVE, true},
        {"PKCS8 scalar only", KEY_NO_CURVE | KEY_NO_PUBLIC, true},
        {"legacy SEC1 uses P-256 context", KEY_SEC1 | KEY_NO_CURVE | KEY_NO_PUBLIC, true},
        {"SEC1 derives omitted public point", KEY_SEC1 | KEY_NO_PUBLIC, true},
        {"PKCS8 attributes", KEY_ATTRIBUTES, true},
        {"PKCS8 application metadata", KEY_ATTRIBUTES | KEY_ATTRIBUTE_VALUE, true},
        {"RFC5958 public point", KEY_V2 | KEY_OUTER_PUBLIC, true},
        {"RFC5958 sole public point", KEY_V2 | KEY_OUTER_PUBLIC | KEY_NO_PUBLIC, true},
        {"RFC5958 attributes", KEY_V2 | KEY_OUTER_PUBLIC | KEY_ATTRIBUTES, true},
        {"inner version", KEY_VERSION, false},
        {"SEC1 version", KEY_SEC1 | KEY_VERSION, false},
        {"conflicting inner curve", KEY_CURVE, false},
        {"SEC1 unsupported curve", KEY_SEC1 | KEY_CURVE, false},
        {"mismatched public point", KEY_PUBLIC, false},
        {"invalid point encoding", KEY_PREFIX, false},
        {"zero private scalar", KEY_ZERO, false},
        {"short private scalar", KEY_SHORT, false},
        {"duplicate curve", KEY_DUP_CURVE, false},
        {"duplicate public point", KEY_DUP_PUBLIC, false},
        {"reversed SEC1 fields", KEY_ORDER, false},
        {"truncated SEC1 optional field", KEY_INNER_TAIL, false},
        {"trailing DER bytes", KEY_OUTER_TAIL, false},
        {"key agreement algorithm cannot sign", KEY_ALGORITHM, false},
        {"unsupported outer curve", KEY_OUTER_CURVE, false},
        {"missing PKCS8 curve", KEY_OUTER_NO_CURVE, false},
        {"NULL PKCS8 curve", KEY_OUTER_NULL_CURVE, false},
        {"algorithm trailing field", KEY_OUTER_ALG_TAIL, false},
        {"outer version", KEY_OUTER_VERSION, false},
        {"v1 forbids outer public point", KEY_OUTER_PUBLIC, false},
        {"v2 requires outer public point", KEY_V2, false},
        {"outer point mismatch", KEY_V2 | KEY_OUTER_PUBLIC | KEY_OUTER_PUBLIC_BAD, false},
        {"outer point must be implicit", KEY_V2 | KEY_OUTER_PUBLIC | KEY_OUTER_PUBLIC_TAG, false},
        {"duplicate outer point", KEY_V2 | KEY_OUTER_PUBLIC | KEY_DUP_OUTER_PUBLIC, false},
        {"duplicate attributes", KEY_ATTRIBUTES | KEY_DUP_ATTRIBUTES, false},
        {"attribute missing values", KEY_ATTRIBUTES | KEY_ATTRIBUTE_BAD, false},
        {"attributes not DER sorted", KEY_ATTRIBUTES | KEY_ATTRIBUTE_UNSORTED, false},
        {"attributes after public point", KEY_V2 | KEY_OUTER_PUBLIC | KEY_ATTRIBUTES_AFTER_PUBLIC, false}
    };
    size_t failures = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        str pem = keyTestPem(key, cases[i].flags);
        xacmees256key ec, previous;
        xacmecertkey cert, prior;
        bool ok;
        testRequire(pem != NULL, "EC key import fixture allocation failed");
        memset(&ec, 0xa5, sizeof(ec)); previous = ec;
        xrtSetErrorInfo(XERR_MEMORY, "fixture.previous", 777, "previous unrelated operation");
        ok = xacmeKeyPemRead(pem, strlen(pem), &ec);
        if (ok != cases[i].valid || (ok ? memcmp(&ec, key, sizeof(ec)) != 0 :
            memcmp(&ec, &previous, sizeof(ec)) != 0 || xrtGetError() == NULL ||
            xrtErrorKind(xrtGetError()) == XERR_MEMORY)) {
            printf("[key-import] EC %s: accepted=%d expected=%d atomic=%d\n", cases[i].label,
                (int)ok, (int)cases[i].valid, (int)(memcmp(&ec, &previous, sizeof(ec)) == 0));
            failures++;
        }
        memset(&cert, 0x5a, sizeof(cert)); prior = cert;
        xrtSetErrorInfo(XERR_MEMORY, "fixture.previous", 777, "previous unrelated operation");
        ok = xacmeCertKeyReadPem(pem, strlen(pem), &cert);
        if (ok != cases[i].valid || (ok ? cert.Kind != XACME_CERT_KEY_ES256 ||
            memcmp(&cert.Ec, key, sizeof(ec)) != 0 :
            memcmp(&cert, &prior, sizeof(cert)) != 0 || xrtGetError() == NULL ||
            xrtErrorKind(xrtGetError()) == XERR_MEMORY)) {
            printf("[key-import] dispatch %s: accepted=%d expected=%d atomic=%d\n", cases[i].label,
                (int)ok, (int)cases[i].valid, (int)(memcmp(&cert, &prior, sizeof(cert)) == 0));
            failures++;
        }
        xrtSecureZero(pem, strlen(pem)); xrtFree(pem);
        xrtSecureZero(&ec, sizeof(ec)); xrtSecureZero(&previous, sizeof(previous));
        xacmeCertKeyUnit(&cert); xrtSecureZero(&prior, sizeof(prior));
        xrtClearError();
    }
    printf("[key-import] %zu EC format cases through two readers; failures=%zu\n",
        sizeof(cases) / sizeof(cases[0]), failures);
    testRequire(failures == 0, "EC private key format and atomicity boundaries failed");
}

#endif
