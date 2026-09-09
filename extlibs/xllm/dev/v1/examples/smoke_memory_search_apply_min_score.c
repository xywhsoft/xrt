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
    pResult->pHits = (xllm_memory_hit *)xrtCalloc(2u, sizeof(xllm_memory_hit));
    if ( !pResult->pHits ) {
        return 2;
    }
    pResult->iHitCount = 2u;

    pResult->pHits[0].eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    pResult->pHits[0].sRecordId = dup_cstr_local("hit-alpha");
    pResult->pHits[0].sTitle = dup_cstr_local("Alpha");
    pResult->pHits[0].sSourceUri = dup_cstr_local("memory://alpha");
    pResult->pHits[0].sText = dup_cstr_local("ALPHA-123456");
    pResult->pHits[0].fScore = 0.91;

    pResult->pHits[1].eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    pResult->pHits[1].sRecordId = dup_cstr_local("hit-beta");
    pResult->pHits[1].sTitle = dup_cstr_local("Beta");
    pResult->pHits[1].sSourceUri = dup_cstr_local("memory://beta");
    pResult->pHits[1].sText = dup_cstr_local("BETA-abcdefg");
    pResult->pHits[1].fScore = 0.75;

    if ( !pResult->pHits[0].sRecordId ||
         !pResult->pHits[0].sTitle ||
         !pResult->pHits[0].sSourceUri ||
         !pResult->pHits[0].sText ||
         !pResult->pHits[1].sRecordId ||
         !pResult->pHits[1].sTitle ||
         !pResult->pHits[1].sSourceUri ||
         !pResult->pHits[1].sText ) {
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
    xllm_turn_request tTurn;
    xllm_error tError;
    const xllm_context_block *pBlock;
    const char *sText;
    int iStatus;
    int iRc = 1;

    memset(&tSearchResult, 0, sizeof(tSearchResult));
    xllm_memory_context_options_init(&tContextOptions);
    xllm_request_init(&tRequest);
    xllm_turn_init(&tTurn);
    xllm_error_init(&tError);

    if ( init_manual_search_result(&tSearchResult) != 0 ) {
        fprintf(stderr, "failed to initialize manual min-score search result\n");
        goto cleanup;
    }

    tContextOptions.sLabel = "Min-score request context:";
    tContextOptions.uMaxHits = 2u;
    tContextOptions.tMinScore.bSet = true;
    tContextOptions.tMinScore.fValue = 0.80;
    iStatus = xllm_memory_apply_search_to_request(&tRequest, &tSearchResult, &tContextOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "apply search min-score to request failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tRequest.iContextBlockCount == 1u, "expected one min-score request context block") != 0 ) {
        goto cleanup;
    }
    pBlock = &tRequest.pContextBlocks[0];
    sText = first_context_text(pBlock);
    if ( require_true(sText != NULL, "min-score request context text missing") != 0 ||
         require_true(strstr(sText, "Min-score request context:") != NULL, "min-score request label missing") != 0 ||
         require_true(strstr(sText, "Retrieved knowledge context (1 hits):") != NULL, "min-score request hit count mismatch") != 0 ||
         require_true(strstr(sText, "ALPHA-123456") != NULL, "min-score request should include alpha hit") != 0 ||
         require_true(strstr(sText, "BETA-abcdefg") == NULL, "min-score request should filter beta hit") != 0 ) {
        goto cleanup;
    }

    xllm_memory_context_options_init(&tContextOptions);
    tContextOptions.sLabel = "Min-score turn context:";
    tContextOptions.eKindOverride = XLLM_CONTEXT_MEMORY;
    tContextOptions.tMinScore.bSet = true;
    tContextOptions.tMinScore.fValue = 1.00;
    iStatus = xllm_memory_apply_search_to_turn_request(&tTurn, &tSearchResult, &tContextOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "apply search min-score to turn failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tTurn.iContextBlockCount == 0u, "expected no turn context block when all hits are below min_score") != 0 ) {
        goto cleanup;
    }

    printf("smoke_memory_search_apply_min_score ok\n");
    iRc = 0;

cleanup:
    xllm_turn_reset(&tTurn);
    xllm_request_reset(&tRequest);
    xllm_memory_search_result_reset(&tSearchResult);
    xllm_error_reset(&tError);
    return iRc;
}
