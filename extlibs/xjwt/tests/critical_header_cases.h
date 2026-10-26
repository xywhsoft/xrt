#ifndef XJWT_TEST_CRITICAL_HEADER_CASES_H
#define XJWT_TEST_CRITICAL_HEADER_CASES_H

static void jwt_test_critical_headers(void)
{
    static const struct { const char* fields; bool valid; } cases[] = {
        { "", true },
        { ",\"private-extension\":true", true },
        { ",\"b64\":true", true },
        { ",\"b64\":true,\"crit\":[\"b64\"]", true },
        { ",\"crit\":[\"private-extension\"],\"private-extension\":true", false },
        { ",\"crit\":[\"missing\"]", false },
        { ",\"crit\":[]", false },
        { ",\"crit\":null", false },
        { ",\"crit\":\"b64\"", false },
        { ",\"crit\":{}", false },
        { ",\"crit\":[1]", false },
        { ",\"crit\":[\"alg\"]", false },
        { ",\"b64\":true,\"crit\":[\"b64\",\"b64\"]", false },
        { ",\"b64\":true,\"crit\":[\"b64\",1]", false },
        { ",\"b64\":true,\"crit\":[\"b64\\u0000ignored\"]", false },
        { ",\"crit\":[\"b64\"]", false },
        { ",\"b64\":false", false },
        { ",\"b64\":false,\"crit\":[\"b64\"]", false },
        { ",\"b64\":\"true\"", false },
        { ",\"b64\":1", false },
        { ",\"b64\":null", false }
    };
    static const int algorithms[] = {XJWT_ALG_HS256, XJWT_ALG_RS256, XJWT_ALG_ES256};
    xjwtjwks* jwks = xjwtJwksParse(K_JWKS);
    CHECK(jwks != NULL, "critical header JWKS fixture parses");
    for (size_t family = 0; family < sizeof(algorithms) / sizeof(algorithms[0]); ++family) {
        int alg = algorithms[family];
        const char* algorithm_name = xjwtAlgName(alg);
        const char* private_key = family == 0 ? "critical-test-key" : family == 1 ? K_RSA_PRIV : K_EC_PRIV;
        const char* public_key = family == 0 ? private_key : family == 1 ? K_RSA_PUB : K_EC_PUB;
        const char* kid = family == 2 ? "ec-key-1" : "rsa-key-1";
        xjwtkey* cached = family ? xjwtKeyParse(public_key) : NULL;
        if (!algorithm_name) {
            CHECK(false, "critical header fixture has a supported algorithm name");
            xjwtKeyFree(cached);
            continue;
        }
        if (family) CHECK(cached != NULL, "critical header cached key fixture parses");
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
            char header[512], label[128];
            char* token;
            xvalue* value;
            int parsed_alg = XJWT_ALG_INVALID;
            snprintf(header, sizeof(header), "{\"alg\":\"%s\",\"kid\":\"%s\"%s}",
                algorithm_name, kid, cases[i].fields);
            token = jwt_test_sign_json(header, "{\"sub\":\"critical-test\"}", alg, private_key);
            CHECK(token != NULL, "critical header fixture has a real signature");
            if (!token) continue;
            value = xjwtDecodeHeader(token, &parsed_alg, NULL);
            CHECK(value && parsed_alg == alg, "raw header remains inspectable without verification");
            xrtValueRelease(value);
            xrtClearError();
            value = xjwtVerify(token, public_key, NULL);
            snprintf(label, sizeof(label), "%s critical header case %zu via PEM/HMAC", algorithm_name, i);
            CHECK((value != NULL) == cases[i].valid &&
                (cases[i].valid || xjwtLastError() == XJWT_ERROR_MALFORMED), label);
            xrtValueRelease(value);
            if (family && cached && jwks) {
                xrtClearError();
                value = xjwtVerifyKey(token, cached, NULL);
                snprintf(label, sizeof(label), "%s critical header case %zu via cached key", algorithm_name, i);
                CHECK((value != NULL) == cases[i].valid &&
                    (cases[i].valid || xjwtLastError() == XJWT_ERROR_MALFORMED), label);
                xrtValueRelease(value);
                xrtClearError();
                value = xjwtVerifyJwks(token, jwks, NULL);
                snprintf(label, sizeof(label), "%s critical header case %zu via JWKS", algorithm_name, i);
                CHECK((value != NULL) == cases[i].valid &&
                    (cases[i].valid || xjwtLastError() == XJWT_ERROR_MALFORMED), label);
                xrtValueRelease(value);
            }
            xrtFree(token);
        }
        xjwtKeyFree(cached);
    }
    xjwtJwksFree(jwks);
    xrtClearError();
}

#endif
