#include "xllm-memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

XLLM_API void xllm_turn_init(xllm_turn *pTurn);
XLLM_API void xllm_turn_reset(xllm_turn *pTurn);

typedef struct {
    const char *sName;
    uint32 uMaxHits;
    uint32 uMaxCharsPerHit;
    uint32 uMaxTotalChars;
    bool bDistinctByRecord;
    bool bMinScore;
    double fMinScore;
    bool bExpectAlpha;
    bool bExpectAlphaDuplicate;
    bool bExpectBeta;
    bool bExpectGamma;
    bool bExpectLowScore;
    bool bExpectBudgetSmallerThanFull;
} context_eval_case;

typedef struct {
    const char *sName;
    int iStatus;
    size_t iContextBlockCount;
    size_t iOutputChars;
    unsigned uHeaderHits;
    unsigned uRetainedTokens;
    bool bHasAlpha;
    bool bHasAlphaDuplicate;
    bool bHasBeta;
    bool bHasGamma;
    bool bHasLowScore;
    bool bPassed;
} context_eval_case_result;

static unsigned xllm__eval_count_hits_from_text(const char *sText)
{
    unsigned uCount = 0u;
    const char *p = sText;

    if ( !p ) {
        return 0u;
    }
    while ( (p = strstr(p, "\n[")) != NULL ) {
        ++uCount;
        p += 2;
    }
    return uCount;
}

static const char *xllm__eval_context_text(const xllm_turn *pTurn)
{
    const xllm_context_block *pBlock;
    const xllm_message *pMessage;
    const xllm_content_part *pPart;

    if ( !pTurn || pTurn->iContextBlockCount == 0u || !pTurn->pContextBlocks ) {
        return NULL;
    }
    pBlock = &pTurn->pContextBlocks[0];
    if ( pBlock->iMessageCount == 0u || !pBlock->pMessages ) {
        return NULL;
    }
    pMessage = &pBlock->pMessages[0];
    if ( pMessage->iPartCount == 0u || !pMessage->pParts ) {
        return NULL;
    }
    pPart = &pMessage->pParts[0];
    if ( pPart->eKind != XLLM_PART_TEXT || pPart->as.tSource.eKind != XLLM_SOURCE_INLINE_TEXT ) {
        return NULL;
    }
    return pPart->as.tSource.as.sText;
}

static void xllm__eval_build_result(xllm_memory_search_result *pResult, xllm_memory_hit *pHits)
{
    memset(pResult, 0, sizeof(*pResult));
    memset(pHits, 0, 5u * sizeof(*pHits));

    pHits[0].eScope = XLLM_MEMORY_SCOPE_MEMORY;
    pHits[0].sRecordId = "record-alpha";
    pHits[0].sChunkId = "chunk-alpha-0";
    pHits[0].sTitle = "Alpha critical note";
    pHits[0].sSourceUri = "memory://alpha/0";
    pHits[0].sText = "alpha-critical-token establishes the primary project constraint and should survive full packing.";
    pHits[0].fScore = 0.98;
    pHits[0].uChunkIndex = 0u;

    pHits[1].eScope = XLLM_MEMORY_SCOPE_MEMORY;
    pHits[1].sRecordId = "record-alpha";
    pHits[1].sChunkId = "chunk-alpha-1";
    pHits[1].sTitle = "Alpha duplicate note";
    pHits[1].sSourceUri = "memory://alpha/1";
    pHits[1].sText = "alpha-duplicate-token repeats the same record and should disappear when distinct-by-record is enabled.";
    pHits[1].fScore = 0.92;
    pHits[1].uChunkIndex = 1u;

    pHits[2].eScope = XLLM_MEMORY_SCOPE_MEMORY;
    pHits[2].sRecordId = "record-beta";
    pHits[2].sChunkId = "chunk-beta-0";
    pHits[2].sTitle = "Beta implementation note";
    pHits[2].sSourceUri = "memory://beta/0";
    pHits[2].sText = "beta-critical-token captures the required API behavior for the next integration point.";
    pHits[2].fScore = 0.81;
    pHits[2].uChunkIndex = 0u;

    pHits[3].eScope = XLLM_MEMORY_SCOPE_MEMORY;
    pHits[3].sRecordId = "record-gamma";
    pHits[3].sChunkId = "chunk-gamma-0";
    pHits[3].sTitle = "Gamma design note";
    pHits[3].sSourceUri = "memory://gamma/0";
    pHits[3].sText = "gamma-critical-token documents lower priority design context for the packer budget.";
    pHits[3].fScore = 0.67;
    pHits[3].uChunkIndex = 0u;

    pHits[4].eScope = XLLM_MEMORY_SCOPE_MEMORY;
    pHits[4].sRecordId = "record-low";
    pHits[4].sChunkId = "chunk-low-0";
    pHits[4].sTitle = "Low score note";
    pHits[4].sSourceUri = "memory://low/0";
    pHits[4].sText = "low-score-token is intentionally below the min-score threshold.";
    pHits[4].fScore = 0.20;
    pHits[4].uChunkIndex = 0u;

    pResult->pHits = pHits;
    pResult->iHitCount = 5u;
}

static context_eval_case_result xllm__eval_run_case(
    const context_eval_case *pCase,
    size_t iFullOutputChars
)
{
    xllm_memory_hit aHits[5];
    xllm_memory_search_result tSearchResult;
    xllm_memory_context_options tOptions;
    xllm_turn tTurn;
    xllm_error tError;
    context_eval_case_result tResult;
    const char *sText;

    memset(&tResult, 0, sizeof(tResult));
    memset(&tError, 0, sizeof(tError));
    tResult.sName = pCase->sName;

    xllm__eval_build_result(&tSearchResult, aHits);
    xllm_memory_context_options_init(&tOptions);
    tOptions.uMaxHits = pCase->uMaxHits;
    tOptions.uMaxCharsPerHit = pCase->uMaxCharsPerHit;
    tOptions.uMaxTotalChars = pCase->uMaxTotalChars;
    tOptions.bDistinctByRecord = pCase->bDistinctByRecord;
    tOptions.sLabel = pCase->sName;
    if ( pCase->bMinScore ) {
        tOptions.tMinScore.bSet = true;
        tOptions.tMinScore.fValue = pCase->fMinScore;
    }

    xllm_turn_init(&tTurn);
    tResult.iStatus = xllm_memory_apply_search_to_turn(&tTurn, &tSearchResult, &tOptions, &tError);
    tResult.iContextBlockCount = tTurn.iContextBlockCount;
    sText = xllm__eval_context_text(&tTurn);
    if ( sText ) {
        tResult.iOutputChars = strlen(sText);
        tResult.uHeaderHits = xllm__eval_count_hits_from_text(sText);
        tResult.bHasAlpha = strstr(sText, "alpha-critical-token") != NULL;
        tResult.bHasAlphaDuplicate = strstr(sText, "alpha-duplicate-token") != NULL;
        tResult.bHasBeta = strstr(sText, "beta-critical-token") != NULL;
        tResult.bHasGamma = strstr(sText, "gamma-critical-token") != NULL;
        tResult.bHasLowScore = strstr(sText, "low-score-token") != NULL;
        tResult.uRetainedTokens =
            (unsigned)(tResult.bHasAlpha ? 1u : 0u) +
            (unsigned)(tResult.bHasAlphaDuplicate ? 1u : 0u) +
            (unsigned)(tResult.bHasBeta ? 1u : 0u) +
            (unsigned)(tResult.bHasGamma ? 1u : 0u) +
            (unsigned)(tResult.bHasLowScore ? 1u : 0u);
    }

    tResult.bPassed = tResult.iStatus == XRT_NET_OK && tResult.iContextBlockCount == 1u && sText != NULL;
    if ( pCase->bExpectAlpha && !tResult.bHasAlpha ) {
        tResult.bPassed = false;
    }
    if ( pCase->bExpectAlphaDuplicate && !tResult.bHasAlphaDuplicate ) {
        tResult.bPassed = false;
    }
    if ( !pCase->bExpectAlphaDuplicate && tResult.bHasAlphaDuplicate ) {
        tResult.bPassed = false;
    }
    if ( pCase->bExpectBeta && !tResult.bHasBeta ) {
        tResult.bPassed = false;
    }
    if ( pCase->bExpectGamma && !tResult.bHasGamma ) {
        tResult.bPassed = false;
    }
    if ( pCase->bExpectLowScore && !tResult.bHasLowScore ) {
        tResult.bPassed = false;
    }
    if ( !pCase->bExpectLowScore && tResult.bHasLowScore ) {
        tResult.bPassed = false;
    }
    if ( pCase->bExpectBudgetSmallerThanFull &&
         (iFullOutputChars == 0u || tResult.iOutputChars >= iFullOutputChars) ) {
        tResult.bPassed = false;
    }

    xllm_turn_reset(&tTurn);
    xllm_error_reset(&tError);
    return tResult;
}

static int xllm__eval_write_reports(
    const char *sOutputDir,
    const context_eval_case_result *pResults,
    size_t iResultCount
)
{
    char sJsonPath[1024];
    char sTxtPath[1024];
    FILE *pJson;
    FILE *pTxt;
    size_t i;
    size_t iPassed = 0u;

    snprintf(sJsonPath, sizeof(sJsonPath), "%s/context_packer_eval_report.json", sOutputDir);
    snprintf(sTxtPath, sizeof(sTxtPath), "%s/context_packer_eval_report.txt", sOutputDir);
    pJson = fopen(sJsonPath, "wb");
    pTxt = fopen(sTxtPath, "wb");
    if ( !pJson || !pTxt ) {
        if ( pJson ) {
            fclose(pJson);
        }
        if ( pTxt ) {
            fclose(pTxt);
        }
        return 1;
    }

    for ( i = 0u; i < iResultCount; ++i ) {
        if ( pResults[i].bPassed ) {
            ++iPassed;
        }
    }

    fprintf(pJson, "{\n");
    fprintf(pJson, "  \"dataset\": \"synthetic_context_packer_v1\",\n");
    fprintf(pJson, "  \"case_count\": %u,\n", (unsigned)iResultCount);
    fprintf(pJson, "  \"passed_count\": %u,\n", (unsigned)iPassed);
    fprintf(pJson, "  \"cases\": [\n");
    for ( i = 0u; i < iResultCount; ++i ) {
        const context_eval_case_result *p = &pResults[i];
        fprintf(pJson, "    {\n");
        fprintf(pJson, "      \"name\": \"%s\",\n", p->sName);
        fprintf(pJson, "      \"passed\": %s,\n", p->bPassed ? "true" : "false");
        fprintf(pJson, "      \"status\": %d,\n", p->iStatus);
        fprintf(pJson, "      \"context_block_count\": %u,\n", (unsigned)p->iContextBlockCount);
        fprintf(pJson, "      \"output_chars\": %u,\n", (unsigned)p->iOutputChars);
        fprintf(pJson, "      \"header_hits\": %u,\n", p->uHeaderHits);
        fprintf(pJson, "      \"retained_tokens\": %u,\n", p->uRetainedTokens);
        fprintf(pJson, "      \"has_alpha\": %s,\n", p->bHasAlpha ? "true" : "false");
        fprintf(pJson, "      \"has_alpha_duplicate\": %s,\n", p->bHasAlphaDuplicate ? "true" : "false");
        fprintf(pJson, "      \"has_beta\": %s,\n", p->bHasBeta ? "true" : "false");
        fprintf(pJson, "      \"has_gamma\": %s,\n", p->bHasGamma ? "true" : "false");
        fprintf(pJson, "      \"has_low_score\": %s\n", p->bHasLowScore ? "true" : "false");
        fprintf(pJson, "    }%s\n", i + 1u == iResultCount ? "" : ",");
    }
    fprintf(pJson, "  ]\n");
    fprintf(pJson, "}\n");

    fprintf(pTxt, "dataset: synthetic_context_packer_v1\n");
    fprintf(pTxt, "case_count: %u\n", (unsigned)iResultCount);
    fprintf(pTxt, "passed_count: %u\n\n", (unsigned)iPassed);
    for ( i = 0u; i < iResultCount; ++i ) {
        const context_eval_case_result *p = &pResults[i];
        fprintf(
            pTxt,
            "%s: passed=%s status=%d blocks=%u chars=%u hits=%u retained_tokens=%u alpha=%u duplicate=%u beta=%u gamma=%u low=%u\n",
            p->sName,
            p->bPassed ? "true" : "false",
            p->iStatus,
            (unsigned)p->iContextBlockCount,
            (unsigned)p->iOutputChars,
            p->uHeaderHits,
            p->uRetainedTokens,
            (unsigned)p->bHasAlpha,
            (unsigned)p->bHasAlphaDuplicate,
            (unsigned)p->bHasBeta,
            (unsigned)p->bHasGamma,
            (unsigned)p->bHasLowScore
        );
    }

    fclose(pJson);
    fclose(pTxt);
    printf("context packer eval json: %s\n", sJsonPath);
    printf("context packer eval txt:  %s\n", sTxtPath);
    return iPassed == iResultCount ? 0 : 2;
}

int main(int argc, char **argv)
{
    const char *sOutputDir = argc > 1 ? argv[1] : ".";
    const context_eval_case aCases[] = {
        {"full", 5u, 120u, 0u, false, false, 0.0, true, true, true, true, true, false},
        {"budget_40", 5u, 120u, 40u, false, false, 0.0, true, false, false, false, false, true},
        {"distinct_budget_240", 5u, 120u, 240u, true, false, 0.0, true, false, true, false, false, true},
        {"min_score_0_5", 5u, 120u, 0u, false, true, 0.5, true, true, true, true, false, true}
    };
    context_eval_case_result aResults[sizeof(aCases) / sizeof(aCases[0])];
    size_t i;
    size_t iFullOutputChars;
    int iWriteStatus;

    memset(aResults, 0, sizeof(aResults));
    aResults[0] = xllm__eval_run_case(&aCases[0], 0u);
    iFullOutputChars = aResults[0].iOutputChars;
    for ( i = 1u; i < sizeof(aCases) / sizeof(aCases[0]); ++i ) {
        aResults[i] = xllm__eval_run_case(&aCases[i], iFullOutputChars);
    }

    iWriteStatus = xllm__eval_write_reports(
        sOutputDir,
        aResults,
        sizeof(aResults) / sizeof(aResults[0])
    );
    return iWriteStatus;
}
