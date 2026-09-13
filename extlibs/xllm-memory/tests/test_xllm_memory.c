#include "../../xllm/xllm.c"
#include "../xllm-memory.c"

static int g_iMemoryFailures = 0;

#define MEMORY_CHECK(expr, name) do { \
    bool xllm_memory_ok__ = !!(expr); \
    printf("  %-62s %s\n", (name), xllm_memory_ok__ ? "PASS" : "FAIL"); \
    if ( !xllm_memory_ok__ ) ++g_iMemoryFailures; \
} while (0)

static bool put_record(xllm_memory* pMemory, xllm_memory_record_input* pInput,
    xllm_memory_receipt* pReceipt, const char* sId, xllm_memory_scope eScope,
    xllm_memory_kind eKind, xllm_memory_trust eTrust,
    xllm_memory_sensitivity eSensitivity, const char* sSource,
    const char* sText, int64_t iExpiresAt)
{
    xllm_error tError;
    xllmMemoryRecordInputInit(pInput);
    pInput->sRecordId = sId;
    pInput->eScope = eScope;
    pInput->eKind = eKind;
    pInput->eTrust = eTrust;
    pInput->eSensitivity = eSensitivity;
    pInput->sTitle = sId;
    pInput->sSourceUri = sSource;
    pInput->sText = sText;
    pInput->sActor = "test-user";
    pInput->sReason = "explicit test approval";
    pInput->iExpiresAtUnix = iExpiresAt;
    return xllmMemoryPut(pMemory, pInput, pReceipt, &tError);
}

static void test_auditable_memory(void)
{
    static const char sStorePath[] = "build/memory_audit_test.json";
    xllm_memory_config tConfig;
    xllm_memory_config tWrongConfig;
    xllm_memory* pMemory;
    xllm_memory* pReloaded;
    xllm_memory* pWrong;
    xllm_memory_record_input tInput;
    xllm_memory_receipt tReceipt;
    xllm_memory_stats tStats;
    xllm_memory_search_options tSearch;
    xllm_memory_search_result tResult;
    xllm_error tError;
    char* sContext = NULL;
    uint64_t uFirstHash;
    int64_t iNow = (int64_t)xrtTimeUnix(xrtNow());

    (void)xrtFileDelete((str)sStorePath);
    xllmMemoryConfigInit(&tConfig);
    tConfig.sPath = sStorePath;
    tConfig.sNamespace = "project-alpha";
    pMemory = xllmMemoryOpen(&tConfig, &tError);
    MEMORY_CHECK(pMemory != NULL && xrtFileExists((str)sStorePath),
        "new namespaced memory store persists an empty snapshot");
    MEMORY_CHECK(pMemory && strcmp(xllmMemoryNamespace(pMemory), "project-alpha") == 0 &&
        strcmp(xllmMemoryPath(pMemory), sStorePath) == 0,
        "memory store exposes namespace and durable path");
    MEMORY_CHECK(pMemory && xllmMemoryGetStats(pMemory, iNow, &tStats) &&
        tStats.uStoreRevision == 0u && tStats.uRecordCount == 0u,
        "new memory store starts at revision zero");
    if ( !pMemory ) return;

    MEMORY_CHECK(put_record(pMemory, &tInput, &tReceipt, "fact:c-stack",
        XLLM_MEMORY_SCOPE_MEMORY, XLLM_MEMORY_KIND_FACT,
        XLLM_MEMORY_TRUST_USER_APPROVED, XLLM_MEMORY_SENSITIVITY_INTERNAL,
        "user://conversation/42", "The project uses a pure C technology stack.", 0),
        "explicit approved fact is persisted");
    MEMORY_CHECK(tReceipt.eAction == XLLM_MEMORY_ACTION_CREATED &&
        tReceipt.uStoreRevision == 1u && tReceipt.uRecordRevision == 1u &&
        tReceipt.uContentHash != 0u && strcmp(tReceipt.sRecordId, "fact:c-stack") == 0,
        "create receipt exposes record, revision, and content fingerprint");
    uFirstHash = tReceipt.uContentHash;

    MEMORY_CHECK(!xllmMemoryPut(pMemory, &tInput, &tReceipt, &tError) &&
        tError.eCode == XLLM_ERROR_INVALID_ARGUMENT,
        "replacement is rejected unless caller opts in explicitly");
    tInput.bReplaceExisting = true;
    MEMORY_CHECK(xllmMemoryPut(pMemory, &tInput, &tReceipt, &tError) &&
        tReceipt.eAction == XLLM_MEMORY_ACTION_UNCHANGED &&
        tReceipt.uStoreRevision == 1u && tReceipt.uRecordRevision == 1u,
        "identical replacement is an auditable no-op");
    tInput.sText = "The project uses a pure C technology stack and keeps module boundaries explicit.";
    MEMORY_CHECK(xllmMemoryPut(pMemory, &tInput, &tReceipt, &tError) &&
        tReceipt.eAction == XLLM_MEMORY_ACTION_REPLACED &&
        tReceipt.uStoreRevision == 2u && tReceipt.uRecordRevision == 2u &&
        tReceipt.uPreviousContentHash == uFirstHash && tReceipt.uContentHash != uFirstHash,
        "replacement receipt links previous and new fingerprints");

    MEMORY_CHECK(put_record(pMemory, &tInput, &tReceipt, "knowledge:http",
        XLLM_MEMORY_SCOPE_KNOWLEDGE, XLLM_MEMORY_KIND_KNOWLEDGE,
        XLLM_MEMORY_TRUST_LOCAL, XLLM_MEMORY_SENSITIVITY_PUBLIC,
        "workspace://docs/http.md",
        "The xrt HTTP client currently targets HTTP/1.1. 会话压缩必须保留工具结果。", 0),
        "local project knowledge is persisted with source URI");
    MEMORY_CHECK(put_record(pMemory, &tInput, &tReceipt, "preference:secret",
        XLLM_MEMORY_SCOPE_MEMORY, XLLM_MEMORY_KIND_PREFERENCE,
        XLLM_MEMORY_TRUST_USER_APPROVED, XLLM_MEMORY_SENSITIVITY_SECRET,
        "user://preference/private", "Secret preference must require an explicit sensitivity override.", 0),
        "secret record is stored with an explicit sensitivity label");
    MEMORY_CHECK(put_record(pMemory, &tInput, &tReceipt, "task:expired",
        XLLM_MEMORY_SCOPE_MEMORY, XLLM_MEMORY_KIND_TASK,
        XLLM_MEMORY_TRUST_LOCAL, XLLM_MEMORY_SENSITIVITY_INTERNAL,
        "task://expired", "Expired task must not enter default retrieval.", iNow - 1),
        "expired task is retained for audit");
    MEMORY_CHECK(xllmMemoryGetStats(pMemory, iNow, &tStats) &&
        tStats.uStoreRevision == 5u && tStats.uRecordCount == 4u &&
        tStats.uMemoryRecordCount == 3u && tStats.uKnowledgeRecordCount == 1u &&
        tStats.uExpiredRecordCount == 1u && tStats.uSensitiveRecordCount == 1u,
        "stats separate scope, expiry, and sensitivity counts");

    xllmMemorySearchOptionsInit(&tSearch);
    tSearch.sQuery = "HTTP client";
    memset(&tResult, 0, sizeof(tResult));
    MEMORY_CHECK(xllmMemorySearch(pMemory, &tSearch, &tResult, &tError) &&
        tResult.iHitCount == 1u && strcmp(tResult.pHits[0].sRecordId, "knowledge:http") == 0 &&
        strcmp(tResult.pHits[0].sSourceUri, "workspace://docs/http.md") == 0 &&
        tResult.pHits[0].eTrust == XLLM_MEMORY_TRUST_LOCAL,
        "deterministic lexical search returns provenance and trust");
    MEMORY_CHECK(xllmMemoryRenderContext(pMemory, &tResult, 2048u, &sContext, &tError) &&
        strstr(sContext, "untrusted reference material") &&
        strstr(sContext, "workspace://docs/http.md") && strstr(sContext, "HTTP/1.1") &&
        !strstr(sContext, "Secret preference") && strlen(sContext) <= 2048u,
        "bounded context rendering labels retrieved records as untrusted");
    xllmMemoryFree(sContext);
    sContext = NULL;
    xllmMemorySearchResultUnit(&tResult);

    xllmMemorySearchOptionsInit(&tSearch);
    tSearch.sQuery = "definitely-unmatched-query";
    MEMORY_CHECK(xllmMemorySearch(pMemory, &tSearch, &tResult, &tError) && tResult.iHitCount == 0u,
        "priority and provenance cannot create a result without lexical evidence");
    xllmMemorySearchResultUnit(&tResult);

    xllmMemorySearchOptionsInit(&tSearch);
    tSearch.sQuery = "会话压缩";
    MEMORY_CHECK(xllmMemorySearch(pMemory, &tSearch, &tResult, &tError) &&
        tResult.iHitCount == 1u && strcmp(tResult.pHits[0].sRecordId, "knowledge:http") == 0,
        "exact UTF-8 phrase retrieval works without an embedding dependency");
    xllmMemorySearchResultUnit(&tResult);

    xllmMemorySearchOptionsInit(&tSearch);
    tSearch.sQuery = "Secret preference";
    MEMORY_CHECK(xllmMemorySearch(pMemory, &tSearch, &tResult, &tError) && tResult.iHitCount == 0u,
        "default search excludes sensitive and secret records");
    xllmMemorySearchResultUnit(&tResult);
    tSearch.eMaximumSensitivity = XLLM_MEMORY_SENSITIVITY_SECRET;
    MEMORY_CHECK(xllmMemorySearch(pMemory, &tSearch, &tResult, &tError) &&
        tResult.iHitCount == 1u && tResult.pHits[0].eSensitivity == XLLM_MEMORY_SENSITIVITY_SECRET,
        "explicit sensitivity override can retrieve secret records");
    xllmMemorySearchResultUnit(&tResult);

    xllmMemorySearchOptionsInit(&tSearch);
    tSearch.sQuery = NULL;
    tSearch.uMaxHits = 10u;
    MEMORY_CHECK(xllmMemorySearch(pMemory, &tSearch, &tResult, &tError) && tResult.iHitCount == 2u,
        "empty-query audit listing respects expiry and sensitivity defaults");
    xllmMemorySearchResultUnit(&tResult);

    xllmMemoryClose(pMemory);
    pMemory = NULL;
    tConfig.bCreateIfMissing = false;
    pReloaded = xllmMemoryOpen(&tConfig, &tError);
    MEMORY_CHECK(pReloaded && xllmMemoryGetStats(pReloaded, iNow, &tStats) &&
        tStats.uStoreRevision == 5u && tStats.uRecordCount == 4u,
        "store revision and records survive process-style reopen");
    MEMORY_CHECK(pReloaded && xllmMemoryRemove(pReloaded, "fact:c-stack", &tReceipt, &tError) &&
        tReceipt.eAction == XLLM_MEMORY_ACTION_REMOVED && tReceipt.uStoreRevision == 6u &&
        tReceipt.uPreviousContentHash != 0u && tReceipt.uContentHash == 0u,
        "remove returns an auditable tombstone receipt");
    xllmMemoryClose(pReloaded);

    tWrongConfig = tConfig;
    tWrongConfig.sNamespace = "other-project";
    pWrong = xllmMemoryOpen(&tWrongConfig, &tError);
    MEMORY_CHECK(pWrong == NULL && tError.eCode == XLLM_ERROR_PARSE,
        "namespace mismatch refuses cross-project store reuse");
    xllmMemoryClose(pWrong);

    {
        char* sJson;
        size_t iJsonLen = 0u;
        char* sHash;
        sJson = (char*)xrtFileReadAll(sStorePath, &iJsonLen);
        sHash = sJson ? strstr(sJson, "\"content_hash\":\"") : NULL;
        MEMORY_CHECK(sHash != NULL, "persisted store contains content fingerprints");
        if ( sHash ) {
            sHash += strlen("\"content_hash\":\"");
            sHash[0] = sHash[0] == '0' ? '1' : '0';
            MEMORY_CHECK(xrtFileWriteAtomic(sStorePath,
                (xbytesview){ (const uint8*)sJson, iJsonLen }),
                "test tampers one persisted fingerprint atomically");
            pWrong = xllmMemoryOpen(&tConfig, &tError);
            MEMORY_CHECK(pWrong == NULL && tError.eCode == XLLM_ERROR_PARSE,
                "content fingerprint mismatch is detected on reopen");
            xllmMemoryClose(pWrong);
        }
        xrtFree(sJson);
    }
    (void)xrtFileDelete((str)sStorePath);
}

int main(void)
{
    printf("xllm-memory v2 tests\n");
    test_auditable_memory();
    printf("xllm-memory v2: %s (%d failures)\n",
        g_iMemoryFailures ? "FAIL" : "PASS", g_iMemoryFailures);
    return g_iMemoryFailures ? 1 : 0;
}
