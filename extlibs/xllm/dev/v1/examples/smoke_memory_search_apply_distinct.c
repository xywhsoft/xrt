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

static char *dup_cstr_local(const char *sText)
{
    size_t iLen;
    char *sCopy;

    if ( !sText ) {
        return NULL;
    }

    iLen = strlen(sText);
    sCopy = (char *)xrtCalloc(iLen + 1u, sizeof(char));
    if ( !sCopy ) {
        return NULL;
    }
    memcpy(sCopy, sText, iLen);
    sCopy[iLen] = '\0';
    return sCopy;
}

static const char *first_context_text(const xllm_context_block *pBlock)
{
    if ( !pBlock ||
         pBlock->iMessageCount == 0u ||
         !pBlock->pMessages ||
         pBlock->pMessages[0].iPartCount == 0u ||
         !pBlock->pMessages[0].pParts ) {
        return NULL;
    }

    if ( pBlock->pMessages[0].pParts[0].eKind != XLLM_PART_TEXT ||
         pBlock->pMessages[0].pParts[0].as.tSource.eKind != XLLM_SOURCE_INLINE_TEXT ) {
        return NULL;
    }

    return pBlock->pMessages[0].pParts[0].as.tSource.as.sText;
}

static int init_manual_search_result(xllm_memory_search_result *pResult)
{
    if ( !pResult ) {
        return 1;
    }

    memset(pResult, 0, sizeof(*pResult));
    pResult->pHits = (xllm_memory_hit *)xrtCalloc(3u, sizeof(xllm_memory_hit));
    if ( !pResult->pHits ) {
        return 2;
    }
    pResult->iHitCount = 3u;

    pResult->pHits[0].eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    pResult->pHits[0].sRecordId = dup_cstr_local("record-alpha");
    pResult->pHits[0].sTitle = dup_cstr_local("Alpha One");
    pResult->pHits[0].sSourceUri = dup_cstr_local("memory://alpha/1");
    pResult->pHits[0].sText = dup_cstr_local("ALPHA-FIRST");
    pResult->pHits[0].fScore = 0.95;

    pResult->pHits[1].eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    pResult->pHits[1].sRecordId = dup_cstr_local("record-alpha");
    pResult->pHits[1].sTitle = dup_cstr_local("Alpha Two");
    pResult->pHits[1].sSourceUri = dup_cstr_local("memory://alpha/2");
    pResult->pHits[1].sText = dup_cstr_local("ALPHA-SECOND");
    pResult->pHits[1].fScore = 0.90;

    pResult->pHits[2].eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    pResult->pHits[2].sRecordId = dup_cstr_local("record-beta");
    pResult->pHits[2].sTitle = dup_cstr_local("Beta");
    pResult->pHits[2].sSourceUri = dup_cstr_local("memory://beta/1");
    pResult->pHits[2].sText = dup_cstr_local("BETA-ONLY");
    pResult->pHits[2].fScore = 0.85;

    if ( !pResult->pHits[0].sRecordId ||
         !pResult->pHits[0].sTitle ||
         !pResult->pHits[0].sSourceUri ||
         !pResult->pHits[0].sText ||
         !pResult->pHits[1].sRecordId ||
         !pResult->pHits[1].sTitle ||
         !pResult->pHits[1].sSourceUri ||
         !pResult->pHits[1].sText ||
         !pResult->pHits[2].sRecordId ||
         !pResult->pHits[2].sTitle ||
         !pResult->pHits[2].sSourceUri ||
         !pResult->pHits[2].sText ) {
        xllm_memory_search_result_reset(pResult);
        return 3;
    }

    return 0;
}

int main(void)
{
    xllm_memory_search_result tSearchResult;
    xllm_memory_context_options tContextOptions;
    xllm_request tRequest;
    xllm_error tError;
    const xllm_context_block *pBlock;
    const char *sText;
    int iStatus;
    int iRc = 1;

    memset(&tSearchResult, 0, sizeof(tSearchResult));
    xllm_memory_context_options_init(&tContextOptions);
    xllm_request_init(&tRequest);
    xllm_error_init(&tError);

    if ( init_manual_search_result(&tSearchResult) != 0 ) {
        fprintf(stderr, "failed to initialize manual distinct search result\n");
        goto cleanup;
    }

    tContextOptions.sLabel = "Distinct request context:";
    tContextOptions.uMaxHits = 2u;
    tContextOptions.uMaxCharsPerHit = 64u;
    tContextOptions.bDistinctByRecord = true;
    iStatus = xllm_memory_apply_search_to_request(&tRequest, &tSearchResult, &tContextOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "apply search distinct to request failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tRequest.iContextBlockCount == 1u, "expected one distinct request context block") != 0 ) {
        goto cleanup;
    }
    pBlock = &tRequest.pContextBlocks[0];
    sText = first_context_text(pBlock);
    if ( require_true(sText != NULL, "distinct request context text missing") != 0 ||
         require_true(strstr(sText, "Distinct request context:") != NULL, "distinct request label missing") != 0 ||
         require_true(strstr(sText, "Retrieved knowledge context (2 hits):") != NULL, "distinct request hit count mismatch") != 0 ||
         require_true(strstr(sText, "ALPHA-FIRST") != NULL, "distinct request should include first alpha hit") != 0 ||
         require_true(strstr(sText, "ALPHA-SECOND") == NULL, "distinct request should skip duplicate alpha record hit") != 0 ||
         require_true(strstr(sText, "BETA-ONLY") != NULL, "distinct request should include beta hit") != 0 ) {
        goto cleanup;
    }

    printf("smoke_memory_search_apply_distinct ok\n");
    iRc = 0;

cleanup:
    xllm_request_reset(&tRequest);
    xllm_memory_search_result_reset(&tSearchResult);
    xllm_error_reset(&tError);
    return iRc;
}
