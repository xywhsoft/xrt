#include "../xwork_core/xwork_internal.h"

static void xwork__artifact_snapshot_free_cstr(const char **psText)
{
    if ( psText && *psText ) {
        free((void *)*psText);
        *psText = NULL;
    }
}

static xwork_status xwork__artifact_snapshot_replace_cstr(
    const char **psTarget,
    const char *sText
)
{
    char *sCopy = NULL;

    if ( !psTarget ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    if ( sText ) {
        sCopy = xwork__dup_cstr(sText);
        if ( !sCopy ) {
            return XWORK_ERROR_NO_MEMORY;
        }
    }

    xwork__artifact_snapshot_free_cstr(psTarget);
    *psTarget = sCopy;
    return XWORK_OK;
}

static bool xwork__artifact_line_has_prefix(const char *sLine, const char *sPrefix)
{
    return sLine && sPrefix && strncmp(sLine, sPrefix, strlen(sPrefix)) == 0;
}

static void xwork__artifact_analyze_content_text(
    const char *sText,
    bool *pbHasContentStats,
    size_t *piContentByteCount,
    size_t *piContentLineCount
)
{
    size_t iLen;
    size_t iLineCount = 0u;
    size_t i;

    if ( pbHasContentStats ) *pbHasContentStats = sText != NULL;
    if ( piContentByteCount ) *piContentByteCount = 0u;
    if ( piContentLineCount ) *piContentLineCount = 0u;
    if ( !sText ) {
        return;
    }

    iLen = strlen(sText);
    if ( iLen > 0u ) {
        iLineCount = 1u;
        for ( i = 0u; i < iLen; ++i ) {
            if ( sText[i] == '\n' ) {
                ++iLineCount;
            }
        }
        if ( sText[iLen - 1u] == '\n' ) {
            --iLineCount;
        }
    }

    if ( piContentByteCount ) *piContentByteCount = iLen;
    if ( piContentLineCount ) *piContentLineCount = iLineCount;
}

static xwork_artifact_output_class xwork__artifact_infer_output_class(
    xwork_artifact_kind eKind,
    const char *sMimeType,
    xwork_artifact_output_class eOutputClass
)
{
    if ( eOutputClass != XWORK_ARTIFACT_OUTPUT_UNSPECIFIED ) {
        return eOutputClass;
    }
    if ( eKind != XWORK_ARTIFACT_OUTPUT && eKind != XWORK_ARTIFACT_REPORT ) {
        return XWORK_ARTIFACT_OUTPUT_UNSPECIFIED;
    }
    if ( sMimeType && strncmp(sMimeType, "application/json", 16u) == 0 ) {
        return XWORK_ARTIFACT_OUTPUT_JSON;
    }
    return XWORK_ARTIFACT_OUTPUT_TEXT;
}

static xwork_artifact_report_class xwork__artifact_infer_report_class(
    xwork_artifact_kind eKind,
    xwork_artifact_report_class eReportClass
)
{
    if ( eReportClass != XWORK_ARTIFACT_REPORT_UNSPECIFIED ) {
        return eReportClass;
    }
    if ( eKind == XWORK_ARTIFACT_REPORT ) {
        return XWORK_ARTIFACT_REPORT_DOCUMENT;
    }
    return XWORK_ARTIFACT_REPORT_UNSPECIFIED;
}

static void xwork__artifact_analyze_patch_text(
    const char *sPatchText,
    bool *pbHasPatchStats,
    size_t *piPatchFileCount,
    size_t *piPatchHunkCount,
    size_t *piPatchAddedLineCount,
    size_t *piPatchDeletedLineCount
)
{
    const char *sCursor;
    size_t iDiffFileCount = 0u;
    size_t iUnifiedFileCount = 0u;
    size_t iHunkCount = 0u;
    size_t iAddedLineCount = 0u;
    size_t iDeletedLineCount = 0u;

    if ( pbHasPatchStats ) *pbHasPatchStats = sPatchText != NULL;
    if ( piPatchFileCount ) *piPatchFileCount = 0u;
    if ( piPatchHunkCount ) *piPatchHunkCount = 0u;
    if ( piPatchAddedLineCount ) *piPatchAddedLineCount = 0u;
    if ( piPatchDeletedLineCount ) *piPatchDeletedLineCount = 0u;
    if ( !sPatchText ) {
        return;
    }

    sCursor = sPatchText;
    while ( *sCursor ) {
        const char *sLine = sCursor;

        if ( xwork__artifact_line_has_prefix(sLine, "diff --git ") ) {
            ++iDiffFileCount;
        } else if ( xwork__artifact_line_has_prefix(sLine, "+++ ") ) {
            ++iUnifiedFileCount;
        }
        if ( xwork__artifact_line_has_prefix(sLine, "@@") ) {
            ++iHunkCount;
        }
        if ( sLine[0] == '+' && !xwork__artifact_line_has_prefix(sLine, "+++") ) {
            ++iAddedLineCount;
        } else if ( sLine[0] == '-' && !xwork__artifact_line_has_prefix(sLine, "---") ) {
            ++iDeletedLineCount;
        }

        while ( *sCursor && *sCursor != '\n' ) {
            ++sCursor;
        }
        if ( *sCursor == '\n' ) {
            ++sCursor;
        }
    }

    if ( piPatchFileCount ) {
        *piPatchFileCount = iDiffFileCount ? iDiffFileCount : iUnifiedFileCount;
        if ( *piPatchFileCount == 0u &&
             (iHunkCount != 0u || iAddedLineCount != 0u || iDeletedLineCount != 0u) ) {
            *piPatchFileCount = 1u;
        }
    }
    if ( piPatchHunkCount ) *piPatchHunkCount = iHunkCount;
    if ( piPatchAddedLineCount ) *piPatchAddedLineCount = iAddedLineCount;
    if ( piPatchDeletedLineCount ) *piPatchDeletedLineCount = iDeletedLineCount;
}

static xwork_status xwork__artifact_copy(
    xwork_artifact *pTarget,
    const xwork_artifact *pSource
)
{
    xwork_status iStatus;

    if ( !pTarget || !pSource ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_artifact_reset(pTarget);
    iStatus = xwork__artifact_snapshot_replace_cstr(&pTarget->sArtifactId, pSource->sArtifactId);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__artifact_snapshot_replace_cstr(&pTarget->sRunId, pSource->sRunId);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__artifact_snapshot_replace_cstr(&pTarget->sName, pSource->sName);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__artifact_snapshot_replace_cstr(&pTarget->sMimeType, pSource->sMimeType);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__artifact_snapshot_replace_cstr(&pTarget->sStorageRef, pSource->sStorageRef);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__artifact_snapshot_replace_cstr(&pTarget->sSummary, pSource->sSummary);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__artifact_snapshot_replace_cstr(&pTarget->sOutputRole, pSource->sOutputRole);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__artifact_snapshot_replace_cstr(&pTarget->sReportSubjectRef, pSource->sReportSubjectRef);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__artifact_snapshot_replace_cstr(&pTarget->sContentText, pSource->sContentText);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__artifact_snapshot_replace_cstr(
        &pTarget->sPatchApplyResultJson,
        pSource->sPatchApplyResultJson
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__artifact_snapshot_replace_cstr(
        &pTarget->sPatchFileSummaryJson,
        pSource->sPatchFileSummaryJson
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__artifact_snapshot_replace_cstr(&pTarget->sCommandText, pSource->sCommandText);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    pTarget->eKind = pSource->eKind;
    pTarget->eOutputClass = pSource->eOutputClass;
    pTarget->eReportClass = pSource->eReportClass;
    pTarget->bHasContentStats = pSource->bHasContentStats;
    pTarget->iContentByteCount = pSource->iContentByteCount;
    pTarget->iContentLineCount = pSource->iContentLineCount;
    pTarget->bHasPatchStats = pSource->bHasPatchStats;
    pTarget->iPatchFileCount = pSource->iPatchFileCount;
    pTarget->iPatchHunkCount = pSource->iPatchHunkCount;
    pTarget->iPatchAddedLineCount = pSource->iPatchAddedLineCount;
    pTarget->iPatchDeletedLineCount = pSource->iPatchDeletedLineCount;
    pTarget->bHasCommandIoStats = pSource->bHasCommandIoStats;
    pTarget->iStdoutByteCount = pSource->iStdoutByteCount;
    pTarget->iStderrByteCount = pSource->iStderrByteCount;
    pTarget->bStdoutTruncated = pSource->bStdoutTruncated;
    pTarget->bStderrTruncated = pSource->bStderrTruncated;
    pTarget->bHasExitCode = pSource->bHasExitCode;
    pTarget->iExitCode = pSource->iExitCode;
    pTarget->iSequence = pSource->iSequence;
    return XWORK_OK;
}

static void xwork__artifact_record_reset(xwork_artifact_record *pRecord)
{
    if ( !pRecord ) {
        return;
    }

    xwork__free_cstr(&pRecord->sArtifactId);
    xwork__free_cstr(&pRecord->sName);
    xwork__free_cstr(&pRecord->sMimeType);
    xwork__free_cstr(&pRecord->sStorageRef);
    xwork__free_cstr(&pRecord->sSummary);
    xwork__free_cstr(&pRecord->sOutputRole);
    xwork__free_cstr(&pRecord->sReportSubjectRef);
    xwork__free_cstr(&pRecord->sContentText);
    xwork__free_cstr(&pRecord->sPatchApplyResultJson);
    xwork__free_cstr(&pRecord->sPatchFileSummaryJson);
    xwork__free_cstr(&pRecord->sCommandText);
    memset(&pRecord->tArtifact, 0, sizeof(pRecord->tArtifact));
}

static xwork_status xwork__ensure_artifact_capacity(xwork_run *pRun)
{
    xwork_artifact_record *pNewItems;
    size_t iNewCapacity;

    if ( !pRun ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( pRun->iArtifactCount < pRun->iArtifactCapacity ) {
        return XWORK_OK;
    }

    iNewCapacity = pRun->iArtifactCapacity ? pRun->iArtifactCapacity * 2u : 4u;
    pNewItems = (xwork_artifact_record *)realloc(
        pRun->pArtifactLog,
        iNewCapacity * sizeof(*pNewItems)
    );
    if ( !pNewItems ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    memset(
        pNewItems + pRun->iArtifactCapacity,
        0,
        (iNewCapacity - pRun->iArtifactCapacity) * sizeof(*pNewItems)
    );
    pRun->pArtifactLog = pNewItems;
    pRun->iArtifactCapacity = iNewCapacity;
    return XWORK_OK;
}

static xwork_status xwork__append_artifact_record_owned(
    xwork_run *pRun,
    char *sArtifactId,
    xwork_artifact_kind eKind,
    xwork_artifact_output_class eOutputClass,
    char *sOutputRole,
    xwork_artifact_report_class eReportClass,
    char *sReportSubjectRef,
    char *sName,
    char *sMimeType,
    char *sStorageRef,
    char *sSummary,
    char *sContentText,
    char *sPatchApplyResultJson,
    char *sPatchFileSummaryJson,
    char *sCommandText,
    bool bHasContentStats,
    size_t iContentByteCount,
    size_t iContentLineCount,
    bool bHasPatchStats,
    size_t iPatchFileCount,
    size_t iPatchHunkCount,
    size_t iPatchAddedLineCount,
    size_t iPatchDeletedLineCount,
    bool bHasCommandIoStats,
    size_t iStdoutByteCount,
    size_t iStderrByteCount,
    bool bStdoutTruncated,
    bool bStderrTruncated,
    bool bHasExitCode,
    int iExitCode,
    size_t iSequence,
    xwork_artifact *pArtifact
)
{
    xwork_artifact_record *pRecord;
    xwork_status iStatus;

    if ( !pRun || !sArtifactId || !sArtifactId[0] ) {
        free(sArtifactId);
        free(sOutputRole);
        free(sReportSubjectRef);
        free(sName);
        free(sMimeType);
        free(sStorageRef);
        free(sSummary);
        free(sContentText);
        free(sPatchApplyResultJson);
        free(sPatchFileSummaryJson);
        free(sCommandText);
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__ensure_artifact_capacity(pRun);
    if ( iStatus != XWORK_OK ) {
        free(sArtifactId);
        free(sOutputRole);
        free(sReportSubjectRef);
        free(sName);
        free(sMimeType);
        free(sStorageRef);
        free(sSummary);
        free(sContentText);
        free(sPatchApplyResultJson);
        free(sPatchFileSummaryJson);
        free(sCommandText);
        return iStatus;
    }

    pRecord = &pRun->pArtifactLog[pRun->iArtifactCount];
    memset(pRecord, 0, sizeof(*pRecord));

    pRecord->sArtifactId = sArtifactId;
    pRecord->sOutputRole = sOutputRole;
    pRecord->sReportSubjectRef = sReportSubjectRef;
    pRecord->sName = sName;
    pRecord->sMimeType = sMimeType;
    pRecord->sStorageRef = sStorageRef;
    pRecord->sSummary = sSummary;
    pRecord->sContentText = sContentText;
    pRecord->sPatchApplyResultJson = sPatchApplyResultJson;
    pRecord->sPatchFileSummaryJson = sPatchFileSummaryJson;
    pRecord->sCommandText = sCommandText;
    pRecord->bHasContentStats = bHasContentStats;
    pRecord->iContentByteCount = iContentByteCount;
    pRecord->iContentLineCount = iContentLineCount;
    pRecord->bHasPatchStats = bHasPatchStats;
    pRecord->iPatchFileCount = iPatchFileCount;
    pRecord->iPatchHunkCount = iPatchHunkCount;
    pRecord->iPatchAddedLineCount = iPatchAddedLineCount;
    pRecord->iPatchDeletedLineCount = iPatchDeletedLineCount;
    pRecord->bHasCommandIoStats = bHasCommandIoStats;
    pRecord->iStdoutByteCount = iStdoutByteCount;
    pRecord->iStderrByteCount = iStderrByteCount;
    pRecord->bStdoutTruncated = bStdoutTruncated;
    pRecord->bStderrTruncated = bStderrTruncated;
    pRecord->bHasExitCode = bHasExitCode;
    pRecord->iExitCode = iExitCode;
    pRecord->eOutputClass = eOutputClass;
    pRecord->eReportClass = eReportClass;
    pRecord->tArtifact.sArtifactId = pRecord->sArtifactId;
    pRecord->tArtifact.sRunId = pRun->sRunId;
    pRecord->tArtifact.eKind = eKind;
    pRecord->tArtifact.eOutputClass = pRecord->eOutputClass;
    pRecord->tArtifact.sOutputRole = pRecord->sOutputRole;
    pRecord->tArtifact.eReportClass = pRecord->eReportClass;
    pRecord->tArtifact.sReportSubjectRef = pRecord->sReportSubjectRef;
    pRecord->tArtifact.sName = pRecord->sName;
    pRecord->tArtifact.sMimeType = pRecord->sMimeType;
    pRecord->tArtifact.sStorageRef = pRecord->sStorageRef;
    pRecord->tArtifact.sSummary = pRecord->sSummary;
    pRecord->tArtifact.sContentText = pRecord->sContentText;
    pRecord->tArtifact.sPatchApplyResultJson = pRecord->sPatchApplyResultJson;
    pRecord->tArtifact.sPatchFileSummaryJson = pRecord->sPatchFileSummaryJson;
    pRecord->tArtifact.sCommandText = pRecord->sCommandText;
    pRecord->tArtifact.bHasContentStats = pRecord->bHasContentStats;
    pRecord->tArtifact.iContentByteCount = pRecord->iContentByteCount;
    pRecord->tArtifact.iContentLineCount = pRecord->iContentLineCount;
    pRecord->tArtifact.bHasPatchStats = pRecord->bHasPatchStats;
    pRecord->tArtifact.iPatchFileCount = pRecord->iPatchFileCount;
    pRecord->tArtifact.iPatchHunkCount = pRecord->iPatchHunkCount;
    pRecord->tArtifact.iPatchAddedLineCount = pRecord->iPatchAddedLineCount;
    pRecord->tArtifact.iPatchDeletedLineCount = pRecord->iPatchDeletedLineCount;
    pRecord->tArtifact.bHasCommandIoStats = pRecord->bHasCommandIoStats;
    pRecord->tArtifact.iStdoutByteCount = pRecord->iStdoutByteCount;
    pRecord->tArtifact.iStderrByteCount = pRecord->iStderrByteCount;
    pRecord->tArtifact.bStdoutTruncated = pRecord->bStdoutTruncated;
    pRecord->tArtifact.bStderrTruncated = pRecord->bStderrTruncated;
    pRecord->tArtifact.bHasExitCode = pRecord->bHasExitCode;
    pRecord->tArtifact.iExitCode = pRecord->iExitCode;
    pRecord->tArtifact.iSequence = iSequence;

    ++pRun->iArtifactCount;
    if ( iSequence > pRun->iNextArtifactSequence ) {
        pRun->iNextArtifactSequence = iSequence;
    }

    if ( pArtifact ) {
        *pArtifact = pRecord->tArtifact;
    }

    return XWORK_OK;
}

void xwork_artifact_options_init(xwork_artifact_options *pOptions)
{
    if ( pOptions ) {
        memset(pOptions, 0, sizeof(*pOptions));
        pOptions->eKind = XWORK_ARTIFACT_OUTPUT;
    }
}

void xwork_patch_artifact_options_init(xwork_patch_artifact_options *pOptions)
{
    if ( pOptions ) {
        memset(pOptions, 0, sizeof(*pOptions));
    }
}

void xwork_report_artifact_options_init(xwork_report_artifact_options *pOptions)
{
    if ( pOptions ) {
        memset(pOptions, 0, sizeof(*pOptions));
        pOptions->sMimeType = "text/markdown";
    }
}

void xwork_output_artifact_options_init(xwork_output_artifact_options *pOptions)
{
    if ( pOptions ) {
        memset(pOptions, 0, sizeof(*pOptions));
        pOptions->sMimeType = "text/plain";
    }
}

void xwork_command_artifact_options_init(xwork_command_artifact_options *pOptions)
{
    if ( pOptions ) {
        memset(pOptions, 0, sizeof(*pOptions));
        pOptions->sMimeType = "text/plain";
    }
}

void xwork_artifact_init(xwork_artifact *pArtifact)
{
    if ( pArtifact ) {
        memset(pArtifact, 0, sizeof(*pArtifact));
        pArtifact->eKind = XWORK_ARTIFACT_OUTPUT;
    }
}

void xwork_artifact_reset(xwork_artifact *pArtifact)
{
    if ( !pArtifact ) {
        return;
    }

    xwork__free_cstr((char **)&pArtifact->sArtifactId);
    xwork__free_cstr((char **)&pArtifact->sRunId);
    xwork__free_cstr((char **)&pArtifact->sName);
    xwork__free_cstr((char **)&pArtifact->sMimeType);
    xwork__free_cstr((char **)&pArtifact->sStorageRef);
    xwork__free_cstr((char **)&pArtifact->sSummary);
    xwork__free_cstr((char **)&pArtifact->sOutputRole);
    xwork__free_cstr((char **)&pArtifact->sReportSubjectRef);
    xwork__free_cstr((char **)&pArtifact->sContentText);
    xwork__free_cstr((char **)&pArtifact->sPatchApplyResultJson);
    xwork__free_cstr((char **)&pArtifact->sPatchFileSummaryJson);
    xwork__free_cstr((char **)&pArtifact->sCommandText);
    xwork_artifact_init(pArtifact);
}

void xwork__run_reset_artifacts(xwork_run *pRun)
{
    size_t i;

    if ( !pRun ) {
        return;
    }

    for ( i = 0u; i < pRun->iArtifactCount; ++i ) {
        xwork__artifact_record_reset(&pRun->pArtifactLog[i]);
    }
    free(pRun->pArtifactLog);
    pRun->pArtifactLog = NULL;
    pRun->iArtifactCount = 0u;
    pRun->iArtifactCapacity = 0u;
    pRun->iNextArtifactSequence = 0u;
}

xwork_status xwork__run_append_artifact_record(
    xwork_run *pRun,
    const xwork_artifact_options *pOptions,
    xwork_artifact *pArtifact
)
{
    char *sArtifactId = NULL;
    char *sOutputRole = NULL;
    char *sReportSubjectRef = NULL;
    char *sName = NULL;
    char *sMimeType = NULL;
    char *sStorageRef = NULL;
    char *sSummary = NULL;
    char *sContentText = NULL;
    char *sPatchApplyResultJson = NULL;
    char *sPatchFileSummaryJson = NULL;
    char *sCommandText = NULL;
    bool bHasContentStats;
    size_t iContentByteCount;
    size_t iContentLineCount;
    size_t i;
    size_t iSequence;

    if ( !pRun || !pOptions ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iSequence = pRun->iNextArtifactSequence + 1u;

    if ( pOptions->sArtifactId && pOptions->sArtifactId[0] ) {
        sArtifactId = xwork__dup_cstr(pOptions->sArtifactId);
    } else {
        sArtifactId = xwork__dup_scoped_id(pRun->sRunId, "artifact", iSequence);
    }
    if ( !sArtifactId ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    for ( i = 0u; i < pRun->iArtifactCount; ++i ) {
        if ( pRun->pArtifactLog[i].tArtifact.sArtifactId &&
             strcmp(pRun->pArtifactLog[i].tArtifact.sArtifactId, sArtifactId) == 0 ) {
            free(sArtifactId);
            return XWORK_ERROR_ALREADY_EXISTS;
        }
    }

    if ( pOptions->sOutputRole ) {
        sOutputRole = xwork__dup_cstr(pOptions->sOutputRole);
        if ( !sOutputRole ) {
            free(sArtifactId);
            return XWORK_ERROR_NO_MEMORY;
        }
    }
    if ( pOptions->sReportSubjectRef ) {
        sReportSubjectRef = xwork__dup_cstr(pOptions->sReportSubjectRef);
        if ( !sReportSubjectRef ) {
            free(sOutputRole);
            free(sArtifactId);
            return XWORK_ERROR_NO_MEMORY;
        }
    }
    if ( pOptions->sName ) {
        sName = xwork__dup_cstr(pOptions->sName);
        if ( !sName ) {
            free(sReportSubjectRef);
            free(sOutputRole);
            free(sArtifactId);
            return XWORK_ERROR_NO_MEMORY;
        }
    }
    if ( pOptions->sMimeType ) {
        sMimeType = xwork__dup_cstr(pOptions->sMimeType);
        if ( !sMimeType ) {
            free(sName);
            free(sReportSubjectRef);
            free(sOutputRole);
            free(sArtifactId);
            return XWORK_ERROR_NO_MEMORY;
        }
    }
    if ( pOptions->sStorageRef ) {
        sStorageRef = xwork__dup_cstr(pOptions->sStorageRef);
        if ( !sStorageRef ) {
            free(sMimeType);
            free(sName);
            free(sReportSubjectRef);
            free(sOutputRole);
            free(sArtifactId);
            return XWORK_ERROR_NO_MEMORY;
        }
    }
    if ( pOptions->sSummary ) {
        sSummary = xwork__dup_cstr(pOptions->sSummary);
        if ( !sSummary ) {
            free(sStorageRef);
            free(sMimeType);
            free(sName);
            free(sReportSubjectRef);
            free(sOutputRole);
            free(sArtifactId);
            return XWORK_ERROR_NO_MEMORY;
        }
    }
    if ( pOptions->sContentText ) {
        sContentText = xwork__dup_cstr(pOptions->sContentText);
        if ( !sContentText ) {
            free(sSummary);
            free(sStorageRef);
            free(sMimeType);
            free(sName);
            free(sReportSubjectRef);
            free(sOutputRole);
            free(sArtifactId);
            return XWORK_ERROR_NO_MEMORY;
        }
    }
    if ( pOptions->sCommandText ) {
        sCommandText = xwork__dup_cstr(pOptions->sCommandText);
        if ( !sCommandText ) {
            free(sPatchFileSummaryJson);
            free(sPatchApplyResultJson);
            free(sContentText);
            free(sSummary);
            free(sStorageRef);
            free(sMimeType);
            free(sName);
            free(sReportSubjectRef);
            free(sOutputRole);
            free(sArtifactId);
            return XWORK_ERROR_NO_MEMORY;
        }
    }
    if ( pOptions->sPatchApplyResultJson ) {
        sPatchApplyResultJson = xwork__dup_cstr(pOptions->sPatchApplyResultJson);
        if ( !sPatchApplyResultJson ) {
            free(sCommandText);
            free(sContentText);
            free(sSummary);
            free(sStorageRef);
            free(sMimeType);
            free(sName);
            free(sReportSubjectRef);
            free(sOutputRole);
            free(sArtifactId);
            return XWORK_ERROR_NO_MEMORY;
        }
    }
    if ( pOptions->sPatchFileSummaryJson ) {
        sPatchFileSummaryJson = xwork__dup_cstr(pOptions->sPatchFileSummaryJson);
        if ( !sPatchFileSummaryJson ) {
            free(sPatchApplyResultJson);
            free(sCommandText);
            free(sContentText);
            free(sSummary);
            free(sStorageRef);
            free(sMimeType);
            free(sName);
            free(sReportSubjectRef);
            free(sOutputRole);
            free(sArtifactId);
            return XWORK_ERROR_NO_MEMORY;
        }
    }

    bHasContentStats = pOptions->bHasContentStats;
    iContentByteCount = pOptions->iContentByteCount;
    iContentLineCount = pOptions->iContentLineCount;
    if ( !bHasContentStats && pOptions->sContentText ) {
        xwork__artifact_analyze_content_text(
            pOptions->sContentText,
            &bHasContentStats,
            &iContentByteCount,
            &iContentLineCount
        );
    }

    return xwork__append_artifact_record_owned(
        pRun,
        sArtifactId,
        pOptions->eKind,
        xwork__artifact_infer_output_class(
            pOptions->eKind,
            pOptions->sMimeType,
            pOptions->eOutputClass
        ),
        sOutputRole,
        xwork__artifact_infer_report_class(pOptions->eKind, pOptions->eReportClass),
        sReportSubjectRef,
        sName,
        sMimeType,
        sStorageRef,
        sSummary,
        sContentText,
        sPatchApplyResultJson,
        sPatchFileSummaryJson,
        sCommandText,
        bHasContentStats,
        iContentByteCount,
        iContentLineCount,
        pOptions->bHasPatchStats,
        pOptions->iPatchFileCount,
        pOptions->iPatchHunkCount,
        pOptions->iPatchAddedLineCount,
        pOptions->iPatchDeletedLineCount,
        pOptions->bHasCommandIoStats,
        pOptions->iStdoutByteCount,
        pOptions->iStderrByteCount,
        pOptions->bStdoutTruncated,
        pOptions->bStderrTruncated,
        pOptions->bHasExitCode,
        pOptions->iExitCode,
        iSequence,
        pArtifact
    );
}

void xwork__run_snapshot_reset_artifacts(xwork_run_snapshot *pSnapshot)
{
    size_t i;
    xwork_artifact *pArtifacts;

    if ( !pSnapshot || !pSnapshot->pArtifacts ) {
        return;
    }

    pArtifacts = (xwork_artifact *)pSnapshot->pArtifacts;
    for ( i = 0u; i < pSnapshot->iArtifactCount; ++i ) {
        xwork__artifact_snapshot_free_cstr(&pArtifacts[i].sArtifactId);
        if ( pArtifacts[i].sRunId != pSnapshot->sRunId ) {
            xwork__artifact_snapshot_free_cstr(&pArtifacts[i].sRunId);
        }
        xwork__artifact_snapshot_free_cstr(&pArtifacts[i].sName);
        xwork__artifact_snapshot_free_cstr(&pArtifacts[i].sMimeType);
        xwork__artifact_snapshot_free_cstr(&pArtifacts[i].sStorageRef);
        xwork__artifact_snapshot_free_cstr(&pArtifacts[i].sSummary);
        xwork__artifact_snapshot_free_cstr(&pArtifacts[i].sOutputRole);
        xwork__artifact_snapshot_free_cstr(&pArtifacts[i].sReportSubjectRef);
        xwork__artifact_snapshot_free_cstr(&pArtifacts[i].sContentText);
        xwork__artifact_snapshot_free_cstr(&pArtifacts[i].sPatchApplyResultJson);
        xwork__artifact_snapshot_free_cstr(&pArtifacts[i].sPatchFileSummaryJson);
        xwork__artifact_snapshot_free_cstr(&pArtifacts[i].sCommandText);
    }
    free(pArtifacts);
    pSnapshot->pArtifacts = NULL;
    pSnapshot->iArtifactCount = 0u;
}

xwork_status xwork__run_snapshot_copy_artifacts(
    xwork_run_snapshot *pSnapshot,
    const xwork_run *pRun
)
{
    xwork_artifact *pArtifacts;
    size_t i;

    if ( !pSnapshot || !pRun ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork__run_snapshot_reset_artifacts(pSnapshot);

    if ( pRun->iArtifactCount == 0u ) {
        return XWORK_OK;
    }

    pArtifacts = (xwork_artifact *)calloc(pRun->iArtifactCount, sizeof(*pArtifacts));
    if ( !pArtifacts ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    for ( i = 0u; i < pRun->iArtifactCount; ++i ) {
        xwork_status iStatus;
        const xwork_artifact *pSource = &pRun->pArtifactLog[i].tArtifact;

        xwork_artifact_init(&pArtifacts[i]);
        iStatus = xwork__artifact_snapshot_replace_cstr(&pArtifacts[i].sArtifactId, pSource->sArtifactId);
        if ( iStatus != XWORK_OK ) {
            pSnapshot->pArtifacts = pArtifacts;
            pSnapshot->iArtifactCount = i + 1u;
            xwork__run_snapshot_reset_artifacts(pSnapshot);
            return iStatus;
        }
        iStatus = xwork__artifact_snapshot_replace_cstr(&pArtifacts[i].sName, pSource->sName);
        if ( iStatus != XWORK_OK ) {
            pSnapshot->pArtifacts = pArtifacts;
            pSnapshot->iArtifactCount = i + 1u;
            xwork__run_snapshot_reset_artifacts(pSnapshot);
            return iStatus;
        }
        iStatus = xwork__artifact_snapshot_replace_cstr(&pArtifacts[i].sMimeType, pSource->sMimeType);
        if ( iStatus != XWORK_OK ) {
            pSnapshot->pArtifacts = pArtifacts;
            pSnapshot->iArtifactCount = i + 1u;
            xwork__run_snapshot_reset_artifacts(pSnapshot);
            return iStatus;
        }
        iStatus = xwork__artifact_snapshot_replace_cstr(&pArtifacts[i].sStorageRef, pSource->sStorageRef);
        if ( iStatus != XWORK_OK ) {
            pSnapshot->pArtifacts = pArtifacts;
            pSnapshot->iArtifactCount = i + 1u;
            xwork__run_snapshot_reset_artifacts(pSnapshot);
            return iStatus;
        }
        iStatus = xwork__artifact_snapshot_replace_cstr(&pArtifacts[i].sSummary, pSource->sSummary);
        if ( iStatus != XWORK_OK ) {
            pSnapshot->pArtifacts = pArtifacts;
            pSnapshot->iArtifactCount = i + 1u;
            xwork__run_snapshot_reset_artifacts(pSnapshot);
            return iStatus;
        }
        iStatus = xwork__artifact_snapshot_replace_cstr(&pArtifacts[i].sOutputRole, pSource->sOutputRole);
        if ( iStatus != XWORK_OK ) {
            pSnapshot->pArtifacts = pArtifacts;
            pSnapshot->iArtifactCount = i + 1u;
            xwork__run_snapshot_reset_artifacts(pSnapshot);
            return iStatus;
        }
        iStatus = xwork__artifact_snapshot_replace_cstr(
            &pArtifacts[i].sReportSubjectRef,
            pSource->sReportSubjectRef
        );
        if ( iStatus != XWORK_OK ) {
            pSnapshot->pArtifacts = pArtifacts;
            pSnapshot->iArtifactCount = i + 1u;
            xwork__run_snapshot_reset_artifacts(pSnapshot);
            return iStatus;
        }
        iStatus = xwork__artifact_snapshot_replace_cstr(&pArtifacts[i].sContentText, pSource->sContentText);
        if ( iStatus != XWORK_OK ) {
            pSnapshot->pArtifacts = pArtifacts;
            pSnapshot->iArtifactCount = i + 1u;
            xwork__run_snapshot_reset_artifacts(pSnapshot);
            return iStatus;
        }
        iStatus = xwork__artifact_snapshot_replace_cstr(
            &pArtifacts[i].sPatchApplyResultJson,
            pSource->sPatchApplyResultJson
        );
        if ( iStatus != XWORK_OK ) {
            pSnapshot->pArtifacts = pArtifacts;
            pSnapshot->iArtifactCount = i + 1u;
            xwork__run_snapshot_reset_artifacts(pSnapshot);
            return iStatus;
        }
        iStatus = xwork__artifact_snapshot_replace_cstr(
            &pArtifacts[i].sPatchFileSummaryJson,
            pSource->sPatchFileSummaryJson
        );
        if ( iStatus != XWORK_OK ) {
            pSnapshot->pArtifacts = pArtifacts;
            pSnapshot->iArtifactCount = i + 1u;
            xwork__run_snapshot_reset_artifacts(pSnapshot);
            return iStatus;
        }
        iStatus = xwork__artifact_snapshot_replace_cstr(&pArtifacts[i].sCommandText, pSource->sCommandText);
        if ( iStatus != XWORK_OK ) {
            pSnapshot->pArtifacts = pArtifacts;
            pSnapshot->iArtifactCount = i + 1u;
            xwork__run_snapshot_reset_artifacts(pSnapshot);
            return iStatus;
        }

        pArtifacts[i].sRunId = pSnapshot->sRunId;
        pArtifacts[i].eKind = pSource->eKind;
        pArtifacts[i].eOutputClass = pSource->eOutputClass;
        pArtifacts[i].eReportClass = pSource->eReportClass;
        pArtifacts[i].bHasContentStats = pSource->bHasContentStats;
        pArtifacts[i].iContentByteCount = pSource->iContentByteCount;
        pArtifacts[i].iContentLineCount = pSource->iContentLineCount;
        pArtifacts[i].bHasPatchStats = pSource->bHasPatchStats;
        pArtifacts[i].iPatchFileCount = pSource->iPatchFileCount;
        pArtifacts[i].iPatchHunkCount = pSource->iPatchHunkCount;
        pArtifacts[i].iPatchAddedLineCount = pSource->iPatchAddedLineCount;
        pArtifacts[i].iPatchDeletedLineCount = pSource->iPatchDeletedLineCount;
        pArtifacts[i].bHasCommandIoStats = pSource->bHasCommandIoStats;
        pArtifacts[i].iStdoutByteCount = pSource->iStdoutByteCount;
        pArtifacts[i].iStderrByteCount = pSource->iStderrByteCount;
        pArtifacts[i].bStdoutTruncated = pSource->bStdoutTruncated;
        pArtifacts[i].bStderrTruncated = pSource->bStderrTruncated;
        pArtifacts[i].bHasExitCode = pSource->bHasExitCode;
        pArtifacts[i].iExitCode = pSource->iExitCode;
        pArtifacts[i].iSequence = pSource->iSequence;
    }

    pSnapshot->pArtifacts = pArtifacts;
    pSnapshot->iArtifactCount = pRun->iArtifactCount;
    return XWORK_OK;
}

xwork_status xwork__run_apply_snapshot_artifacts(
    xwork_run *pRun,
    const xwork_run_snapshot *pSnapshot
)
{
    size_t i;

    if ( !pRun || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork__run_reset_artifacts(pRun);

    for ( i = 0u; i < pSnapshot->iArtifactCount; ++i ) {
        const xwork_artifact *pSource = &pSnapshot->pArtifacts[i];
        xwork_status iStatus;
        char *sArtifactId = xwork__dup_cstr(pSource->sArtifactId);
        char *sName = xwork__dup_cstr(pSource->sName);
        char *sMimeType = xwork__dup_cstr(pSource->sMimeType);
        char *sStorageRef = xwork__dup_cstr(pSource->sStorageRef);
        char *sSummary = xwork__dup_cstr(pSource->sSummary);
        char *sOutputRole = xwork__dup_cstr(pSource->sOutputRole);
        char *sReportSubjectRef = xwork__dup_cstr(pSource->sReportSubjectRef);
        char *sContentText = xwork__dup_cstr(pSource->sContentText);
        char *sPatchApplyResultJson = xwork__dup_cstr(pSource->sPatchApplyResultJson);
        char *sPatchFileSummaryJson = xwork__dup_cstr(pSource->sPatchFileSummaryJson);
        char *sCommandText = xwork__dup_cstr(pSource->sCommandText);

        if ( (pSource->sArtifactId && !sArtifactId) ||
             (pSource->sName && !sName) ||
             (pSource->sMimeType && !sMimeType) ||
             (pSource->sStorageRef && !sStorageRef) ||
             (pSource->sSummary && !sSummary) ||
             (pSource->sOutputRole && !sOutputRole) ||
             (pSource->sReportSubjectRef && !sReportSubjectRef) ||
             (pSource->sContentText && !sContentText) ||
             (pSource->sPatchApplyResultJson && !sPatchApplyResultJson) ||
             (pSource->sPatchFileSummaryJson && !sPatchFileSummaryJson) ||
             (pSource->sCommandText && !sCommandText) ) {
            free(sCommandText);
            free(sPatchFileSummaryJson);
            free(sPatchApplyResultJson);
            free(sContentText);
            free(sReportSubjectRef);
            free(sOutputRole);
            free(sSummary);
            free(sStorageRef);
            free(sMimeType);
            free(sName);
            free(sArtifactId);
            xwork__run_reset_artifacts(pRun);
            return XWORK_ERROR_NO_MEMORY;
        }

        iStatus = xwork__append_artifact_record_owned(
            pRun,
            sArtifactId,
            pSource->eKind,
            pSource->eOutputClass,
            sOutputRole,
            pSource->eReportClass,
            sReportSubjectRef,
            sName,
            sMimeType,
            sStorageRef,
            sSummary,
            sContentText,
            sPatchApplyResultJson,
            sPatchFileSummaryJson,
            sCommandText,
            pSource->bHasContentStats,
            pSource->iContentByteCount,
            pSource->iContentLineCount,
            pSource->bHasPatchStats,
            pSource->iPatchFileCount,
            pSource->iPatchHunkCount,
            pSource->iPatchAddedLineCount,
            pSource->iPatchDeletedLineCount,
            pSource->bHasCommandIoStats,
            pSource->iStdoutByteCount,
            pSource->iStderrByteCount,
            pSource->bStdoutTruncated,
            pSource->bStderrTruncated,
            pSource->bHasExitCode,
            pSource->iExitCode,
            pSource->iSequence,
            NULL
        );
        if ( iStatus != XWORK_OK ) {
            xwork__run_reset_artifacts(pRun);
            return iStatus;
        }
    }

    if ( pSnapshot->iNextArtifactSequence > pRun->iNextArtifactSequence ) {
        pRun->iNextArtifactSequence = pSnapshot->iNextArtifactSequence;
    }

    return XWORK_OK;
}

char *xwork__run_build_artifact_refs(const xwork_run *pRun)
{
    char *sRefs;
    size_t i;
    size_t iLen = 0u;

    if ( !pRun || pRun->iArtifactCount == 0u ) {
        return NULL;
    }

    for ( i = 0u; i < pRun->iArtifactCount; ++i ) {
        const char *sArtifactId = pRun->pArtifactLog[i].tArtifact.sArtifactId;

        if ( !sArtifactId || !sArtifactId[0] ) {
            continue;
        }
        iLen += strlen(sArtifactId);
        if ( i > 0u ) {
            ++iLen;
        }
    }

    if ( iLen == 0u ) {
        return NULL;
    }

    sRefs = (char *)calloc(iLen + 1u, sizeof(char));
    if ( !sRefs ) {
        return NULL;
    }

    for ( i = 0u; i < pRun->iArtifactCount; ++i ) {
        const char *sArtifactId = pRun->pArtifactLog[i].tArtifact.sArtifactId;

        if ( !sArtifactId || !sArtifactId[0] ) {
            continue;
        }
        if ( sRefs[0] ) {
            strcat(sRefs, ",");
        }
        strcat(sRefs, sArtifactId);
    }

    return sRefs;
}

xwork_status xwork_run_emit_artifact(
    xwork_run *pRun,
    const xwork_artifact_options *pOptions,
    xwork_artifact *pArtifact
)
{
    xwork_artifact tArtifact;
    xwork_status iStatus;

    if ( !pRun || !pOptions ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_artifact_init(&tArtifact);
    iStatus = xwork__run_append_artifact_record(pRun, pOptions, &tArtifact);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    iStatus = xwork__runtime_store_artifact(pRun->pRuntime, &tArtifact);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    iStatus = xwork__run_record_event(
        pRun,
        XWORK_EVENT_ARTIFACT_EMITTED,
        pRun->sLastToolId,
        pRun->sLastApprovalRequestId,
        pRun->sLastCheckpointId,
        tArtifact.sSummary ? tArtifact.sSummary : "Artifact emitted."
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    if ( pArtifact ) {
        iStatus = xwork__artifact_copy(pArtifact, &tArtifact);
        if ( iStatus != XWORK_OK ) {
            return iStatus;
        }
    }
    return XWORK_OK;
}

xwork_status xwork_run_emit_patch_artifact(
    xwork_run *pRun,
    const xwork_patch_artifact_options *pOptions,
    xwork_artifact *pArtifact
)
{
    xwork_artifact_options tOptions;

    if ( !pOptions ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_artifact_options_init(&tOptions);
    tOptions.sArtifactId = pOptions->sArtifactId;
    tOptions.eKind = XWORK_ARTIFACT_PATCH;
    tOptions.sName = pOptions->sName;
    tOptions.sMimeType = "text/x-diff";
    tOptions.sStorageRef = pOptions->sTargetRef;
    tOptions.sSummary = pOptions->sSummary;
    tOptions.sContentText = pOptions->sPatchText;
    tOptions.sPatchApplyResultJson = pOptions->sApplyResultJson;
    tOptions.sPatchFileSummaryJson = pOptions->sFileSummaryJson;
    xwork__artifact_analyze_patch_text(
        pOptions->sPatchText,
        &tOptions.bHasPatchStats,
        &tOptions.iPatchFileCount,
        &tOptions.iPatchHunkCount,
        &tOptions.iPatchAddedLineCount,
        &tOptions.iPatchDeletedLineCount
    );
    return xwork_run_emit_artifact(pRun, &tOptions, pArtifact);
}

xwork_status xwork_run_emit_report_artifact(
    xwork_run *pRun,
    const xwork_report_artifact_options *pOptions,
    xwork_artifact *pArtifact
)
{
    xwork_artifact_options tOptions;

    if ( !pOptions ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_artifact_options_init(&tOptions);
    tOptions.sArtifactId = pOptions->sArtifactId;
    tOptions.eKind = XWORK_ARTIFACT_REPORT;
    tOptions.sName = pOptions->sName;
    tOptions.sMimeType = pOptions->sMimeType ? pOptions->sMimeType : "text/markdown";
    tOptions.sStorageRef = pOptions->sStorageRef;
    tOptions.sSummary = pOptions->sSummary;
    tOptions.eOutputClass = pOptions->eOutputClass;
    tOptions.sOutputRole = pOptions->sOutputRole;
    tOptions.eReportClass = pOptions->eReportClass;
    tOptions.sReportSubjectRef = pOptions->sReportSubjectRef;
    tOptions.sContentText = pOptions->sReportText;
    return xwork_run_emit_artifact(pRun, &tOptions, pArtifact);
}

xwork_status xwork_run_emit_output_artifact(
    xwork_run *pRun,
    const xwork_output_artifact_options *pOptions,
    xwork_artifact *pArtifact
)
{
    xwork_artifact_options tOptions;

    if ( !pOptions ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_artifact_options_init(&tOptions);
    tOptions.sArtifactId = pOptions->sArtifactId;
    tOptions.eKind = XWORK_ARTIFACT_OUTPUT;
    tOptions.sName = pOptions->sName;
    tOptions.sMimeType = pOptions->sMimeType ? pOptions->sMimeType : "text/plain";
    tOptions.sStorageRef = pOptions->sStorageRef;
    tOptions.sSummary = pOptions->sSummary;
    tOptions.eOutputClass = pOptions->eOutputClass;
    tOptions.sOutputRole = pOptions->sOutputRole;
    tOptions.sContentText = pOptions->sOutputText;
    return xwork_run_emit_artifact(pRun, &tOptions, pArtifact);
}

xwork_status xwork_run_emit_command_artifact(
    xwork_run *pRun,
    const xwork_command_artifact_options *pOptions,
    xwork_artifact *pArtifact
)
{
    xwork_artifact_options tOptions;

    if ( !pOptions ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_artifact_options_init(&tOptions);
    tOptions.sArtifactId = pOptions->sArtifactId;
    tOptions.eKind = XWORK_ARTIFACT_COMMAND;
    tOptions.sName = pOptions->sName;
    tOptions.sMimeType = pOptions->sMimeType ? pOptions->sMimeType : "text/plain";
    tOptions.sStorageRef = pOptions->sStorageRef;
    tOptions.sSummary = pOptions->sSummary;
    tOptions.sContentText = pOptions->sOutputText;
    tOptions.sCommandText = pOptions->sCommandText;
    tOptions.bHasCommandIoStats = pOptions->bHasCommandIoStats;
    tOptions.iStdoutByteCount = pOptions->iStdoutByteCount;
    tOptions.iStderrByteCount = pOptions->iStderrByteCount;
    tOptions.bStdoutTruncated = pOptions->bStdoutTruncated;
    tOptions.bStderrTruncated = pOptions->bStderrTruncated;
    tOptions.bHasExitCode = pOptions->bHasExitCode;
    tOptions.iExitCode = pOptions->iExitCode;
    return xwork_run_emit_artifact(pRun, &tOptions, pArtifact);
}

size_t xwork_run_get_artifact_count(const xwork_run *pRun)
{
    return pRun ? pRun->iArtifactCount : 0u;
}

xwork_status xwork_run_get_artifact(
    const xwork_run *pRun,
    size_t iIndex,
    xwork_artifact *pArtifact
)
{
    if ( !pRun || !pArtifact ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( iIndex >= pRun->iArtifactCount ) {
        return XWORK_ERROR_NOT_FOUND;
    }

    return xwork__artifact_copy(pArtifact, &pRun->pArtifactLog[iIndex].tArtifact);
}
