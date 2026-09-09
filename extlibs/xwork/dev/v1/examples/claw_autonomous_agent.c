#include "../xwork.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define XWORK_EXAMPLE_CLAW_COMMAND "cmd /c echo xwork-claw-example"
#else
#define XWORK_EXAMPLE_CLAW_COMMAND "printf %s xwork-claw-example"
#endif

static int xwork_example_check(xwork_status iStatus, const char *sStep)
{
    if ( iStatus == XWORK_OK ) {
        return 0;
    }
    fprintf(stderr, "%s failed: %d\n", sStep, (int)iStatus);
    return 1;
}

int main(void)
{
    xwork_profile tProfile;
    xwork_file_persistence tStore;
    xwork_file_persistence_options tStoreOptions;
    xwork_persistence_backend tPersistence;
    xwork_local_host tHost;
    xwork_local_host_options tHostOptions;
    xwork_host_services tHostServices;
    xwork_runtime_options tRuntimeOptions;
    xwork_workspace_options tWorkspaceOptions;
    xwork_run_options tRunOptions;
    xwork_tool_result tProcessResult;
    xwork_command_artifact_options tCommandArtifactOptions;
    xwork_report_artifact_options tReportArtifactOptions;
    xwork_run_snapshot tSnapshot;
    xwork_run_summary tRecoveredSummary;
    xwork_runtime *pRuntime = NULL;
    xwork_workspace *pWorkspace = NULL;
    xwork_run *pRun = NULL;
    xwork_run *pRecoveredRun = NULL;
    const char *asWorkspaceIds[1];
    int iExit = 1;

    const char *sReportJson =
        "{\"schema\":\"" XWORK_REPORT_SCHEMA_V1 "\","
        "\"report_kind\":\"final\",\"status\":\"completed\","
        "\"subject_ref\":\"process://" XWORK_TOOL_PROCESS_EXEC "\","
        "\"title\":\"claw autonomous command\","
        "\"summary\":\"Executed a local process through the xclaw profile.\","
        "\"body_markdown\":\"# Result\\n\\n- process.exec completed.\\n"
        "- run was persisted and recovered.\\n\","
        "\"items\":[{\"title\":\"execute process\",\"status\":\"completed\"},"
        "{\"title\":\"persist run\",\"status\":\"completed\"}]}";

    xwork_profile_init(&tProfile);
    xwork_file_persistence_init(&tStore);
    xwork_file_persistence_options_init(&tStoreOptions);
    xwork_persistence_backend_init(&tPersistence);
    xwork_local_host_init(&tHost);
    xwork_local_host_options_init(&tHostOptions);
    xwork_host_services_init(&tHostServices);
    xwork_runtime_options_init(&tRuntimeOptions);
    xwork_workspace_options_init(&tWorkspaceOptions);
    xwork_run_options_init(&tRunOptions);
    xwork_tool_result_init(&tProcessResult);
    xwork_run_snapshot_init(&tSnapshot);
    xwork_run_summary_init(&tRecoveredSummary);

    if ( xwork_example_check(
             xwork_profile_get_builtin(XWORK_PROFILE_XCLAW, &tProfile),
             "load xclaw profile"
         ) ) goto cleanup;

    tStoreOptions.sRootPath = "examples/.xwork_claw_store";
    if ( xwork_example_check(
             xwork_file_persistence_configure_backend(
                 &tStore,
                 &tStoreOptions,
                 &tPersistence
             ),
             "configure file persistence"
         ) ) goto cleanup;

    tHostOptions.sDefaultWorkingDirectory = ".";
    tHostOptions.bDenyDestructiveCommands = true;
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
    tRuntimeOptions.pPersistenceBackend = &tPersistence;
    if ( xwork_example_check(
             xwork_runtime_create(&tRuntimeOptions, &pRuntime),
             "create runtime"
         ) ) goto cleanup;

    if ( xwork_example_check(
             xwork_runtime_register_builtin_tool(pRuntime, XWORK_TOOL_PROCESS_EXEC),
             "register process tool"
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
    tRunOptions.sRunId = "example-claw-autonomous";
    tRunOptions.sInstruction = "Run a local process and persist the result.";
    tRunOptions.psWorkspaceIds = asWorkspaceIds;
    tRunOptions.iWorkspaceCount = 1u;
    if ( xwork_example_check(
             xwork_profile_apply_run_options(&tProfile, &tRunOptions),
             "apply run profile"
         ) ) goto cleanup;
    if ( xwork_example_check(
             xwork_run_create(pRuntime, &tRunOptions, &pRun),
             "create run"
         ) ) goto cleanup;
    if ( xwork_example_check(xwork_run_start(pRun), "start run") ) goto cleanup;

    if ( xwork_example_check(
             xwork_runtime_invoke_host_service(
                 pRuntime,
                 XWORK_HOST_PROCESS,
                 XWORK_HOST_PROCESS_EXEC,
                 "{\"command\":\"" XWORK_EXAMPLE_CLAW_COMMAND "\","
                 "\"include_events\":true}",
                 &tProcessResult
             ),
             "invoke process.exec"
         ) ) goto cleanup;

    xwork_command_artifact_options_init(&tCommandArtifactOptions);
    tCommandArtifactOptions.sName = "process.exec.json";
    tCommandArtifactOptions.sMimeType = "application/json";
    tCommandArtifactOptions.sStorageRef = "process://exec";
    tCommandArtifactOptions.sSummary = "Autonomous process execution.";
    tCommandArtifactOptions.sCommandText = XWORK_EXAMPLE_CLAW_COMMAND;
    tCommandArtifactOptions.sOutputText = tProcessResult.sOutputText;
    tCommandArtifactOptions.bHasExitCode = true;
    tCommandArtifactOptions.iExitCode = 0;
    if ( xwork_example_check(
             xwork_run_emit_command_artifact(pRun, &tCommandArtifactOptions, NULL),
             "emit command artifact"
         ) ) goto cleanup;

    xwork_report_artifact_options_init(&tReportArtifactOptions);
    tReportArtifactOptions.sName = "claw-final-report.json";
    tReportArtifactOptions.sMimeType = "application/json";
    tReportArtifactOptions.sStorageRef = "report://claw/final";
    tReportArtifactOptions.sSummary = "claw final report.";
    tReportArtifactOptions.eOutputClass = XWORK_ARTIFACT_OUTPUT_JSON;
    tReportArtifactOptions.sOutputRole = "report.final";
    tReportArtifactOptions.eReportClass = XWORK_ARTIFACT_REPORT_FINAL;
    tReportArtifactOptions.sReportSubjectRef = "process://" XWORK_TOOL_PROCESS_EXEC;
    tReportArtifactOptions.sReportText = sReportJson;
    if ( xwork_example_check(
             xwork_run_emit_report_artifact(pRun, &tReportArtifactOptions, NULL),
             "emit report artifact"
         ) ) goto cleanup;

    if ( xwork_example_check(xwork_run_complete(pRun), "complete run") ) goto cleanup;
    if ( xwork_example_check(
             xwork_run_get_snapshot(pRun, &tSnapshot),
             "capture run snapshot"
         ) ) goto cleanup;
    if ( !tPersistence.pfnStoreRunSnapshot ) {
        fprintf(stderr, "file persistence did not expose snapshot storage\n");
        goto cleanup;
    }
    if ( xwork_example_check(
             tPersistence.pfnStoreRunSnapshot(&tSnapshot, tPersistence.pUserData),
             "store run snapshot"
         ) ) goto cleanup;
    xwork_run_destroy(pRun);
    pRun = NULL;

    if ( xwork_example_check(
             xwork_runtime_recover_run_from_persistence(
                 pRuntime,
                 "example-claw-autonomous",
                 &pRecoveredRun
             ),
             "recover persisted run"
         ) ) goto cleanup;
    if ( xwork_example_check(
             xwork_run_get_summary(pRecoveredRun, &tRecoveredSummary),
             "load recovered summary"
         ) ) goto cleanup;

    printf(
        "claw example recovered run '%s' with state %d.\n",
        tRecoveredSummary.sRunId,
        (int)tRecoveredSummary.eState
    );
    iExit = 0;

cleanup:
    xwork_run_summary_reset(&tRecoveredSummary);
    xwork_run_snapshot_reset(&tSnapshot);
    if ( pRecoveredRun ) {
        xwork_run_destroy(pRecoveredRun);
    }
    if ( pRun ) {
        xwork_run_destroy(pRun);
    }
    if ( pRuntime ) {
        xwork_runtime_destroy(pRuntime);
    }
    xwork_local_host_reset(&tHost);
    xwork_file_persistence_reset(&tStore);
    return iExit;
}
