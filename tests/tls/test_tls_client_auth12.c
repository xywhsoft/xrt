#define TEST_TLS_AUTH_EMBEDDED
#include "test_tls_client_auth.c"
#undef TEST_TLS_AUTH_EMBEDDED
#define main testTlsClientAuth12LegacyRegression
#include "test_tls_server12.c"
#undef main

/* The native peer does not request certificates. Insert the request and verify
 * the actual client proof, then give its existing state machine Certificate /
 * ClientKeyExchange / CertificateVerify in the correct transcript order. Both
 * real Finished values and both application epochs are still verified. */
static void testAuth12Flight(unsigned mode, size_t prefix, bool fault)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    auth_signer signer = {0}; signer.Mode = mode == 4u ? 2u : mode == 5u ? 3u : 0u;
    xtlsidentity* identity = testAuthIdentity(&signer), *server_identity = testTlsServerIdentity();
    xtlscontext* context = testTlsServer12Context(); xtlsverifier* verifier = testTlsClientVerifier();
    xtlsclientconfig config; xrtTlsClientConfigInit(&config); config.Context = context;
    config.Verifier = verifier; config.ServerName = XRT_STR_LITERAL("example.com");
    xtlsclientauthconfigv1 auth; xrtTlsClientAuthConfigInitV1(&auth); auth.Identity = identity;
    xtlsserverconfig sc; xrtTlsServerConfigInit(&sc); sc.Context = context; sc.Identity = server_identity;
    xtlssession* client = xrtTlsClientCreateAuthV1(&config, &auth, NULL), *server = xrtTlsServerCreate(&sc, NULL);
    testRequire(client && server, "TLS 1.2 authenticated fixture");
    xrtTlsIdentityRelease(identity); xrtTlsIdentityRelease(server_identity);
    xrtTlsVerifierRelease(verifier); xrtTlsContextRelease(context);
    xtlsclientstate* state = __xrtTlsSessionRoleData(client);
    xtlsserverstate* peer = __xrtTlsSessionRoleData(server);
    uint8 raw[16384], wire[8192], request[32], forward[4096], digest_before[32], digest_after[32];
    size_t raw_size = 0; bool triggered = false, hit = false, inserted = false;
    xtlstranscript transcript; memset(&transcript, 0, sizeof(transcript));
    xnetspan span; xtlsrecord record; xtlshandshake message;
    uint8 body[] = {1u, 1u, 0, 4u, 8u, 4u, 4u, 1u, 0, 0};
    testRequire(xrtTlsHandshakeEncode(XTLS_HANDSHAKE_CERTIFICATE_REQUEST,
        (xbytesview){body, sizeof(body)}, request, sizeof(request)) &&
        xrtTlsSessionSendFront(client, &span) && xrtTlsRecordParse((xbytesview){span.Data, span.Size}, &record, NULL) == XTLS_OK,
        "TLS 1.2 auth initial request/offer framing");
    memcpy(raw, record.Payload.Data, record.Payload.Size); raw_size = record.Payload.Size;
    testRequire(xrtTlsSessionFeed(server, span.Data, span.Size) == XTLS_OK && xrtTlsSessionSendConsume(client, span.Size) &&
        xrtTlsServerDrive(server) == XTLS_OK, "TLS 1.2 auth server flight");
    while (xrtTlsSessionSendSize(server)) {
        testRequire(xrtTlsSessionSendFront(server, &span) && span.Size <= sizeof(wire), "TLS 1.2 auth server wire bound");
        size_t size = span.Size; memcpy(wire, span.Data, size); testRequire(xrtTlsSessionSendConsume(server, size), "TLS 1.2 auth server consume");
        for (size_t offset = 0; offset < size;) {
            testRequire(xrtTlsRecordParse((xbytesview){wire + offset, size - offset}, &record, NULL) == XTLS_OK,
                "TLS 1.2 auth server record parse"); offset += record.EncodedSize;
            for (size_t item = 0; item < record.Payload.Size;) {
                testRequire(xrtTlsHandshakeParse((xbytesview){record.Payload.Data + item, record.Payload.Size - item}, &message, NULL) == XTLS_OK,
                    "TLS 1.2 auth server message parse");
                xbytesview encoded = {record.Payload.Data + item, message.EncodedSize}; item += encoded.Size;
                if (message.Type == XTLS_HANDSHAKE_SERVER_HELLO_DONE) {
                    size_t request_size = xrtTlsHandshakeSize(sizeof(body));
                    testRequire(testTlsClient12RequestFeed(client, (xbytesview){request, request_size}) == XTLS_OK && state->Auth->Selected,
                        "TLS 1.2 auth compatible request");
                    memcpy(raw + raw_size, request, request_size); raw_size += request_size; inserted = true;
                    testRequire(!signer.Signs && !signer.Queries && state->Auth->TranscriptSize == raw_size &&
                        !memcmp(raw, state->Auth->Transcript, raw_size), "TLS 1.2 raw signing tape matches actual messages");
                    size_t fill = 0;
                    if (mode == 1u) {
                        size_t exchange = xrtTlsHandshakeSize(xrtTls12ClientKeyExchangeSize((xbytesview){state->PublicKey, state->PublicKeySize}));
                        size_t payload = state->Auth->Certificate12 + exchange + 8u + state->Auth->SignatureCapacity;
                        const xtlscipherinfo* cipher = xrtTlsCipherInfo(state->Cipher);
                        size_t flight = payload + XTLS_RECORD_HEADER_SIZE + xrtTlsRecordSize(1u) + XTLS_RECORD_HEADER_SIZE +
                            cipher->ExplicitNonceSize + 4u + XTLS12_FINISHED_SIZE + cipher->TagSize;
                        fill = xrtTlsContextLimits(client->Context)->SendLimit - flight + 1u;
                        bytes data = xrtMalloc(fill); testRequire(data != NULL, "TLS 1.2 auth filler allocation"); memset(data, 0xA5, fill);
                        testRequire(__xrtTlsSessionSendTake(client, data, fill) == XTLS_OK, "TLS 1.2 auth filler transfer");
                    }
                    if (mode == 3u) state->Auth->Config.MaxTranscriptBytes = raw_size + encoded.Size;
                    size_t forward_size = xrtTlsRecordSize(encoded.Size);
                    testRequire(xrtTlsRecordEncode(XTLS_RECORD_HANDSHAKE, XTLS_VERSION_12, encoded, forward, sizeof(forward)) &&
                        xrtTlsSessionFeed(client, forward, forward_size) == XTLS_OK &&
                        __xrtTlsTranscriptDigest(&state->Transcript, digest_before, sizeof(digest_before)), "TLS 1.2 auth trigger preparation");
                    if (fault) testRequire(xrtMemDebugFailAfter(prefix), "TLS 1.2 auth OOM arm");
                    xtlsresult result = xrtTlsClientDrive(client); triggered = true;
                    hit = fault && xrtMemDebugFailTriggered(); if (fault) xrtMemDebugFailClear();
                    if (mode == 1u) {
                        testRequire(result == XTLS_AGAIN && !signer.Signs && !signer.Queries && !client->WriteKey.Ready &&
                            __xrtTlsTranscriptDigest(&state->Transcript, digest_after, sizeof(digest_after)) &&
                            !memcmp(digest_before, digest_after, sizeof(digest_before)) && xrtTlsSessionSendConsume(client, fill),
                            "TLS 1.2 auth backpressure does not sign or commit");
                        result = xrtTlsClientDrive(client);
                    }
                    if (hit || mode >= 3u) {
                        testRequire(result == XTLS_ERROR && xrtTlsSessionState(client) == XTLS_STATE_FAILED &&
                            !client->WriteKey.Ready && !state->PendingReadKey.Ready && !xrtTlsClientCertificateQueuedV1(client) &&
                            __xrtTlsTranscriptDigest(&state->Transcript, digest_after, sizeof(digest_after)) &&
                            !memcmp(digest_before, digest_after, sizeof(digest_before)) && signer.Signs <= 1u,
                            "TLS 1.2 auth failed flight never commits either epoch or transcript");
                        if (mode == 3u) testRequire(!signer.Signs && !signer.Queries && xrtErrorIs(xrtGetError(), XERR_RANGE), "TLS 1.2 raw tape cap before signing");
                        if (mode == 4u || mode == 5u) testRequire(xrtErrorIs(xrtGetError(), XERR_CANCELLED) != NULL, "TLS 1.2 checked signer cause");
                        unsigned calls = signer.Signs; testRequire(xrtTlsClientDrive(client) == XTLS_ERROR && calls == signer.Signs,
                            "TLS 1.2 auth terminal failure never signs again"); goto cleanup;
                    }
                    testRequire(result == XTLS_OK && signer.Signs == 1u && signer.Queries == 1u && xrtTlsClientCertificateQueuedV1(client) &&
                        !state->Auth->Transcript && !state->Auth->TranscriptSize && !state->Auth->TranscriptCapacity,
                        "TLS 1.2 queues one proof and clears raw tape");
                } else testRequire(testTlsClient12RequestFeed(client, encoded) == XTLS_OK, "TLS 1.2 auth earlier server message");
                testRequire(encoded.Size <= sizeof(raw) - raw_size, "TLS 1.2 auth test raw bound");
                memcpy(raw + raw_size, encoded.Data, encoded.Size); raw_size += encoded.Size;
            }
        }
    }
    testRequire(inserted && __xrtTlsTranscriptInit(&transcript, XCRYPTO_HASH_SHA256) &&
        __xrtTlsTranscriptUpdate(&transcript, (xbytesview){raw, raw_size}), "TLS 1.2 auth peer transcript");
    __xrtTlsTranscriptClear(&peer->Transcript); peer->Transcript = transcript; memset(&transcript, 0, sizeof(transcript));
    bool proof = false;
    while (xrtTlsSessionSendSize(client)) {
        testRequire(xrtTlsSessionSendFront(client, &span) && span.Size <= sizeof(wire), "TLS 1.2 auth client wire bound");
        size_t size = span.Size; memcpy(wire, span.Data, size); testRequire(xrtTlsSessionSendConsume(client, size), "TLS 1.2 auth client consume");
        for (size_t offset = 0; offset < size;) {
            testRequire(xrtTlsRecordParse((xbytesview){wire + offset, size - offset}, &record, NULL) == XTLS_OK,
                "TLS 1.2 auth client record parse");
            xbytesview forwarded = {wire + offset, record.EncodedSize}; offset += record.EncodedSize;
            if (!proof) {
                size_t item = 0;
                testRequire(record.Type == XTLS_RECORD_HANDSHAKE && xrtTlsHandshakeParse(record.Payload, &message, NULL) == XTLS_OK &&
                    message.Type == XTLS_HANDSHAKE_CERTIFICATE, "TLS 1.2 auth certificate first");
                xtlscertificatemessage certificate; xtlscertificatecursor cursor; xtlscertificateentry entry;
                testRequire(xrtTlsCertificateParse(XTLS_VERSION_12, message.Body, &certificate) && xrtTlsCertificateEntries(&certificate, &cursor) &&
                    xrtTlsCertificatesRead(&cursor, &entry) == XTLS_ITEM_VALUE && entry.Data.Size == sizeof(X509_LEGACY_RSA_CERT) &&
                    !memcmp(entry.Data.Data, X509_LEGACY_RSA_CERT, entry.Data.Size) && xrtTlsCertificatesRead(&cursor, &entry) == XTLS_ITEM_DONE &&
                    __xrtTlsTranscriptUpdate(&peer->Transcript, (xbytesview){record.Payload.Data, message.EncodedSize}), "TLS 1.2 exact client chain");
                item += message.EncodedSize;
                memcpy(raw + raw_size, record.Payload.Data, message.EncodedSize); raw_size += message.EncodedSize;
                testRequire(xrtTlsHandshakeParse((xbytesview){record.Payload.Data + item, record.Payload.Size - item}, &message, NULL) == XTLS_OK &&
                    message.Type == XTLS_HANDSHAKE_CLIENT_KEY_EXCHANGE && xrtTlsRecordEncode(XTLS_RECORD_HANDSHAKE, XTLS_VERSION_12,
                        (xbytesview){record.Payload.Data + item, message.EncodedSize}, forward, sizeof(forward)) &&
                    xrtTlsSessionFeed(server, forward, xrtTlsRecordSize(message.EncodedSize)) == XTLS_OK && xrtTlsServerDrive(server) == XTLS_OK,
                    "TLS 1.2 peer derives real EMS before CertificateVerify");
                memcpy(raw + raw_size, record.Payload.Data + item, message.EncodedSize); raw_size += message.EncodedSize; item += message.EncodedSize;
                testRequire(xrtTlsHandshakeParse((xbytesview){record.Payload.Data + item, record.Payload.Size - item}, &message, NULL) == XTLS_OK &&
                    message.Type == XTLS_HANDSHAKE_CERTIFICATE_VERIFY && item + message.EncodedSize == record.Payload.Size,
                    "TLS 1.2 exact proof after key exchange");
                xtlscertificateverify verify; xx509pubkey key; xx509signature scheme = {0};
                scheme.Type = X509_SIGNATURE_RSA_PSS; scheme.Hash = scheme.MaskHash = X509_HASH_SHA256; scheme.SaltSize = 32u; scheme.Trailer = 1u;
                testRequire(xrtTlsCertificateVerifyParse(message.Body, &verify) && verify.Scheme == XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256 &&
                    xrtTlsIdentityPublicKey(state->Auth->Identity, &key) && xrtX509SignatureVerify(&scheme, (xbytesview){raw, raw_size}, verify.Signature, &key) &&
                    __xrtTlsTranscriptUpdate(&peer->Transcript, (xbytesview){record.Payload.Data + item, message.EncodedSize}), "TLS 1.2 public verifier accepts actual full-message client proof");
                proof = true; continue;
            }
            testRequire(xrtTlsSessionFeed(server, forwarded.Data, forwarded.Size) == XTLS_OK && xrtTlsServerDrive(server) == XTLS_OK,
                "TLS 1.2 peer verifies real client Finished including proof");
        }
    }
    test_tls_server_rng rng = {UINT32_C(0xAF1213)};
    testRequire(proof && testTlsServerHandshake(client, server, &rng) &&
        testTlsServerTransfer(client, server, true, "client auth", 12u, &rng) &&
        testTlsServerTransfer(server, client, false, "server auth", 12u, &rng), "TLS 1.2 mutual Finished and application epochs");
cleanup:
    testRequire(triggered && (!fault || prefix || hit), "TLS 1.2 OOM reaches flight preparation");
    xrtClearError(); __xrtTlsTranscriptClear(&transcript); xrtTlsSessionDestroy(server); xrtTlsSessionDestroy(client);
    testRequire(signer.Drops == 1u && !signer.Key, "TLS 1.2 client signer released once"); testAuthBalanced(before);
    xrtSecureZero(raw, sizeof(raw));
}

int main(void)
{
    testRequire(xrtMemDebugEnable(true), "TLS 1.2 auth memory instrumentation");
    testAuth12Flight(0u, 0u, false); testAuth12Flight(1u, 0u, false);
    testAuth12Flight(3u, 0u, false); testAuth12Flight(4u, 0u, false); testAuth12Flight(5u, 0u, false);
    for (size_t point = 0; point < 160u; point++) testAuth12Flight(2u, point, true);
    puts("TLS 1.2 client auth: actual proof/EMS/Finished/data, no duplicate signing, raw limit, 160 OOM prefixes balanced");
    return 0;
}
