#ifndef XJWT_TEST_CLAIMS_VALIDATION_CASES_H
#define XJWT_TEST_CLAIMS_VALIDATION_CASES_H

static void jwt_test_audience_types(void)
{
    static const struct { const char* audience; bool valid; } cases[] = {
        { "[\"myapp-web\",1]", false },
        { "[1,\"myapp-web\"]", false },
        { "[\"myapp-web\",null]", false },
        { "[\"myapp-web\",{}]", false },
        { "[\"myapp-web\",[\"other\"]]", false },
        { "[true,\"myapp-web\"]", false },
        { "\"myapp-web\"", true },
        { "[\"myapp-web\",\"other\"]", true },
        { "[\"other\",\"myapp-web\"]", true }
    };
    xjwtcheck check;
    xjwtCheckInit(&check);
    check.Audience = "myapp-web";
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        char json[128];
        xvalue* claims;
        xvalue* verified;
        char* token;
        snprintf(json, sizeof(json), "{\"aud\":%s}", cases[i].audience);
        claims = xrtJsonParse(xrtStrView(json));
        CHECK(claims && xjwtClaimsValid(claims, &check) == cases[i].valid,
            "audience validation checks every array element type");
        token = jwt_test_sign_json("{\"alg\":\"HS256\"}", json, XJWT_ALG_HS256, "audience-test");
        CHECK(token != NULL, "audience type fixture has a real signature");
        xrtClearError();
        verified = token ? xjwtVerify(token, "audience-test", &check) : NULL;
        CHECK((verified != NULL) == cases[i].valid &&
            (cases[i].valid || xjwtLastError() == XJWT_ERROR_AUDIENCE),
            "matching audience cannot hide a malformed array member");
        xrtValueRelease(verified);
        xrtValueRelease(claims);
        xrtFree(token);
        xrtClearError();
    }
}

#endif
