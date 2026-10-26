#ifndef XOAUTH2_TEST_CONFIGURATION_ALIAS_CASES_H
#define XOAUTH2_TEST_CONFIGURATION_ALIAS_CASES_H

static void oauth2_test_configuration_aliases(void)
{
    static const size_t fields[] = {
        offsetof(xoauth2config, AuthorizeUrl), offsetof(xoauth2config, TokenUrl),
        offsetof(xoauth2config, UserInfoUrl), offsetof(xoauth2config, JwksUrl),
        offsetof(xoauth2config, Issuer), offsetof(xoauth2config, ClientId),
        offsetof(xoauth2config, ClientSecret), offsetof(xoauth2config, RedirectUri),
        offsetof(xoauth2config, Scope)
    };
    xoauth2client client = {0};
    xoauth2config config;
    xallocationbudget* budget = NULL;
    xallocationscope scope = {0};
    xallocationbudgetinfo info = {0};
    char* url;
    size_t allocations;
    bool entered;
    CHECK(xrtAllocationBudgetCreate(SIZE_MAX, &budget), "configuration alias allocation tracker starts");
    if (!budget) return;
    entered = xrtAllocationBudgetEnter(budget, &scope);
    CHECK(entered, "configuration alias allocation scope starts");
    if (!entered) { xrtAllocationBudgetFree(budget); return; }
    info.Size = sizeof(info);
    xoauth2ConfigInit(&config);
    config.AuthorizeUrl = "https://idp.example/auth";
    config.TokenUrl = "https://idp.example/token";
    config.ClientId = "client";
    config.RedirectUri = "https://app.example/callback";
    config.Scope = "openid";
    config.UseNonce = true;
    xoauth2UseCustom(&client, &config);
    url = xoauth2BeginLogin(&client);
    CHECK(url && client.sState[0] && client.sVerifier[0] && client.sNonce[0],
        "configuration alias fixture starts with an active transaction");
    xrtFree(url);
    xoauth2UseCustom(&client, &client.Config);
    CHECK(client.Config.AuthorizeUrl == config.AuthorizeUrl &&
        client.Config.ClientId == config.ClientId && client.Config.UseNonce &&
        client.Config.UsePkce && client.Config.Scope == config.Scope,
        "custom configuration accepts the client's own Config address");
    CHECK(!client.sState[0] && !client.sVerifier[0] && !client.sChallenge[0] && !client.sNonce[0],
        "reconfiguration clears the previous transaction even with aliased input");
    url = xoauth2BeginLogin(&client);
    CHECK(url && strstr(url, "&client_id=client&") && strstr(url, "&nonce="),
        "a self-aliased custom configuration can start another login");
    xrtFree(url);
    xoauth2ClientUnit(&client);
    xrtClearError();

    xrtAllocationBudgetInfo(budget, &info);
    allocations = info.LiveCount;
    xoauth2UseMicrosoft(&client, "client", "fixture", config.RedirectUri, "tenant-x");
    config = client.Config;
    config.Scope = "openid custom";
    xoauth2UseCustom(&client, &config);
    url = xoauth2BeginLogin(&client);
    CHECK(url && strstr(url, "https://login.microsoftonline.com/tenant-x/oauth2/v2.0/authorize?") == url &&
        strstr(url, "scope=openid%20custom"),
        "customized Microsoft configuration retains live provider endpoints");
    CHECK(client.Config.TokenUrl && strcmp(client.Config.TokenUrl,
        "https://login.microsoftonline.com/tenant-x/oauth2/v2.0/token") == 0 &&
        client.Config.Issuer && strcmp(client.Config.Issuer,
        "https://login.microsoftonline.com/tenant-x/v2.0") == 0,
        "customized provider configuration retains token and issuer values");
    xrtFree(url);
    xoauth2ClientUnit(&client);
    xrtAllocationBudgetInfo(budget, &info);
    CHECK(info.LiveCount == allocations, "customized provider endpoints are freed by ClientUnit");

    /* Each string may borrow an interior substring of any old owned endpoint.
     * Keep just that allocation; replacement endpoints must retire immediately. */
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i) {
        char* owners[3];
        const char* alias;
        char expected[256];
        xrtAllocationBudgetInfo(budget, &info);
        allocations = info.LiveCount;
        xoauth2UseMicrosoft(&client, "client", "fixture", "https://app.example/callback", "tenant-x");
        owners[0] = client.pOwnedAuthUrl;
        owners[1] = client.pOwnedTokenUrl;
        owners[2] = client.pOwnedIssuerUrl;
        alias = owners[i % 3u] + 8u;
        snprintf(expected, sizeof(expected), "%s", alias);
        config = client.Config;
        config.AuthorizeUrl = "https://idp.example/auth";
        config.TokenUrl = "https://idp.example/token";
        config.Issuer = "https://idp.example";
        memcpy((char*)&config + fields[i], &alias, sizeof(alias));
        xoauth2UseCustom(&client, &config);
        memcpy(&alias, (const char*)&client.Config + fields[i], sizeof(alias));
        CHECK(alias && strcmp(alias, expected) == 0,
            "custom configuration preserves interior aliases in every string field");
        xrtAllocationBudgetInfo(budget, &info);
        if (info.LiveCount != allocations + 1u)
            printf("configuration field %zu: live allocations %zu -> %zu\n", i, allocations, info.LiveCount);
        CHECK(info.LiveCount == allocations + 1,
            "reconfiguration releases unreferenced owned endpoints immediately");
        xoauth2ClientUnit(&client);
        xrtAllocationBudgetInfo(budget, &info);
        CHECK(info.LiveCount == allocations, "retained interior alias has exactly one owner");
    }
    xrtClearError();
    CHECK(xrtAllocationBudgetLeave(&scope), "configuration alias allocation scope closes");
    xrtAllocationBudgetFree(budget);
}

#endif
