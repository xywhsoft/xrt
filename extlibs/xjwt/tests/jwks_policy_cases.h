#ifndef XJWT_TEST_JWKS_POLICY_CASES_H
#define XJWT_TEST_JWKS_POLICY_CASES_H

static void jwt_test_jwks_policy(void)
{
    static const struct { const char* field; const char* value; bool usable; } cases[] = {
        { "use", NULL, true },
        { "use", "\"sig\"", true },
        { "use", "\"enc\"", false },
        { "use", "\"SIG\"", false },
        { "use", "null", false },
        { "use", "1", false },
        { "use", "\"sig\\u0000other\"", false },
        { "key_ops", "[\"verify\"]", true },
        { "key_ops", "[\"sign\",\"verify\"]", true },
        { "key_ops", "[\"verify\",\"sign\"]", true },
        { "key_ops", "[]", false },
        { "key_ops", "[\"sign\"]", false },
        { "key_ops", "[\"encrypt\"]", false },
        { "key_ops", "[\"verify\",\"encrypt\"]", false },
        { "key_ops", "[\"verify\",\"verify\"]", false },
        { "key_ops", "[\"verify\",1]", false },
        { "key_ops", "[\"verify\\u0000other\"]", false },
        { "key_ops", "\"verify\"", false },
        { "key_ops", "null", false },
        { "alg", "\"unknown\"", false },
        { "alg", "\"none\"", false },
        { "alg", "\"HS256\"", false },
        { "alg", "null", false },
        { "alg", "1", false }
    };
    xvalue* source = xrtJsonParse(xrtStrView(K_JWKS));
    xvalue* keys = source ? xrtValueObjectGet(source, xrtStrView("keys")) : NULL;
    CHECK(keys != NULL, "JWKS metadata source parses");
    if (!keys) { xrtValueRelease(source); return; }
    xrtClearError();
    CHECK(!xjwtJwksParse(NULL) && xjwtLastError() == XJWT_ERROR_ARGUMENT,
        "null JWKS reports its argument error");
    for (size_t family = 0; family < 2u; ++family) {
        int alg = family ? XJWT_ALG_ES256 : XJWT_ALG_RS256;
        char header[128];
        char* token;
        snprintf(header, sizeof(header), "{\"alg\":\"%s\",\"kid\":\"%s\"}",
            xjwtAlgName(alg), family ? "ec-key-1" : "rsa-key-1");
        token = jwt_test_sign_json(header, "{}", alg, family ? K_EC_PRIV : K_RSA_PRIV);
        CHECK(token != NULL, "JWKS metadata fixture has a real signature");
        if (!token) continue;
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]) + 3u; ++i) {
            xvalue* jwk = xrtValueClone(xrtValueArrayGet(keys, family));
            xvalue* set = xrtValueObject();
            xvalue* list = xrtValueArray();
            bool usable = i < sizeof(cases) / sizeof(cases[0]) ? cases[i].usable : true;
            bool matches = usable;
            char label[128];
            char* json;
            xjwtjwks* jwks;
            xvalue* out;
            CHECK(jwk && set && list, "JWKS metadata fixture DOM allocates");
            if (i < sizeof(cases) / sizeof(cases[0])) {
                if (cases[i].value) {
                    CHECK(xrtValueObjectSetNew(jwk, xrtStrView(cases[i].field),
                        xrtJsonParse(xrtStrView(cases[i].value))), "JWKS metadata fixture field set");
                } else CHECK(xrtValueObjectRemove(jwk, xrtStrView(cases[i].field)), "optional JWK use omitted");
            } else {
                size_t extra = i - sizeof(cases) / sizeof(cases[0]);
                const char* binding = extra == 0 ? xjwtAlgName(alg) :
                    extra == 1 ? (family ? "RS256" : "ES256") : (family ? "ES256" : "RS384");
                CHECK(xrtValueObjectSetNew(jwk, xrtStrView("alg"), xrtValueString(xrtStrView(binding))),
                    "JWK algorithm binding set");
                usable = extra != 1;
                matches = usable && (extra != 2 || family);
            }
            CHECK(xrtValueArrayAppend(list, jwk) && xrtValueObjectSet(set, xrtStrView("keys"), list),
                "JWKS metadata fixture set assembled");
            json = xrtJsonStringify(set, false, NULL);
            jwks = json ? xjwtJwksParse(json) : NULL;
            snprintf(label, sizeof(label), "%s JWK metadata case %zu availability", xjwtAlgName(alg), i);
            CHECK(jwks && jwks->nKeys == (usable ? 1 : 0), label);
            xrtClearError();
            out = jwks ? xjwtVerifyJwks(token, jwks, NULL) : NULL;
            snprintf(label, sizeof(label), "%s JWK metadata case %zu verification", xjwtAlgName(alg), i);
            CHECK((out != NULL) == matches && (matches || xjwtLastError() ==
                (usable ? XJWT_ERROR_ALG_MISMATCH : XJWT_ERROR_KEY_NOT_FOUND)), label);
            xrtValueRelease(out);
            if (i == sizeof(cases) / sizeof(cases[0]) + 2u && !family && jwks) {
                char* bound = jwt_test_sign_json("{\"alg\":\"RS384\",\"kid\":\"rsa-key-1\"}",
                    "{}", XJWT_ALG_RS384, K_RSA_PRIV);
                out = bound ? xjwtVerifyJwks(bound, jwks, NULL) : NULL;
                CHECK(out != NULL, "RS384-bound JWK accepts a matching signed token");
                xrtValueRelease(out);
                xrtFree(bound);
            }
            xjwtJwksFree(jwks);
            xrtFree(json);
            xrtValueRelease(list);
            xrtValueRelease(set);
            xrtValueRelease(jwk);
            xrtClearError();
        }
        xrtFree(token);
    }
    xrtValueRelease(source);
}

#endif
