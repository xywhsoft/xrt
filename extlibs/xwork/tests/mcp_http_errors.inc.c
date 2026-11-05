/* A bounded HTTP probe; the Python peer changes only its first response. */
static unsigned test_mcp_http_status(const xwork_error* pError)
{
#if XWORK_ABI_VERSION >= 8u
    return pError->uHttpStatus;
#else
    (void)pError;
    return 0u;
#endif
}

static int run_mcp_http_errors(const char* sBase)
{
    static const struct { const char* sId; unsigned uStatus; } Cases[] = {
        { "auth", 401u }, { "payment", 402u }, { "permission", 403u },
        { "limit", 429u }, { "malformed", 503u }, { "missing", 404u },
        { "method", 405u }, { "false-success", 401u }, { "wrong-id", 429u },
        { "notification", 401u }, { "listing", 503u }, { "expired", 404u }
    };
    xwork_runtime_config Config;
    xwork_runtime* Runtime;
    xwork_error Error;
    size_t i;
    setvbuf(stdout, NULL, _IONBF, 0);
    g_iFailures = 0;
    xworkRuntimeConfigInit(&Config);
    Runtime = xworkRuntimeCreate(&Config, &Error);
    if (!Runtime) return 1;
    for (i = 0u; i < sizeof(Cases) / sizeof(Cases[0]); ++i) {
        char Endpoint[512];
        xwork_mcp_server_config Server;
        xwork_mcp_server_info Info;
        bool Connected;
        snprintf(Endpoint, sizeof(Endpoint), "%s/%s", sBase, Cases[i].sId);
        xworkMcpServerConfigInit(&Server);
        Server.sServerId = Cases[i].sId;
        Server.sSummary = "Loopback HTTP failure and recovery probe.";
        Server.eTransport = XWORK_MCP_TRANSPORT_STREAMABLE_HTTP;
        Server.sEndpoint = Endpoint;
        Server.sProtocolVersion = "2025-11-25";
        Server.uStartupTimeoutMs = 1000u;
        Server.uRequestTimeoutMs = 1000u;
        CHECK(xworkRuntimeRegisterMcpServer(Runtime, &Server, &Error),
            "HTTP failure fixture registration succeeds");
        Connected = xworkRuntimeRefreshMcpServer(Runtime, Cases[i].sId,
            NULL, XLLM_DEADLINE_NEVER, &Error);
        printf("http_error=%s status:%u code:%d connected:%d\n", Cases[i].sId,
            test_mcp_http_status(&Error), (int)Error.eCode, (int)Connected);
        CHECK(!Connected && Error.eCode == XWORK_ERROR_IO &&
            test_mcp_http_status(&Error) == Cases[i].uStatus &&
            Error.tModelError.iHttpStatus == 0 &&
            strstr(Error.sMessage, "qa-private-marker") == NULL,
            "failed HTTP response retains status without remote text or model metadata");
        xworkMcpServerInfoInit(&Info);
        CHECK(xworkRuntimeMcpServerGetInfo(Runtime, Cases[i].sId, &Info) &&
            !Info.bToolsDiscovered,
            "an HTTP error never publishes successful discovery");
        Connected = xworkRuntimeRefreshMcpServer(Runtime, Cases[i].sId, NULL,
            XLLM_DEADLINE_NEVER, &Error);
        if (!Connected) printf("recovery_error=%s\n", Error.sMessage);
        CHECK(Connected && Error.eCode == XWORK_ERROR_NONE &&
            test_mcp_http_status(&Error) == 0u,
            "explicit retry clears stale HTTP error and discovers tools");
    }
    {
        static const char* Cases[] = { "modern-auth", "modern-wrong-id", "modern-both" };
        for (i = 0u; i < sizeof(Cases) / sizeof(Cases[0]); ++i) {
            char Endpoint[512];
            xwork_mcp_server_config Server;
            xwork_mcp_client* Client;
            xwork_tool_output Output;
            snprintf(Endpoint, sizeof(Endpoint), "%s/%s", sBase, Cases[i]);
            xworkMcpServerConfigInit(&Server);
            Server.sServerId = Cases[i];
            Server.sSummary = "Untrusted routing-rejection signal probe.";
            Server.eTransport = XWORK_MCP_TRANSPORT_STREAMABLE_HTTP;
            Server.sEndpoint = Endpoint;
            Server.uStartupTimeoutMs = Server.uRequestTimeoutMs = 1000u;
            CHECK(xworkRuntimeRegisterMcpServer(Runtime, &Server, &Error) &&
                xworkRuntimeRefreshMcpServer(Runtime, Cases[i], NULL,
                    XLLM_DEADLINE_NEVER, &Error), "modern failure probe initializes");
            Client = xwork__runtime_mcp_find(Runtime, Cases[i]);
            xworkToolOutputInit(&Output);
            CHECK(Client && xworkMcpClientCallTool(Client, "echo", "{}", NULL,
                    XLLM_DEADLINE_NEVER, &Output, &Error) == XWORK_RESULT_OK &&
                !Output.bSuccess && Output.sContent &&
                strstr(Output.sContent, "qa-private-marker") == NULL &&
                Error.eCode == XWORK_ERROR_NONE && test_mcp_http_status(&Error) == 0u,
                "untrusted rejection remains one safe tool failure without replay");
            xworkToolOutputUnit(&Output);
        }
    }
    xworkRuntimeRelease(Runtime);
    printf("xwork MCP HTTP errors: %s (%d failures)\n",
        g_iFailures ? "FAIL" : "PASS", g_iFailures);
    return g_iFailures ? 1 : 0;
}
