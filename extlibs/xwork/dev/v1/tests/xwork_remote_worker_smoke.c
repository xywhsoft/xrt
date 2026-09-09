#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define XRT_IMPLEMENTATION
#include "../lib/xrt.h"
#define XLLM_SESSION_IMPLEMENTATION
#include "../lib/xllm-session.h"
#define XLLM_MEMORY_IMPLEMENTATION
#include "../lib/xllm-memory.h"
#include "../xwork.c"

#ifdef _WIN32
#define XWORK_REMOTE_SMOKE_COMMAND "cmd /c echo xwork-remote-worker"
#define XWORK_REMOTE_SMOKE_TIMEOUT_COMMAND "ping -n 3 127.0.0.1 >nul"
#define XWORK_REMOTE_SMOKE_DESTRUCTIVE_COMMAND "cmd /c echo rm -rf blocked"
#define XWORK_REMOTE_SMOKE_TERMINAL_COMMAND "cmd"
#else
#define XWORK_REMOTE_SMOKE_COMMAND "printf %s xwork-remote-worker"
#define XWORK_REMOTE_SMOKE_TIMEOUT_COMMAND "sleep 1"
#define XWORK_REMOTE_SMOKE_DESTRUCTIVE_COMMAND "printf %s rm -rf blocked"
#define XWORK_REMOTE_SMOKE_TERMINAL_COMMAND "sh"
#endif

int main(void)
{
    xwork_runtime *pRuntime = NULL;
    xwork_runtime_options tRuntimeOptions;
    xwork_host_services tHostServices;
    xwork_local_host_options tHostOptions;
    xwork_local_host tHost;
    xwork_control_plane_options tPlaneOptions;
    xwork_control_plane_options tApprovalPlaneOptions;
    xwork_control_plane_options tTerminalPlaneOptions;
    xwork_worker_options tWorkerOptions;
    xwork_remote_task_options tTaskOptions;
    xwork_remote_task_assignment tAssignment;
    xwork_remote_task_result tTaskResult;
    xwork_artifact_summary tRemoteArtifact;
    xwork_artifact_summary tUploadedArtifact;
    xwork_remote_artifact_upload tArtifactUpload;
    xwork_remote_output_chunk tOutputChunk;
    xwork_remote_blob_chunk_summary_list tBlobList;
    xwork_remote_task_summary tTaskSummary;
    xwork_remote_task_summary_list tTaskList;
    xwork_worker_summary_list tWorkerList;
    xwork_control_plane_snapshot tPlaneSnapshot;
    xwork_control_plane_snapshot tLoadedPlaneSnapshot;
    xwork_file_persistence tStore;
    xwork_file_persistence_options tStoreOptions;
    xwork_persistence_backend tBackend;
    xwork_control_plane *pPlane = NULL;
    xwork_control_plane *pApprovalPlane = NULL;
    xwork_control_plane *pHttpPlane = NULL;
    xwork_control_plane *pTerminalPlane = NULL;
    xwork_control_plane *pRecoveredPlane = NULL;
    xwork_worker *pWorker = NULL;
    xwork_worker *pTerminalWorker = NULL;
    const char *asCapabilities[4];
    const char *asForbiddenCapabilities[1];
    const char *asTerminalCapabilities[4];
    const char *asNetworkDenyHostPatterns[1];
    const char *sUploadData = "remote upload log\n";
    size_t iOrphanedCount = 0u;
    char sStoreRoot[128];

    xwork_runtime_options_init(&tRuntimeOptions);
    xwork_host_services_init(&tHostServices);
    xwork_local_host_options_init(&tHostOptions);
    xwork_local_host_init(&tHost);
    xwork_control_plane_options_init(&tPlaneOptions);
    xwork_control_plane_options_init(&tApprovalPlaneOptions);
    xwork_control_plane_options_init(&tTerminalPlaneOptions);
    xwork_worker_options_init(&tWorkerOptions);
    xwork_remote_task_options_init(&tTaskOptions);
    xwork_remote_task_assignment_init(&tAssignment);
    xwork_remote_task_result_init(&tTaskResult);
    xwork_artifact_summary_init(&tRemoteArtifact);
    xwork_artifact_summary_init(&tUploadedArtifact);
    xwork_remote_artifact_upload_init(&tArtifactUpload);
    xwork_remote_output_chunk_init(&tOutputChunk);
    xwork_remote_blob_chunk_summary_list_init(&tBlobList);
    xwork_remote_task_summary_init(&tTaskSummary);
    xwork_remote_task_summary_list_init(&tTaskList);
    xwork_worker_summary_list_init(&tWorkerList);
    xwork_control_plane_snapshot_init(&tPlaneSnapshot);
    xwork_control_plane_snapshot_init(&tLoadedPlaneSnapshot);
    xwork_file_persistence_init(&tStore);
    xwork_file_persistence_options_init(&tStoreOptions);
    xwork_persistence_backend_init(&tBackend);

    snprintf(
        sStoreRoot,
        sizeof(sStoreRoot),
        "tests/remote_worker_store_%ld_%ld",
        (long)time(NULL),
        (long)clock()
    );
    tStoreOptions.sRootPath = sStoreRoot;
    assert(xwork_file_persistence_configure_backend(
        &tStore,
        &tStoreOptions,
        &tBackend
    ) == XWORK_OK);

    asNetworkDenyHostPatterns[0] = "blocked.example";
    tRuntimeOptions.tPolicy.psNetworkDenyHostPatterns = asNetworkDenyHostPatterns;
    tRuntimeOptions.tPolicy.iNetworkDenyHostPatternCount = 1u;
    tHostOptions.sDefaultWorkingDirectory = ".";
    tHostOptions.bEnforceFilesystemRoot = true;
    tHostOptions.bDenyDestructiveCommands = true;
    assert(xwork_local_host_configure_services(
        &tHost,
        &tHostOptions,
        &tHostServices
    ) == XWORK_OK);

    tRuntimeOptions.pHostServices = &tHostServices;
    assert(xwork_runtime_create(&tRuntimeOptions, &pRuntime) == XWORK_OK);

    tPlaneOptions.sPlaneId = "remote-smoke-plane";
    tPlaneOptions.pRuntime = pRuntime;
    tPlaneOptions.iNowMs = 100u;
    tPlaneOptions.iDefaultLeaseTimeoutMs = 25u;
    tPlaneOptions.psAllowedCapabilities = asCapabilities;
    tPlaneOptions.iAllowedCapabilityCount = 4u;
    tPlaneOptions.bEnforceCapabilityAllowlist = true;
    asCapabilities[0] = XWORK_TOOL_PROCESS_EXEC;
    asCapabilities[1] = XWORK_TOOL_FILESYSTEM_READ_TEXT;
    asCapabilities[2] = XWORK_TOOL_FILESYSTEM_WRITE_TEXT;
    asCapabilities[3] = XWORK_TOOL_FILESYSTEM_APPLY_PATCH;
    asTerminalCapabilities[0] = XWORK_TOOL_PROCESS_START_TERMINAL;
    asTerminalCapabilities[1] = XWORK_TOOL_PROCESS_LIST_TERMINALS;
    asTerminalCapabilities[2] = XWORK_TOOL_PROCESS_TERMINAL_READ;
    asTerminalCapabilities[3] = XWORK_TOOL_PROCESS_TERMINAL_STOP;

    tApprovalPlaneOptions.sPlaneId = "approval-gate-plane";
    tApprovalPlaneOptions.pRuntime = pRuntime;
    tApprovalPlaneOptions.psAllowedCapabilities = asCapabilities;
    tApprovalPlaneOptions.iAllowedCapabilityCount = 4u;
    tApprovalPlaneOptions.bEnforceCapabilityAllowlist = true;
    tApprovalPlaneOptions.bAutoApproveTasks = false;
    assert(xwork_control_plane_create(
        &tApprovalPlaneOptions,
        &pApprovalPlane
    ) == XWORK_OK);
    xwork_remote_task_options_init(&tTaskOptions);
    tTaskOptions.sTaskId = "approval-required-process";
    tTaskOptions.eKind = XWORK_REMOTE_TASK_PROCESS_EXEC;
    tTaskOptions.sRequiredCapability = XWORK_TOOL_PROCESS_EXEC;
    tTaskOptions.sRequestJson =
        "{\"command\":\"" XWORK_REMOTE_SMOKE_COMMAND "\",\"max_output_bytes\":4096}";
    assert(xwork_control_plane_enqueue_task(
        pApprovalPlane,
        &tTaskOptions
    ) == XWORK_ERROR_PAUSED);
    xwork_control_plane_destroy(pApprovalPlane);
    pApprovalPlane = NULL;

    tPlaneOptions.eTransport = XWORK_REMOTE_TRANSPORT_HTTP_BOUNDARY;
    assert(xwork_control_plane_create(&tPlaneOptions, &pHttpPlane) == XWORK_OK);
    assert(xwork_control_plane_start(pHttpPlane) == XWORK_OK);
    assert(xwork_control_plane_get_snapshot(pHttpPlane, &tPlaneSnapshot) == XWORK_OK);
    assert(tPlaneSnapshot.eTransport == XWORK_REMOTE_TRANSPORT_HTTP_BOUNDARY);
    assert(tPlaneSnapshot.iProtocolVersion == XWORK_REMOTE_PROTOCOL_VERSION_CURRENT);
    xwork_control_plane_snapshot_reset(&tPlaneSnapshot);
    assert(xwork_control_plane_stop(pHttpPlane) == XWORK_OK);
    xwork_control_plane_destroy(pHttpPlane);
    pHttpPlane = NULL;
    tPlaneOptions.eTransport = XWORK_REMOTE_TRANSPORT_IN_PROCESS;

    assert(xwork_control_plane_create(&tPlaneOptions, &pPlane) == XWORK_OK);
    assert(xwork_control_plane_start(pPlane) == XWORK_OK);

    tWorkerOptions.sWorkerId = "local-worker";
    tWorkerOptions.sDisplayName = "Local Worker";
    tWorkerOptions.psCapabilities = asCapabilities;
    tWorkerOptions.iCapabilityCount = 4u;
    tWorkerOptions.iLeaseTimeoutMs = 20u;
    tWorkerOptions.pRuntime = pRuntime;
    assert(xwork_control_plane_register_worker(
        pPlane,
        &tWorkerOptions,
        &pWorker
    ) == XWORK_OK);
    assert(pWorker != NULL);
    assert(xwork_control_plane_list_workers(pPlane, &tWorkerList) == XWORK_OK);
    assert(tWorkerList.iCount == 1u);
    assert(tWorkerList.pItems[0].eState == XWORK_WORKER_ONLINE);
    assert(tWorkerList.pItems[0].iProtocolVersion == XWORK_REMOTE_PROTOCOL_VERSION_CURRENT);
    xwork_worker_summary_list_reset(&tWorkerList);

    asForbiddenCapabilities[0] = XWORK_TOOL_PROCESS_START_TERMINAL;
    xwork_worker_options_init(&tWorkerOptions);
    tWorkerOptions.sWorkerId = "forbidden-worker";
    tWorkerOptions.sDisplayName = "Forbidden Worker";
    tWorkerOptions.psCapabilities = asForbiddenCapabilities;
    tWorkerOptions.iCapabilityCount = 1u;
    tWorkerOptions.pRuntime = pRuntime;
    assert(xwork_control_plane_register_worker(
        pPlane,
        &tWorkerOptions,
        NULL
    ) == XWORK_ERROR_UNSUPPORTED);

    xwork_worker_options_init(&tWorkerOptions);
    tWorkerOptions.sWorkerId = "future-protocol-worker";
    tWorkerOptions.iProtocolVersion = XWORK_REMOTE_PROTOCOL_VERSION_CURRENT + 1u;
    tWorkerOptions.psCapabilities = asCapabilities;
    tWorkerOptions.iCapabilityCount = 1u;
    assert(xwork_control_plane_register_worker(
        pPlane,
        &tWorkerOptions,
        NULL
    ) == XWORK_ERROR_UNSUPPORTED);

    xwork_remote_task_options_init(&tTaskOptions);
    tTaskOptions.sTaskId = "forbidden-capability-task";
    tTaskOptions.eKind = XWORK_REMOTE_TASK_HOST_TOOL;
    tTaskOptions.eHostService = XWORK_HOST_PROCESS;
    tTaskOptions.sOperationId = XWORK_HOST_PROCESS_START_TERMINAL;
    tTaskOptions.sRequiredCapability = XWORK_TOOL_PROCESS_START_TERMINAL;
    tTaskOptions.sRequestJson =
        "{\"command\":\"" XWORK_REMOTE_SMOKE_COMMAND "\",\"session_name\":\"blocked\"}";
    assert(xwork_control_plane_enqueue_task(
        pPlane,
        &tTaskOptions
    ) == XWORK_ERROR_UNSUPPORTED);

    xwork_remote_task_options_init(&tTaskOptions);
    tTaskOptions.sTaskId = "network-policy-denied";
    tTaskOptions.eKind = XWORK_REMOTE_TASK_HOST_TOOL;
    tTaskOptions.eHostService = XWORK_HOST_NETWORK;
    tTaskOptions.sOperationId = "fetch";
    tTaskOptions.sRequestJson = "{\"host\":\"blocked.example\"}";
    assert(xwork_control_plane_enqueue_task(
        pPlane,
        &tTaskOptions
    ) == XWORK_ERROR_UNSUPPORTED);

    assert(xwork_control_plane_worker_heartbeat(
        pPlane,
        "local-worker",
        110u
    ) == XWORK_OK);

    xwork_remote_task_options_init(&tTaskOptions);
    tTaskOptions.sTaskId = "remote-process";
    tTaskOptions.eKind = XWORK_REMOTE_TASK_PROCESS_EXEC;
    tTaskOptions.sRequiredCapability = XWORK_TOOL_PROCESS_EXEC;
    tTaskOptions.sRequestJson =
        "{\"command\":\"" XWORK_REMOTE_SMOKE_COMMAND "\",\"max_output_bytes\":4096}";
    assert(xwork_control_plane_enqueue_task(pPlane, &tTaskOptions) == XWORK_OK);
    assert(xwork_control_plane_execute_next_local(
        pPlane,
        "local-worker",
        &tAssignment
    ) == XWORK_OK);
    assert(strcmp(tAssignment.sTaskId, "remote-process") == 0);
    xwork_remote_task_assignment_reset(&tAssignment);
    assert(xwork_control_plane_get_task_summary(
        pPlane,
        "remote-process",
        &tTaskSummary
    ) == XWORK_OK);
    assert(tTaskSummary.eState == XWORK_REMOTE_TASK_COMPLETED);
    assert(tTaskSummary.sOutputText != NULL);
    assert(strstr(tTaskSummary.sOutputText, "xwork-remote-worker") != NULL);
    xwork_remote_task_summary_reset(&tTaskSummary);

    xwork_remote_task_options_init(&tTaskOptions);
    tTaskOptions.sTaskId = "remote-process-timeout";
    tTaskOptions.eKind = XWORK_REMOTE_TASK_PROCESS_EXEC;
    tTaskOptions.sRequiredCapability = XWORK_TOOL_PROCESS_EXEC;
    tTaskOptions.sRequestJson =
        "{\"command\":\"" XWORK_REMOTE_SMOKE_TIMEOUT_COMMAND "\","
        "\"timeout_ms\":50,\"timeout_stop\":\"kill_tree\","
        "\"max_output_bytes\":4096}";
    assert(xwork_control_plane_enqueue_task(pPlane, &tTaskOptions) == XWORK_OK);
    assert(xwork_control_plane_execute_next_local(
        pPlane,
        "local-worker",
        &tAssignment
    ) == XWORK_ERROR_EXTERNAL_FAILURE);
    assert(strcmp(tAssignment.sTaskId, "remote-process-timeout") == 0);
    assert(tAssignment.iProtocolVersion == XWORK_REMOTE_PROTOCOL_VERSION_CURRENT);
    xwork_remote_task_assignment_reset(&tAssignment);
    assert(xwork_control_plane_get_task_summary(
        pPlane,
        "remote-process-timeout",
        &tTaskSummary
    ) == XWORK_OK);
    assert(tTaskSummary.eState == XWORK_REMOTE_TASK_FAILED);
    assert(tTaskSummary.iStatus == XWORK_ERROR_EXTERNAL_FAILURE);
    assert(strcmp(tTaskSummary.sVisibleSummary, "process.exec timed out") == 0);
    assert(strcmp(tTaskSummary.sErrorKind, "external_failure") == 0);
    assert(strcmp(tTaskSummary.sErrorMessage, "process.exec timed out") == 0);
    assert(tTaskSummary.iProtocolVersion == XWORK_REMOTE_PROTOCOL_VERSION_CURRENT);
    assert(strstr(tTaskSummary.sOutputText, "\"error_kind\":\"timeout\"") != NULL);
    assert(strstr(tTaskSummary.sOutputText, "\"timeout_ms\":50") != NULL);
    assert(strstr(tTaskSummary.sOutputText, "\"timeout_stop\":\"kill_tree\"") != NULL);
    xwork_remote_task_summary_reset(&tTaskSummary);

    xwork_remote_task_options_init(&tTaskOptions);
    tTaskOptions.sTaskId = "remote-process-destructive-denied";
    tTaskOptions.eKind = XWORK_REMOTE_TASK_PROCESS_EXEC;
    tTaskOptions.sRequiredCapability = XWORK_TOOL_PROCESS_EXEC;
    tTaskOptions.sRequestJson =
        "{\"command\":\"" XWORK_REMOTE_SMOKE_DESTRUCTIVE_COMMAND "\","
        "\"max_output_bytes\":4096}";
    assert(xwork_control_plane_enqueue_task(pPlane, &tTaskOptions) == XWORK_OK);
    assert(xwork_control_plane_execute_next_local(
        pPlane,
        "local-worker",
        &tAssignment
    ) == XWORK_ERROR_INVALID_STATE);
    assert(strcmp(tAssignment.sTaskId, "remote-process-destructive-denied") == 0);
    xwork_remote_task_assignment_reset(&tAssignment);
    assert(xwork_control_plane_get_task_summary(
        pPlane,
        "remote-process-destructive-denied",
        &tTaskSummary
    ) == XWORK_OK);
    assert(tTaskSummary.eState == XWORK_REMOTE_TASK_FAILED);
    assert(tTaskSummary.iStatus == XWORK_ERROR_INVALID_STATE);
    assert(strcmp(
        tTaskSummary.sVisibleSummary,
        "process.exec denied by command policy"
    ) == 0);
    assert(strstr(
        tTaskSummary.sOutputText,
        "\"error_kind\":\"destructive_command\""
    ) != NULL);
    xwork_remote_task_summary_reset(&tTaskSummary);

    xwork_remote_task_options_init(&tTaskOptions);
    tTaskOptions.sTaskId = "manual-complete";
    tTaskOptions.eKind = XWORK_REMOTE_TASK_PROCESS_EXEC;
    tTaskOptions.sRequiredCapability = XWORK_TOOL_PROCESS_EXEC;
    tTaskOptions.sRequestJson =
        "{\"command\":\"" XWORK_REMOTE_SMOKE_COMMAND "\",\"max_output_bytes\":4096}";
    assert(xwork_control_plane_enqueue_task(pPlane, &tTaskOptions) == XWORK_OK);
    assert(xwork_control_plane_claim_task(
        pPlane,
        "local-worker",
        &tAssignment
    ) == XWORK_OK);
    tTaskResult.iStatus = XWORK_OK;
    tTaskResult.sOutputText = "{\"ok\":true,\"manual\":true}";
    tTaskResult.sVisibleSummary = "manual task complete";
    tRemoteArtifact.sArtifactId = "remote-diagnostics-artifact";
    tRemoteArtifact.eKind = XWORK_ARTIFACT_REPORT;
    tRemoteArtifact.eOutputClass = XWORK_ARTIFACT_OUTPUT_JSON;
    tRemoteArtifact.sOutputRole = "diagnostics";
    tRemoteArtifact.eReportClass = XWORK_ARTIFACT_REPORT_DIAGNOSTICS;
    tRemoteArtifact.sReportSubjectRef = "remote-task://manual-complete";
    tRemoteArtifact.sName = "remote-diagnostics.json";
    tRemoteArtifact.sMimeType = "application/json";
    tRemoteArtifact.sStorageRef =
        "remote://worker/local-worker/artifacts/remote-diagnostics-artifact";
    tRemoteArtifact.sSummary = "remote diagnostics";
    tRemoteArtifact.iSequence = 1u;
    tTaskResult.pArtifacts = &tRemoteArtifact;
    tTaskResult.iArtifactCount = 1u;
    assert(xwork_control_plane_complete_task(
        pPlane,
        tAssignment.sAssignmentId,
        &tTaskResult
    ) == XWORK_OK);
    xwork_remote_task_assignment_reset(&tAssignment);
    assert(xwork_control_plane_get_task_summary(
        pPlane,
        "manual-complete",
        &tTaskSummary
    ) == XWORK_OK);
    assert(tTaskSummary.eState == XWORK_REMOTE_TASK_COMPLETED);
    assert(strcmp(tTaskSummary.sVisibleSummary, "manual task complete") == 0);
    assert(tTaskSummary.iArtifactCount == 1u);
    assert(strcmp(tTaskSummary.pArtifacts[0].sName, "remote-diagnostics.json") == 0);
    assert(tTaskSummary.pArtifacts[0].eReportClass ==
           XWORK_ARTIFACT_REPORT_DIAGNOSTICS);
    xwork_remote_task_summary_reset(&tTaskSummary);

    xwork_remote_artifact_upload_init(&tArtifactUpload);
    tArtifactUpload.sTaskId = "manual-complete";
    tArtifactUpload.sWorkerId = "local-worker";
    tArtifactUpload.sBlobRef = "remote://worker/local-worker/artifacts/uploaded-log";
    tArtifactUpload.sContentHash = "sha256:remote-uploaded-log";
    tArtifactUpload.iChunkIndex = 0u;
    tArtifactUpload.iChunkCount = 1u;
    tArtifactUpload.pChunkData = sUploadData;
    tArtifactUpload.iChunkSize = strlen(sUploadData);
    tArtifactUpload.bFinalChunk = true;
    tUploadedArtifact.sArtifactId = "remote-uploaded-log";
    tUploadedArtifact.eKind = XWORK_ARTIFACT_OUTPUT;
    tUploadedArtifact.eOutputClass = XWORK_ARTIFACT_OUTPUT_TEXT;
    tUploadedArtifact.sOutputRole = "remote.artifact.upload";
    tUploadedArtifact.sName = "remote-uploaded.log";
    tUploadedArtifact.sMimeType = "text/plain";
    tUploadedArtifact.sStorageRef = tArtifactUpload.sBlobRef;
    tUploadedArtifact.sSummary = "remote uploaded log";
    tUploadedArtifact.iSequence = 2u;
    tArtifactUpload.pArtifact = &tUploadedArtifact;
    assert(xwork_control_plane_upload_artifact(pPlane, &tArtifactUpload) == XWORK_OK);
    assert(xwork_control_plane_get_task_summary(
        pPlane,
        "manual-complete",
        &tTaskSummary
    ) == XWORK_OK);
    assert(tTaskSummary.iArtifactCount == 2u);
    assert(strcmp(tTaskSummary.pArtifacts[1].sArtifactId, "remote-uploaded-log") == 0);
    assert(strcmp(tTaskSummary.pArtifacts[1].sStorageRef,
                  "remote://worker/local-worker/artifacts/uploaded-log") == 0);
    xwork_remote_task_summary_reset(&tTaskSummary);
    assert(xwork_control_plane_list_artifact_blobs(
        pPlane,
        "manual-complete",
        "remote-uploaded-log",
        &tBlobList
    ) == XWORK_OK);
    assert(tBlobList.iCount == 1u);
    assert(strcmp(tBlobList.pItems[0].sBlobRef,
                  "remote://worker/local-worker/artifacts/uploaded-log") == 0);
    assert(tBlobList.pItems[0].iChunkSize == strlen(sUploadData));
    assert(tBlobList.pItems[0].pChunkData != NULL);
    assert(memcmp(tBlobList.pItems[0].pChunkData,
                  sUploadData,
                  strlen(sUploadData)) == 0);
    xwork_remote_blob_chunk_summary_list_reset(&tBlobList);

    xwork_remote_output_chunk_init(&tOutputChunk);
    tOutputChunk.sTaskId = "manual-complete";
    tOutputChunk.sWorkerId = "local-worker";
    tOutputChunk.eStream = XWORK_REMOTE_OUTPUT_STDOUT;
    tOutputChunk.iChunkIndex = 0u;
    tOutputChunk.iOffsetBytes = 0u;
    tOutputChunk.sText = "stdout chunk 0\n";
    tOutputChunk.iByteCount = strlen(tOutputChunk.sText);
    tOutputChunk.sContentHash = "sha256:stdout-0";
    assert(xwork_control_plane_upload_output_chunk(pPlane, &tOutputChunk) == XWORK_OK);
    xwork_remote_output_chunk_init(&tOutputChunk);
    tOutputChunk.sTaskId = "manual-complete";
    tOutputChunk.sWorkerId = "local-worker";
    tOutputChunk.eStream = XWORK_REMOTE_OUTPUT_STDERR;
    tOutputChunk.iChunkIndex = 1u;
    tOutputChunk.iOffsetBytes = 0u;
    tOutputChunk.sText = "stderr chunk 0\n";
    tOutputChunk.iByteCount = strlen(tOutputChunk.sText);
    tOutputChunk.sContentHash = "sha256:stderr-0";
    tOutputChunk.bFinalChunk = true;
    assert(xwork_control_plane_upload_output_chunk(pPlane, &tOutputChunk) == XWORK_OK);
    assert(xwork_control_plane_get_task_summary(
        pPlane,
        "manual-complete",
        &tTaskSummary
    ) == XWORK_OK);
    assert(tTaskSummary.iOutputChunkCount == 2u);
    assert(tTaskSummary.pOutputChunks[0].eStream == XWORK_REMOTE_OUTPUT_STDOUT);
    assert(strcmp(tTaskSummary.pOutputChunks[0].sText, "stdout chunk 0\n") == 0);
    assert(tTaskSummary.pOutputChunks[1].eStream == XWORK_REMOTE_OUTPUT_STDERR);
    assert(tTaskSummary.pOutputChunks[1].bFinalChunk);
    assert(strcmp(tTaskSummary.pOutputChunks[1].sContentHash, "sha256:stderr-0") == 0);
    xwork_remote_task_summary_reset(&tTaskSummary);
    xwork_remote_task_result_init(&tTaskResult);

    xwork_remote_task_options_init(&tTaskOptions);
    tTaskOptions.sTaskId = "remote-secret-redaction";
    tTaskOptions.eKind = XWORK_REMOTE_TASK_PROCESS_EXEC;
    tTaskOptions.sRequiredCapability = XWORK_TOOL_PROCESS_EXEC;
    tTaskOptions.sRequestJson =
        "{\"command\":\"" XWORK_REMOTE_SMOKE_COMMAND "\","
        "\"api_key\":\"sk-remote-secret\",\"max_output_bytes\":4096}";
    assert(xwork_control_plane_enqueue_task(pPlane, &tTaskOptions) == XWORK_OK);
    assert(xwork_control_plane_claim_task(
        pPlane,
        "local-worker",
        &tAssignment
    ) == XWORK_OK);
    xwork_remote_task_result_init(&tTaskResult);
    tTaskResult.iStatus = XWORK_OK;
    tTaskResult.sOutputText = "{\"ok\":true,\"access_token\":\"token-secret\"}";
    tTaskResult.sVisibleSummary = "secret redaction complete";
    assert(xwork_control_plane_complete_task(
        pPlane,
        tAssignment.sAssignmentId,
        &tTaskResult
    ) == XWORK_OK);
    xwork_remote_task_assignment_reset(&tAssignment);
    assert(xwork_control_plane_get_task_summary(
        pPlane,
        "remote-secret-redaction",
        &tTaskSummary
    ) == XWORK_OK);
    assert(tTaskSummary.eState == XWORK_REMOTE_TASK_COMPLETED);
    assert(strstr(tTaskSummary.sRequestJson, "sk-remote-secret") == NULL);
    assert(strstr(tTaskSummary.sRequestJson, "\"api_key\":\"[REDACTED]\"") != NULL);
    assert(strstr(tTaskSummary.sOutputText, "token-secret") == NULL);
    assert(strstr(tTaskSummary.sOutputText, "\"access_token\":\"[REDACTED]\"") != NULL);
    xwork_remote_task_summary_reset(&tTaskSummary);

    xwork_remote_task_options_init(&tTaskOptions);
    tTaskOptions.sTaskId = "remote-filesystem-write";
    tTaskOptions.eKind = XWORK_REMOTE_TASK_HOST_TOOL;
    tTaskOptions.eHostService = XWORK_HOST_FILESYSTEM;
    tTaskOptions.sOperationId = XWORK_HOST_FILESYSTEM_WRITE_TEXT;
    tTaskOptions.sRequiredCapability = XWORK_TOOL_FILESYSTEM_WRITE_TEXT;
    tTaskOptions.sRequestJson =
        "{\"path\":\"tests/xwork_remote_worker_fs_tmp.txt\","
        "\"text\":\"alpha\\nbeta\\ngamma\\n\",\"mode\":\"overwrite\"}";
    assert(xwork_control_plane_enqueue_task(pPlane, &tTaskOptions) == XWORK_OK);
    assert(xwork_control_plane_execute_next_local(
        pPlane,
        "local-worker",
        &tAssignment
    ) == XWORK_OK);
    assert(strcmp(tAssignment.sTaskId, "remote-filesystem-write") == 0);
    xwork_remote_task_assignment_reset(&tAssignment);
    assert(xwork_control_plane_get_task_summary(
        pPlane,
        "remote-filesystem-write",
        &tTaskSummary
    ) == XWORK_OK);
    assert(tTaskSummary.eState == XWORK_REMOTE_TASK_COMPLETED);
    assert(strstr(tTaskSummary.sOutputText, "\"ok\":true") != NULL);
    xwork_remote_task_summary_reset(&tTaskSummary);

    xwork_remote_task_options_init(&tTaskOptions);
    tTaskOptions.sTaskId = "remote-filesystem-read";
    tTaskOptions.eKind = XWORK_REMOTE_TASK_HOST_TOOL;
    tTaskOptions.eHostService = XWORK_HOST_FILESYSTEM;
    tTaskOptions.sOperationId = XWORK_HOST_FILESYSTEM_READ_TEXT;
    tTaskOptions.sRequiredCapability = XWORK_TOOL_FILESYSTEM_READ_TEXT;
    tTaskOptions.sRequestJson =
        "{\"path\":\"tests/xwork_remote_worker_fs_tmp.txt\"}";
    assert(xwork_control_plane_enqueue_task(pPlane, &tTaskOptions) == XWORK_OK);
    assert(xwork_control_plane_execute_next_local(
        pPlane,
        "local-worker",
        &tAssignment
    ) == XWORK_OK);
    xwork_remote_task_assignment_reset(&tAssignment);
    assert(xwork_control_plane_get_task_summary(
        pPlane,
        "remote-filesystem-read",
        &tTaskSummary
    ) == XWORK_OK);
    assert(tTaskSummary.eState == XWORK_REMOTE_TASK_COMPLETED);
    assert(strstr(tTaskSummary.sOutputText, "alpha\\nbeta\\ngamma") != NULL);
    xwork_remote_task_summary_reset(&tTaskSummary);

    xwork_remote_task_options_init(&tTaskOptions);
    tTaskOptions.sTaskId = "remote-filesystem-apply-patch";
    tTaskOptions.eKind = XWORK_REMOTE_TASK_HOST_TOOL;
    tTaskOptions.eHostService = XWORK_HOST_FILESYSTEM;
    tTaskOptions.sOperationId = XWORK_HOST_FILESYSTEM_APPLY_PATCH;
    tTaskOptions.sRequiredCapability = XWORK_TOOL_FILESYSTEM_APPLY_PATCH;
    tTaskOptions.sRequestJson =
        "{\"path\":\"tests/xwork_remote_worker_fs_tmp.txt\","
        "\"old_text\":\"beta\\n\",\"new_text\":\"BETA\\n\"}";
    assert(xwork_control_plane_enqueue_task(pPlane, &tTaskOptions) == XWORK_OK);
    assert(xwork_control_plane_execute_next_local(
        pPlane,
        "local-worker",
        &tAssignment
    ) == XWORK_OK);
    xwork_remote_task_assignment_reset(&tAssignment);
    assert(xwork_control_plane_get_task_summary(
        pPlane,
        "remote-filesystem-apply-patch",
        &tTaskSummary
    ) == XWORK_OK);
    assert(tTaskSummary.eState == XWORK_REMOTE_TASK_COMPLETED);
    assert(strstr(tTaskSummary.sOutputText, "\"changed\":true") != NULL);
    xwork_remote_task_summary_reset(&tTaskSummary);

    xwork_remote_task_options_init(&tTaskOptions);
    tTaskOptions.sTaskId = "remote-filesystem-read-patched";
    tTaskOptions.eKind = XWORK_REMOTE_TASK_HOST_TOOL;
    tTaskOptions.eHostService = XWORK_HOST_FILESYSTEM;
    tTaskOptions.sOperationId = XWORK_HOST_FILESYSTEM_READ_TEXT;
    tTaskOptions.sRequiredCapability = XWORK_TOOL_FILESYSTEM_READ_TEXT;
    tTaskOptions.sRequestJson =
        "{\"path\":\"tests/xwork_remote_worker_fs_tmp.txt\"}";
    assert(xwork_control_plane_enqueue_task(pPlane, &tTaskOptions) == XWORK_OK);
    assert(xwork_control_plane_execute_next_local(
        pPlane,
        "local-worker",
        &tAssignment
    ) == XWORK_OK);
    xwork_remote_task_assignment_reset(&tAssignment);
    assert(xwork_control_plane_get_task_summary(
        pPlane,
        "remote-filesystem-read-patched",
        &tTaskSummary
    ) == XWORK_OK);
    assert(tTaskSummary.eState == XWORK_REMOTE_TASK_COMPLETED);
    assert(strstr(tTaskSummary.sOutputText, "alpha\\nBETA\\ngamma") != NULL);
    xwork_remote_task_summary_reset(&tTaskSummary);

    xwork_remote_task_options_init(&tTaskOptions);
    tTaskOptions.sTaskId = "remote-filesystem-root-denied";
    tTaskOptions.eKind = XWORK_REMOTE_TASK_HOST_TOOL;
    tTaskOptions.eHostService = XWORK_HOST_FILESYSTEM;
    tTaskOptions.sOperationId = XWORK_HOST_FILESYSTEM_READ_TEXT;
    tTaskOptions.sRequiredCapability = XWORK_TOOL_FILESYSTEM_READ_TEXT;
    tTaskOptions.sRequestJson = "{\"path\":\"../xwork.h\"}";
    assert(xwork_control_plane_enqueue_task(pPlane, &tTaskOptions) == XWORK_OK);
    assert(xwork_control_plane_execute_next_local(
        pPlane,
        "local-worker",
        &tAssignment
    ) == XWORK_ERROR_INVALID_STATE);
    xwork_remote_task_assignment_reset(&tAssignment);
    assert(xwork_control_plane_get_task_summary(
        pPlane,
        "remote-filesystem-root-denied",
        &tTaskSummary
    ) == XWORK_OK);
    assert(tTaskSummary.eState == XWORK_REMOTE_TASK_FAILED);
    assert(tTaskSummary.iStatus == XWORK_ERROR_INVALID_STATE);
    assert(strcmp(
        tTaskSummary.sVisibleSummary,
        "filesystem.read_text denied by path policy"
    ) == 0);
    assert(strstr(tTaskSummary.sOutputText, "\"error_kind\":\"path_denied\"") != NULL);
    xwork_remote_task_summary_reset(&tTaskSummary);

    if ( xrtProcessTerminalSupported() ) {
        tTerminalPlaneOptions.sPlaneId = "remote-terminal-plane";
        tTerminalPlaneOptions.pRuntime = pRuntime;
        tTerminalPlaneOptions.iNowMs = 300u;
        tTerminalPlaneOptions.iDefaultLeaseTimeoutMs = 1000u;
        tTerminalPlaneOptions.psAllowedCapabilities = asTerminalCapabilities;
        tTerminalPlaneOptions.iAllowedCapabilityCount = 4u;
        tTerminalPlaneOptions.bEnforceCapabilityAllowlist = true;
        assert(xwork_control_plane_create(
            &tTerminalPlaneOptions,
            &pTerminalPlane
        ) == XWORK_OK);
        assert(xwork_control_plane_start(pTerminalPlane) == XWORK_OK);

        xwork_worker_options_init(&tWorkerOptions);
        tWorkerOptions.sWorkerId = "terminal-worker";
        tWorkerOptions.sDisplayName = "Terminal Worker";
        tWorkerOptions.psCapabilities = asTerminalCapabilities;
        tWorkerOptions.iCapabilityCount = 4u;
        tWorkerOptions.pRuntime = pRuntime;
        assert(xwork_control_plane_register_worker(
            pTerminalPlane,
            &tWorkerOptions,
            &pTerminalWorker
        ) == XWORK_OK);
        assert(pTerminalWorker != NULL);

        xwork_remote_task_options_init(&tTaskOptions);
        tTaskOptions.sTaskId = "remote-terminal-start";
        tTaskOptions.eKind = XWORK_REMOTE_TASK_HOST_TOOL;
        tTaskOptions.eHostService = XWORK_HOST_PROCESS;
        tTaskOptions.sOperationId = XWORK_HOST_PROCESS_START_TERMINAL;
        tTaskOptions.sRequiredCapability = XWORK_TOOL_PROCESS_START_TERMINAL;
        tTaskOptions.sRequestJson =
            "{\"command\":\"" XWORK_REMOTE_SMOKE_TERMINAL_COMMAND "\","
            "\"session_name\":\"remote-shell\","
            "\"terminal_cols\":80,\"terminal_rows\":24,\"max_events\":8}";
        assert(xwork_control_plane_enqueue_task(
            pTerminalPlane,
            &tTaskOptions
        ) == XWORK_OK);
        assert(xwork_control_plane_execute_next_local(
            pTerminalPlane,
            "terminal-worker",
            &tAssignment
        ) == XWORK_OK);
        assert(strcmp(tAssignment.sTaskId, "remote-terminal-start") == 0);
        xwork_remote_task_assignment_reset(&tAssignment);
        assert(xwork_control_plane_get_task_summary(
            pTerminalPlane,
            "remote-terminal-start",
            &tTaskSummary
        ) == XWORK_OK);
        assert(tTaskSummary.eState == XWORK_REMOTE_TASK_COMPLETED);
        assert(strcmp(tTaskSummary.sVisibleSummary, "process.start_terminal ok") == 0);
        assert(strstr(tTaskSummary.sOutputText, "\"schema\":\"" XWORK_TERMINAL_STATE_SCHEMA_V1 "\"") != NULL);
        assert(strstr(tTaskSummary.sOutputText, "\"session_id\":\"terminal-session-1\"") != NULL);
        assert(strstr(tTaskSummary.sOutputText, "\"session_name\":\"remote-shell\"") != NULL);
        assert(strstr(tTaskSummary.sOutputText, "\"terminal_cols\":80") != NULL);
        xwork_remote_task_summary_reset(&tTaskSummary);

        xwork_remote_task_options_init(&tTaskOptions);
        tTaskOptions.sTaskId = "remote-terminal-list";
        tTaskOptions.eKind = XWORK_REMOTE_TASK_HOST_TOOL;
        tTaskOptions.eHostService = XWORK_HOST_PROCESS;
        tTaskOptions.sOperationId = XWORK_HOST_PROCESS_LIST_TERMINALS;
        tTaskOptions.sRequiredCapability = XWORK_TOOL_PROCESS_LIST_TERMINALS;
        tTaskOptions.sRequestJson =
            "{\"session_name\":\"remote-shell\",\"running\":true,\"limit\":4}";
        assert(xwork_control_plane_enqueue_task(
            pTerminalPlane,
            &tTaskOptions
        ) == XWORK_OK);
        assert(xwork_control_plane_execute_next_local(
            pTerminalPlane,
            "terminal-worker",
            &tAssignment
        ) == XWORK_OK);
        xwork_remote_task_assignment_reset(&tAssignment);
        assert(xwork_control_plane_get_task_summary(
            pTerminalPlane,
            "remote-terminal-list",
            &tTaskSummary
        ) == XWORK_OK);
        assert(tTaskSummary.eState == XWORK_REMOTE_TASK_COMPLETED);
        assert(strstr(tTaskSummary.sOutputText, "\"schema\":\"" XWORK_TERMINAL_INVENTORY_SCHEMA_V1 "\"") != NULL);
        assert(strstr(tTaskSummary.sOutputText, "\"session_id\":\"terminal-session-1\"") != NULL);
        assert(strstr(tTaskSummary.sOutputText, "\"session_name\":\"remote-shell\"") != NULL);
        xwork_remote_task_summary_reset(&tTaskSummary);

        xwork_remote_task_options_init(&tTaskOptions);
        tTaskOptions.sTaskId = "remote-terminal-stop";
        tTaskOptions.eKind = XWORK_REMOTE_TASK_HOST_TOOL;
        tTaskOptions.eHostService = XWORK_HOST_PROCESS;
        tTaskOptions.sOperationId = XWORK_HOST_PROCESS_TERMINAL_STOP;
        tTaskOptions.sRequiredCapability = XWORK_TOOL_PROCESS_TERMINAL_STOP;
        tTaskOptions.sRequestJson = "{\"session_id\":\"terminal-session-1\"}";
        assert(xwork_control_plane_enqueue_task(
            pTerminalPlane,
            &tTaskOptions
        ) == XWORK_OK);
        assert(xwork_control_plane_execute_next_local(
            pTerminalPlane,
            "terminal-worker",
            &tAssignment
        ) == XWORK_OK);
        xwork_remote_task_assignment_reset(&tAssignment);
        assert(xwork_control_plane_get_task_summary(
            pTerminalPlane,
            "remote-terminal-stop",
            &tTaskSummary
        ) == XWORK_OK);
        assert(tTaskSummary.eState == XWORK_REMOTE_TASK_COMPLETED);
        assert(strcmp(tTaskSummary.sVisibleSummary, "process.terminal_stop ok") == 0);
        assert(strstr(tTaskSummary.sOutputText, "\"removed\":true") != NULL);
        xwork_remote_task_summary_reset(&tTaskSummary);

        assert(xwork_control_plane_stop(pTerminalPlane) == XWORK_OK);
        xwork_control_plane_destroy(pTerminalPlane);
        pTerminalPlane = NULL;
        pTerminalWorker = NULL;
    }

    xwork_remote_task_options_init(&tTaskOptions);
    tTaskOptions.sTaskId = "stale-assignment";
    tTaskOptions.eKind = XWORK_REMOTE_TASK_PROCESS_EXEC;
    tTaskOptions.sRequiredCapability = XWORK_TOOL_PROCESS_EXEC;
    tTaskOptions.sRequestJson =
        "{\"command\":\"" XWORK_REMOTE_SMOKE_COMMAND "\",\"max_output_bytes\":4096}";
    assert(xwork_control_plane_enqueue_task(pPlane, &tTaskOptions) == XWORK_OK);
    assert(xwork_control_plane_claim_task(
        pPlane,
        "local-worker",
        &tAssignment
    ) == XWORK_OK);
    xwork_remote_task_assignment_reset(&tAssignment);
    assert(xwork_control_plane_sweep_stale(
        pPlane,
        200u,
        &iOrphanedCount
    ) == XWORK_OK);
    assert(iOrphanedCount == 1u);
    assert(xwork_control_plane_get_task_summary(
        pPlane,
        "stale-assignment",
        &tTaskSummary
    ) == XWORK_OK);
    assert(tTaskSummary.eState == XWORK_REMOTE_TASK_ORPHANED);
    xwork_remote_task_summary_reset(&tTaskSummary);
    assert(xwork_control_plane_list_workers(pPlane, &tWorkerList) == XWORK_OK);
    assert(tWorkerList.iCount == 1u);
    assert(tWorkerList.pItems[0].eState == XWORK_WORKER_STALE);
    xwork_worker_summary_list_reset(&tWorkerList);

    assert(xwork_control_plane_worker_heartbeat(
        pPlane,
        "local-worker",
        210u
    ) == XWORK_OK);

    xwork_remote_task_options_init(&tTaskOptions);
    tTaskOptions.sTaskId = "cancelled-task";
    tTaskOptions.eKind = XWORK_REMOTE_TASK_PROCESS_EXEC;
    tTaskOptions.sRequiredCapability = XWORK_TOOL_PROCESS_EXEC;
    tTaskOptions.sRequestJson =
        "{\"command\":\"" XWORK_REMOTE_SMOKE_COMMAND "\",\"max_output_bytes\":4096}";
    assert(xwork_control_plane_enqueue_task(pPlane, &tTaskOptions) == XWORK_OK);
    assert(xwork_control_plane_cancel_task(
        pPlane,
        "cancelled-task",
        "smoke cancel"
    ) == XWORK_OK);
    assert(xwork_control_plane_get_task_summary(
        pPlane,
        "cancelled-task",
        &tTaskSummary
    ) == XWORK_OK);
    assert(tTaskSummary.eState == XWORK_REMOTE_TASK_CANCELLED);
    assert(tTaskSummary.iStatus == XWORK_ERROR_CANCELLED);
    xwork_remote_task_summary_reset(&tTaskSummary);

    xwork_remote_task_options_init(&tTaskOptions);
    tTaskOptions.sTaskId = "recover-queued";
    tTaskOptions.eKind = XWORK_REMOTE_TASK_PROCESS_EXEC;
    tTaskOptions.sRequiredCapability = XWORK_TOOL_PROCESS_EXEC;
    tTaskOptions.sRequestJson =
        "{\"command\":\"" XWORK_REMOTE_SMOKE_COMMAND "\",\"max_output_bytes\":4096}";
    assert(xwork_control_plane_enqueue_task(pPlane, &tTaskOptions) == XWORK_OK);

    xwork_remote_task_options_init(&tTaskOptions);
    tTaskOptions.sTaskId = "recover-assigned";
    tTaskOptions.eKind = XWORK_REMOTE_TASK_PROCESS_EXEC;
    tTaskOptions.sRequiredCapability = XWORK_TOOL_PROCESS_EXEC;
    tTaskOptions.sRequestJson =
        "{\"command\":\"" XWORK_REMOTE_SMOKE_COMMAND "\",\"max_output_bytes\":4096}";
    assert(xwork_control_plane_enqueue_task(pPlane, &tTaskOptions) == XWORK_OK);
    assert(xwork_control_plane_claim_task(
        pPlane,
        "local-worker",
        &tAssignment
    ) == XWORK_OK);
    xwork_remote_task_assignment_reset(&tAssignment);

    assert(xwork_control_plane_get_snapshot(pPlane, &tPlaneSnapshot) == XWORK_OK);
    assert(tPlaneSnapshot.tWorkers.iCount == 1u);
    assert(tPlaneSnapshot.tTasks.iCount == 14u);
    assert(tPlaneSnapshot.tBlobChunks.iCount == 1u);
    assert(tPlaneSnapshot.iProtocolVersion == XWORK_REMOTE_PROTOCOL_VERSION_CURRENT);
    assert(tPlaneSnapshot.tWorkers.pItems[0].iProtocolVersion ==
        XWORK_REMOTE_PROTOCOL_VERSION_CURRENT);
    assert(xwork_file_persistence_store_control_plane_snapshot(
        &tStore,
        &tPlaneSnapshot
    ) == XWORK_OK);
    assert(xwork_file_persistence_load_control_plane_snapshot(
        &tStore,
        "remote-smoke-plane",
        &tLoadedPlaneSnapshot
    ) == XWORK_OK);
    assert(strcmp(tLoadedPlaneSnapshot.sPlaneId, "remote-smoke-plane") == 0);
    assert(tLoadedPlaneSnapshot.iProtocolVersion == XWORK_REMOTE_PROTOCOL_VERSION_CURRENT);
    assert(tLoadedPlaneSnapshot.tWorkers.pItems[0].iProtocolVersion ==
        XWORK_REMOTE_PROTOCOL_VERSION_CURRENT);
    assert(tLoadedPlaneSnapshot.tTasks.pItems[0].iProtocolVersion ==
        XWORK_REMOTE_PROTOCOL_VERSION_CURRENT);
    assert(tLoadedPlaneSnapshot.tBlobChunks.iCount == 1u);
    assert(tLoadedPlaneSnapshot.tBlobChunks.pItems[0].iChunkSize ==
        strlen(sUploadData));
    assert(memcmp(tLoadedPlaneSnapshot.tBlobChunks.pItems[0].pChunkData,
                  sUploadData,
                  strlen(sUploadData)) == 0);
    assert(xwork_file_persistence_recover_control_plane(
        &tStore,
        pRuntime,
        "remote-smoke-plane",
        NULL,
        &pRecoveredPlane
    ) == XWORK_OK);
    assert(xwork_control_plane_get_task_summary(
        pRecoveredPlane,
        "manual-complete",
        &tTaskSummary
    ) == XWORK_OK);
    assert(tTaskSummary.eState == XWORK_REMOTE_TASK_COMPLETED);
    assert(tTaskSummary.iArtifactCount == 2u);
    assert(strcmp(tTaskSummary.pArtifacts[0].sStorageRef,
                  "remote://worker/local-worker/artifacts/remote-diagnostics-artifact") == 0);
    assert(strcmp(tTaskSummary.pArtifacts[1].sStorageRef,
                  "remote://worker/local-worker/artifacts/uploaded-log") == 0);
    assert(tTaskSummary.iOutputChunkCount == 2u);
    assert(strcmp(tTaskSummary.pOutputChunks[0].sText, "stdout chunk 0\n") == 0);
    assert(strcmp(tTaskSummary.pOutputChunks[1].sText, "stderr chunk 0\n") == 0);
    xwork_remote_task_summary_reset(&tTaskSummary);
    assert(xwork_control_plane_list_artifact_blobs(
        pRecoveredPlane,
        "manual-complete",
        "remote-uploaded-log",
        &tBlobList
    ) == XWORK_OK);
    assert(tBlobList.iCount == 1u);
    assert(tBlobList.pItems[0].iChunkSize == strlen(sUploadData));
    assert(memcmp(tBlobList.pItems[0].pChunkData,
                  sUploadData,
                  strlen(sUploadData)) == 0);
    xwork_remote_blob_chunk_summary_list_reset(&tBlobList);
    assert(xwork_control_plane_get_task_summary(
        pRecoveredPlane,
        "recover-assigned",
        &tTaskSummary
    ) == XWORK_OK);
    assert(tTaskSummary.eState == XWORK_REMOTE_TASK_ORPHANED);
    assert(tTaskSummary.iStatus == XWORK_ERROR_CANCELLED);
    xwork_remote_task_summary_reset(&tTaskSummary);
    assert(xwork_control_plane_get_task_summary(
        pRecoveredPlane,
        "recover-queued",
        &tTaskSummary
    ) == XWORK_OK);
    assert(tTaskSummary.eState == XWORK_REMOTE_TASK_QUEUED);
    xwork_remote_task_summary_reset(&tTaskSummary);
    assert(xwork_control_plane_execute_next_local(
        pRecoveredPlane,
        "local-worker",
        &tAssignment
    ) == XWORK_OK);
    assert(strcmp(tAssignment.sTaskId, "recover-queued") == 0);
    xwork_remote_task_assignment_reset(&tAssignment);

    assert(xwork_control_plane_list_tasks(pPlane, &tTaskList) == XWORK_OK);
    assert(tTaskList.iCount == 14u);
    xwork_remote_task_summary_list_reset(&tTaskList);

    remove("tests/xwork_remote_worker_fs_tmp.txt");
    assert(xwork_control_plane_stop(pPlane) == XWORK_OK);
    xwork_control_plane_destroy(pApprovalPlane);
    xwork_control_plane_destroy(pHttpPlane);
    xwork_control_plane_destroy(pTerminalPlane);
    xwork_control_plane_destroy(pRecoveredPlane);
    xwork_control_plane_destroy(pPlane);
    xwork_runtime_destroy(pRuntime);
    xwork_local_host_reset(&tHost);
    xwork_control_plane_snapshot_reset(&tPlaneSnapshot);
    xwork_control_plane_snapshot_reset(&tLoadedPlaneSnapshot);
    xwork_file_persistence_reset(&tStore);
    xwork_remote_task_assignment_reset(&tAssignment);
    xwork_remote_task_summary_reset(&tTaskSummary);
    xwork_remote_task_summary_list_reset(&tTaskList);
    xwork_remote_blob_chunk_summary_list_reset(&tBlobList);
    xwork_worker_summary_list_reset(&tWorkerList);

    printf("xwork remote worker smoke passed\n");
    return 0;
}
