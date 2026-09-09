#include <stdio.h>
#include <string.h>

#include "xllm.h"

static int expect_name(const char *sActual, const char *sExpected)
{
    if ( !sActual || strcmp(sActual, sExpected) != 0 ) {
        fprintf(stderr, "expected '%s', got '%s'\n", sExpected, sActual ? sActual : "(null)");
        return 1;
    }
    return 0;
}

int main(void)
{
    if ( expect_name(xllm_log_level_name(XLLM_LOG_WARN), "warn") != 0 ) {
        return 1;
    }
    if ( expect_name(xllm_log_level_name((xllm_log_level)999), "unknown") != 0 ) {
        return 1;
    }
    if ( expect_name(xllm_trace_kind_name(XLLM_TRACE_TOOL_LOOP), "tool_loop") != 0 ) {
        return 1;
    }
    if ( expect_name(xllm_trace_kind_name((xllm_trace_kind)999), "unknown") != 0 ) {
        return 1;
    }
    if ( expect_name(xllm_log_event_name(XLLM_LOG_EVENT_PROVIDER_REQUEST_START), "provider.request_start") != 0 ) {
        return 1;
    }
    if ( expect_name(xllm_log_event_name(XLLM_LOG_EVENT_MEMORY_HEALTH_CHECK), "memory.health_check") != 0 ) {
        return 1;
    }
    if ( expect_name(xllm_log_event_name((xllm_log_event)999), "unknown") != 0 ) {
        return 1;
    }

    printf("smoke_log_event_taxonomy ok\n");
    return 0;
}
