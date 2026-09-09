#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define main smoke_real_provider_probe_entry
#include "real_provider_probe.c"
#undef main

static int smoke_anthropic_json_schema_unsupported_setenv_cstr(const char *sName, const char *sValue)
{
#if defined(_WIN32) || defined(_WIN64)
    return _putenv_s(sName, sValue ? sValue : "");
#else
    if ( sValue == NULL || sValue[0] == '\0' ) {
        return unsetenv(sName);
    }
    return setenv(sName, sValue, 1);
#endif
}

static int smoke_anthropic_json_schema_unsupported_file_contains(const char *sPath, const char *sNeedle)
{
    FILE *pFile = fopen(sPath, "rb");
    long iSize;
    char *sBuffer;
    int iFound;

    if ( !pFile ) {
        return 0;
    }
    if ( fseek(pFile, 0, SEEK_END) != 0 ) {
        fclose(pFile);
        return 0;
    }
    iSize = ftell(pFile);
    if ( iSize < 0 || fseek(pFile, 0, SEEK_SET) != 0 ) {
        fclose(pFile);
        return 0;
    }
    sBuffer = (char *)calloc((size_t)iSize + 1u, 1u);
    if ( !sBuffer ) {
        fclose(pFile);
        return 0;
    }
    if ( iSize > 0 ) {
        (void)fread(sBuffer, 1u, (size_t)iSize, pFile);
    }
    fclose(pFile);
    iFound = (strstr(sBuffer, sNeedle) != NULL) ? 1 : 0;
    free(sBuffer);
    return iFound;
}

int main(void)
{
    const char *sSummaryPath = "build\\probe_anthropic_json_schema_unsupported.summary.json";
    const char *sSchema =
        "{\"type\":\"object\",\"properties\":{\"ok\":{\"type\":\"boolean\"}},"
        "\"required\":[\"ok\"],\"additionalProperties\":false}";
    int iProbeExitCode = 0;

    remove(sSummaryPath);
    if ( smoke_anthropic_json_schema_unsupported_setenv_cstr("XLLM_REAL_ADAPTER", "anthropic_native") != 0 ||
         smoke_anthropic_json_schema_unsupported_setenv_cstr("ANTHROPIC_BASE_URL", "http://127.0.0.1:1/v1") != 0 ||
         smoke_anthropic_json_schema_unsupported_setenv_cstr("ANTHROPIC_MODEL", "anthropic-test-json-schema-unsupported") != 0 ||
         smoke_anthropic_json_schema_unsupported_setenv_cstr("ANTHROPIC_API_KEY", "alias-anthropic-json-schema-unsupported-key") != 0 ||
         smoke_anthropic_json_schema_unsupported_setenv_cstr("XLLM_REAL_RESPONSE_FORMAT", "json_schema") != 0 ||
         smoke_anthropic_json_schema_unsupported_setenv_cstr("XLLM_REAL_JSON_SCHEMA", sSchema) != 0 ||
         smoke_anthropic_json_schema_unsupported_setenv_cstr("XLLM_REAL_PROMPT", "return a tiny structured response") != 0 ||
         smoke_anthropic_json_schema_unsupported_setenv_cstr("XLLM_REAL_SUMMARY_PATH", sSummaryPath) != 0 ) {
        fprintf(stderr, "set environment variables failed\n");
        return 1;
    }

    iProbeExitCode = smoke_real_provider_probe_entry();
    if ( iProbeExitCode == 0 ) {
        fprintf(stderr, "probe unexpectedly succeeded\n");
        return 2;
    }
    if ( !smoke_anthropic_json_schema_unsupported_file_contains(sSummaryPath, "\"error_code\":\"unsupported_capability\"") ||
         !smoke_anthropic_json_schema_unsupported_file_contains(sSummaryPath, "anthropic-native adapter currently supports structured output only in best-effort mode") ) {
        fprintf(stderr, "unexpected summary content for anthropic json_schema unsupported case\n");
        return 3;
    }

    printf("smoke_real_provider_probe_anthropic_json_schema_unsupported ok\n");
    return 0;
}
