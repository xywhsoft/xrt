#include <assert.h>
#include <string.h>

#define XRT_IMPLEMENTATION
#include "../lib/xrt.h"
#define XLLM_SESSION_IMPLEMENTATION
#include "../lib/xllm-session.h"
#define XLLM_MEMORY_IMPLEMENTATION
#include "../lib/xllm-memory.h"
#define XWORK_IMPLEMENTATION
#include "xwork.h"

int main(void)
{
    xwork_runtime *pRuntime = NULL;
    xwork_runtime_options tOptions;

    assert(strcmp(xwork_version(), "0.1.0") == 0);

    xwork_runtime_options_init(&tOptions);
    assert(xwork_runtime_create(&tOptions, &pRuntime) == XWORK_OK);
    assert(pRuntime != NULL);
    assert(xwork_runtime_get_workspace_count(pRuntime) == 0u);
    assert(xwork_runtime_get_tool_count(pRuntime) == 0u);
    assert(xwork_runtime_get_run_count(pRuntime) == 0u);

    xwork_runtime_destroy(pRuntime);
    return 0;
}
