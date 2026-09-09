#include <stdio.h>
#include <string.h>

#include "xllm-memory.h"

static int require_true(int condition, const char *sMessage)
{
    if ( !condition ) {
        fprintf(stderr, "%s\n", sMessage);
        return 1;
    }
    return 0;
}

static const xllm_memory_record_info *find_record(
    const xllm_memory_record_list_result *pRecords,
    const char *sRecordId
)
{
    size_t i;

    if ( !pRecords || !sRecordId ) {
        return NULL;
    }
    for ( i = 0u; i < pRecords->iRecordCount; ++i ) {
        if ( pRecords->pRecords[i].sRecordId &&
             strcmp(pRecords->pRecords[i].sRecordId, sRecordId) == 0 ) {
            return &pRecords->pRecords[i];
        }
    }
    return NULL;
}

int main(void)
{
    xllm_runtime *pRuntime = NULL;
    xllm_memory *pMemory = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_ingest_fact_options tFactOptions;
    xllm_memory_ingest_preference_options tPreferenceOptions;
    xllm_memory_list_options tListOptions;
    xllm_memory_record_list_result tRecords;
    xllm_memory_search_options tSearchOptions;
    xllm_memory_search_result tSearchResult;
    xllm_error tError;
    const xllm_memory_record_info *pRecord;
    int iStatus;
    int iRc = 1;

    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_ingest_fact_options_init(&tFactOptions);
    xllm_memory_ingest_preference_options_init(&tPreferenceOptions);
    xllm_memory_list_options_init(&tListOptions);
    memset(&tRecords, 0, sizeof(tRecords));
    xllm_memory_search_options_init(&tSearchOptions);
    memset(&tSearchResult, 0, sizeof(tSearchResult));
    xllm_error_init(&tError);

    iStatus = xllm_runtime_create(NULL, &pRuntime);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "runtime create failed: %d\n", iStatus);
        goto cleanup;
    }

    tMemoryOptions.sNamespace = "smoke-memory-fact-preference-type";
    tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
    tMemoryOptions.bEnableHybridSearch = false;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        goto cleanup;
    }

    xllm_memory_ingest_fact_options_init(&tFactOptions);
    tFactOptions.sFactId = "project-goal";
    tFactOptions.sSubject = "xllm";
    tFactOptions.sPredicate = "core_goal";
    tFactOptions.sObject = "agent development library for AI IDE and claw infrastructure";
    tFactOptions.sText = "This is a host-confirmed durable product fact.";
    tFactOptions.sSourceConversationId = "conv-fact";
    tFactOptions.sSourceTurnId = "turn-fact";
    tFactOptions.iPriority = 6;
    tFactOptions.uChunkChars = 4096u;
    iStatus = xllm_memory_ingest_fact(pMemory, &tFactOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "fact ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    xllm_memory_ingest_preference_options_init(&tPreferenceOptions);
    tPreferenceOptions.sPreferenceId = "status-style";
    tPreferenceOptions.sSubject = "user";
    tPreferenceOptions.sKey = "status_update_style";
    tPreferenceOptions.sValue = "concise direct progress with verification results";
    tPreferenceOptions.sText = "User prefers terse engineering updates without fluff.";
    tPreferenceOptions.sSourceConversationId = "conv-pref";
    tPreferenceOptions.sSourceTurnId = "turn-pref";
    tPreferenceOptions.iPriority = 4;
    tPreferenceOptions.uChunkChars = 4096u;
    iStatus = xllm_memory_ingest_preference(pMemory, &tPreferenceOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "preference ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tListOptions.sMetadataKey = "memory_type";
    tListOptions.sMetadataValue = "fact.v1";
    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tRecords, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list fact records failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tRecords.iRecordCount == 1u, "expected one fact record") != 0 ) {
        goto cleanup;
    }
    pRecord = find_record(&tRecords, "fact:project-goal");
    if ( require_true(pRecord != NULL, "missing fact record") != 0 ||
         require_true(strcmp(pRecord->sSourceUri, "fact://project-goal") == 0, "fact source uri mismatch") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(pRecord->tMetadata, (str)"memory_type", 0u), "fact.v1") == 0, "fact memory_type mismatch") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(pRecord->tMetadata, (str)"extraction_policy", 0u), "fact") == 0, "fact extraction policy mismatch") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(pRecord->tMetadata, (str)"fact_subject", 0u), "xllm") == 0, "fact subject mismatch") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(pRecord->tMetadata, (str)"fact_predicate", 0u), "core_goal") == 0, "fact predicate mismatch") != 0 ||
         require_true((int)xvoTableGetInt(pRecord->tMetadata, (str)"priority", 0u) == 6, "fact priority mismatch") != 0 ) {
        goto cleanup;
    }
    xllm_memory_record_list_result_reset(&tRecords);

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tListOptions.sMetadataKey = "preference_key";
    tListOptions.sMetadataValue = "status_update_style";
    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tRecords, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list preference records failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tRecords.iRecordCount == 1u, "expected one preference record") != 0 ) {
        goto cleanup;
    }
    pRecord = find_record(&tRecords, "preference:status-style");
    if ( require_true(pRecord != NULL, "missing preference record") != 0 ||
         require_true(strcmp(pRecord->sSourceUri, "preference://status-style") == 0, "preference source uri mismatch") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(pRecord->tMetadata, (str)"memory_type", 0u), "preference.v1") == 0, "preference memory_type mismatch") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(pRecord->tMetadata, (str)"extraction_policy", 0u), "preference") == 0, "preference extraction policy mismatch") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(pRecord->tMetadata, (str)"preference_subject", 0u), "user") == 0, "preference subject mismatch") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(pRecord->tMetadata, (str)"preference_value", 0u), "concise direct progress with verification results") == 0, "preference value mismatch") != 0 ) {
        goto cleanup;
    }
    xllm_memory_record_list_result_reset(&tRecords);

    xllm_memory_search_options_init(&tSearchOptions);
    tSearchOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tSearchOptions.sQuery = "agent development library AI IDE claw infrastructure";
    tSearchOptions.sMetadataKey = "memory_type";
    tSearchOptions.sMetadataValue = "fact.v1";
    tSearchOptions.uMaxHits = 3u;
    iStatus = xllm_memory_search(pMemory, &tSearchOptions, &tSearchResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "fact search failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tSearchResult.iHitCount >= 1u, "fact search should return a hit") != 0 ||
         require_true(tSearchResult.pHits[0].sRecordId &&
                      strcmp(tSearchResult.pHits[0].sRecordId, "fact:project-goal") == 0,
                      "fact search returned wrong record") != 0 ) {
        goto cleanup;
    }
    xllm_memory_search_result_reset(&tSearchResult);

    xllm_memory_search_options_init(&tSearchOptions);
    tSearchOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tSearchOptions.sQuery = "terse concise progress verification results";
    tSearchOptions.sMetadataKey = "memory_type";
    tSearchOptions.sMetadataValue = "preference.v1";
    tSearchOptions.uMaxHits = 3u;
    iStatus = xllm_memory_search(pMemory, &tSearchOptions, &tSearchResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "preference search failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tSearchResult.iHitCount >= 1u, "preference search should return a hit") != 0 ||
         require_true(tSearchResult.pHits[0].sRecordId &&
                      strcmp(tSearchResult.pHits[0].sRecordId, "preference:status-style") == 0,
                      "preference search returned wrong record") != 0 ) {
        goto cleanup;
    }

    printf("smoke_memory_fact_preference_type ok\n");
    iRc = 0;

cleanup:
    xllm_memory_search_result_reset(&tSearchResult);
    xllm_memory_record_list_result_reset(&tRecords);
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    xllm_error_free(&tError);
    return iRc;
}
