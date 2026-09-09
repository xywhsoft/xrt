#include <assert.h>
#include <stdio.h>
#include <string.h>

#define XRT_IMPLEMENTATION
#include "../lib/xrt.h"
#define XLLM_SESSION_IMPLEMENTATION
#include "../lib/xllm-session.h"
#define XLLM_MEMORY_IMPLEMENTATION
#include "../lib/xllm-memory.h"
#include "../xwork.c"

int main(void)
{
    xwork_runtime *pRuntime = NULL;
    xwork_runtime_options tRuntimeOptions;
    xwork_host_services tHostServices;
    xwork_local_host_options tHostOptions;
    xwork_local_host tHost;
    xwork_tool_result tResult;

    xwork_local_host_options_init(&tHostOptions);
    xwork_local_host_init(&tHost);
    xwork_host_services_init(&tHostServices);
    xwork_tool_result_init(&tResult);

    tHostOptions.sDefaultWorkingDirectory = ".";
    tHostOptions.bEnforceFilesystemRoot = true;
    tHostOptions.iMaxProcessOutputBytes = 32768u;
    assert(xwork_local_host_configure_services(&tHost, &tHostOptions, &tHostServices) == XWORK_OK);

    xwork_runtime_options_init(&tRuntimeOptions);
    tRuntimeOptions.pHostServices = &tHostServices;
    assert(xwork_runtime_create(&tRuntimeOptions, &pRuntime) == XWORK_OK);

    assert(
        xwork_runtime_invoke_host_service(
            pRuntime,
            XWORK_HOST_FILESYSTEM,
            XWORK_HOST_FILESYSTEM_WRITE_TEXT,
            "{\"path\":\"tests/xwork_host_smoke_tmp.txt\",\"text\":\"host smoke\"}",
            &tResult
        ) == XWORK_OK
    );
    assert(tResult.sOutputText != NULL);
    assert(strstr(tResult.sOutputText, "\"ok\":true") != NULL);

    assert(
        xwork_runtime_invoke_host_service(
            pRuntime,
            XWORK_HOST_FILESYSTEM,
            XWORK_HOST_FILESYSTEM_READ_TEXT,
            "{\"path\":\"tests/xwork_host_smoke_tmp.txt\"}",
            &tResult
        ) == XWORK_OK
    );
    assert(tResult.sOutputText != NULL);
    assert(strstr(tResult.sOutputText, "host smoke") != NULL);

    assert(
        xwork_runtime_invoke_host_service(
            pRuntime,
            XWORK_HOST_FILESYSTEM,
            XWORK_HOST_FILESYSTEM_STAT,
            "{\"path\":\"tests/xwork_host_smoke_tmp.txt\"}",
            &tResult
        ) == XWORK_OK
    );
    assert(tResult.sOutputText != NULL);
    assert(strstr(tResult.sOutputText, "\"exists\":true") != NULL);

#ifdef _WIN32
    assert(
        xwork_runtime_invoke_host_service(
            pRuntime,
            XWORK_HOST_PROCESS,
            XWORK_HOST_PROCESS_EXEC,
            "{\"command\":\"echo xwork-host-process-smoke\",\"max_output_bytes\":4096}",
            &tResult
        ) == XWORK_OK
    );
#else
    assert(
        xwork_runtime_invoke_host_service(
            pRuntime,
            XWORK_HOST_PROCESS,
            XWORK_HOST_PROCESS_EXEC,
            "{\"command\":\"printf xwork-host-process-smoke\",\"max_output_bytes\":4096}",
            &tResult
        ) == XWORK_OK
    );
#endif
    assert(tResult.sOutputText != NULL);
    assert(strstr(tResult.sOutputText, "\"ok\":true") != NULL);
    assert(strstr(tResult.sOutputText, "xwork-host-process-smoke") != NULL);

    assert(
        xwork_runtime_invoke_host_service(
            pRuntime,
            XWORK_HOST_FILESYSTEM,
            XWORK_HOST_FILESYSTEM_DELETE,
            "{\"path\":\"tests/xwork_host_smoke_tmp.txt\",\"dry_run\":false}",
            &tResult
        ) == XWORK_OK
    );
    assert(tResult.sOutputText != NULL);
    assert(strstr(tResult.sOutputText, "\"ok\":true") != NULL);

    xwork_runtime_destroy(pRuntime);
    xwork_local_host_reset(&tHost);
    puts("xwork host smoke passed");
    return 0;
}
