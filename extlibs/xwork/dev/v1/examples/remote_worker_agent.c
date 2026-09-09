#include "../xwork.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define XWORK_EXAMPLE_REMOTE_COMMAND "cmd /c echo xwork-remote-worker-example"
#else
#define XWORK_EXAMPLE_REMOTE_COMMAND "printf %s xwork-remote-worker-example"
#endif

static int xwork_example_check(xwork_status iStatus, const char *sStep)
{
    if ( iStatus == XWORK_OK ) {
        return 0;
    }
    fprintf(stderr, "%s failed: %d\n", sStep, (int)iStatus);
    return 1;
}

static int enqueue_process_task(
    xwork_control_plane *pPlane,
    const char *sTaskId
)
{
    xwork_remote_task_options tTaskOptions;

    xwork_remote_task_options_init(&tTaskOptions);
    tTaskOptions.sTaskId = sTaskId;
    tTaskOptions.eKind = XWORK_REMOTE_TASK_PROCESS_EXEC;
    tTaskOptions.sRequiredCapability = XWORK_TOOL_PROCESS_EXEC;
    tTaskOptions.sRequestJson =
        "{\"command\":\"" XWORK_EXAMPLE_REMOTE_COMMAND "\","
        "\"max_output_bytes\":4096}";
    return xwork_example_check(
        xwork_control_plane_enqueue_task(pPlane, &tTaskOptions),
        "enqueue remote process task"
    );
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
    xwork_control_plane_options tPlaneOptions;
    xwork_worker_options tWorkerOptions;
    xwork_remote_task_assignment tAssignment;
    xwork_remote_task_summary tTaskSummary;
    xwork_control_plane_snapshot tPlaneSnapshot;
    xwork_runtime *pRuntime = NULL;
    xwork_control_plane *pPlane = NULL;
    xwork_control_plane *pRecoveredPlane = NULL;
    xwork_worker *pWorker = NULL;
    const char *asCapabilities[1];
    int iExit = 1;

    xwork_profile_init(&tProfile);
    xwork_file_persistence_init(&tStore);
    xwork_file_persistence_options_init(&tStoreOptions);
    xwork_persistence_backend_init(&tPersistence);
    xwork_local_host_init(&tHost);
    xwork_local_host_options_init(&tHostOptions);
    xwork_host_services_init(&tHostServices);
    xwork_runtime_options_init(&tRuntimeOptions);
    xwork_control_plane_options_init(&tPlaneOptions);
    xwork_worker_options_init(&tWorkerOptions);
    xwork_remote_task_assignment_init(&tAssignment);
    xwork_remote_task_summary_init(&tTaskSummary);
    xwork_control_plane_snapshot_init(&tPlaneSnapshot);

    if ( xwork_example_check(
             xwork_profile_get_builtin(XWORK_PROFILE_XCLAW, &tProfile),
             "load xclaw profile"
         ) ) goto cleanup;

    tStoreOptions.sRootPath = "examples/.xwork_remote_worker_store";
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

    tPlaneOptions.sPlaneId = "example-remote-worker-plane";
    tPlaneOptions.pRuntime = pRuntime;
    tPlaneOptions.iDefaultLeaseTimeoutMs = 30000u;
    tPlaneOptions.iNowMs = 100u;
    if ( xwork_example_check(
             xwork_control_plane_create(&tPlaneOptions, &pPlane),
             "create control plane"
         ) ) goto cleanup;
    if ( xwork_example_check(xwork_control_plane_start(pPlane), "start plane") ) {
        goto cleanup;
    }

    asCapabilities[0] = XWORK_TOOL_PROCESS_EXEC;
    tWorkerOptions.sWorkerId = "local-process-worker";
    tWorkerOptions.sDisplayName = "Local Process Worker";
    tWorkerOptions.sEndpoint = "inproc://local-process-worker";
    tWorkerOptions.psCapabilities = asCapabilities;
    tWorkerOptions.iCapabilityCount = 1u;
    tWorkerOptions.pRuntime = pRuntime;
    if ( xwork_example_check(
             xwork_control_plane_register_worker(
                 pPlane,
                 &tWorkerOptions,
                 &pWorker
             ),
             "register worker"
         ) ) goto cleanup;
    if ( !pWorker ) {
        fprintf(stderr, "worker registration returned no worker handle\n");
        goto cleanup;
    }

    if ( enqueue_process_task(pPlane, "remote-process-now") ) goto cleanup;
    if ( xwork_example_check(
             xwork_control_plane_execute_next_local(
                 pPlane,
                 "local-process-worker",
                 &tAssignment
             ),
             "execute remote process task"
         ) ) goto cleanup;
    printf("executed assignment %s for task %s\n", tAssignment.sAssignmentId, tAssignment.sTaskId);
    xwork_remote_task_assignment_reset(&tAssignment);

    if ( xwork_example_check(
             xwork_control_plane_get_task_summary(
                 pPlane,
                 "remote-process-now",
                 &tTaskSummary
             ),
             "query completed task"
         ) ) goto cleanup;
    if ( tTaskSummary.eState != XWORK_REMOTE_TASK_COMPLETED ||
         !tTaskSummary.sOutputText ||
         !strstr(tTaskSummary.sOutputText, "xwork-remote-worker-example") ) {
        fprintf(stderr, "remote task did not complete with expected output\n");
        goto cleanup;
    }
    xwork_remote_task_summary_reset(&tTaskSummary);

    if ( enqueue_process_task(pPlane, "remote-process-after-recovery") ) {
        goto cleanup;
    }
    if ( enqueue_process_task(pPlane, "remote-process-inflight") ) {
        goto cleanup;
    }
    if ( xwork_example_check(
             xwork_control_plane_claim_task(
                 pPlane,
                 "local-process-worker",
                 &tAssignment
             ),
             "claim in-flight task before snapshot"
         ) ) goto cleanup;
    xwork_remote_task_assignment_reset(&tAssignment);

    if ( xwork_example_check(
             xwork_control_plane_get_snapshot(pPlane, &tPlaneSnapshot),
             "snapshot control plane"
         ) ) goto cleanup;
    if ( xwork_example_check(
             xwork_file_persistence_store_control_plane_snapshot(
                 &tStore,
                 &tPlaneSnapshot
             ),
             "store control plane snapshot"
         ) ) goto cleanup;

    if ( xwork_example_check(
             xwork_file_persistence_recover_control_plane(
                 &tStore,
                 pRuntime,
                 "example-remote-worker-plane",
                 NULL,
                 &pRecoveredPlane
             ),
             "recover control plane"
         ) ) goto cleanup;
    if ( xwork_example_check(
             xwork_control_plane_get_task_summary(
                 pRecoveredPlane,
                 "remote-process-inflight",
                 &tTaskSummary
             ),
             "query recovered orphaned task"
         ) ) goto cleanup;
    if ( tTaskSummary.eState != XWORK_REMOTE_TASK_ORPHANED ) {
        fprintf(stderr, "in-flight recovered task was not orphaned\n");
        goto cleanup;
    }
    xwork_remote_task_summary_reset(&tTaskSummary);

    if ( xwork_example_check(
             xwork_control_plane_execute_next_local(
                 pRecoveredPlane,
                 "local-process-worker",
                 &tAssignment
             ),
             "execute queued task after recovery"
         ) ) goto cleanup;
    printf("recovered plane executed queued task %s\n", tAssignment.sTaskId);
    iExit = 0;

cleanup:
    xwork_remote_task_assignment_reset(&tAssignment);
    xwork_remote_task_summary_reset(&tTaskSummary);
    xwork_control_plane_snapshot_reset(&tPlaneSnapshot);
    if ( pRecoveredPlane ) {
        xwork_control_plane_destroy(pRecoveredPlane);
    }
    if ( pPlane ) {
        xwork_control_plane_destroy(pPlane);
    }
    if ( pRuntime ) {
        xwork_runtime_destroy(pRuntime);
    }
    xwork_local_host_reset(&tHost);
    xwork_file_persistence_reset(&tStore);
    return iExit;
}
