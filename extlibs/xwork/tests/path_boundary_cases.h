#ifndef XWORK_TEST_PATH_BOUNDARY_CASES_H
#define XWORK_TEST_PATH_BOUNDARY_CASES_H

typedef struct path_alias_model {
    unsigned calls;
    bool denied;
} path_alias_model;

static xllm_result path_alias_complete(void* data, const xllm_request* request,
    const xllm_stream_callbacks* callbacks, xllm_response** response, xllm_error* error)
{
    path_alias_model* state = (path_alias_model*)data;
    (void)callbacks;
    (void)error;
    if (++state->calls == 1u) {
        *response = mock_response("", 1u);
        if (!*response || !mock_set_call(*response, 0u, "alias_read", "read",
            "{\"path\":\"control-alias/secret.txt\"}")) {
            xllmResponseDestroy(*response);
            *response = NULL;
            return XLLM_RESULT_ERROR;
        }
    } else {
        state->denied = request_has_text(request, "denied by approval policy") &&
            !request_has_text(request, "private-control-sentinel");
        *response = mock_response("path policy checked", 0u);
    }
    return *response ? XLLM_RESULT_OK : XLLM_RESULT_ERROR;
}

static void test_path_boundaries(void)
{
    static const char root[] = "tests/tmp_xwork_path_boundary";
    static const char outside[] = "tests/tmp_xwork_path_boundary_outside";
    static const char alias[] = "tests/tmp_xwork_path_boundary_alias";
    xllm_session_config session_config;
    xwork_agent_config config;
    xllm_session* session;
    xwork_agent* agent;
    xwork_error error;
    xwork_tool_output output;
    xwork_tool_context context = {0};
    const xwork_tool_entry* read_tool;
    const xwork_tool_entry* write_tool;
    char* inside_real;
    char* outside_real;
    char* resolved;
    bool links;
    path_alias_model model = {0};
    (void)xrtDirRemoveAll(root);
    (void)xrtDirRemoveAll(outside);
    CHECK(xrtDirCreateAll("tests/tmp_xwork_path_boundary/inside") && xrtDirCreateAll(outside),
        "path boundary directories created");
    CHECK(xrtFileWriteAll("tests/tmp_xwork_path_boundary/inside/safe.txt",
        (xbytesview){(const uint8*)"inside-sentinel", 15u}) &&
        xrtFileWriteAll("tests/tmp_xwork_path_boundary_outside/secret.txt",
        (xbytesview){(const uint8*)"outside-sentinel", 16u}), "path boundary files created");
    inside_real = xrtPathReal("tests/tmp_xwork_path_boundary/inside");
    outside_real = xrtPathReal(outside);
    xllmSessionConfigInit(&session_config);
    session = xllmSessionCreate(&session_config, NULL);
    xworkAgentConfigInit(&config);
    config.pSession = session;
    config.sWorkspaceRoot = root;
    config.OnModelComplete = path_alias_complete;
    config.pModelUserData = &model;
    agent = xworkAgentCreate(&config, &error);
    CHECK(agent && inside_real && outside_real, "path boundary agent created");
    if (!agent || !inside_real || !outside_real) goto done;
    read_tool = xwork__find_tool(agent, "read");
    write_tool = xwork__find_tool(agent, "write");
    context.pAgent = agent;
    context.sWorkspaceRoot = xworkAgentWorkspaceRoot(agent);
    resolved = xwork__resolve_path(agent, "../tmp_xwork_path_boundary_outside/secret.txt", &error);
    CHECK(!resolved && error.eCode == XWORK_ERROR_POLICY, "sibling prefix is outside the workspace");
    free(resolved);
    resolved = xwork__resolve_path(agent, "inside/new/deep.txt", &error);
    CHECK(resolved != NULL, "missing descendants of an internal directory remain usable");
    free(resolved);

    links = xrtLinkCreate(outside_real, "tests/tmp_xwork_path_boundary/escape", true);
#if defined(_WIN32)
    if (!links) printf("  SKIP path symlinks: Windows did not grant link creation\n");
#else
    CHECK(links, "POSIX symlink fixture created");
#endif
    if (links) {
        {
            char* target = xrtPathJoin(outside_real, "secret.txt");
            CHECK(target && xrtLinkCreate(target, "tests/tmp_xwork_path_boundary/file-alias", false),
                "external file symlink created");
            xrtFree(target);
            xworkToolOutputInit(&output);
            CHECK(read_tool && read_tool->OnExecute(read_tool->pUserData, &context,
                "{\"path\":\"file-alias\"}", &output, &error) == XWORK_RESULT_OK &&
                !output.bSuccess && error.eCode == XWORK_ERROR_POLICY,
                "read rejects a symlink leaf pointing outside the workspace");
            xworkToolOutputUnit(&output);
            target = xrtPathJoin(outside_real, "missing.txt");
            CHECK(target && xrtLinkCreate(target, "tests/tmp_xwork_path_boundary/dangling", false),
                "dangling symlink fixture created");
            xrtFree(target);
            resolved = xwork__resolve_path(agent, "dangling", &error);
            CHECK(!resolved, "dangling symlink is not treated as a new internal file");
            free(resolved);
        }
        xworkToolOutputInit(&output);
        CHECK(read_tool && read_tool->OnExecute(read_tool->pUserData, &context,
            "{\"path\":\"escape/secret.txt\"}", &output, &error) == XWORK_RESULT_OK &&
            !output.bSuccess && error.eCode == XWORK_ERROR_POLICY,
            "read rejects a directory symlink escaping the workspace");
        xworkToolOutputUnit(&output);
        xworkToolOutputInit(&output);
        CHECK(write_tool && write_tool->OnExecute(write_tool->pUserData, &context,
            "{\"path\":\"escape/new/deep.txt\",\"content\":\"blocked\"}", &output, &error) == XWORK_RESULT_OK &&
            !output.bSuccess && error.eCode == XWORK_ERROR_POLICY &&
            !xrtFileExists("tests/tmp_xwork_path_boundary_outside/new/deep.txt"),
            "write rejects missing descendants through an external symlink");
        xworkToolOutputUnit(&output);
        CHECK(xrtLinkCreate(inside_real, "tests/tmp_xwork_path_boundary/inside-alias", true),
            "internal directory symlink created");
        resolved = xwork__resolve_path(agent, "inside-alias/new/deep.txt", &error);
        CHECK(resolved && strstr(resolved, "inside-alias") == NULL,
            "internal symlink resolves to its physical target for new paths");
        free(resolved);
        xworkToolOutputInit(&output);
        CHECK(read_tool && read_tool->OnExecute(read_tool->pUserData, &context,
            "{\"path\":\"inside-alias/safe.txt\"}", &output, &error) == XWORK_RESULT_OK &&
            output.bSuccess && output.sContent && strstr(output.sContent, "inside-sentinel"),
            "internal symlink reads remain supported");
        xworkToolOutputUnit(&output);
        {
            char* control;
            xwork_readonly_subagent_config child_config;
            xwork_run_result child_result = {0};
            CHECK(xrtDirCreateAll("tests/tmp_xwork_path_boundary/.xcode") &&
                xrtFileWriteAll("tests/tmp_xwork_path_boundary/.xcode/secret.txt",
                    (xbytesview){(const uint8*)"private-control-sentinel", 24u}),
                "protected control fixture created");
            control = xrtPathReal("tests/tmp_xwork_path_boundary/.xcode");
            CHECK(control && xrtLinkCreate(control, "tests/tmp_xwork_path_boundary/control-alias", true),
                "protected directory alias created");
            xrtFree(control);
            xworkReadOnlySubagentConfigInit(&child_config);
            CHECK(xworkAgentRunReadOnlySubagent(agent, &child_config, "Check the path policy",
                &child_result, &error) == XWORK_RESULT_OK && model.calls == 2u && model.denied,
                "read-only child denies physical aliases into control directories");
            xworkRunResultUnit(&child_result);
        }
        {
            char* root_real = xrtPathReal(root);
            CHECK(root_real && xrtLinkCreate(root_real, alias, true), "workspace root alias created");
            xrtFree(root_real);
            config.sWorkspaceRoot = alias;
            {
                xwork_agent* aliased = xworkAgentCreate(&config, &error);
                resolved = aliased ? xwork__resolve_path(aliased, "inside/safe.txt", &error) : NULL;
                CHECK(resolved && strstr(resolved, "boundary_alias") == NULL,
                    "workspace symlink roots use the physical containment boundary");
                free(resolved);
                xworkAgentDestroy(aliased);
            }
        }
    }
#if !defined(_WIN32)
    config.sWorkspaceRoot = "/";
    {
        xwork_agent* filesystem = xworkAgentCreate(&config, &error);
        resolved = filesystem ? xwork__resolve_path(filesystem, "/tmp", &error) : NULL;
        CHECK(resolved != NULL, "filesystem root contains its descendants");
        free(resolved);
        xworkAgentDestroy(filesystem);
    }
#endif
done:
    xworkAgentDestroy(agent);
    xllmSessionDestroy(session);
    xrtFree(inside_real);
    xrtFree(outside_real);
    (void)xrtLinkDelete(alias);
    (void)xrtLinkDelete("tests/tmp_xwork_path_boundary/escape");
    (void)xrtLinkDelete("tests/tmp_xwork_path_boundary/inside-alias");
    (void)xrtLinkDelete("tests/tmp_xwork_path_boundary/control-alias");
    (void)xrtLinkDelete("tests/tmp_xwork_path_boundary/file-alias");
    (void)xrtLinkDelete("tests/tmp_xwork_path_boundary/dangling");
    (void)xrtDirRemoveAll(root);
    (void)xrtDirRemoveAll(outside);
}

#endif
