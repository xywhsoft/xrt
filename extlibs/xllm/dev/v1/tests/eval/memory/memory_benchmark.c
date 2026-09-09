#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <windows.h>
#include <sys/stat.h>
#else
#include <sys/stat.h>
#include <time.h>
#endif

#include "xllm-memory.h"

#define BENCH_RECORD_COUNT 160u
#define BENCH_QUERY_COUNT 40u

typedef struct {
    const char *sName;
    const char *sNeedle;
    const char *sText;
} bench_topic;

typedef struct {
    double fIngestTotalMs;
    double fIngestAvgMs;
    double fSearchTotalMs;
    double fSearchAvgMs;
    double fSearchP50Ms;
    double fSearchP95Ms;
    double fReloadMs;
    size_t iDbBytes;
    size_t iRecordCount;
    size_t iChunkCount;
    size_t iReloadRecordCount;
    size_t iReloadChunkCount;
    size_t iTotalHits;
} bench_metrics;

static const bench_topic gTopics[] = {
    {
        "workspace_security",
        "secret scanner credential skip",
        "workspace security excludes env files credential stores private keys token dumps and large binary artifacts before indexing"
    },
    {
        "watcher_worker",
        "watcher debounce queue",
        "watcher worker drains queued file events applies debounce windows and syncs changed workspace files into memory"
    },
    {
        "session_summary",
        "summary compact budget",
        "session summary compaction keeps durable facts active tool state user preferences and unresolved tasks under token budget"
    },
    {
        "provider_probe",
        "provider probe capability",
        "provider probe reports stable adapter capability status timeout retry upstream status and unsupported feature baselines"
    },
    {
        "retrieval_debug",
        "retrieval debug ranks",
        "retrieval debug dump exposes query terms document frequency lexical vector rrf ranks and final included hits"
    },
    {
        "task_memory",
        "task memory lifecycle",
        "task memory records open done canceled owner deadline source conversation and source turn lifecycle metadata"
    },
    {
        "context_apply",
        "context block citation",
        "context apply injects retrieved memory with source uri chunk id byte range title and bounded context characters"
    },
    {
        "sqlite_policy",
        "sqlite wal busy timeout",
        "sqlite memory storage uses wal mode synchronous normal busy timeout begin immediate and reopen consistency checks"
    }
};

static double bench_now_ms(void)
{
#if defined(_WIN32) || defined(_WIN64)
    LARGE_INTEGER tFreq;
    LARGE_INTEGER tNow;
    QueryPerformanceFrequency(&tFreq);
    QueryPerformanceCounter(&tNow);
    if ( tFreq.QuadPart == 0 ) {
        return 0.0;
    }
    return ((double)tNow.QuadPart * 1000.0) / (double)tFreq.QuadPart;
#else
    struct timespec tNow;
    if ( clock_gettime(CLOCK_MONOTONIC, &tNow) != 0 ) {
        return 0.0;
    }
    return ((double)tNow.tv_sec * 1000.0) + ((double)tNow.tv_nsec / 1000000.0);
#endif
}

static int bench_compare_double(const void *pLeft, const void *pRight)
{
    double fLeft = *(const double *)pLeft;
    double fRight = *(const double *)pRight;
    if ( fLeft < fRight ) {
        return -1;
    }
    if ( fLeft > fRight ) {
        return 1;
    }
    return 0;
}

static size_t bench_file_size(const char *sPath)
{
#if defined(_WIN32) || defined(_WIN64)
    struct __stat64 tStat;
    if ( !sPath || _stat64(sPath, &tStat) != 0 ) {
        return 0u;
    }
    return (size_t)tStat.st_size;
#else
    struct stat tStat;
    if ( !sPath || stat(sPath, &tStat) != 0 ) {
        return 0u;
    }
    return (size_t)tStat.st_size;
#endif
}

static size_t bench_sqlite_bytes(const char *sDbPath)
{
    char sWalPath[1024];
    char sShmPath[1024];

    if ( !sDbPath || !sDbPath[0] ) {
        return 0u;
    }
    snprintf(sWalPath, sizeof(sWalPath), "%s-wal", sDbPath);
    snprintf(sShmPath, sizeof(sShmPath), "%s-shm", sDbPath);
    return bench_file_size(sDbPath) + bench_file_size(sWalPath) + bench_file_size(sShmPath);
}

static int bench_ingest_record(
    xllm_memory *pMemory,
    uint32 uIndex,
    xllm_error *pError
)
{
    const bench_topic *pTopic = &gTopics[uIndex % (sizeof(gTopics) / sizeof(gTopics[0]))];
    xllm_memory_ingest_options tOptions;
    char sRecordId[64];
    char sTitle[128];
    char sSourceUri[160];
    char sText[1024];

    snprintf(sRecordId, sizeof(sRecordId), "bench.record.%03u", (unsigned)uIndex);
    snprintf(sTitle, sizeof(sTitle), "Benchmark %s %03u", pTopic->sName, (unsigned)uIndex);
    snprintf(sSourceUri, sizeof(sSourceUri), "bench://memory/%s/%03u", pTopic->sName, (unsigned)uIndex);
    snprintf(
        sText,
        sizeof(sText),
        "%s. topic=%s record=%03u stable marker benchmark memory ingest search reload database size latency. %s.",
        pTopic->sText,
        pTopic->sName,
        (unsigned)uIndex,
        pTopic->sNeedle
    );

    xllm_memory_ingest_options_init(&tOptions);
    tOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tOptions.sRecordId = sRecordId;
    tOptions.sTitle = sTitle;
    tOptions.sSourceUri = sSourceUri;
    tOptions.sText = sText;
    tOptions.bReplaceExisting = true;
    tOptions.uChunkChars = 512u;
    tOptions.uChunkOverlapChars = 64u;
    return xllm_memory_ingest_text(pMemory, &tOptions, pError);
}

static int bench_run_query(
    xllm_memory *pMemory,
    uint32 uIndex,
    size_t *piHits,
    double *pfLatencyMs,
    xllm_error *pError
)
{
    const bench_topic *pTopic = &gTopics[uIndex % (sizeof(gTopics) / sizeof(gTopics[0]))];
    xllm_memory_search_options tOptions;
    xllm_memory_search_result tResult;
    double fStart;
    double fEnd;
    int iStatus;

    memset(&tResult, 0, sizeof(tResult));
    xllm_memory_search_options_init(&tOptions);
    tOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tOptions.sQuery = pTopic->sNeedle;
    tOptions.uMaxHits = 5u;
    tOptions.uMaxCharsPerHit = 1024u;

    fStart = bench_now_ms();
    iStatus = xllm_memory_search(pMemory, &tOptions, &tResult, pError);
    fEnd = bench_now_ms();
    if ( iStatus == XRT_NET_OK ) {
        *piHits = tResult.iHitCount;
        *pfLatencyMs = fEnd - fStart;
    }
    xllm_memory_search_result_reset(&tResult);
    return iStatus;
}

static int bench_write_reports(const char *sOutputDir, const bench_metrics *pMetrics)
{
    char sJsonPath[1024];
    char sTxtPath[1024];
    FILE *pJson;
    FILE *pTxt;

    snprintf(sJsonPath, sizeof(sJsonPath), "%s\\memory_benchmark_report.json", sOutputDir);
    snprintf(sTxtPath, sizeof(sTxtPath), "%s\\memory_benchmark_report.txt", sOutputDir);

    pJson = fopen(sJsonPath, "wb");
    pTxt = fopen(sTxtPath, "wb");
    if ( !pJson || !pTxt ) {
        if ( pJson ) fclose(pJson);
        if ( pTxt ) fclose(pTxt);
        return 1;
    }

    fprintf(pJson, "{\n");
    fprintf(pJson, "  \"dataset\": \"memory_benchmark_v1\",\n");
    fprintf(pJson, "  \"record_count\": %u,\n", (unsigned)BENCH_RECORD_COUNT);
    fprintf(pJson, "  \"query_count\": %u,\n", (unsigned)BENCH_QUERY_COUNT);
    fprintf(pJson, "  \"metrics\": {\n");
    fprintf(pJson, "    \"ingest_total_ms\": %.3f,\n", pMetrics->fIngestTotalMs);
    fprintf(pJson, "    \"ingest_avg_ms\": %.6f,\n", pMetrics->fIngestAvgMs);
    fprintf(pJson, "    \"search_total_ms\": %.3f,\n", pMetrics->fSearchTotalMs);
    fprintf(pJson, "    \"search_avg_ms\": %.6f,\n", pMetrics->fSearchAvgMs);
    fprintf(pJson, "    \"search_p50_ms\": %.6f,\n", pMetrics->fSearchP50Ms);
    fprintf(pJson, "    \"search_p95_ms\": %.6f,\n", pMetrics->fSearchP95Ms);
    fprintf(pJson, "    \"reload_ms\": %.6f,\n", pMetrics->fReloadMs);
    fprintf(pJson, "    \"sqlite_bytes\": %u,\n", (unsigned)pMetrics->iDbBytes);
    fprintf(pJson, "    \"indexed_records\": %u,\n", (unsigned)pMetrics->iRecordCount);
    fprintf(pJson, "    \"indexed_chunks\": %u,\n", (unsigned)pMetrics->iChunkCount);
    fprintf(pJson, "    \"reload_records\": %u,\n", (unsigned)pMetrics->iReloadRecordCount);
    fprintf(pJson, "    \"reload_chunks\": %u,\n", (unsigned)pMetrics->iReloadChunkCount);
    fprintf(pJson, "    \"total_hits\": %u\n", (unsigned)pMetrics->iTotalHits);
    fprintf(pJson, "  }\n");
    fprintf(pJson, "}\n");

    fprintf(pTxt, "dataset: memory_benchmark_v1\n");
    fprintf(pTxt, "record_count: %u\n", (unsigned)BENCH_RECORD_COUNT);
    fprintf(pTxt, "query_count: %u\n", (unsigned)BENCH_QUERY_COUNT);
    fprintf(pTxt, "ingest_total_ms: %.3f\n", pMetrics->fIngestTotalMs);
    fprintf(pTxt, "ingest_avg_ms: %.6f\n", pMetrics->fIngestAvgMs);
    fprintf(pTxt, "search_total_ms: %.3f\n", pMetrics->fSearchTotalMs);
    fprintf(pTxt, "search_avg_ms: %.6f\n", pMetrics->fSearchAvgMs);
    fprintf(pTxt, "search_p50_ms: %.6f\n", pMetrics->fSearchP50Ms);
    fprintf(pTxt, "search_p95_ms: %.6f\n", pMetrics->fSearchP95Ms);
    fprintf(pTxt, "reload_ms: %.6f\n", pMetrics->fReloadMs);
    fprintf(pTxt, "sqlite_bytes: %u\n", (unsigned)pMetrics->iDbBytes);
    fprintf(pTxt, "indexed_records: %u\n", (unsigned)pMetrics->iRecordCount);
    fprintf(pTxt, "indexed_chunks: %u\n", (unsigned)pMetrics->iChunkCount);
    fprintf(pTxt, "reload_records: %u\n", (unsigned)pMetrics->iReloadRecordCount);
    fprintf(pTxt, "reload_chunks: %u\n", (unsigned)pMetrics->iReloadChunkCount);
    fprintf(pTxt, "total_hits: %u\n", (unsigned)pMetrics->iTotalHits);

    fclose(pJson);
    fclose(pTxt);
    printf("memory benchmark report json: %s\n", sJsonPath);
    printf("memory benchmark report txt:  %s\n", sTxtPath);
    return 0;
}

int main(int argc, char **argv)
{
    const char *sOutputDir = (argc > 1 && argv[1] && argv[1][0]) ? argv[1] : "build\\memory_benchmark";
    char sDbPath[1024];
    xllm_runtime *pRuntime = NULL;
    xllm_memory *pMemory = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_error tError;
    bench_metrics tMetrics;
    double pSearchLatencies[BENCH_QUERY_COUNT];
    double fStart;
    double fEnd;
    uint32 i;
    int iStatus;
    int iRc = 1;

    memset(&tMetrics, 0, sizeof(tMetrics));
    memset(pSearchLatencies, 0, sizeof(pSearchLatencies));
    xllm_memory_options_init(&tMemoryOptions);
    xllm_error_init(&tError);
    snprintf(sDbPath, sizeof(sDbPath), "%s\\memory_benchmark.db", sOutputDir);

    iStatus = xllm_runtime_create(NULL, &pRuntime);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "runtime create failed: %d\n", iStatus);
        goto cleanup;
    }

    tMemoryOptions.sNamespace = "memory-benchmark-v1";
    tMemoryOptions.sSqlitePath = sDbPath;
    tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
    tMemoryOptions.bEnableHybridSearch = false;
    tMemoryOptions.uDefaultChunkChars = 512u;
    tMemoryOptions.uDefaultChunkOverlapChars = 64u;
    tMemoryOptions.uSqliteBusyTimeoutMs = 5000u;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %s\n", tError.sMessage ? tError.sMessage : "(no error)");
        goto cleanup;
    }

    fStart = bench_now_ms();
    for ( i = 0u; i < BENCH_RECORD_COUNT; ++i ) {
        iStatus = bench_ingest_record(pMemory, i, &tError);
        if ( iStatus != XRT_NET_OK ) {
            fprintf(stderr, "ingest %u failed: %s\n", (unsigned)i, tError.sMessage ? tError.sMessage : "(no error)");
            goto cleanup;
        }
    }
    fEnd = bench_now_ms();
    tMetrics.fIngestTotalMs = fEnd - fStart;
    tMetrics.fIngestAvgMs = tMetrics.fIngestTotalMs / (double)BENCH_RECORD_COUNT;
    tMetrics.iRecordCount = xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_KNOWLEDGE);
    tMetrics.iChunkCount = xllm_memory_chunk_count(pMemory, XLLM_MEMORY_SCOPE_KNOWLEDGE);

    fStart = bench_now_ms();
    for ( i = 0u; i < BENCH_QUERY_COUNT; ++i ) {
        size_t iHits = 0u;
        iStatus = bench_run_query(pMemory, i, &iHits, &pSearchLatencies[i], &tError);
        if ( iStatus != XRT_NET_OK ) {
            fprintf(stderr, "query %u failed: %s\n", (unsigned)i, tError.sMessage ? tError.sMessage : "(no error)");
            goto cleanup;
        }
        if ( iHits == 0u ) {
            fprintf(stderr, "query %u returned no hits\n", (unsigned)i);
            goto cleanup;
        }
        tMetrics.iTotalHits += iHits;
    }
    fEnd = bench_now_ms();
    tMetrics.fSearchTotalMs = fEnd - fStart;
    tMetrics.fSearchAvgMs = tMetrics.fSearchTotalMs / (double)BENCH_QUERY_COUNT;
    qsort(pSearchLatencies, BENCH_QUERY_COUNT, sizeof(pSearchLatencies[0]), bench_compare_double);
    tMetrics.fSearchP50Ms = pSearchLatencies[BENCH_QUERY_COUNT / 2u];
    tMetrics.fSearchP95Ms = pSearchLatencies[(BENCH_QUERY_COUNT * 95u) / 100u];

    xllm_memory_destroy(pMemory);
    pMemory = NULL;
    tMetrics.iDbBytes = bench_sqlite_bytes(sDbPath);

    fStart = bench_now_ms();
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    fEnd = bench_now_ms();
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory reload failed: %s\n", tError.sMessage ? tError.sMessage : "(no error)");
        goto cleanup;
    }
    tMetrics.fReloadMs = fEnd - fStart;
    tMetrics.iReloadRecordCount = xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_KNOWLEDGE);
    tMetrics.iReloadChunkCount = xllm_memory_chunk_count(pMemory, XLLM_MEMORY_SCOPE_KNOWLEDGE);
    if ( tMetrics.iReloadRecordCount != tMetrics.iRecordCount ||
         tMetrics.iReloadChunkCount != tMetrics.iChunkCount ) {
        fprintf(stderr, "reload count mismatch\n");
        goto cleanup;
    }

    if ( bench_write_reports(sOutputDir, &tMetrics) != 0 ) {
        fprintf(stderr, "failed to write benchmark reports\n");
        goto cleanup;
    }
    iRc = 0;

cleanup:
    if ( pMemory ) {
        xllm_memory_destroy(pMemory);
    }
    if ( pRuntime ) {
        xllm_runtime_destroy(pRuntime);
    }
    xllm_error_free(&tError);
    return iRc;
}
