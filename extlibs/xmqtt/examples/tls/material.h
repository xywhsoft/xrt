#ifndef XMQTT_EXAMPLE_TLS_MATERIAL_H
#define XMQTT_EXAMPLE_TLS_MATERIAL_H
#include <xmqtt.h>
#include <stdio.h>
#include <string.h>

/* Example/interop configuration I/O only; no socket or signing runtime here.
 * Every file is bounded and key/DER scratch is wiped on every return path. */
static inline bytes xmqttExampleTlsRead(cstr path, size_t limit, size_t* size)
{
    *size = 0; FILE* file = fopen(path, "rb"); if (!file) return NULL;
    long length = 0; bytes data = NULL;
    if (fseek(file, 0, SEEK_END) || (length = ftell(file)) <= 0 ||
        (size_t)length > limit || fseek(file, 0, SEEK_SET)) { fclose(file); return NULL; }
    data = xrtMalloc((size_t)length);
    if (!data) { fclose(file); return NULL; }
    bool ok = fread(data, 1, (size_t)length, file) == (size_t)length && !ferror(file);
    if (fclose(file)) ok = false;
    if (!ok) { xrtSecureZero(data, (size_t)length); xrtFree(data); return NULL; }
    *size = (size_t)length; return data;
}
static inline bool xmqttExampleTlsLabel(xstrview label, cstr expected)
{ size_t size = strlen(expected); return label.Size == size && !memcmp(label.Data, expected, size); }
static inline xtlsidentity* xmqttExampleTlsIdentity(cstr chain_path, cstr key_path)
{
    xbytesview certificates[XTLS_CLIENT_AUTH_CERTIFICATES_DEFAULT] = {{0}};
    size_t count = 0, chain_size = 0, key_size = 0, der_size = 0, total = 0;
    bytes chain = xmqttExampleTlsRead(chain_path, 128u * 1024u, &chain_size);
    bytes key = xmqttExampleTlsRead(key_path, 64u * 1024u, &key_size), der = NULL;
    xtlsidentity* identity = NULL; xpemcursor cursor; xpemblock block; xpemresult result;
    if (!chain || !key || !xrtPemInit(&cursor, (cstr)chain, chain_size)) goto done;
    while ((result = xrtPemRead(&cursor, &block)) == XPEM_BLOCK) {
        if (count == XTLS_CLIENT_AUTH_CERTIFICATES_DEFAULT || !xmqttExampleTlsLabel(block.Label, "CERTIFICATE")) goto done;
        bytes certificate = xrtPemDecodeNew(&block, &der_size); if (!certificate) goto done;
        certificates[count++] = (xbytesview){certificate, der_size};
        if (der_size > XTLS_CLIENT_AUTH_CERTIFICATE_BYTES_DEFAULT - total) goto done;
        total += der_size;
    }
    if (result != XPEM_DONE || !count || !xrtPemInit(&cursor, (cstr)key, key_size) ||
        xrtPemRead(&cursor, &block) != XPEM_BLOCK ||
        !(xmqttExampleTlsLabel(block.Label, "PRIVATE KEY") || xmqttExampleTlsLabel(block.Label, "RSA PRIVATE KEY") ||
          xmqttExampleTlsLabel(block.Label, "EC PRIVATE KEY"))) goto done;
    der = xrtPemDecodeNew(&block, &der_size);
    if (!der || xrtPemRead(&cursor, &block) != XPEM_DONE) goto done;
    xx509cert leaf; xx509pubkey public_key;
    if (!xrtX509Parse(certificates[0].Data, certificates[0].Size, &leaf) || !xrtX509PublicKey(&leaf, &public_key)) goto done;
#if defined(XRT_FEATURE_TLS_IDENTITY_RSA)
    if (public_key.Type == X509_KEY_RSA || public_key.Type == X509_KEY_RSA_PSS)
        identity = xrtTlsIdentityRsa(certificates, count, (xbytesview){der, der_size});
#endif
#if defined(XRT_FEATURE_TLS_IDENTITY_P256)
    if (public_key.Type == X509_KEY_EC && public_key.Curve == X509_CURVE_P256)
        identity = xrtTlsIdentityP256(certificates, count, (xbytesview){der, der_size});
#endif
#if defined(XRT_FEATURE_TLS_IDENTITY_P384)
    if (public_key.Type == X509_KEY_EC && public_key.Curve == X509_CURVE_P384)
        identity = xrtTlsIdentityP384(certificates, count, (xbytesview){der, der_size});
#endif
#if defined(XRT_FEATURE_TLS_IDENTITY_ED25519)
    if (public_key.Type == X509_KEY_ED25519)
        identity = xrtTlsIdentityEd25519(certificates, count, (xbytesview){der, der_size});
#endif
done:
    if (der) { xrtSecureZero(der, der_size); xrtFree(der); }
    if (key) { xrtSecureZero(key, key_size); xrtFree(key); }
    if (chain) { xrtSecureZero(chain, chain_size); xrtFree(chain); }
    for (size_t index = 0; index < count; index++) { xrtSecureZero((ptr)certificates[index].Data, certificates[index].Size); xrtFree((ptr)certificates[index].Data); }
    return identity;
}
#endif
