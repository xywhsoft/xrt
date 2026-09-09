#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#define putenv _putenv
#endif

static void smoke_set_env(const char *sName, const char *sValue)
{
    char aBuf[1024];

    if ( sName == NULL || sValue == NULL ) {
        return;
    }

    snprintf(aBuf, sizeof(aBuf), "%s=%s", sName, sValue);
    if ( putenv(aBuf) != 0 ) {
        fprintf(stderr, "failed to set env: %s\n", sName);
        exit(1);
    }
}

static char *smoke_read_file(const char *sPath)
{
    FILE *pFile;
    long iSize;
    size_t iRead;
    char *sData;

    pFile = fopen(sPath, "rb");
    if ( pFile == NULL ) {
        return NULL;
    }
    if ( fseek(pFile, 0, SEEK_END) != 0 ) {
        fclose(pFile);
        return NULL;
    }
    iSize = ftell(pFile);
    if ( iSize < 0 ) {
        fclose(pFile);
        return NULL;
    }
    if ( fseek(pFile, 0, SEEK_SET) != 0 ) {
        fclose(pFile);
        return NULL;
    }

    sData = (char *)calloc((size_t)iSize + 1u, sizeof(char));
    if ( sData == NULL ) {
        fclose(pFile);
        return NULL;
    }

    iRead = fread(sData, 1u, (size_t)iSize, pFile);
    fclose(pFile);
    if ( iRead != (size_t)iSize ) {
        free(sData);
        return NULL;
    }

    sData[iRead] = '\0';
    return sData;
}

int main(void)
{
    const char *sOutputDir = "D:\\Git\\xllm\\build\\smoke_real_provider_probe_matrix_provider_tool_skip";
    const char *sReportPath = "D:\\Git\\xllm\\build\\smoke_real_provider_probe_matrix_provider_tool_skip\\real_provider_probe_report.json";
    char *sReport = NULL;
    int iRc;

    smoke_set_env("XLLM_REAL_ADAPTER", "demo_adapter");
    smoke_set_env("XLLM_REAL_BASE_URL", "http://127.0.0.1:9");
    smoke_set_env("XLLM_REAL_MODEL", "demo-model");
    smoke_set_env("XLLM_REAL_API_KEY", "demo-key");
    smoke_set_env("XLLM_REAL_AUTH_KIND", "bearer");
    smoke_set_env("XLLM_REAL_CASE_FILTER", "provider_tool");
    smoke_set_env("XLLM_REAL_RUN_PROVIDER_TOOL_CASES", "1");
    smoke_set_env("XLLM_REAL_VERBOSE_FAILURE", "0");
    smoke_set_env("XLLM_REAL_OUTPUT_DIR", sOutputDir);

    iRc = system("powershell -NoProfile -ExecutionPolicy Bypass -File .\\run_real_provider_probe_matrix.ps1 > NUL 2>&1");
    if ( iRc != 0 ) {
        fprintf(stderr, "matrix runner should succeed for skipped provider_tool case, rc=%d\n", iRc);
        return 1;
    }

    sReport = smoke_read_file(sReportPath);
    if ( sReport == NULL ) {
        fprintf(stderr, "failed to read report: %s\n", sReportPath);
        return 1;
    }

    if ( strstr(sReport, "\"failed_count\":  0") == NULL ) {
        fprintf(stderr, "expected failed_count=0 in report\n");
        free(sReport);
        return 1;
    }
    if ( strstr(sReport, "\"skipped_count\":  1") == NULL ) {
        fprintf(stderr, "expected skipped_count=1 in report\n");
        free(sReport);
        return 1;
    }
    if ( strstr(sReport, "\"name\":  \"provider_tool\"") == NULL ) {
        fprintf(stderr, "expected provider_tool skipped result in report\n");
        free(sReport);
        return 1;
    }
    if ( strstr(sReport, "\"reason\":  \"provider_tool case is only supported for openai_compat, anthropic_native, and ollama_native\"") == NULL ) {
        fprintf(stderr, "expected provider_tool skip reason in report\n");
        free(sReport);
        return 1;
    }

    free(sReport);
    puts("smoke_real_provider_probe_matrix_provider_tool_skip ok");
    return 0;
}
