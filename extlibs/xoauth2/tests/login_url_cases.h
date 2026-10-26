#ifndef XOAUTH2_TEST_LOGIN_URL_CASES_H
#define XOAUTH2_TEST_LOGIN_URL_CASES_H

static void oauth2_test_login_url_boundaries(void)
{
    static const struct { const char* endpoint; const char* prefix; } queries[] = {
        { "https://idp.example/auth", "https://idp.example/auth?response_type=code&" },
        { "https://idp.example/auth?", "https://idp.example/auth?response_type=code&" },
        { "https://idp.example/auth?audience=api%3Aread", "https://idp.example/auth?audience=api%3Aread&response_type=code&" },
        { "https://idp.example/auth?audience=api%3Aread&", "https://idp.example/auth?audience=api%3Aread&response_type=code&" },
        { "https://idp.example/auth?x=1&hint=a%40b", "https://idp.example/auth?x=1&hint=a%40b&response_type=code&" },
        { "https://idp.example/auth/%23path", "https://idp.example/auth/%23path?response_type=code&" }
    };
    static const char* invalid[] = {
        "https://idp.example/auth#fragment", "https://idp.example/auth?x=1#fragment",
        "https://idp.example/auth#", "", NULL
    };
    xoauth2config config;
    xoauth2client client = {0};
    xoauth2ConfigInit(&config);
    config.AuthorizeUrl = "https://idp.example/auth";
    config.ClientId = "client";
    config.RedirectUri = "https://app.example/callback";
    config.Scope = "read write";
    config.UseNonce = true;
    for (size_t i = 0; i < sizeof(queries) / sizeof(queries[0]); ++i) {
        char* url;
        config.AuthorizeUrl = queries[i].endpoint;
        xoauth2UseCustom(&client, &config);
        url = xoauth2BeginLogin(&client);
        CHECK(url && strncmp(url, queries[i].prefix, strlen(queries[i].prefix)) == 0 &&
            strstr(url, "&client_id=client&") && strstr(url, "&code_challenge=") && strstr(url, "&nonce="),
            "authorization URL preserves its existing query and appends parameters");
        xrtFree(url);
    }
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]) + 2u; ++i) {
        char* url;
        config.AuthorizeUrl = "https://idp.example/auth";
        xoauth2UseCustom(&client, &config);
        url = xoauth2BeginLogin(&client);
        CHECK(url && client.sState[0] && client.sVerifier[0] && client.sChallenge[0] && client.sNonce[0],
            "failed-login replacement fixture starts with a real transaction");
        xrtFree(url);
        if (i < sizeof(invalid) / sizeof(invalid[0])) client.Config.AuthorizeUrl = invalid[i];
        else client.Config.ClientId = i == sizeof(invalid) / sizeof(invalid[0]) ? NULL : "";
        xrtClearError();
        url = xoauth2BeginLogin(&client);
        CHECK(!url && xoauth2LastError() == XOAUTH2_ERROR_ARGUMENT && !client.sState[0] &&
            !client.sVerifier[0] && !client.sChallenge[0] && !client.sNonce[0],
            "invalid authorization configuration clears every prior transaction secret");
        xrtFree(url);
    }
    xoauth2UseWechat(&client, "wx-client", "fixture", config.RedirectUri);
    client.Config.AuthorizeUrl = "https://idp.example/auth?tenant=one";
    client.Config.UsePkce = true;
    client.Config.UseNonce = true;
    {
        const char* prefix = "https://idp.example/auth?tenant=one&appid=wx-client&";
        const char* suffix = "#wechat_redirect";
        char* url = xoauth2BeginLogin(&client);
        const char* fragment = url ? strchr(url, '#') : NULL;
        const char* challenge = url ? strstr(url, "&code_challenge=") : NULL;
        const char* nonce = url ? strstr(url, "&nonce=") : NULL;
        CHECK(url && strncmp(url, prefix, strlen(prefix)) == 0,
            "WeChat authorization URL also preserves endpoint query");
        CHECK(url && strlen(url) >= strlen(suffix) &&
            strcmp(url + strlen(url) - strlen(suffix), suffix) == 0 &&
            fragment && challenge && nonce && challenge < fragment && nonce < fragment,
            "provider fragment follows every authorization query parameter");
        xrtFree(url);
    }
    xoauth2ClientUnit(&client);
    xrtClearError();
}

#endif
