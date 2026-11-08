#include "../../examples/tls_auth/material.h"

typedef struct interop_auth_signer {xtlsidentity* Key; bool Corrupt, Memory;} interop_auth_signer;
static xtlsidentity* InteropIdentity;
static unsigned InteropFactory;
static bool InteropCorrupt, InteropMissing, InteropMemory;
static xatomic32 InteropCreated, InteropDropped, InteropSigns, InteropQueries;
static bool interopAuthSupports(ptr data, xtlsversion version, xtlssignature scheme)
{return xrtTlsIdentityCanSign(((interop_auth_signer*)data)->Key, version, scheme);}
static bool interopAuthSign(ptr data, xtlsversion version, xtlssignature scheme,
    xbytesview message, void* output, size_t capacity, size_t* size)
{
    interop_auth_signer* state = data;
    xrtAtomic32FetchAdd(output ? &InteropSigns : &InteropQueries, 1u, XMEMORY_RELAXED);
    if (output && state->Memory) { xrtSetErrorKind(XERR_MEMORY); return false; }
    bool result = xrtTlsIdentitySign(state->Key, version, scheme, message, output, capacity, size);
    if (result && output && state->Corrupt) ((uint8*)output)[*size - 1u] ^= 1u;
    return result;
}
static void interopAuthDrop(ptr data)
{
    interop_auth_signer* state = data; xrtTlsIdentityRelease(state->Key); xrtFree(state);
    xrtAtomic32FetchAdd(&InteropDropped, 1u, XMEMORY_RELEASE);
}
static bool interopAuthConnect(xnetengine* engine, const xnetaddr* address,
    const xredisclientinitconfig* init, const xredisclienttlsconfig* tls, xredisclient** client)
{
    if (InteropMissing) return xredisClientTlsDial(engine, InteropResolver, XRT_STR_LITERAL("localhost"), address->Port, 0, NULL, init, tls, client);
    size_t count = xrtTlsIdentityCertificateCount(InteropIdentity);
    xbytesview certificates[XTLS_CLIENT_AUTH_CERTIFICATES_DEFAULT];
    interopRequire(count && count <= XTLS_CLIENT_AUTH_CERTIFICATES_DEFAULT, "bounded interop identity");
    for (size_t index = 0; index < count; index++)
        interopRequire(xrtTlsIdentityCertificate(InteropIdentity, index, &certificates[index]), "interop identity certificate");
    interop_auth_signer* state = xrtCalloc(1, sizeof(*state)); interopRequire(state != NULL, "interop signer state");
    state->Key = xrtTlsIdentityRetain(InteropIdentity); state->Corrupt = InteropCorrupt; state->Memory = InteropMemory;
    xtlsidentityconfig config = {0}; config.Type = xrtTlsIdentityType(InteropIdentity);
    config.Certificates = certificates; config.CertificateCount = count; config.Supports = interopAuthSupports;
    config.Sign = interopAuthSign; config.Release = interopAuthDrop; config.Context = state;
    xtlsidentity* identity = xrtTlsIdentityCreate(&config);
    if (!identity) { xrtTlsIdentityRelease(state->Key); xrtFree(state); interopRequire(false, "interop forwarding identity"); }
    xrtAtomic32FetchAdd(&InteropCreated, 1u, XMEMORY_RELAXED);
    xtlsclientauthconfigv1 auth; xrtTlsClientAuthConfigInitV1(&auth); auth.Identity = identity;
    xredisclientconfig options; xredisClientConfigInit(&options); options.ReadBytes = 13; options.ReadFrames = 1;
    xredisclienttlsconfig wire = *tls; wire.WriteBytes = 7;
    bool result; unsigned factory = InteropFactory++ % 3u;
    if (!factory) result = xredisClientTlsAuthDialV1(engine, InteropResolver, XRT_STR_LITERAL("localhost"), address->Port, 0, &options, init, &wire, &auth, client);
    else if (factory == 1u) {
        if (!wire.Session.VerifyName.Size) wire.Session.VerifyName = (xstrview)XRT_STR_LITERAL("localhost");
        result = xredisClientTlsAuthConnectV1(engine, address, 0, &options, init, &wire, &auth, client);
    } else {
        if (!wire.Session.VerifyName.Size) wire.Session.VerifyName = (xstrview)XRT_STR_LITERAL("localhost");
        xnetaddrlist* endpoints = xrtNetAddrListCreate(address, 1); interopRequire(endpoints != NULL, "interop prepared endpoint");
        result = xredisClientTlsAuthDialAddressesV1(engine, endpoints, 0, &options, init, &wire, &auth, client); xrtNetAddrListDestroy(endpoints);
    }
    xrtTlsIdentityRelease(identity); memset(&auth, 0xA5, sizeof(auth)); memset(&wire, 0xA5, sizeof(wire));
    return result;
}
