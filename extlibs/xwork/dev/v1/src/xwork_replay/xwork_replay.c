#include "../xwork_core/xwork_internal.h"

#include <ctype.h>
#include <stdio.h>

typedef struct xwork__replay_entry_record xwork__replay_entry_record;
typedef struct xwork__replay_event_record xwork__replay_event_record;

struct xwork__replay_entry_record {
    size_t iSequence;
    xwork_replay_entry_kind eKind;
    char *sKey;
    char *sOperationId;
    char *sRequestJson;
    char *sResponseJson;
    char *sArgumentsJson;
    char *sResultJson;
    char *sRequestHash;
    char *sResponseHash;
    char *sArgumentsHash;
    char *sResultHash;
    char *sContentHash;
    xwork_status iStatus;
    xwork__replay_entry_record *pNext;
};

struct xwork__replay_event_record {
    size_t iSequence;
    xwork_replay_event_kind eKind;
    char *sKey;
    char *sName;
    int iType;
    char *sPayloadHash;
    char *sContentHash;
    xwork_status iStatus;
    xwork__replay_event_record *pNext;
};

struct xwork_replay_engine {
    char *sReplayId;
    xwork_replay_mode eMode;
    bool bReadonlyFilesystem;
    bool bBlockSideEffects;
    bool bCancelled;
    char *sCancelReason;
    size_t iMaxDivergences;
    size_t iNextSequence;
    size_t iNextEventSequence;
    size_t iReplayCursor;
    size_t iEventReplayCursor;
    size_t iRecordedCount;
    size_t iRecordedEventCount;
    size_t iReplayedCount;
    size_t iReplayedEventCount;
    size_t iDivergenceCount;
    xwork_replay_divergence tFirstDivergence;
    xwork__replay_entry_record *pEntries;
    xwork__replay_entry_record *pLastEntry;
    xwork__replay_event_record *pEvents;
    xwork__replay_event_record *pLastEvent;
};

static void xwork__replay_free_entry(xwork__replay_entry_record *pEntry)
{
    if ( !pEntry ) {
        return;
    }
    free(pEntry->sKey);
    free(pEntry->sOperationId);
    free(pEntry->sRequestJson);
    free(pEntry->sResponseJson);
    free(pEntry->sArgumentsJson);
    free(pEntry->sResultJson);
    free(pEntry->sRequestHash);
    free(pEntry->sResponseHash);
    free(pEntry->sArgumentsHash);
    free(pEntry->sResultHash);
    free(pEntry->sContentHash);
    free(pEntry);
}

static void xwork__replay_free_event(xwork__replay_event_record *pEvent)
{
    if ( !pEvent ) {
        return;
    }
    free(pEvent->sKey);
    free(pEvent->sName);
    free(pEvent->sPayloadHash);
    free(pEvent->sContentHash);
    free(pEvent);
}

static bool xwork__replay_is_side_effect_entry(xwork_replay_entry_kind eKind)
{
    return eKind == XWORK_REPLAY_ENTRY_TOOL ||
        eKind == XWORK_REPLAY_ENTRY_HOST_TOOL ||
        eKind == XWORK_REPLAY_ENTRY_FILESYSTEM ||
        eKind == XWORK_REPLAY_ENTRY_PROCESS ||
        eKind == XWORK_REPLAY_ENTRY_TERMINAL;
}

static const char *xwork__replay_mode_cstr(xwork_replay_mode eMode)
{
    switch ( eMode ) {
    case XWORK_REPLAY_MODE_RECORD: return "record";
    case XWORK_REPLAY_MODE_STRICT: return "strict";
    case XWORK_REPLAY_MODE_AUDIT: return "audit";
    default: return "unknown";
    }
}

static const char *xwork__replay_entry_kind_cstr(xwork_replay_entry_kind eKind)
{
    switch ( eKind ) {
    case XWORK_REPLAY_ENTRY_MODEL: return "model";
    case XWORK_REPLAY_ENTRY_TOOL: return "tool";
    case XWORK_REPLAY_ENTRY_HOST_TOOL: return "host_tool";
    case XWORK_REPLAY_ENTRY_FILESYSTEM: return "filesystem";
    case XWORK_REPLAY_ENTRY_PROCESS: return "process";
    case XWORK_REPLAY_ENTRY_TERMINAL: return "terminal";
    case XWORK_REPLAY_ENTRY_ARTIFACT: return "artifact";
    case XWORK_REPLAY_ENTRY_CHECKPOINT: return "checkpoint";
    default: return "unknown";
    }
}

static const char *xwork__replay_divergence_kind_cstr(
    xwork_replay_divergence_kind eKind
)
{
    switch ( eKind ) {
    case XWORK_REPLAY_DIVERGENCE_NONE: return "none";
    case XWORK_REPLAY_DIVERGENCE_MISSING_ENTRY: return "missing_entry";
    case XWORK_REPLAY_DIVERGENCE_UNEXPECTED_ENTRY: return "unexpected_entry";
    case XWORK_REPLAY_DIVERGENCE_KIND_MISMATCH: return "kind_mismatch";
    case XWORK_REPLAY_DIVERGENCE_KEY_MISMATCH: return "key_mismatch";
    case XWORK_REPLAY_DIVERGENCE_REQUEST_MISMATCH: return "request_mismatch";
    case XWORK_REPLAY_DIVERGENCE_RESPONSE_MISMATCH: return "response_mismatch";
    case XWORK_REPLAY_DIVERGENCE_STATUS_MISMATCH: return "status_mismatch";
    case XWORK_REPLAY_DIVERGENCE_CONTENT_MISMATCH: return "content_mismatch";
    default: return "unknown";
    }
}

static const char *xwork__replay_safe_cstr(const char *sText)
{
    return sText ? sText : "";
}

static xwork_status xwork__replay_dup_hash(
    const char *sText,
    const char *sExistingHash,
    bool bNormalizeJson,
    char **psHash
)
{
    char sHash[17];

    if ( !psHash ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    *psHash = NULL;
    if ( sExistingHash && sExistingHash[0] ) {
        *psHash = xwork__dup_cstr(sExistingHash);
        return *psHash ? XWORK_OK : XWORK_ERROR_NO_MEMORY;
    }
    if ( !sText ) {
        return XWORK_OK;
    }
    if ( bNormalizeJson &&
         xwork_replay_hash_json(sText, sHash, sizeof(sHash)) == XWORK_OK ) {
        *psHash = xwork__dup_cstr(sHash);
        return *psHash ? XWORK_OK : XWORK_ERROR_NO_MEMORY;
    }
    if ( xwork_replay_hash_text(sText, sHash, sizeof(sHash)) != XWORK_OK ) {
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }
    *psHash = xwork__dup_cstr(sHash);
    return *psHash ? XWORK_OK : XWORK_ERROR_NO_MEMORY;
}

static bool xwork__replay_cstr_equal(const char *a, const char *b)
{
    if ( !a || !a[0] ) {
        return !b || !b[0];
    }
    if ( !b ) {
        return false;
    }
    return strcmp(a, b) == 0;
}

static bool xwork__replay_hash_equal_compat(
    const char *sActualHash,
    const char *sExpectedHash,
    const char *sExpectedText
)
{
    char sLegacyHash[17];

    if ( xwork__replay_cstr_equal(sActualHash, sExpectedHash) ) {
        return true;
    }
    if ( !sActualHash || !sExpectedText ) {
        return false;
    }
    if ( xwork_replay_hash_text(sExpectedText, sLegacyHash, sizeof(sLegacyHash)) != XWORK_OK ) {
        return false;
    }
    return strcmp(sActualHash, sLegacyHash) == 0;
}

static xwork_status xwork__replay_copy_summary(
    xwork_replay_entry_summary *pSummary,
    const xwork__replay_entry_record *pEntry
)
{
    xwork_status iStatus;

    if ( !pSummary || !pEntry ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_replay_entry_summary_reset(pSummary);
    iStatus = xwork__replace_cstr((char **)&pSummary->sKey, pEntry->sKey);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSummary->sOperationId, pEntry->sOperationId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSummary->sRequestJson, pEntry->sRequestJson);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSummary->sResponseJson, pEntry->sResponseJson);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSummary->sArgumentsJson, pEntry->sArgumentsJson);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSummary->sResultJson, pEntry->sResultJson);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSummary->sRequestHash, pEntry->sRequestHash);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSummary->sResponseHash, pEntry->sResponseHash);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSummary->sArgumentsHash, pEntry->sArgumentsHash);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSummary->sResultHash, pEntry->sResultHash);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSummary->sContentHash, pEntry->sContentHash);
    if ( iStatus != XWORK_OK ) return iStatus;
    pSummary->iSequence = pEntry->iSequence;
    pSummary->eKind = pEntry->eKind;
    pSummary->iStatus = pEntry->iStatus;
    return XWORK_OK;
}

static xwork_status xwork__replay_copy_filesystem_ref_from_entry(
    xwork_replay_filesystem_ref_summary *pSummary,
    const xwork__replay_entry_record *pEntry
)
{
    xwork_status iStatus;

    if ( !pSummary || !pEntry ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_replay_filesystem_ref_summary_reset(pSummary);
    pSummary->iSequence = pEntry->iSequence;
    pSummary->iStatus = pEntry->iStatus;
    iStatus = xwork__replace_cstr((char **)&pSummary->sRefId, pEntry->sKey);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSummary->sPath, pEntry->sRequestJson);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSummary->sMetadataJson, pEntry->sArgumentsJson);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSummary->sContentHash, pEntry->sContentHash);
    if ( iStatus != XWORK_OK ) return iStatus;
    return XWORK_OK;
}

static void xwork__replay_filesystem_ref_to_entry_options(
    const xwork_replay_filesystem_ref_options *pRef,
    xwork_replay_entry_options *pEntry
)
{
    xwork_replay_entry_options_init(pEntry);
    pEntry->eKind = XWORK_REPLAY_ENTRY_FILESYSTEM;
    pEntry->sKey = pRef->sRefId;
    pEntry->sOperationId = XWORK_REPLAY_FILESYSTEM_SNAPSHOT_REF;
    pEntry->sRequestJson = pRef->sPath;
    pEntry->sArgumentsJson = pRef->sMetadataJson;
    pEntry->sContentHash = pRef->sContentHash;
    pEntry->iStatus = pRef->iStatus;
}

static bool xwork__replay_entry_is_filesystem_ref(
    const xwork__replay_entry_record *pEntry
)
{
    return pEntry &&
        pEntry->eKind == XWORK_REPLAY_ENTRY_FILESYSTEM &&
        xwork__replay_cstr_equal(
            pEntry->sOperationId,
            XWORK_REPLAY_FILESYSTEM_SNAPSHOT_REF
        );
}

static xwork_status xwork__replay_copy_divergence(
    xwork_replay_divergence *pDst,
    const xwork_replay_divergence *pSrc
)
{
    xwork_status iStatus;

    if ( !pDst || !pSrc ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_replay_divergence_reset(pDst);
    iStatus = xwork__replace_cstr((char **)&pDst->sExpectedKey, pSrc->sExpectedKey);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pDst->sActualKey, pSrc->sActualKey);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pDst->sExpectedHash, pSrc->sExpectedHash);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pDst->sActualHash, pSrc->sActualHash);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pDst->sMessage, pSrc->sMessage);
    if ( iStatus != XWORK_OK ) return iStatus;
    pDst->eKind = pSrc->eKind;
    pDst->iSequence = pSrc->iSequence;
    pDst->eExpectedEntryKind = pSrc->eExpectedEntryKind;
    pDst->eActualEntryKind = pSrc->eActualEntryKind;
    return XWORK_OK;
}

static xwork_status xwork__replay_store_divergence(
    xwork_replay_engine *pEngine,
    xwork_replay_divergence_kind eKind,
    const xwork__replay_entry_record *pActual,
    const xwork_replay_entry_options *pExpected,
    const char *sExpectedHash,
    const char *sActualHash,
    const char *sMessage
)
{
    xwork_replay_divergence tDivergence;
    xwork_status iStatus = XWORK_OK;

    if ( !pEngine ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_replay_divergence_init(&tDivergence);
    tDivergence.eKind = eKind;
    tDivergence.iSequence = pActual ? pActual->iSequence : pEngine->iReplayCursor + 1u;
    tDivergence.eExpectedEntryKind = pExpected ? pExpected->eKind : XWORK_REPLAY_ENTRY_MODEL;
    tDivergence.eActualEntryKind = pActual ? pActual->eKind : XWORK_REPLAY_ENTRY_MODEL;
    iStatus = xwork__replace_cstr(
        (char **)&tDivergence.sExpectedKey,
        pExpected ? pExpected->sKey : NULL
    );
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replace_cstr(
        (char **)&tDivergence.sActualKey,
        pActual ? pActual->sKey : NULL
    );
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replace_cstr((char **)&tDivergence.sExpectedHash, sExpectedHash);
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replace_cstr((char **)&tDivergence.sActualHash, sActualHash);
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replace_cstr((char **)&tDivergence.sMessage, sMessage);
    if ( iStatus != XWORK_OK ) goto done;
    if ( pEngine->iDivergenceCount == 0u ) {
        iStatus = xwork__replay_copy_divergence(
            &pEngine->tFirstDivergence,
            &tDivergence
        );
        if ( iStatus != XWORK_OK ) goto done;
    }
    ++pEngine->iDivergenceCount;

done:
    xwork_replay_divergence_reset(&tDivergence);
    return iStatus;
}

static xwork__replay_entry_record *xwork__replay_entry_at(
    const xwork_replay_engine *pEngine,
    size_t iIndex
)
{
    xwork__replay_entry_record *pEntry;
    size_t i = 0u;

    if ( !pEngine ) {
        return NULL;
    }
    for ( pEntry = pEngine->pEntries; pEntry; pEntry = pEntry->pNext ) {
        if ( i == iIndex ) {
            return pEntry;
        }
        ++i;
    }
    return NULL;
}

static xwork__replay_event_record *xwork__replay_event_at(
    const xwork_replay_engine *pEngine,
    size_t iIndex
)
{
    xwork__replay_event_record *pEvent;
    size_t i = 0u;

    if ( !pEngine ) {
        return NULL;
    }
    for ( pEvent = pEngine->pEvents; pEvent; pEvent = pEvent->pNext ) {
        if ( i == iIndex ) {
            return pEvent;
        }
        ++i;
    }
    return NULL;
}

static xwork_status xwork__replay_copy_event_summary(
    xwork_replay_event_summary *pSummary,
    const xwork__replay_event_record *pEvent
)
{
    xwork_status iStatus;

    if ( !pSummary || !pEvent ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_replay_event_summary_reset(pSummary);
    iStatus = xwork__replace_cstr((char **)&pSummary->sKey, pEvent->sKey);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSummary->sName, pEvent->sName);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSummary->sPayloadHash, pEvent->sPayloadHash);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSummary->sContentHash, pEvent->sContentHash);
    if ( iStatus != XWORK_OK ) return iStatus;
    pSummary->iSequence = pEvent->iSequence;
    pSummary->eKind = pEvent->eKind;
    pSummary->iType = pEvent->iType;
    pSummary->iStatus = pEvent->iStatus;
    return XWORK_OK;
}

static xwork_status xwork__replay_store_event_divergence(
    xwork_replay_engine *pEngine,
    xwork_replay_divergence_kind eKind,
    const xwork__replay_event_record *pActual,
    const xwork_replay_event_options *pExpected,
    const char *sExpectedHash,
    const char *sActualHash,
    const char *sMessage
)
{
    xwork_replay_divergence tDivergence;
    xwork_status iStatus = XWORK_OK;

    if ( !pEngine ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_replay_divergence_init(&tDivergence);
    tDivergence.eKind = eKind;
    tDivergence.iSequence = pActual ? pActual->iSequence : pEngine->iEventReplayCursor + 1u;
    tDivergence.eExpectedEntryKind = XWORK_REPLAY_ENTRY_MODEL;
    tDivergence.eActualEntryKind = XWORK_REPLAY_ENTRY_MODEL;
    iStatus = xwork__replace_cstr(
        (char **)&tDivergence.sExpectedKey,
        pExpected ? pExpected->sKey : NULL
    );
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replace_cstr(
        (char **)&tDivergence.sActualKey,
        pActual ? pActual->sKey : NULL
    );
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replace_cstr((char **)&tDivergence.sExpectedHash, sExpectedHash);
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replace_cstr((char **)&tDivergence.sActualHash, sActualHash);
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replace_cstr((char **)&tDivergence.sMessage, sMessage);
    if ( iStatus != XWORK_OK ) goto done;
    if ( pEngine->iDivergenceCount == 0u ) {
        iStatus = xwork__replay_copy_divergence(
            &pEngine->tFirstDivergence,
            &tDivergence
        );
        if ( iStatus != XWORK_OK ) goto done;
    }
    ++pEngine->iDivergenceCount;

done:
    xwork_replay_divergence_reset(&tDivergence);
    return iStatus;
}

static xwork_status xwork__replay_append_entry(
    xwork_replay_engine *pEngine,
    const xwork_replay_entry_options *pEntryOptions,
    bool bApplyRecordPolicy
)
{
    xwork__replay_entry_record *pEntry;
    xwork_status iStatus;

    if ( !pEngine || !pEntryOptions ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( pEngine->bCancelled ) {
        return XWORK_ERROR_CANCELLED;
    }
    if ( bApplyRecordPolicy &&
         pEngine->bBlockSideEffects &&
         xwork__replay_is_side_effect_entry(pEntryOptions->eKind) ) {
        return XWORK_ERROR_PAUSED;
    }
    pEntry = (xwork__replay_entry_record *)calloc(1u, sizeof(*pEntry));
    if ( !pEntry ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    pEntry->iSequence = pEngine->iNextSequence++;
    pEntry->eKind = pEntryOptions->eKind;
    pEntry->iStatus = pEntryOptions->iStatus;
    iStatus = xwork__replace_cstr(&pEntry->sKey, pEntryOptions->sKey);
    if ( iStatus != XWORK_OK ) goto fail;
    iStatus = xwork__replace_cstr(&pEntry->sOperationId, pEntryOptions->sOperationId);
    if ( iStatus != XWORK_OK ) goto fail;
    iStatus = xwork__replace_cstr(&pEntry->sRequestJson, pEntryOptions->sRequestJson);
    if ( iStatus != XWORK_OK ) goto fail;
    iStatus = xwork__replace_cstr(&pEntry->sResponseJson, pEntryOptions->sResponseJson);
    if ( iStatus != XWORK_OK ) goto fail;
    iStatus = xwork__replace_cstr(&pEntry->sArgumentsJson, pEntryOptions->sArgumentsJson);
    if ( iStatus != XWORK_OK ) goto fail;
    iStatus = xwork__replace_cstr(&pEntry->sResultJson, pEntryOptions->sResultJson);
    if ( iStatus != XWORK_OK ) goto fail;
    iStatus = xwork__replay_dup_hash(
        pEntryOptions->sRequestJson,
        pEntryOptions->sRequestHash,
        true,
        &pEntry->sRequestHash
    );
    if ( iStatus != XWORK_OK ) goto fail;
    iStatus = xwork__replay_dup_hash(
        pEntryOptions->sResponseJson,
        pEntryOptions->sResponseHash,
        true,
        &pEntry->sResponseHash
    );
    if ( iStatus != XWORK_OK ) goto fail;
    iStatus = xwork__replay_dup_hash(
        pEntryOptions->sArgumentsJson,
        pEntryOptions->sArgumentsHash,
        true,
        &pEntry->sArgumentsHash
    );
    if ( iStatus != XWORK_OK ) goto fail;
    iStatus = xwork__replay_dup_hash(
        pEntryOptions->sResultJson,
        pEntryOptions->sResultHash,
        true,
        &pEntry->sResultHash
    );
    if ( iStatus != XWORK_OK ) goto fail;
    iStatus = xwork__replay_dup_hash(
        NULL,
        pEntryOptions->sContentHash,
        false,
        &pEntry->sContentHash
    );
    if ( iStatus != XWORK_OK ) goto fail;
    if ( pEngine->pLastEntry ) {
        pEngine->pLastEntry->pNext = pEntry;
    } else {
        pEngine->pEntries = pEntry;
    }
    pEngine->pLastEntry = pEntry;
    ++pEngine->iRecordedCount;
    return XWORK_OK;

fail:
    xwork__replay_free_entry(pEntry);
    return iStatus;
}

static xwork_status xwork__replay_append_event(
    xwork_replay_engine *pEngine,
    const xwork_replay_event_options *pEventOptions
)
{
    xwork__replay_event_record *pEvent;
    xwork_status iStatus;

    if ( !pEngine || !pEventOptions ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( pEngine->bCancelled ) {
        return XWORK_ERROR_CANCELLED;
    }
    pEvent = (xwork__replay_event_record *)calloc(1u, sizeof(*pEvent));
    if ( !pEvent ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    pEvent->iSequence = pEngine->iNextEventSequence++;
    pEvent->eKind = pEventOptions->eKind;
    pEvent->iType = pEventOptions->iType;
    pEvent->iStatus = pEventOptions->iStatus;
    iStatus = xwork__replace_cstr(&pEvent->sKey, pEventOptions->sKey);
    if ( iStatus != XWORK_OK ) goto fail;
    iStatus = xwork__replace_cstr(&pEvent->sName, pEventOptions->sName);
    if ( iStatus != XWORK_OK ) goto fail;
    iStatus = xwork__replay_dup_hash(
        pEventOptions->sPayloadJson,
        pEventOptions->sPayloadHash,
        true,
        &pEvent->sPayloadHash
    );
    if ( iStatus != XWORK_OK ) goto fail;
    iStatus = xwork__replay_dup_hash(
        pEventOptions->sContentText,
        pEventOptions->sContentHash,
        false,
        &pEvent->sContentHash
    );
    if ( iStatus != XWORK_OK ) goto fail;
    if ( pEngine->pLastEvent ) {
        pEngine->pLastEvent->pNext = pEvent;
    } else {
        pEngine->pEvents = pEvent;
    }
    pEngine->pLastEvent = pEvent;
    ++pEngine->iRecordedEventCount;
    return XWORK_OK;

fail:
    xwork__replay_free_event(pEvent);
    return iStatus;
}

void xwork_replay_options_init(xwork_replay_options *pOptions)
{
    if ( pOptions ) {
        memset(pOptions, 0, sizeof(*pOptions));
        pOptions->eMode = XWORK_REPLAY_MODE_RECORD;
        pOptions->bReadonlyFilesystem = true;
        pOptions->bBlockSideEffects = true;
        pOptions->iMaxDivergences = 1u;
    }
}

void xwork_replay_manifest_init(xwork_replay_manifest *pManifest)
{
    if ( pManifest ) {
        memset(pManifest, 0, sizeof(*pManifest));
    }
}

void xwork_replay_manifest_reset(xwork_replay_manifest *pManifest)
{
    if ( !pManifest ) {
        return;
    }
    free((char *)pManifest->sManifestId);
    free((char *)pManifest->sReplayId);
    free((char *)pManifest->sSourceRunId);
    free((char *)pManifest->sCreatedAtText);
    free((char *)pManifest->sContentHashAlgorithm);
    xwork_replay_manifest_init(pManifest);
}

void xwork_replay_entry_options_init(xwork_replay_entry_options *pOptions)
{
    if ( pOptions ) {
        memset(pOptions, 0, sizeof(*pOptions));
        pOptions->eKind = XWORK_REPLAY_ENTRY_MODEL;
        pOptions->iStatus = XWORK_OK;
    }
}

void xwork_replay_entry_summary_init(xwork_replay_entry_summary *pSummary)
{
    if ( pSummary ) {
        memset(pSummary, 0, sizeof(*pSummary));
        pSummary->eKind = XWORK_REPLAY_ENTRY_MODEL;
        pSummary->iStatus = XWORK_OK;
    }
}

void xwork_replay_entry_summary_reset(xwork_replay_entry_summary *pSummary)
{
    if ( !pSummary ) {
        return;
    }
    free((char *)pSummary->sKey);
    free((char *)pSummary->sOperationId);
    free((char *)pSummary->sRequestJson);
    free((char *)pSummary->sResponseJson);
    free((char *)pSummary->sArgumentsJson);
    free((char *)pSummary->sResultJson);
    free((char *)pSummary->sRequestHash);
    free((char *)pSummary->sResponseHash);
    free((char *)pSummary->sArgumentsHash);
    free((char *)pSummary->sResultHash);
    free((char *)pSummary->sContentHash);
    xwork_replay_entry_summary_init(pSummary);
}

void xwork_replay_entry_summary_list_init(xwork_replay_entry_summary_list *pList)
{
    if ( pList ) {
        memset(pList, 0, sizeof(*pList));
    }
}

void xwork_replay_entry_summary_list_reset(xwork_replay_entry_summary_list *pList)
{
    xwork_replay_entry_summary *pItems;
    size_t i;

    if ( !pList ) {
        return;
    }
    pItems = pList->pItems;
    if ( pItems ) {
        for ( i = 0u; i < pList->iCount; ++i ) {
            xwork_replay_entry_summary_reset(&pItems[i]);
        }
        free(pItems);
    }
    xwork_replay_entry_summary_list_init(pList);
}

void xwork_replay_filesystem_ref_options_init(
    xwork_replay_filesystem_ref_options *pOptions
)
{
    if ( pOptions ) {
        memset(pOptions, 0, sizeof(*pOptions));
        pOptions->iStatus = XWORK_OK;
    }
}

void xwork_replay_filesystem_ref_summary_init(
    xwork_replay_filesystem_ref_summary *pSummary
)
{
    if ( pSummary ) {
        memset(pSummary, 0, sizeof(*pSummary));
        pSummary->iStatus = XWORK_OK;
    }
}

void xwork_replay_filesystem_ref_summary_reset(
    xwork_replay_filesystem_ref_summary *pSummary
)
{
    if ( !pSummary ) {
        return;
    }
    free((char *)pSummary->sRefId);
    free((char *)pSummary->sPath);
    free((char *)pSummary->sMetadataJson);
    free((char *)pSummary->sContentHash);
    xwork_replay_filesystem_ref_summary_init(pSummary);
}

void xwork_replay_filesystem_ref_summary_list_init(
    xwork_replay_filesystem_ref_summary_list *pList
)
{
    if ( pList ) {
        memset(pList, 0, sizeof(*pList));
    }
}

void xwork_replay_filesystem_ref_summary_list_reset(
    xwork_replay_filesystem_ref_summary_list *pList
)
{
    xwork_replay_filesystem_ref_summary *pItems;
    size_t i;

    if ( !pList ) {
        return;
    }
    pItems = pList->pItems;
    if ( pItems ) {
        for ( i = 0u; i < pList->iCount; ++i ) {
            xwork_replay_filesystem_ref_summary_reset(&pItems[i]);
        }
        free(pItems);
    }
    xwork_replay_filesystem_ref_summary_list_init(pList);
}

void xwork_replay_event_options_init(xwork_replay_event_options *pOptions)
{
    if ( pOptions ) {
        memset(pOptions, 0, sizeof(*pOptions));
        pOptions->eKind = XWORK_REPLAY_EVENT_GENERIC;
        pOptions->iStatus = XWORK_OK;
    }
}

void xwork_replay_event_options_from_model_event(
    const xwork_model_event *pEvent,
    xwork_replay_event_options *pOptions
)
{
    if ( !pOptions ) {
        return;
    }
    xwork_replay_event_options_init(pOptions);
    if ( !pEvent ) {
        return;
    }
    pOptions->eKind = XWORK_REPLAY_EVENT_MODEL_STREAM;
    pOptions->sName = "xllm.model_event";
    pOptions->iType = pEvent->eType;
    pOptions->sKey = pEvent->sResponseId
        ? pEvent->sResponseId
        : (pEvent->sToolCallId
            ? pEvent->sToolCallId
            : pEvent->sArtifactId);
    pOptions->sContentText = pEvent->sText
        ? pEvent->sText
        : (pEvent->sArgumentsDelta
            ? pEvent->sArgumentsDelta
            : pEvent->sFormat);
}

void xwork_replay_event_summary_init(xwork_replay_event_summary *pSummary)
{
    if ( pSummary ) {
        memset(pSummary, 0, sizeof(*pSummary));
        pSummary->eKind = XWORK_REPLAY_EVENT_GENERIC;
        pSummary->iStatus = XWORK_OK;
    }
}

void xwork_replay_event_summary_reset(xwork_replay_event_summary *pSummary)
{
    if ( !pSummary ) {
        return;
    }
    free((char *)pSummary->sKey);
    free((char *)pSummary->sName);
    free((char *)pSummary->sPayloadHash);
    free((char *)pSummary->sContentHash);
    xwork_replay_event_summary_init(pSummary);
}

void xwork_replay_event_summary_list_init(xwork_replay_event_summary_list *pList)
{
    if ( pList ) {
        memset(pList, 0, sizeof(*pList));
    }
}

void xwork_replay_event_summary_list_reset(xwork_replay_event_summary_list *pList)
{
    xwork_replay_event_summary *pItems;
    size_t i;

    if ( !pList ) {
        return;
    }
    pItems = pList->pItems;
    if ( pItems ) {
        for ( i = 0u; i < pList->iCount; ++i ) {
            xwork_replay_event_summary_reset(&pItems[i]);
        }
        free(pItems);
    }
    xwork_replay_event_summary_list_init(pList);
}

void xwork_replay_divergence_init(xwork_replay_divergence *pDivergence)
{
    if ( pDivergence ) {
        memset(pDivergence, 0, sizeof(*pDivergence));
    }
}

void xwork_replay_divergence_reset(xwork_replay_divergence *pDivergence)
{
    if ( !pDivergence ) {
        return;
    }
    free((char *)pDivergence->sExpectedKey);
    free((char *)pDivergence->sActualKey);
    free((char *)pDivergence->sExpectedHash);
    free((char *)pDivergence->sActualHash);
    free((char *)pDivergence->sMessage);
    xwork_replay_divergence_init(pDivergence);
}

void xwork_replay_result_init(xwork_replay_result *pResult)
{
    if ( pResult ) {
        memset(pResult, 0, sizeof(*pResult));
        pResult->iStatus = XWORK_OK;
        xwork_replay_divergence_init(&pResult->tFirstDivergence);
    }
}

void xwork_replay_result_reset(xwork_replay_result *pResult)
{
    if ( !pResult ) {
        return;
    }
    xwork_replay_divergence_reset(&pResult->tFirstDivergence);
    xwork_replay_result_init(pResult);
}

typedef struct xwork__json_buffer {
    char *sData;
    size_t iSize;
    size_t iCapacity;
} xwork__json_buffer;

typedef struct xwork__json_member {
    char *sKey;
    char *sValue;
} xwork__json_member;

static void xwork__json_buffer_reset(xwork__json_buffer *pBuffer)
{
    if ( pBuffer ) {
        free(pBuffer->sData);
        memset(pBuffer, 0, sizeof(*pBuffer));
    }
}

static xwork_status xwork__json_buffer_reserve(
    xwork__json_buffer *pBuffer,
    size_t iExtra
)
{
    size_t iNeeded;
    size_t iCapacity;
    char *sData;

    if ( !pBuffer ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    iNeeded = pBuffer->iSize + iExtra + 1u;
    if ( iNeeded <= pBuffer->iCapacity ) {
        return XWORK_OK;
    }
    iCapacity = pBuffer->iCapacity ? pBuffer->iCapacity : 64u;
    while ( iCapacity < iNeeded ) {
        if ( iCapacity > ((size_t)-1) / 2u ) {
            return XWORK_ERROR_NO_MEMORY;
        }
        iCapacity *= 2u;
    }
    sData = (char *)realloc(pBuffer->sData, iCapacity);
    if ( !sData ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    pBuffer->sData = sData;
    pBuffer->iCapacity = iCapacity;
    return XWORK_OK;
}

static xwork_status xwork__json_buffer_append_char(
    xwork__json_buffer *pBuffer,
    char c
)
{
    xwork_status iStatus = xwork__json_buffer_reserve(pBuffer, 1u);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    pBuffer->sData[pBuffer->iSize++] = c;
    pBuffer->sData[pBuffer->iSize] = '\0';
    return XWORK_OK;
}

static xwork_status xwork__json_buffer_append_text(
    xwork__json_buffer *pBuffer,
    const char *sText,
    size_t iSize
)
{
    xwork_status iStatus;

    if ( !sText && iSize ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    iStatus = xwork__json_buffer_reserve(pBuffer, iSize);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    if ( iSize ) {
        memcpy(pBuffer->sData + pBuffer->iSize, sText, iSize);
        pBuffer->iSize += iSize;
    }
    pBuffer->sData[pBuffer->iSize] = '\0';
    return XWORK_OK;
}

static const char *xwork__json_skip_ws(const char *p)
{
    while ( p && *p && isspace((unsigned char)*p) ) {
        ++p;
    }
    return p;
}

static bool xwork__json_is_hex(char c)
{
    return (c >= '0' && c <= '9') ||
        (c >= 'a' && c <= 'f') ||
        (c >= 'A' && c <= 'F');
}

static int xwork__json_hex_value(char c)
{
    if ( c >= '0' && c <= '9' ) return c - '0';
    if ( c >= 'a' && c <= 'f' ) return 10 + c - 'a';
    if ( c >= 'A' && c <= 'F' ) return 10 + c - 'A';
    return -1;
}

static xwork_status xwork__json_append_escaped_ascii(
    xwork__json_buffer *pBuffer,
    unsigned int iCodepoint
)
{
    char sEscaped[7];

    switch ( iCodepoint ) {
    case '"': return xwork__json_buffer_append_text(pBuffer, "\\\"", 2u);
    case '\\': return xwork__json_buffer_append_text(pBuffer, "\\\\", 2u);
    case '\b': return xwork__json_buffer_append_text(pBuffer, "\\b", 2u);
    case '\f': return xwork__json_buffer_append_text(pBuffer, "\\f", 2u);
    case '\n': return xwork__json_buffer_append_text(pBuffer, "\\n", 2u);
    case '\r': return xwork__json_buffer_append_text(pBuffer, "\\r", 2u);
    case '\t': return xwork__json_buffer_append_text(pBuffer, "\\t", 2u);
    default:
        if ( iCodepoint < 0x20u ) {
            snprintf(sEscaped, sizeof(sEscaped), "\\u%04x", iCodepoint);
            return xwork__json_buffer_append_text(pBuffer, sEscaped, 6u);
        }
        return xwork__json_buffer_append_char(pBuffer, (char)iCodepoint);
    }
}

static xwork_status xwork__json_normalize_string(
    const char **pp,
    xwork__json_buffer *pOut
)
{
    const char *p;
    xwork_status iStatus;

    if ( !pp || !*pp || **pp != '"' ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    p = *pp + 1;
    iStatus = xwork__json_buffer_append_char(pOut, '"');
    if ( iStatus != XWORK_OK ) return iStatus;
    while ( *p ) {
        unsigned char c = (unsigned char)*p++;
        if ( c == '"' ) {
            iStatus = xwork__json_buffer_append_char(pOut, '"');
            if ( iStatus != XWORK_OK ) return iStatus;
            *pp = p;
            return XWORK_OK;
        }
        if ( c == '\\' ) {
            unsigned int iCodepoint;
            char sEscaped[7];
            char e = *p++;
            switch ( e ) {
            case '"':
            case '\\':
            case '/':
                iStatus = xwork__json_append_escaped_ascii(pOut, (unsigned int)e);
                if ( iStatus != XWORK_OK ) return iStatus;
                break;
            case 'b': iStatus = xwork__json_append_escaped_ascii(pOut, '\b'); if ( iStatus != XWORK_OK ) return iStatus; break;
            case 'f': iStatus = xwork__json_append_escaped_ascii(pOut, '\f'); if ( iStatus != XWORK_OK ) return iStatus; break;
            case 'n': iStatus = xwork__json_append_escaped_ascii(pOut, '\n'); if ( iStatus != XWORK_OK ) return iStatus; break;
            case 'r': iStatus = xwork__json_append_escaped_ascii(pOut, '\r'); if ( iStatus != XWORK_OK ) return iStatus; break;
            case 't': iStatus = xwork__json_append_escaped_ascii(pOut, '\t'); if ( iStatus != XWORK_OK ) return iStatus; break;
            case 'u':
                if ( !xwork__json_is_hex(p[0]) ||
                     !xwork__json_is_hex(p[1]) ||
                     !xwork__json_is_hex(p[2]) ||
                     !xwork__json_is_hex(p[3]) ) {
                    return XWORK_ERROR_INVALID_ARGUMENT;
                }
                iCodepoint =
                    ((unsigned int)xwork__json_hex_value(p[0]) << 12) |
                    ((unsigned int)xwork__json_hex_value(p[1]) << 8) |
                    ((unsigned int)xwork__json_hex_value(p[2]) << 4) |
                    (unsigned int)xwork__json_hex_value(p[3]);
                p += 4;
                if ( iCodepoint >= 0x20u && iCodepoint <= 0x7eu ) {
                    iStatus = xwork__json_append_escaped_ascii(pOut, iCodepoint);
                } else {
                    snprintf(sEscaped, sizeof(sEscaped), "\\u%04x", iCodepoint);
                    iStatus = xwork__json_buffer_append_text(pOut, sEscaped, 6u);
                }
                if ( iStatus != XWORK_OK ) return iStatus;
                break;
            default:
                return XWORK_ERROR_INVALID_ARGUMENT;
            }
        } else {
            if ( c < 0x20u ) {
                return XWORK_ERROR_INVALID_ARGUMENT;
            }
            iStatus = xwork__json_append_escaped_ascii(pOut, c);
            if ( iStatus != XWORK_OK ) return iStatus;
        }
    }
    return XWORK_ERROR_INVALID_ARGUMENT;
}

static xwork_status xwork__json_normalize_value(
    const char **pp,
    xwork__json_buffer *pOut
);

static int xwork__json_member_compare(const void *a, const void *b)
{
    const xwork__json_member *pA = (const xwork__json_member *)a;
    const xwork__json_member *pB = (const xwork__json_member *)b;
    return strcmp(pA->sKey ? pA->sKey : "", pB->sKey ? pB->sKey : "");
}

static void xwork__json_member_list_reset(
    xwork__json_member *pItems,
    size_t iCount
)
{
    size_t i;
    for ( i = 0u; i < iCount; ++i ) {
        free(pItems[i].sKey);
        free(pItems[i].sValue);
    }
    free(pItems);
}

static xwork_status xwork__json_normalize_object(
    const char **pp,
    xwork__json_buffer *pOut
)
{
    const char *p;
    xwork__json_member *pMembers = NULL;
    size_t iCount = 0u;
    size_t iCapacity = 0u;
    size_t i;
    xwork_status iStatus = XWORK_OK;

    if ( !pp || !*pp || **pp != '{' ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    p = xwork__json_skip_ws(*pp + 1);
    if ( *p != '}' ) {
        while ( true ) {
            xwork__json_buffer tKey = {0};
            xwork__json_buffer tValue = {0};
            xwork__json_member *pNewItems;
            const char *pValue;

            if ( *p != '"' ) {
                iStatus = XWORK_ERROR_INVALID_ARGUMENT;
                goto done;
            }
            iStatus = xwork__json_normalize_string(&p, &tKey);
            if ( iStatus != XWORK_OK ) {
                xwork__json_buffer_reset(&tKey);
                goto done;
            }
            p = xwork__json_skip_ws(p);
            if ( *p != ':' ) {
                xwork__json_buffer_reset(&tKey);
                iStatus = XWORK_ERROR_INVALID_ARGUMENT;
                goto done;
            }
            pValue = xwork__json_skip_ws(p + 1);
            iStatus = xwork__json_normalize_value(&pValue, &tValue);
            if ( iStatus != XWORK_OK ) {
                xwork__json_buffer_reset(&tKey);
                xwork__json_buffer_reset(&tValue);
                goto done;
            }
            if ( iCount == iCapacity ) {
                size_t iNewCapacity = iCapacity ? iCapacity * 2u : 8u;
                pNewItems = (xwork__json_member *)realloc(
                    pMembers,
                    iNewCapacity * sizeof(*pMembers)
                );
                if ( !pNewItems ) {
                    xwork__json_buffer_reset(&tKey);
                    xwork__json_buffer_reset(&tValue);
                    iStatus = XWORK_ERROR_NO_MEMORY;
                    goto done;
                }
                pMembers = pNewItems;
                memset(pMembers + iCapacity, 0, (iNewCapacity - iCapacity) * sizeof(*pMembers));
                iCapacity = iNewCapacity;
            }
            pMembers[iCount].sKey = tKey.sData;
            pMembers[iCount].sValue = tValue.sData;
            ++iCount;
            p = xwork__json_skip_ws(pValue);
            if ( *p == '}' ) {
                break;
            }
            if ( *p != ',' ) {
                iStatus = XWORK_ERROR_INVALID_ARGUMENT;
                goto done;
            }
            p = xwork__json_skip_ws(p + 1);
        }
    }
    qsort(pMembers, iCount, sizeof(*pMembers), xwork__json_member_compare);
    iStatus = xwork__json_buffer_append_char(pOut, '{');
    if ( iStatus != XWORK_OK ) goto done;
    for ( i = 0u; i < iCount; ++i ) {
        if ( i ) {
            iStatus = xwork__json_buffer_append_char(pOut, ',');
            if ( iStatus != XWORK_OK ) goto done;
        }
        iStatus = xwork__json_buffer_append_text(pOut, pMembers[i].sKey, strlen(pMembers[i].sKey));
        if ( iStatus != XWORK_OK ) goto done;
        iStatus = xwork__json_buffer_append_char(pOut, ':');
        if ( iStatus != XWORK_OK ) goto done;
        iStatus = xwork__json_buffer_append_text(pOut, pMembers[i].sValue, strlen(pMembers[i].sValue));
        if ( iStatus != XWORK_OK ) goto done;
    }
    iStatus = xwork__json_buffer_append_char(pOut, '}');
    if ( iStatus != XWORK_OK ) goto done;
    *pp = *p == '}' ? p + 1 : p;

done:
    xwork__json_member_list_reset(pMembers, iCount);
    return iStatus;
}

static xwork_status xwork__json_normalize_array(
    const char **pp,
    xwork__json_buffer *pOut
)
{
    const char *p;
    bool bFirst = true;
    xwork_status iStatus;

    if ( !pp || !*pp || **pp != '[' ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    iStatus = xwork__json_buffer_append_char(pOut, '[');
    if ( iStatus != XWORK_OK ) return iStatus;
    p = xwork__json_skip_ws(*pp + 1);
    while ( *p && *p != ']' ) {
        if ( !bFirst ) {
            if ( *p != ',' ) {
                return XWORK_ERROR_INVALID_ARGUMENT;
            }
            iStatus = xwork__json_buffer_append_char(pOut, ',');
            if ( iStatus != XWORK_OK ) return iStatus;
            p = xwork__json_skip_ws(p + 1);
        }
        iStatus = xwork__json_normalize_value(&p, pOut);
        if ( iStatus != XWORK_OK ) return iStatus;
        p = xwork__json_skip_ws(p);
        bFirst = false;
    }
    if ( *p != ']' ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    iStatus = xwork__json_buffer_append_char(pOut, ']');
    if ( iStatus != XWORK_OK ) return iStatus;
    *pp = p + 1;
    return XWORK_OK;
}

static xwork_status xwork__json_normalize_number(
    const char **pp,
    xwork__json_buffer *pOut
)
{
    const char *p;
    bool bHasDigit = false;
    bool bInExponent = false;
    xwork_status iStatus;

    if ( !pp || !*pp ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    p = *pp;
    if ( *p == '-' ) {
        iStatus = xwork__json_buffer_append_char(pOut, *p++);
        if ( iStatus != XWORK_OK ) return iStatus;
    }
    while ( isdigit((unsigned char)*p) ) {
        bHasDigit = true;
        iStatus = xwork__json_buffer_append_char(pOut, *p++);
        if ( iStatus != XWORK_OK ) return iStatus;
    }
    if ( !bHasDigit ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( *p == '.' ) {
        iStatus = xwork__json_buffer_append_char(pOut, *p++);
        if ( iStatus != XWORK_OK ) return iStatus;
        if ( !isdigit((unsigned char)*p) ) {
            return XWORK_ERROR_INVALID_ARGUMENT;
        }
        while ( isdigit((unsigned char)*p) ) {
            iStatus = xwork__json_buffer_append_char(pOut, *p++);
            if ( iStatus != XWORK_OK ) return iStatus;
        }
    }
    if ( *p == 'e' || *p == 'E' ) {
        bInExponent = true;
        iStatus = xwork__json_buffer_append_char(pOut, 'e');
        if ( iStatus != XWORK_OK ) return iStatus;
        ++p;
        if ( *p == '-' ) {
            iStatus = xwork__json_buffer_append_char(pOut, *p++);
            if ( iStatus != XWORK_OK ) return iStatus;
        } else if ( *p == '+' ) {
            ++p;
        }
        if ( !isdigit((unsigned char)*p) ) {
            return XWORK_ERROR_INVALID_ARGUMENT;
        }
        while ( isdigit((unsigned char)*p) ) {
            iStatus = xwork__json_buffer_append_char(pOut, *p++);
            if ( iStatus != XWORK_OK ) return iStatus;
        }
    }
    (void)bInExponent;
    *pp = p;
    return XWORK_OK;
}

static xwork_status xwork__json_normalize_literal(
    const char **pp,
    xwork__json_buffer *pOut,
    const char *sLiteral
)
{
    size_t iSize = strlen(sLiteral);
    if ( strncmp(*pp, sLiteral, iSize) != 0 ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    *pp += iSize;
    return xwork__json_buffer_append_text(pOut, sLiteral, iSize);
}

static xwork_status xwork__json_normalize_value(
    const char **pp,
    xwork__json_buffer *pOut
)
{
    const char *p;
    xwork_status iStatus;

    if ( !pp || !*pp || !pOut ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    p = xwork__json_skip_ws(*pp);
    switch ( *p ) {
    case '{':
        iStatus = xwork__json_normalize_object(&p, pOut);
        break;
    case '[':
        iStatus = xwork__json_normalize_array(&p, pOut);
        break;
    case '"':
        iStatus = xwork__json_normalize_string(&p, pOut);
        break;
    case 't':
        iStatus = xwork__json_normalize_literal(&p, pOut, "true");
        break;
    case 'f':
        iStatus = xwork__json_normalize_literal(&p, pOut, "false");
        break;
    case 'n':
        iStatus = xwork__json_normalize_literal(&p, pOut, "null");
        break;
    default:
        if ( *p == '-' || isdigit((unsigned char)*p) ) {
            iStatus = xwork__json_normalize_number(&p, pOut);
        } else {
            iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        }
        break;
    }
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    *pp = p;
    return XWORK_OK;
}

xwork_status xwork_replay_hash_text(
    const char *sText,
    char *sBuffer,
    size_t iBufferSize
)
{
    const unsigned char *p;
    unsigned long long iHash = 1469598103934665603ull;

    if ( !sBuffer || iBufferSize < 17u ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( sText ) {
        for ( p = (const unsigned char *)sText; *p; ++p ) {
            iHash ^= (unsigned long long)(*p);
            iHash *= 1099511628211ull;
        }
    }
    snprintf(sBuffer, iBufferSize, "%016llx", iHash);
    return XWORK_OK;
}

xwork_status xwork_replay_hash_json(
    const char *sJson,
    char *sBuffer,
    size_t iBufferSize
)
{
    xwork__json_buffer tNormalized = {0};
    const char *p;
    xwork_status iStatus;

    if ( !sJson || !sBuffer || iBufferSize < 17u ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    p = xwork__json_skip_ws(sJson);
    iStatus = xwork__json_normalize_value(&p, &tNormalized);
    if ( iStatus != XWORK_OK ) {
        xwork__json_buffer_reset(&tNormalized);
        return iStatus;
    }
    p = xwork__json_skip_ws(p);
    if ( *p != '\0' ) {
        xwork__json_buffer_reset(&tNormalized);
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    iStatus = xwork_replay_hash_text(
        tNormalized.sData ? tNormalized.sData : "",
        sBuffer,
        iBufferSize
    );
    xwork__json_buffer_reset(&tNormalized);
    return iStatus;
}

xwork_status xwork_replay_engine_create(
    const xwork_replay_options *pOptions,
    xwork_replay_engine **ppEngine
)
{
    xwork_replay_options tOptions;
    xwork_replay_engine *pEngine;

    if ( !ppEngine ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    *ppEngine = NULL;
    if ( pOptions ) {
        tOptions = *pOptions;
    } else {
        xwork_replay_options_init(&tOptions);
    }
    if ( tOptions.eMode != XWORK_REPLAY_MODE_RECORD &&
         tOptions.eMode != XWORK_REPLAY_MODE_STRICT &&
         tOptions.eMode != XWORK_REPLAY_MODE_AUDIT ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    pEngine = (xwork_replay_engine *)calloc(1u, sizeof(*pEngine));
    if ( !pEngine ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    pEngine->eMode = tOptions.eMode;
    pEngine->bReadonlyFilesystem = tOptions.bReadonlyFilesystem;
    pEngine->bBlockSideEffects = tOptions.bBlockSideEffects;
    pEngine->iMaxDivergences = tOptions.iMaxDivergences
        ? tOptions.iMaxDivergences
        : 1u;
    pEngine->iNextSequence = 1u;
    pEngine->iNextEventSequence = 1u;
    pEngine->sReplayId = xwork__dup_cstr(
        tOptions.sReplayId ? tOptions.sReplayId : "replay"
    );
    if ( !pEngine->sReplayId ) {
        free(pEngine);
        return XWORK_ERROR_NO_MEMORY;
    }
    xwork_replay_divergence_init(&pEngine->tFirstDivergence);
    *ppEngine = pEngine;
    return XWORK_OK;
}

void xwork_replay_engine_destroy(xwork_replay_engine *pEngine)
{
    xwork__replay_entry_record *pEntry;
    xwork__replay_event_record *pEvent;

    if ( !pEngine ) {
        return;
    }
    while ( pEngine->pEntries ) {
        pEntry = pEngine->pEntries;
        pEngine->pEntries = pEntry->pNext;
        xwork__replay_free_entry(pEntry);
    }
    while ( pEngine->pEvents ) {
        pEvent = pEngine->pEvents;
        pEngine->pEvents = pEvent->pNext;
        xwork__replay_free_event(pEvent);
    }
    free(pEngine->sReplayId);
    free(pEngine->sCancelReason);
    xwork_replay_divergence_reset(&pEngine->tFirstDivergence);
    free(pEngine);
}

xwork_replay_mode xwork_replay_engine_get_mode(const xwork_replay_engine *pEngine)
{
    return pEngine ? pEngine->eMode : XWORK_REPLAY_MODE_RECORD;
}

bool xwork_replay_engine_blocks_side_effects(const xwork_replay_engine *pEngine)
{
    return pEngine ? pEngine->bBlockSideEffects : false;
}

xwork_status xwork_replay_engine_record_entry(
    xwork_replay_engine *pEngine,
    const xwork_replay_entry_options *pEntryOptions
)
{
    if ( !pEngine || !pEntryOptions ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( pEngine->eMode != XWORK_REPLAY_MODE_RECORD ) {
        return XWORK_ERROR_INVALID_STATE;
    }
    return xwork__replay_append_entry(pEngine, pEntryOptions, true);
}

xwork_status xwork_replay_engine_load_entry(
    xwork_replay_engine *pEngine,
    const xwork_replay_entry_options *pEntryOptions
)
{
    return xwork__replay_append_entry(pEngine, pEntryOptions, false);
}

xwork_status xwork_replay_engine_replay_entry(
    xwork_replay_engine *pEngine,
    const xwork_replay_entry_options *pExpected,
    xwork_replay_entry_summary *pActual
)
{
    xwork__replay_entry_record *pEntry;
    char *sExpectedRequestHash = NULL;
    char *sExpectedResponseHash = NULL;
    char *sExpectedArgumentsHash = NULL;
    char *sExpectedResultHash = NULL;
    xwork_status iStatus;
    bool bDiverged = false;

    if ( !pEngine || !pExpected ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( pActual ) {
        xwork_replay_entry_summary_reset(pActual);
    }
    if ( pEngine->bCancelled ) {
        return XWORK_ERROR_CANCELLED;
    }
    if ( pEngine->eMode == XWORK_REPLAY_MODE_RECORD ) {
        return XWORK_ERROR_INVALID_STATE;
    }
    pEntry = xwork__replay_entry_at(pEngine, pEngine->iReplayCursor);
    if ( !pEntry ) {
        iStatus = xwork__replay_store_divergence(
            pEngine,
            XWORK_REPLAY_DIVERGENCE_MISSING_ENTRY,
            NULL,
            pExpected,
            NULL,
            NULL,
            "missing replay cassette entry"
        );
        if ( iStatus != XWORK_OK ) return iStatus;
        return pEngine->eMode == XWORK_REPLAY_MODE_STRICT
            ? XWORK_ERROR_EXTERNAL_FAILURE
            : XWORK_OK;
    }
    if ( pActual ) {
        iStatus = xwork__replay_copy_summary(pActual, pEntry);
        if ( iStatus != XWORK_OK ) return iStatus;
    }
    iStatus = xwork__replay_dup_hash(
        pExpected->sRequestJson,
        pExpected->sRequestHash,
        true,
        &sExpectedRequestHash
    );
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replay_dup_hash(
        pExpected->sResponseJson,
        pExpected->sResponseHash,
        true,
        &sExpectedResponseHash
    );
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replay_dup_hash(
        pExpected->sArgumentsJson,
        pExpected->sArgumentsHash,
        true,
        &sExpectedArgumentsHash
    );
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replay_dup_hash(
        pExpected->sResultJson,
        pExpected->sResultHash,
        true,
        &sExpectedResultHash
    );
    if ( iStatus != XWORK_OK ) goto done;

    if ( pEntry->eKind != pExpected->eKind ) {
        bDiverged = true;
        iStatus = xwork__replay_store_divergence(
            pEngine,
            XWORK_REPLAY_DIVERGENCE_KIND_MISMATCH,
            pEntry,
            pExpected,
            NULL,
            NULL,
            "replay entry kind mismatch"
        );
        goto done;
    }
    if ( !xwork__replay_cstr_equal(pEntry->sKey, pExpected->sKey) ) {
        bDiverged = true;
        iStatus = xwork__replay_store_divergence(
            pEngine,
            XWORK_REPLAY_DIVERGENCE_KEY_MISMATCH,
            pEntry,
            pExpected,
            NULL,
            NULL,
            "replay entry key mismatch"
        );
        goto done;
    }
    if ( !xwork__replay_hash_equal_compat(
             pEntry->sRequestHash,
             sExpectedRequestHash,
             pExpected->sRequestJson
         ) ||
         !xwork__replay_hash_equal_compat(
             pEntry->sArgumentsHash,
             sExpectedArgumentsHash,
             pExpected->sArgumentsJson
         ) ) {
        bDiverged = true;
        iStatus = xwork__replay_store_divergence(
            pEngine,
            XWORK_REPLAY_DIVERGENCE_REQUEST_MISMATCH,
            pEntry,
            pExpected,
            sExpectedRequestHash ? sExpectedRequestHash : sExpectedArgumentsHash,
            pEntry->sRequestHash ? pEntry->sRequestHash : pEntry->sArgumentsHash,
            "replay request mismatch"
        );
        goto done;
    }
    if ( (sExpectedResponseHash || sExpectedResultHash || pExpected->sContentHash) &&
         (!xwork__replay_hash_equal_compat(
              pEntry->sResponseHash,
              sExpectedResponseHash,
              pExpected->sResponseJson
          ) ||
          !xwork__replay_hash_equal_compat(
              pEntry->sResultHash,
              sExpectedResultHash,
              pExpected->sResultJson
          ) ||
          !xwork__replay_cstr_equal(pEntry->sContentHash, pExpected->sContentHash)) ) {
        bDiverged = true;
        iStatus = xwork__replay_store_divergence(
            pEngine,
            XWORK_REPLAY_DIVERGENCE_RESPONSE_MISMATCH,
            pEntry,
            pExpected,
            sExpectedResponseHash ? sExpectedResponseHash : sExpectedResultHash,
            pEntry->sResponseHash ? pEntry->sResponseHash : pEntry->sResultHash,
            "replay response mismatch"
        );
        goto done;
    }
    if ( pEntry->iStatus != pExpected->iStatus ) {
        bDiverged = true;
        iStatus = xwork__replay_store_divergence(
            pEngine,
            XWORK_REPLAY_DIVERGENCE_STATUS_MISMATCH,
            pEntry,
            pExpected,
            NULL,
            NULL,
            "replay status mismatch"
        );
        goto done;
    }
    ++pEngine->iReplayedCount;
    ++pEngine->iReplayCursor;
    iStatus = XWORK_OK;

done:
    free(sExpectedRequestHash);
    free(sExpectedResponseHash);
    free(sExpectedArgumentsHash);
    free(sExpectedResultHash);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    if ( bDiverged ) {
        return pEngine->eMode == XWORK_REPLAY_MODE_STRICT
            ? XWORK_ERROR_EXTERNAL_FAILURE
            : XWORK_OK;
    }
    return XWORK_OK;
}

xwork_status xwork_replay_engine_record_filesystem_ref(
    xwork_replay_engine *pEngine,
    const xwork_replay_filesystem_ref_options *pRef
)
{
    xwork_replay_entry_options tEntry;

    if ( !pEngine || !pRef || !pRef->sRefId || !pRef->sRefId[0] ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork__replay_filesystem_ref_to_entry_options(pRef, &tEntry);
    return xwork_replay_engine_record_entry(pEngine, &tEntry);
}

xwork_status xwork_replay_engine_load_filesystem_ref(
    xwork_replay_engine *pEngine,
    const xwork_replay_filesystem_ref_options *pRef
)
{
    xwork_replay_entry_options tEntry;

    if ( !pEngine || !pRef || !pRef->sRefId || !pRef->sRefId[0] ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork__replay_filesystem_ref_to_entry_options(pRef, &tEntry);
    return xwork_replay_engine_load_entry(pEngine, &tEntry);
}

xwork_status xwork_replay_engine_replay_filesystem_ref(
    xwork_replay_engine *pEngine,
    const xwork_replay_filesystem_ref_options *pExpected,
    xwork_replay_filesystem_ref_summary *pActual
)
{
    xwork_replay_entry_options tEntry;
    xwork_replay_entry_summary tActualEntry;
    xwork_status iStatus;

    if ( !pEngine || !pExpected || !pExpected->sRefId || !pExpected->sRefId[0] ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork__replay_filesystem_ref_to_entry_options(pExpected, &tEntry);
    xwork_replay_entry_summary_init(&tActualEntry);
    iStatus = xwork_replay_engine_replay_entry(
        pEngine,
        &tEntry,
        pActual ? &tActualEntry : NULL
    );
    if ( iStatus == XWORK_OK && pActual ) {
        xwork_replay_filesystem_ref_summary_reset(pActual);
        pActual->iSequence = tActualEntry.iSequence;
        pActual->iStatus = tActualEntry.iStatus;
        iStatus = xwork__replace_cstr((char **)&pActual->sRefId, tActualEntry.sKey);
        if ( iStatus == XWORK_OK ) {
            iStatus = xwork__replace_cstr((char **)&pActual->sPath, tActualEntry.sRequestJson);
        }
        if ( iStatus == XWORK_OK ) {
            iStatus = xwork__replace_cstr((char **)&pActual->sMetadataJson, tActualEntry.sArgumentsJson);
        }
        if ( iStatus == XWORK_OK ) {
            iStatus = xwork__replace_cstr((char **)&pActual->sContentHash, tActualEntry.sContentHash);
        }
    }
    xwork_replay_entry_summary_reset(&tActualEntry);
    return iStatus;
}

xwork_status xwork_replay_engine_list_filesystem_refs(
    const xwork_replay_engine *pEngine,
    xwork_replay_filesystem_ref_summary_list *pList
)
{
    xwork__replay_entry_record *pEntry;
    xwork_replay_filesystem_ref_summary *pItems;
    size_t iCount = 0u;
    size_t i = 0u;
    xwork_status iStatus = XWORK_OK;

    if ( !pEngine || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_replay_filesystem_ref_summary_list_reset(pList);
    for ( pEntry = pEngine->pEntries; pEntry; pEntry = pEntry->pNext ) {
        if ( xwork__replay_entry_is_filesystem_ref(pEntry) ) {
            ++iCount;
        }
    }
    if ( iCount == 0u ) {
        return XWORK_OK;
    }
    pItems = (xwork_replay_filesystem_ref_summary *)calloc(iCount, sizeof(*pItems));
    if ( !pItems ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    for ( i = 0u; i < iCount; ++i ) {
        xwork_replay_filesystem_ref_summary_init(&pItems[i]);
    }
    i = 0u;
    for ( pEntry = pEngine->pEntries; pEntry; pEntry = pEntry->pNext ) {
        if ( xwork__replay_entry_is_filesystem_ref(pEntry) ) {
            iStatus = xwork__replay_copy_filesystem_ref_from_entry(&pItems[i], pEntry);
            if ( iStatus != XWORK_OK ) {
                break;
            }
            ++i;
        }
    }
    if ( iStatus != XWORK_OK ) {
        xwork_replay_filesystem_ref_summary_list tList;
        tList.pItems = pItems;
        tList.iCount = iCount;
        xwork_replay_filesystem_ref_summary_list_reset(&tList);
        return iStatus;
    }
    pList->pItems = pItems;
    pList->iCount = iCount;
    return XWORK_OK;
}

xwork_status xwork_replay_engine_record_event(
    xwork_replay_engine *pEngine,
    const xwork_replay_event_options *pEvent
)
{
    if ( !pEngine || !pEvent ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( pEngine->eMode != XWORK_REPLAY_MODE_RECORD ) {
        return XWORK_ERROR_INVALID_STATE;
    }
    return xwork__replay_append_event(pEngine, pEvent);
}

xwork_status xwork_replay_engine_load_event(
    xwork_replay_engine *pEngine,
    const xwork_replay_event_options *pEvent
)
{
    return xwork__replay_append_event(pEngine, pEvent);
}

xwork_status xwork_replay_engine_replay_event(
    xwork_replay_engine *pEngine,
    const xwork_replay_event_options *pExpected,
    xwork_replay_event_summary *pActual
)
{
    xwork__replay_event_record *pEvent;
    char *sExpectedPayloadHash = NULL;
    char *sExpectedContentHash = NULL;
    xwork_status iStatus;
    bool bDiverged = false;

    if ( !pEngine || !pExpected ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( pActual ) {
        xwork_replay_event_summary_reset(pActual);
    }
    if ( pEngine->bCancelled ) {
        return XWORK_ERROR_CANCELLED;
    }
    if ( pEngine->eMode == XWORK_REPLAY_MODE_RECORD ) {
        return XWORK_ERROR_INVALID_STATE;
    }
    pEvent = xwork__replay_event_at(pEngine, pEngine->iEventReplayCursor);
    if ( !pEvent ) {
        iStatus = xwork__replay_store_event_divergence(
            pEngine,
            XWORK_REPLAY_DIVERGENCE_MISSING_ENTRY,
            NULL,
            pExpected,
            NULL,
            NULL,
            "missing replay event log entry"
        );
        if ( iStatus != XWORK_OK ) return iStatus;
        return pEngine->eMode == XWORK_REPLAY_MODE_STRICT
            ? XWORK_ERROR_EXTERNAL_FAILURE
            : XWORK_OK;
    }
    if ( pActual ) {
        iStatus = xwork__replay_copy_event_summary(pActual, pEvent);
        if ( iStatus != XWORK_OK ) return iStatus;
    }
    iStatus = xwork__replay_dup_hash(
        pExpected->sPayloadJson,
        pExpected->sPayloadHash,
        true,
        &sExpectedPayloadHash
    );
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replay_dup_hash(
        pExpected->sContentText,
        pExpected->sContentHash,
        false,
        &sExpectedContentHash
    );
    if ( iStatus != XWORK_OK ) goto done;

    if ( pEvent->eKind != pExpected->eKind ) {
        bDiverged = true;
        iStatus = xwork__replay_store_event_divergence(
            pEngine,
            XWORK_REPLAY_DIVERGENCE_KIND_MISMATCH,
            pEvent,
            pExpected,
            NULL,
            NULL,
            "replay event kind mismatch"
        );
        goto done;
    }
    if ( !xwork__replay_cstr_equal(pEvent->sKey, pExpected->sKey) ||
         !xwork__replay_cstr_equal(pEvent->sName, pExpected->sName) ||
         pEvent->iType != pExpected->iType ) {
        bDiverged = true;
        iStatus = xwork__replay_store_event_divergence(
            pEngine,
            XWORK_REPLAY_DIVERGENCE_KEY_MISMATCH,
            pEvent,
            pExpected,
            NULL,
            NULL,
            "replay event identity mismatch"
        );
        goto done;
    }
    if ( !xwork__replay_hash_equal_compat(
             pEvent->sPayloadHash,
             sExpectedPayloadHash,
             pExpected->sPayloadJson
         ) ||
         !xwork__replay_hash_equal_compat(
             pEvent->sContentHash,
             sExpectedContentHash,
             pExpected->sContentText
         ) ) {
        bDiverged = true;
        iStatus = xwork__replay_store_event_divergence(
            pEngine,
            XWORK_REPLAY_DIVERGENCE_CONTENT_MISMATCH,
            pEvent,
            pExpected,
            sExpectedPayloadHash ? sExpectedPayloadHash : sExpectedContentHash,
            pEvent->sPayloadHash ? pEvent->sPayloadHash : pEvent->sContentHash,
            "replay event content mismatch"
        );
        goto done;
    }
    if ( pEvent->iStatus != pExpected->iStatus ) {
        bDiverged = true;
        iStatus = xwork__replay_store_event_divergence(
            pEngine,
            XWORK_REPLAY_DIVERGENCE_STATUS_MISMATCH,
            pEvent,
            pExpected,
            NULL,
            NULL,
            "replay event status mismatch"
        );
        goto done;
    }
    ++pEngine->iReplayedEventCount;
    ++pEngine->iEventReplayCursor;
    iStatus = XWORK_OK;

done:
    free(sExpectedPayloadHash);
    free(sExpectedContentHash);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    if ( bDiverged ) {
        return pEngine->eMode == XWORK_REPLAY_MODE_STRICT
            ? XWORK_ERROR_EXTERNAL_FAILURE
            : XWORK_OK;
    }
    return XWORK_OK;
}

xwork_status xwork_replay_engine_seek_checkpoint(
    xwork_replay_engine *pEngine,
    const char *sCheckpointId
)
{
    xwork__replay_entry_record *pEntry;
    size_t iIndex = 0u;

    if ( !pEngine || !sCheckpointId || !sCheckpointId[0] ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( pEngine->bCancelled ) {
        return XWORK_ERROR_CANCELLED;
    }
    if ( pEngine->eMode == XWORK_REPLAY_MODE_RECORD ) {
        return XWORK_ERROR_INVALID_STATE;
    }

    for ( pEntry = pEngine->pEntries; pEntry; pEntry = pEntry->pNext ) {
        if ( pEntry->eKind == XWORK_REPLAY_ENTRY_CHECKPOINT &&
             xwork__replay_cstr_equal(pEntry->sKey, sCheckpointId) ) {
            pEngine->iReplayCursor = iIndex + 1u;
            pEngine->iReplayedCount = 0u;
            pEngine->iDivergenceCount = 0u;
            xwork_replay_divergence_reset(&pEngine->tFirstDivergence);
            return XWORK_OK;
        }
        ++iIndex;
    }
    return XWORK_ERROR_NOT_FOUND;
}

xwork_status xwork_replay_engine_emit_report_artifact(
    const xwork_replay_engine *pEngine,
    xwork_run *pRun,
    const char *sArtifactId,
    xwork_artifact *pArtifact
)
{
    xwork_replay_result tResult;
    xwork_report_artifact_options tReport;
    char *sReportText = NULL;
    xwork_status iStatus;

    if ( !pEngine || !pRun ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_replay_result_init(&tResult);
    iStatus = xwork_replay_engine_get_result(pEngine, &tResult);
    if ( iStatus != XWORK_OK ) {
        xwork_replay_result_reset(&tResult);
        return iStatus;
    }

    if ( tResult.bDiverged ) {
        sReportText = xwork__dup_printf(
            "# xwork Replay Divergence Report\n\n"
            "- replay_id: %s\n"
            "- mode: %s\n"
            "- status: %s\n"
            "- recorded_count: %zu\n"
            "- replayed_count: %zu\n"
            "- divergence_count: %zu\n\n"
            "## First Divergence\n\n"
            "- kind: %s\n"
            "- sequence: %zu\n"
            "- expected_entry_kind: %s\n"
            "- actual_entry_kind: %s\n"
            "- expected_key: %s\n"
            "- actual_key: %s\n"
            "- expected_hash: %s\n"
            "- actual_hash: %s\n"
            "- message: %s\n",
            xwork__replay_safe_cstr(pEngine->sReplayId),
            xwork__replay_mode_cstr(pEngine->eMode),
            xwork_status_cstr(tResult.iStatus),
            tResult.iRecordedCount,
            tResult.iReplayedCount,
            tResult.iDivergenceCount,
            xwork__replay_divergence_kind_cstr(tResult.tFirstDivergence.eKind),
            tResult.tFirstDivergence.iSequence,
            xwork__replay_entry_kind_cstr(tResult.tFirstDivergence.eExpectedEntryKind),
            xwork__replay_entry_kind_cstr(tResult.tFirstDivergence.eActualEntryKind),
            xwork__replay_safe_cstr(tResult.tFirstDivergence.sExpectedKey),
            xwork__replay_safe_cstr(tResult.tFirstDivergence.sActualKey),
            xwork__replay_safe_cstr(tResult.tFirstDivergence.sExpectedHash),
            xwork__replay_safe_cstr(tResult.tFirstDivergence.sActualHash),
            xwork__replay_safe_cstr(tResult.tFirstDivergence.sMessage)
        );
    } else {
        sReportText = xwork__dup_printf(
            "# xwork Replay Report\n\n"
            "- replay_id: %s\n"
            "- mode: %s\n"
            "- status: %s\n"
            "- recorded_count: %zu\n"
            "- replayed_count: %zu\n"
            "- divergence_count: %zu\n\n"
            "No divergence detected.\n",
            xwork__replay_safe_cstr(pEngine->sReplayId),
            xwork__replay_mode_cstr(pEngine->eMode),
            xwork_status_cstr(tResult.iStatus),
            tResult.iRecordedCount,
            tResult.iReplayedCount,
            tResult.iDivergenceCount
        );
    }
    if ( !sReportText ) {
        xwork_replay_result_reset(&tResult);
        return XWORK_ERROR_NO_MEMORY;
    }

    xwork_report_artifact_options_init(&tReport);
    tReport.sArtifactId = sArtifactId;
    tReport.sName = tResult.bDiverged
        ? "xwork-replay-divergence.md"
        : "xwork-replay-report.md";
    tReport.sMimeType = "text/markdown";
    tReport.sSummary = tResult.bDiverged
        ? "Replay divergence report."
        : "Replay success report.";
    tReport.eOutputClass = XWORK_ARTIFACT_OUTPUT_TEXT;
    tReport.sOutputRole = "replay.report";
    tReport.eReportClass = tResult.bDiverged
        ? XWORK_ARTIFACT_REPORT_DIAGNOSTICS
        : XWORK_ARTIFACT_REPORT_SUMMARY;
    tReport.sReportSubjectRef = pEngine->sReplayId;
    tReport.sReportText = sReportText;

    iStatus = xwork_run_emit_report_artifact(pRun, &tReport, pArtifact);
    free(sReportText);
    xwork_replay_result_reset(&tResult);
    return iStatus;
}

xwork_status xwork_replay_engine_cancel(
    xwork_replay_engine *pEngine,
    const char *sReason
)
{
    if ( !pEngine ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    pEngine->bCancelled = true;
    return xwork__replace_cstr(&pEngine->sCancelReason, sReason);
}

xwork_status xwork_replay_engine_get_manifest(
    const xwork_replay_engine *pEngine,
    xwork_replay_manifest *pManifest
)
{
    xwork_status iStatus;

    if ( !pEngine || !pManifest ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_replay_manifest_reset(pManifest);
    iStatus = xwork__replace_cstr((char **)&pManifest->sManifestId, pEngine->sReplayId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pManifest->sReplayId, pEngine->sReplayId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr(
        (char **)&pManifest->sContentHashAlgorithm,
        "fnv1a64"
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    pManifest->iEntryCount = pEngine->iRecordedCount;
    return XWORK_OK;
}

xwork_status xwork_replay_engine_get_result(
    const xwork_replay_engine *pEngine,
    xwork_replay_result *pResult
)
{
    xwork_status iStatus;

    if ( !pEngine || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_replay_result_reset(pResult);
    pResult->iRecordedCount = pEngine->iRecordedCount;
    pResult->iReplayedCount = pEngine->iReplayedCount + pEngine->iReplayedEventCount;
    pResult->iDivergenceCount = pEngine->iDivergenceCount;
    pResult->bDiverged = pEngine->iDivergenceCount > 0u;
    pResult->iStatus = pEngine->bCancelled
        ? XWORK_ERROR_CANCELLED
        : (pResult->bDiverged ? XWORK_ERROR_EXTERNAL_FAILURE : XWORK_OK);
    if ( pResult->bDiverged ) {
        iStatus = xwork__replay_copy_divergence(
            &pResult->tFirstDivergence,
            &pEngine->tFirstDivergence
        );
        if ( iStatus != XWORK_OK ) return iStatus;
    }
    return XWORK_OK;
}

xwork_status xwork_replay_engine_get_first_divergence(
    const xwork_replay_engine *pEngine,
    xwork_replay_divergence *pDivergence
)
{
    if ( !pEngine || !pDivergence ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( pEngine->iDivergenceCount == 0u ) {
        return XWORK_ERROR_NOT_FOUND;
    }
    return xwork__replay_copy_divergence(pDivergence, &pEngine->tFirstDivergence);
}

xwork_status xwork_replay_engine_list_entries(
    const xwork_replay_engine *pEngine,
    xwork_replay_entry_summary_list *pList
)
{
    xwork__replay_entry_record *pEntry;
    xwork_replay_entry_summary *pItems;
    size_t i = 0u;
    xwork_status iStatus = XWORK_OK;

    if ( !pEngine || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_replay_entry_summary_list_reset(pList);
    if ( pEngine->iRecordedCount == 0u ) {
        return XWORK_OK;
    }
    pItems = (xwork_replay_entry_summary *)calloc(
        pEngine->iRecordedCount,
        sizeof(*pItems)
    );
    if ( !pItems ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    for ( i = 0u; i < pEngine->iRecordedCount; ++i ) {
        xwork_replay_entry_summary_init(&pItems[i]);
    }
    i = 0u;
    for ( pEntry = pEngine->pEntries; pEntry; pEntry = pEntry->pNext ) {
        iStatus = xwork__replay_copy_summary(&pItems[i], pEntry);
        if ( iStatus != XWORK_OK ) {
            break;
        }
        ++i;
    }
    if ( iStatus != XWORK_OK ) {
        xwork_replay_entry_summary_list tList;
        tList.pItems = pItems;
        tList.iCount = pEngine->iRecordedCount;
        xwork_replay_entry_summary_list_reset(&tList);
        return iStatus;
    }
    pList->pItems = pItems;
    pList->iCount = pEngine->iRecordedCount;
    return XWORK_OK;
}

xwork_status xwork_replay_engine_list_events(
    const xwork_replay_engine *pEngine,
    xwork_replay_event_summary_list *pList
)
{
    xwork__replay_event_record *pEvent;
    xwork_replay_event_summary *pItems;
    size_t i = 0u;
    xwork_status iStatus = XWORK_OK;

    if ( !pEngine || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_replay_event_summary_list_reset(pList);
    if ( pEngine->iRecordedEventCount == 0u ) {
        return XWORK_OK;
    }
    pItems = (xwork_replay_event_summary *)calloc(
        pEngine->iRecordedEventCount,
        sizeof(*pItems)
    );
    if ( !pItems ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    for ( i = 0u; i < pEngine->iRecordedEventCount; ++i ) {
        xwork_replay_event_summary_init(&pItems[i]);
    }
    i = 0u;
    for ( pEvent = pEngine->pEvents; pEvent; pEvent = pEvent->pNext ) {
        iStatus = xwork__replay_copy_event_summary(&pItems[i], pEvent);
        if ( iStatus != XWORK_OK ) {
            break;
        }
        ++i;
    }
    if ( iStatus != XWORK_OK ) {
        xwork_replay_event_summary_list tList;
        tList.pItems = pItems;
        tList.iCount = pEngine->iRecordedEventCount;
        xwork_replay_event_summary_list_reset(&tList);
        return iStatus;
    }
    pList->pItems = pItems;
    pList->iCount = pEngine->iRecordedEventCount;
    return XWORK_OK;
}
