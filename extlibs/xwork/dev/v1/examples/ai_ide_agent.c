#include "../lib/xllm.h"
#include "../xwork.h"

#include <stdio.h>
#include <string.h>

#define XWORK_EXAMPLE_MOCK_ADAPTER "example_ai_ide_mock"
#define XWORK_EXAMPLE_MOCK_PROFILE "example-ai-ide-mock"

typedef struct {
    size_t iTurnCount;
} xwork_example_mock_ctx;

static int xwork_example_check(xwork_status iStatus, const char *sStep)
{
    if ( iStatus == XWORK_OK ) {
        return 0;
    }
    fprintf(stderr, "%s failed: %d\n", sStep, (int)iStatus);
    return 1;
}

static char *xwork_example_dup_cstr(const char *sText)
{
    return sText ? (char *)xrtCopyStr((str)(const void *)sText, 0u) : NULL;
}

static xllm_response *xwork_example_build_tool_call_response(
    const char *sProfileId,
    const char *sResponseId,
    const char *sToolId,
    const char *sArgumentsJson
)
{
    xllm_response *pResponse = (xllm_response *)xrtCalloc(1u, sizeof(*pResponse));
    if ( !pResponse ) {
        return NULL;
    }

    pResponse->sId = xwork_example_dup_cstr(sResponseId);
    pResponse->sProvider = xwork_example_dup_cstr("example");
    pResponse->sProfileId = xwork_example_dup_cstr(sProfileId);
    pResponse->sModel = xwork_example_dup_cstr("example-ai-ide-mock-model");
    pResponse->eStatus = XLLM_STATUS_COMPLETED;
    pResponse->sFinishReason = xwork_example_dup_cstr("tool_calls");
    pResponse->pOutputs = (xllm_output_item *)xrtCalloc(1u, sizeof(xllm_output_item));
    if ( !pResponse->sId || !pResponse->sProvider || !pResponse->sProfileId ||
         !pResponse->sModel || !pResponse->sFinishReason || !pResponse->pOutputs ) {
        xllm_response_free(pResponse);
        return NULL;
    }

    pResponse->iOutputCount = 1u;
    pResponse->pOutputs[0].eKind = XLLM_OUTPUT_TOOL_CALL;
    pResponse->pOutputs[0].as.tToolCall.sCallId = xwork_example_dup_cstr("example-tool-call-1");
    pResponse->pOutputs[0].as.tToolCall.sToolId = xwork_example_dup_cstr(sToolId);
    pResponse->pOutputs[0].as.tToolCall.sToolName = xwork_example_dup_cstr(sToolId);
    pResponse->pOutputs[0].as.tToolCall.sArgumentsJson = xwork_example_dup_cstr(sArgumentsJson);
    if ( !pResponse->pOutputs[0].as.tToolCall.sCallId ||
         !pResponse->pOutputs[0].as.tToolCall.sToolId ||
         !pResponse->pOutputs[0].as.tToolCall.sToolName ||
         !pResponse->pOutputs[0].as.tToolCall.sArgumentsJson ) {
        xllm_response_free(pResponse);
        return NULL;
    }
    return pResponse;
}

static xllm_response *xwork_example_build_final_response(
    const char *sProfileId,
    const char *sText
)
{
    xllm_response *pResponse = (xllm_response *)xrtCalloc(1u, sizeof(*pResponse));
    xllm_content_part *pPart;
    if ( !pResponse ) {
        return NULL;
    }

    pResponse->sId = xwork_example_dup_cstr("example-final-response");
    pResponse->sProvider = xwork_example_dup_cstr("example");
    pResponse->sProfileId = xwork_example_dup_cstr(sProfileId);
    pResponse->sModel = xwork_example_dup_cstr("example-ai-ide-mock-model");
    pResponse->eStatus = XLLM_STATUS_COMPLETED;
    pResponse->sFinishReason = xwork_example_dup_cstr("stop");
    pResponse->sVisibleText = xwork_example_dup_cstr(sText);
    pResponse->pOutputs = (xllm_output_item *)xrtCalloc(1u, sizeof(xllm_output_item));
    pPart = (xllm_content_part *)xrtCalloc(1u, sizeof(*pPart));
    if ( !pResponse->sId || !pResponse->sProvider || !pResponse->sProfileId ||
         !pResponse->sModel || !pResponse->sFinishReason ||
         !pResponse->sVisibleText || !pResponse->pOutputs || !pPart ) {
        if ( pPart ) {
            xrtFree(pPart);
        }
        xllm_response_free(pResponse);
        return NULL;
    }

    pPart->eKind = XLLM_PART_TEXT;
    pPart->as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
    pPart->as.tSource.sMimeType = xwork_example_dup_cstr("text/plain");
    pPart->as.tSource.as.sText = xwork_example_dup_cstr(sText);
    if ( !pPart->as.tSource.sMimeType || !pPart->as.tSource.as.sText ) {
        xrtFree(pPart);
        xllm_response_free(pResponse);
        return NULL;
    }

    pResponse->iOutputCount = 1u;
    pResponse->pOutputs[0].eKind = XLLM_OUTPUT_MESSAGE;
    pResponse->pOutputs[0].as.tMessage.pParts = pPart;
    pResponse->pOutputs[0].as.tMessage.iPartCount = 1u;
    return pResponse;
}

static int xwork_example_mock_chat(
    void *pCtx,
    const xllm_profile *pProfile,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse,
    xllm_error *pError
)
{
    xwork_example_mock_ctx *pMockCtx = (xwork_example_mock_ctx *)pCtx;
    const char *sProfileId = pProfile && pProfile->sId ? pProfile->sId : XWORK_EXAMPLE_MOCK_PROFILE;
    (void)pOptions;
    (void)pError;

    if ( !pRequest || !ppResponse ) {
        return XRT_NET_ERROR;
    }
    *ppResponse = NULL;
    if ( pMockCtx && pMockCtx->iTurnCount++ == 0u ) {
        *ppResponse = xwork_example_build_tool_call_response(
            sProfileId,
            "example-approval-request-response",
            XWORK_TOOL_FILESYSTEM_APPLY_PATCH,
            "{\"path\":\"README.md\",\"old_text\":\"# xwork\\r\\n\","
            "\"new_text\":\"# xwork\\r\\n\\r\\n"
            "AI IDE agents can emit dry-run patch artifacts before user approval.\\r\\n\","
            "\"dry_run\":true}"
        );
    } else {
        *ppResponse = xwork_example_build_final_response(
            sProfileId,
            "AI IDE approval pause/resume completed."
        );
    }
    return *ppResponse ? XRT_NET_OK : XRT_NET_ERROR;
}

int main(void)
{
    xllm_runtime_options tLlmRuntimeOptions;
    xllm_runtime *pLlmRuntime = NULL;
    xllm_adapter tAdapter;
    xwork_example_mock_ctx tMockCtx;
    xllm_profile tLlmProfile;
    xwork_profile tProfile;
    xwork_local_host tHost;
    xwork_local_host_options tHostOptions;
    xwork_host_services tHostServices;
    xwork_runtime_options tRuntimeOptions;
    xwork_workspace_options tWorkspaceOptions;
    xwork_run_options tRunOptions;
    xwork_orchestrator_options tExecOptions;
    xwork_tool_result tHostResult;
    xwork_approval_request tApproval;
    xwork_output_artifact_options tReadArtifactOptions;
    xwork_patch_artifact_options tPatchArtifactOptions;
    xwork_report_artifact_options tReportArtifactOptions;
    xwork_runtime *pRuntime = NULL;
    xwork_workspace *pWorkspace = NULL;
    xwork_run *pRun = NULL;
    const char *asWorkspaceIds[1];
    int iExit = 1;

    const char *sPatchText =
        "--- README.md\n"
        "+++ README.md\n"
        "@@ -1,3 +1,4 @@\n"
        " # xwork\n"
        "+\n"
        "+AI IDE agents can emit dry-run patch artifacts before user approval.\n";
    const char *sApplyResultJson =
        "{\"schema\":\"" XWORK_PATCH_APPLY_RESULT_SCHEMA_V1 "\","
        "\"tool\":\"" XWORK_TOOL_FILESYSTEM_APPLY_PATCH "\","
        "\"path\":\"README.md\",\"ok\":true,\"dry_run\":true,"
        "\"changed\":true,\"bytes_before\":0,\"bytes_after\":0,"
        "\"error_kind\":\"\",\"error\":\"\"}";
    const char *sFileSummaryJson =
        "{\"schema\":\"" XWORK_PATCH_FILE_SUMMARY_SCHEMA_V1 "\","
        "\"files\":[{\"path\":\"README.md\",\"change_kind\":\"modify\","
        "\"hunks\":1,\"added_lines\":2,\"deleted_lines\":0}]}";
    const char *sReportJson =
        "{\"schema\":\"" XWORK_REPORT_SCHEMA_V1 "\","
        "\"report_kind\":\"final\",\"status\":\"completed\","
        "\"subject_ref\":\"workspace://README.md\","
        "\"title\":\"AI IDE dry-run edit\","
        "\"summary\":\"Read README.md and prepared a dry-run patch artifact.\","
        "\"body_markdown\":\"# Result\\n\\n- README.md was inspected.\\n"
        "- A dry-run patch artifact was emitted for UI approval.\\n\","
        "\"items\":[{\"title\":\"read README.md\",\"status\":\"completed\"},"
        "{\"title\":\"prepare patch\",\"status\":\"completed\"}]}";

    xllm_runtime_options_init(&tLlmRuntimeOptions);
    memset(&tMockCtx, 0, sizeof(tMockCtx));
    xllm_profile_init(&tLlmProfile);
    xwork_profile_init(&tProfile);
    xwork_local_host_init(&tHost);
    xwork_local_host_options_init(&tHostOptions);
    xwork_host_services_init(&tHostServices);
    xwork_runtime_options_init(&tRuntimeOptions);
    xwork_workspace_options_init(&tWorkspaceOptions);
    xwork_run_options_init(&tRunOptions);
    xwork_orchestrator_options_init(&tExecOptions);
    xwork_tool_result_init(&tHostResult);
    xwork_approval_request_init(&tApproval);
    xwork_patch_artifact_options_init(&tPatchArtifactOptions);

    if ( xwork_example_check(
             xwork_profile_get_builtin(XWORK_PROFILE_XCODE, &tProfile),
             "load xcode profile"
         ) ) goto cleanup;
    if ( xllm_runtime_create(&tLlmRuntimeOptions, &pLlmRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "create mock xllm runtime failed\n");
        goto cleanup;
    }

    memset(&tAdapter, 0, sizeof(tAdapter));
    tAdapter.sName = XWORK_EXAMPLE_MOCK_ADAPTER;
    tAdapter.pfnChat = xwork_example_mock_chat;
    tAdapter.pCtx = &tMockCtx;
    if ( xllm_register_adapter(pLlmRuntime, &tAdapter) != XRT_NET_OK ) {
        fprintf(stderr, "register mock xllm adapter failed\n");
        goto cleanup;
    }

    tLlmProfile.sId = XWORK_EXAMPLE_MOCK_PROFILE;
    tLlmProfile.sName = "AI IDE Mock";
    tLlmProfile.sProvider = "example";
    tLlmProfile.sAdapter = XWORK_EXAMPLE_MOCK_ADAPTER;
    tLlmProfile.tModels.tText.sModelId = "example-ai-ide-mock-model";
    if ( xllm_register_profile(pLlmRuntime, &tLlmProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register mock xllm profile failed\n");
        goto cleanup;
    }

    tHostOptions.sDefaultWorkingDirectory = ".";
    tHostOptions.bEnforceFilesystemRoot = true;
    if ( xwork_example_check(
             xwork_local_host_configure_services(
                 &tHost,
                 &tHostOptions,
                 &tHostServices
             ),
             "configure local host"
         ) ) goto cleanup;

    if ( xwork_example_check(
             xwork_profile_apply_runtime_options(&tProfile, &tRuntimeOptions),
             "apply runtime profile"
         ) ) goto cleanup;
    tRuntimeOptions.pHostServices = &tHostServices;
    tRuntimeOptions.pLlmRuntime = pLlmRuntime;
    if ( xwork_example_check(
             xwork_runtime_create(&tRuntimeOptions, &pRuntime),
             "create runtime"
         ) ) goto cleanup;

    if ( xwork_example_check(
             xwork_runtime_register_builtin_tool(
                 pRuntime,
                 XWORK_TOOL_FILESYSTEM_READ_TEXT
             ),
             "register read tool"
         ) ) goto cleanup;
    if ( xwork_example_check(
             xwork_runtime_register_builtin_tool(
                 pRuntime,
                 XWORK_TOOL_FILESYSTEM_APPLY_PATCH
             ),
             "register patch tool"
         ) ) goto cleanup;

    tWorkspaceOptions.sWorkspaceId = "workspace";
    tWorkspaceOptions.sRootPath = ".";
    if ( xwork_example_check(
             xwork_profile_apply_workspace_options(&tProfile, &tWorkspaceOptions),
             "apply workspace profile"
         ) ) goto cleanup;
    if ( xwork_example_check(
             xwork_runtime_add_workspace(
                 pRuntime,
                 &tWorkspaceOptions,
                 &pWorkspace
             ),
             "add workspace"
         ) ) goto cleanup;

    asWorkspaceIds[0] = "workspace";
    tRunOptions.sRunId = "example-ai-ide-agent";
    tRunOptions.sInstruction = "Inspect README.md and prepare an approved UI patch.";
    tRunOptions.psWorkspaceIds = asWorkspaceIds;
    tRunOptions.iWorkspaceCount = 1u;
    if ( xwork_example_check(
             xwork_profile_apply_run_options(&tProfile, &tRunOptions),
             "apply run profile"
         ) ) goto cleanup;
    tRunOptions.sLlmProfileId = XWORK_EXAMPLE_MOCK_PROFILE;
    if ( xwork_example_check(
             xwork_run_create(pRuntime, &tRunOptions, &pRun),
             "create run"
         ) ) goto cleanup;
    if ( xwork_example_check(xwork_run_start(pRun), "start run") ) goto cleanup;

    if ( xwork_example_check(
             xwork_runtime_invoke_host_service(
                 pRuntime,
                 XWORK_HOST_FILESYSTEM,
                 XWORK_HOST_FILESYSTEM_READ_TEXT,
                 "{\"path\":\"README.md\",\"max_bytes\":4096}",
                 &tHostResult
             ),
             "read README.md"
         ) ) goto cleanup;

    xwork_output_artifact_options_init(&tReadArtifactOptions);
    tReadArtifactOptions.sName = "README.md";
    tReadArtifactOptions.sMimeType = "application/json";
    tReadArtifactOptions.sStorageRef = "workspace://README.md";
    tReadArtifactOptions.sSummary = "README.md preview from local host.";
    tReadArtifactOptions.eOutputClass = XWORK_ARTIFACT_OUTPUT_FILE_CONTENT;
    tReadArtifactOptions.sOutputRole = XWORK_TOOL_FILESYSTEM_READ_TEXT;
    tReadArtifactOptions.sOutputText = tHostResult.sOutputText;
    if ( xwork_example_check(
             xwork_run_emit_output_artifact(pRun, &tReadArtifactOptions, NULL),
             "emit read artifact"
         ) ) goto cleanup;

    /*
     * In a real AI IDE this dry-run patch would be shown in the UI and only
     * applied after product-level approval.
     */
    tPatchArtifactOptions.sName = "README.patch";
    tPatchArtifactOptions.sTargetRef = "workspace://README.md";
    tPatchArtifactOptions.sSummary = "Dry-run README.md edit proposal.";
    tPatchArtifactOptions.sPatchText = sPatchText;
    tPatchArtifactOptions.sApplyResultJson = sApplyResultJson;
    tPatchArtifactOptions.sFileSummaryJson = sFileSummaryJson;
    if ( xwork_example_check(
             xwork_run_emit_patch_artifact(pRun, &tPatchArtifactOptions, NULL),
             "emit patch artifact"
         ) ) goto cleanup;

    xwork_report_artifact_options_init(&tReportArtifactOptions);
    tReportArtifactOptions.sName = "ai-ide-final-report.json";
    tReportArtifactOptions.sMimeType = "application/json";
    tReportArtifactOptions.sStorageRef = "report://ai-ide/final";
    tReportArtifactOptions.sSummary = "AI IDE final report.";
    tReportArtifactOptions.eOutputClass = XWORK_ARTIFACT_OUTPUT_JSON;
    tReportArtifactOptions.sOutputRole = "report.final";
    tReportArtifactOptions.eReportClass = XWORK_ARTIFACT_REPORT_FINAL;
    tReportArtifactOptions.sReportSubjectRef = "workspace://README.md";
    tReportArtifactOptions.sReportText = sReportJson;
    if ( xwork_example_check(
             xwork_run_emit_report_artifact(pRun, &tReportArtifactOptions, NULL),
             "emit report artifact"
         ) ) goto cleanup;

    tExecOptions.iMaxTurns = 3u;
    tExecOptions.bAutoApprove = false;
    if ( xwork_example_check(
             xwork_run_execute(pRun, &tExecOptions),
             "execute until approval pause"
         ) ) goto cleanup;
    if ( xwork_run_get_state(pRun) != XWORK_RUN_WAITING_APPROVAL ) {
        fprintf(stderr, "run did not pause for approval\n");
        goto cleanup;
    }
    if ( xwork_example_check(
             xwork_run_get_last_approval_request(pRun, &tApproval),
             "load approval request"
         ) ) goto cleanup;
    if ( strcmp(tApproval.sToolId, XWORK_TOOL_FILESYSTEM_APPLY_PATCH) != 0 ||
         tApproval.eState != XWORK_APPROVAL_PENDING ) {
        fprintf(stderr, "unexpected approval request\n");
        goto cleanup;
    }
    if ( xwork_example_check(
             xwork_run_submit_approval(pRun, XWORK_APPROVAL_APPROVED),
             "approve patch request"
         ) ) goto cleanup;
    if ( xwork_example_check(xwork_run_resume(pRun), "resume approved run") ) goto cleanup;
    if ( xwork_example_check(
             xwork_run_execute(pRun, &tExecOptions),
             "execute after approval"
         ) ) goto cleanup;

    printf(
        "AI IDE example completed with approval pause/resume and %zu artifacts.\n",
        xwork_run_get_artifact_count(pRun)
    );
    iExit = 0;

cleanup:
    xwork_approval_request_reset(&tApproval);
    if ( pRun ) {
        xwork_run_destroy(pRun);
    }
    if ( pRuntime ) {
        xwork_runtime_destroy(pRuntime);
    }
    if ( pLlmRuntime ) {
        xllm_runtime_destroy(pLlmRuntime);
    }
    xwork_local_host_reset(&tHost);
    return iExit;
}
