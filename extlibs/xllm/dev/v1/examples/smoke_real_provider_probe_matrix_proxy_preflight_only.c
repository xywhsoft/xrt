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
    const char *sOutputDir = "D:\\Git\\xllm\\build\\smoke_real_provider_probe_matrix_proxy_preflight_only";
    const char *sReportPath = "D:\\Git\\xllm\\build\\smoke_real_provider_probe_matrix_proxy_preflight_only\\real_provider_probe_report.json";
    const char *sStableReportPath = "D:\\Git\\xllm\\build\\smoke_real_provider_probe_matrix_proxy_preflight_only\\real_provider_probe_stable_summary.json";
    char *sReport = NULL;
    char *sStableReport = NULL;
    int iRc;

    smoke_set_env("XLLM_REAL_PROXY_PREFLIGHT_ONLY", "1");
    smoke_set_env("XLLM_REAL_PROXY_KIND", "none");
    smoke_set_env("XLLM_REAL_PROXY_HOST", "127.0.0.1");
    smoke_set_env("XLLM_REAL_PROXY_PORT", "1");
    smoke_set_env("XLLM_REAL_OUTPUT_DIR", sOutputDir);

    iRc = system("powershell -NoProfile -ExecutionPolicy Bypass -File .\\tests\\probe\\run_real_provider_probe_matrix.ps1 > NUL 2>&1");
    if ( iRc != 0 ) {
        fprintf(stderr, "proxy-preflight-only runner should succeed, rc=%d\n", iRc);
        return 1;
    }

    sReport = smoke_read_file(sReportPath);
    if ( sReport == NULL ) {
        fprintf(stderr, "failed to read report: %s\n", sReportPath);
        return 1;
    }

    if ( strstr(sReport, "\"phase\":  \"proxy_preflight\"") == NULL ) {
        fprintf(stderr, "expected proxy_preflight phase in report\n");
        free(sReport);
        return 1;
    }
    if ( strstr(sReport, "\"success\":  true") == NULL ) {
        fprintf(stderr, "expected success=true in report\n");
        free(sReport);
        return 1;
    }
    if ( strstr(sReport, "\"case_count\":  0") == NULL ) {
        fprintf(stderr, "expected case_count=0 in report\n");
        free(sReport);
        return 1;
    }
    if ( strstr(sReport, "\"kind\":  \"none\"") == NULL ) {
        fprintf(stderr, "expected proxy kind none in report\n");
        free(sReport);
        return 1;
    }
    if ( strstr(sReport, "\"skipped\":  true") == NULL ) {
        fprintf(stderr, "expected skipped=true in proxy_preflight report\n");
        free(sReport);
        return 1;
    }
    if ( strstr(sReport, "\"message\":  \"proxy explicitly disabled\"") == NULL ) {
        fprintf(stderr, "expected disabled-proxy message in report\n");
        free(sReport);
        return 1;
    }

    free(sReport);
    sStableReport = smoke_read_file(sStableReportPath);
    if ( sStableReport == NULL ) {
        fprintf(stderr, "failed to read stable report: %s\n", sStableReportPath);
        return 1;
    }
    if ( strstr(sStableReport, "\"schema_version\":  1") == NULL ) {
        fprintf(stderr, "expected stable summary schema_version=1\n");
        free(sStableReport);
        return 1;
    }
    if ( strstr(sStableReport, "\"phase\":  \"proxy_preflight\"") == NULL ) {
        fprintf(stderr, "expected proxy_preflight phase in stable summary\n");
        free(sStableReport);
        return 1;
    }
    if ( strstr(sStableReport, "\"success\":  true") == NULL ) {
        fprintf(stderr, "expected success=true in stable summary\n");
        free(sStableReport);
        return 1;
    }

    free(sStableReport);
    puts("smoke_real_provider_probe_matrix_proxy_preflight_only ok");
    return 0;
}
