#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "xllm_embed_onnxruntime_compat.h"
#include "xllm_embed_e5_onnx.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef const OrtApiBase *(ORT_API_CALL *xllm__memory_ort_get_api_base_fn)(void);

#define XLLM__MEMORY_E5_QUERY_PREFIX "query: "
#define XLLM__MEMORY_E5_DOCUMENT_PREFIX "passage: "
#define XLLM__MEMORY_E5_MAX_INPUT_TOKENS 512u
#define XLLM__MEMORY_E5_NORMALIZE_OUTPUT true

typedef struct {
    char *sPiece;
    uint32 uId;
    uint32 uLength;
    unsigned char uFirstByte;
    float fScore;
    bool bSpecial;
} xllm__memory_e5_piece;

typedef struct {
    xllm__memory_e5_piece *pPieces;
    size_t iPieceCount;
    uint32 *pBucketPieceIds;
    uint32 auBucketOffsets[257];
    uint32 uBosId;
    uint32 uPadId;
    uint32 uEosId;
    uint32 uUnkId;
    uint32 uMaxInputTokens;
} xllm__memory_e5_tokenizer;

typedef struct {
    volatile long iRefCount;
    xmutex pMutex;
    HMODULE hRuntime;
    const OrtApi *pApi;
    OrtEnv *pEnv;
    OrtSessionOptions *pSessionOptions;
    OrtSession *pSession;
    OrtMemoryInfo *pCpuMemoryInfo;
    OrtAllocator *pAllocator;
    char **ppInputNames;
    size_t iInputCount;
    char **ppOutputNames;
    size_t iOutputCount;
    xllm__memory_e5_tokenizer tTokenizer;
    char *sRuntimeDllPath;
    char *sModelPath;
    char *sTokenizerPath;
    uint32 uEmbeddingDim;
} xllm__memory_e5_onnx_ctx;

typedef struct {
    char *sData;
    size_t iLength;
    size_t iCapacity;
} xllm__memory_e5_string_builder;

static void xllm__memory_e5_string_builder_free(
    xllm__memory_e5_string_builder *pBuilder
)
{
    if ( !pBuilder ) {
        return;
    }
    if ( pBuilder->sData ) {
        xrtFree(pBuilder->sData);
    }
    memset(pBuilder, 0, sizeof(*pBuilder));
}

static int xllm__memory_e5_string_builder_reserve(
    xllm__memory_e5_string_builder *pBuilder,
    size_t iNeeded
)
{
    size_t iNewCapacity;
    char *sNewData;

    if ( !pBuilder ) {
        return XRT_NET_ERROR;
    }
    if ( pBuilder->iCapacity >= iNeeded ) {
        return XRT_NET_OK;
    }

    iNewCapacity = pBuilder->iCapacity > 0u ? pBuilder->iCapacity : 64u;
    while ( iNewCapacity < iNeeded ) {
        iNewCapacity *= 2u;
    }

    sNewData = (char *)xrtRealloc(pBuilder->sData, iNewCapacity);
    if ( !sNewData ) {
        return XRT_NET_ERROR;
    }

    pBuilder->sData = sNewData;
    pBuilder->iCapacity = iNewCapacity;
    return XRT_NET_OK;
}

static int xllm__memory_e5_string_builder_append_bytes(
    xllm__memory_e5_string_builder *pBuilder,
    const char *sData,
    size_t iLength
)
{
    if ( !pBuilder || (!sData && iLength > 0u) ) {
        return XRT_NET_ERROR;
    }
    if ( xllm__memory_e5_string_builder_reserve(
            pBuilder,
            pBuilder->iLength + iLength + 1u
         ) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    if ( iLength > 0u ) {
        memcpy(pBuilder->sData + pBuilder->iLength, sData, iLength);
        pBuilder->iLength += iLength;
    }
    pBuilder->sData[pBuilder->iLength] = '\0';
    return XRT_NET_OK;
}

static int xllm__memory_e5_string_builder_append_cstr(
    xllm__memory_e5_string_builder *pBuilder,
    const char *sText
)
{
    return xllm__memory_e5_string_builder_append_bytes(
        pBuilder,
        sText ? sText : "",
        sText ? strlen(sText) : 0u
    );
}

static size_t xllm__memory_e5_utf8_char_length(const char *sText)
{
    unsigned char c;

    if ( !sText || !sText[0] ) {
        return 0u;
    }

    c = (unsigned char)sText[0];
    if ( c < 0x80u ) {
        return 1u;
    }
    if ( (c & 0xE0u) == 0xC0u ) {
        return 2u;
    }
    if ( (c & 0xF0u) == 0xE0u ) {
        return 3u;
    }
    if ( (c & 0xF8u) == 0xF0u ) {
        return 4u;
    }
    return 1u;
}

static void xllm__memory_e5_piece_free(xllm__memory_e5_piece *pPiece)
{
    if ( !pPiece ) {
        return;
    }
    xllm__free_cstr(&pPiece->sPiece);
    memset(pPiece, 0, sizeof(*pPiece));
}

static void xllm__memory_e5_tokenizer_reset(
    xllm__memory_e5_tokenizer *pTokenizer
)
{
    size_t i;

    if ( !pTokenizer ) {
        return;
    }

    for ( i = 0u; i < pTokenizer->iPieceCount; ++i ) {
        xllm__memory_e5_piece_free(&pTokenizer->pPieces[i]);
    }
    if ( pTokenizer->pPieces ) {
        xrtFree(pTokenizer->pPieces);
    }
    if ( pTokenizer->pBucketPieceIds ) {
        xrtFree(pTokenizer->pBucketPieceIds);
    }
    memset(pTokenizer, 0, sizeof(*pTokenizer));
}

static bool xllm__memory_e5_piece_is_special(const char *sPiece)
{
    size_t iLength;

    if ( !sPiece || !sPiece[0] ) {
        return true;
    }
    iLength = strlen(sPiece);
    return iLength >= 2u && sPiece[0] == '<' && sPiece[iLength - 1u] == '>';
}

static bool xllm__memory_e5_path_exists(const char *sPath)
{
    FILE *pFile;

    if ( !sPath || !sPath[0] ) {
        return false;
    }

    pFile = fopen(sPath, "rb");
    if ( !pFile ) {
        return false;
    }
    fclose(pFile);
    return true;
}

static char *xllm__memory_e5_join_existing_path(
    const char *sBaseDir,
    const char *sLeaf
)
{
    char sBuffer[1024];
    int iWritten;

    if ( !sBaseDir || !sBaseDir[0] || !sLeaf || !sLeaf[0] ) {
        return NULL;
    }

    iWritten = snprintf(
        sBuffer,
        sizeof(sBuffer),
        "%s%s%s",
        sBaseDir,
        (sBaseDir[strlen(sBaseDir) - 1u] == '\\' || sBaseDir[strlen(sBaseDir) - 1u] == '/') ? "" : "\\",
        sLeaf
    );
    if ( iWritten <= 0 || (size_t)iWritten >= sizeof(sBuffer) ) {
        return NULL;
    }
    if ( !xllm__memory_e5_path_exists(sBuffer) ) {
        return NULL;
    }
    return xllm__dup_cstr(sBuffer);
}

static char *xllm__memory_e5_resolve_tokenizer_json(const char *sTokenizerPath)
{
    const char *sLeaf = "tokenizer.json";
    size_t iLength;
    char *sCandidate;
    const char *sSlash;

    if ( !sTokenizerPath || !sTokenizerPath[0] ) {
        return NULL;
    }

    iLength = strlen(sTokenizerPath);
    if ( iLength >= strlen(sLeaf) &&
         _stricmp(sTokenizerPath + iLength - strlen(sLeaf), sLeaf) == 0 ) {
        return xllm__dup_cstr(sTokenizerPath);
    }

    if ( xrtDirExists((str)sTokenizerPath) ) {
        return xllm__memory_e5_join_existing_path(sTokenizerPath, sLeaf);
    }

    sSlash = strrchr(sTokenizerPath, '\\');
    if ( !sSlash ) {
        sSlash = strrchr(sTokenizerPath, '/');
    }
    if ( !sSlash ) {
        return NULL;
    }

    sCandidate = (char *)xrtCalloc((size_t)(sSlash - sTokenizerPath) + strlen(sLeaf) + 2u, sizeof(char));
    if ( !sCandidate ) {
        return NULL;
    }
    memcpy(sCandidate, sTokenizerPath, (size_t)(sSlash - sTokenizerPath));
    sCandidate[sSlash - sTokenizerPath] = '\0';

    {
        char *sResolved = xllm__memory_e5_join_existing_path(sCandidate, sLeaf);
        xrtFree(sCandidate);
        return sResolved;
    }
}

static int xllm__memory_e5_tokenizer_init_from_json(
    xllm__memory_e5_tokenizer *pTokenizer,
    const char *sTokenizerJsonPath,
    xllm_error *pError
)
{
    xvalue tRoot = NULL;
    xvalue tModel = NULL;
    xvalue tVocab = NULL;
    size_t i;
    size_t iPieceCount;
    uint32 auCounts[256];
    uint32 auCursors[256];

    if ( !pTokenizer || !sTokenizerJsonPath || !sTokenizerJsonPath[0] ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "multilingual-e5-small tokenizer.json path is required");
        return XRT_NET_ERROR;
    }

    memset(auCounts, 0, sizeof(auCounts));
    memset(auCursors, 0, sizeof(auCursors));
    xllm__memory_e5_tokenizer_reset(pTokenizer);

    tRoot = xrtParseJSON_File((str)sTokenizerJsonPath);
    if ( !tRoot || xvoType(tRoot) != XVO_DT_TABLE ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "failed to parse multilingual-e5-small tokenizer.json");
        xllm__xvalue_release(&tRoot);
        return XRT_NET_ERROR;
    }

    tModel = xvoTableGetValue(tRoot, (str)"model", 0u);
    if ( !tModel || xvoType(tModel) != XVO_DT_TABLE ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "tokenizer.json missing model object");
        xllm__xvalue_release(&tRoot);
        return XRT_NET_ERROR;
    }
    if ( _stricmp((const char *)xvoTableGetText(tModel, (str)"type", 0u), "Unigram") != 0 ) {
        xllm__error_set(pError, XLLM_ERROR_UNSUPPORTED_CAPABILITY, "only unigram tokenizer.json is supported for multilingual-e5-small");
        xllm__xvalue_release(&tRoot);
        return XRT_NET_ERROR;
    }

    tVocab = xvoTableGetValue(tModel, (str)"vocab", 0u);
    if ( !tVocab || xvoType(tVocab) != XVO_DT_ARRAY ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "tokenizer.json missing unigram vocab array");
        xllm__xvalue_release(&tRoot);
        return XRT_NET_ERROR;
    }

    iPieceCount = (size_t)xvoArrayItemCount(tVocab);
    if ( iPieceCount == 0u ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "tokenizer.json unigram vocab is empty");
        xllm__xvalue_release(&tRoot);
        return XRT_NET_ERROR;
    }

    pTokenizer->pPieces = (xllm__memory_e5_piece *)xrtCalloc(iPieceCount, sizeof(*pTokenizer->pPieces));
    if ( !pTokenizer->pPieces ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate multilingual-e5-small tokenizer pieces");
        xllm__xvalue_release(&tRoot);
        return XRT_NET_ERROR;
    }
    pTokenizer->iPieceCount = iPieceCount;
    pTokenizer->uBosId = 0u;
    pTokenizer->uPadId = 1u;
    pTokenizer->uEosId = 2u;
    pTokenizer->uUnkId = (uint32)xvoTableGetInt(tModel, (str)"unk_id", 0u);
    pTokenizer->uMaxInputTokens = XLLM__MEMORY_E5_MAX_INPUT_TOKENS;

    for ( i = 0u; i < iPieceCount; ++i ) {
        xvalue tEntry = xvoArrayGetValue(tVocab, (uint32)i);
        const char *sPiece;

        if ( !tEntry || xvoType(tEntry) != XVO_DT_ARRAY || xvoArrayItemCount(tEntry) < 2u ) {
            xllm__error_set(pError, XLLM_ERROR_PARSE, "tokenizer.json unigram vocab item is invalid");
            xllm__xvalue_release(&tRoot);
            return XRT_NET_ERROR;
        }
        sPiece = (const char *)xvoArrayGetText(tEntry, 0u);
        pTokenizer->pPieces[i].sPiece = xllm__dup_cstr(sPiece ? sPiece : "");
        if ( !pTokenizer->pPieces[i].sPiece ) {
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to copy multilingual-e5-small tokenizer piece");
            xllm__xvalue_release(&tRoot);
            return XRT_NET_ERROR;
        }
        pTokenizer->pPieces[i].uId = (uint32)i;
        pTokenizer->pPieces[i].uLength = (uint32)strlen(pTokenizer->pPieces[i].sPiece);
        pTokenizer->pPieces[i].uFirstByte = pTokenizer->pPieces[i].uLength > 0u
            ? (unsigned char)pTokenizer->pPieces[i].sPiece[0]
            : 0u;
        pTokenizer->pPieces[i].fScore = (float)xvoGetFloat(xvoArrayGetValue(tEntry, 1u));
        pTokenizer->pPieces[i].bSpecial = xllm__memory_e5_piece_is_special(pTokenizer->pPieces[i].sPiece);
        if ( !pTokenizer->pPieces[i].bSpecial && pTokenizer->pPieces[i].uLength > 0u ) {
            ++auCounts[pTokenizer->pPieces[i].uFirstByte];
        }
    }

    pTokenizer->auBucketOffsets[0] = 0u;
    for ( i = 0u; i < 256u; ++i ) {
        pTokenizer->auBucketOffsets[i + 1u] = pTokenizer->auBucketOffsets[i] + auCounts[i];
        auCursors[i] = pTokenizer->auBucketOffsets[i];
    }
    if ( pTokenizer->auBucketOffsets[256u] > 0u ) {
        pTokenizer->pBucketPieceIds = (uint32 *)xrtCalloc(
            pTokenizer->auBucketOffsets[256u],
            sizeof(*pTokenizer->pBucketPieceIds)
        );
        if ( !pTokenizer->pBucketPieceIds ) {
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate multilingual-e5-small tokenizer buckets");
            xllm__xvalue_release(&tRoot);
            return XRT_NET_ERROR;
        }
    }
    for ( i = 0u; i < iPieceCount; ++i ) {
        if ( !pTokenizer->pPieces[i].bSpecial && pTokenizer->pPieces[i].uLength > 0u ) {
            unsigned char uFirstByte = pTokenizer->pPieces[i].uFirstByte;
            pTokenizer->pBucketPieceIds[auCursors[uFirstByte]++] = (uint32)i;
        }
    }

    xllm__xvalue_release(&tRoot);
    return XRT_NET_OK;
}

static int xllm__memory_e5_set_ort_error(
    const xllm__memory_e5_onnx_ctx *pCtx,
    OrtStatus *pStatus,
    const char *sContext,
    xllm_error *pError
)
{
    const char *sOrtMessage = "unknown runtime error";
    char sBuffer[1024];

    if ( pStatus && pCtx && pCtx->pApi ) {
        sOrtMessage = pCtx->pApi->GetErrorMessage(pStatus);
    }

    snprintf(
        sBuffer,
        sizeof(sBuffer),
        "%s: %s",
        sContext ? sContext : "multilingual-e5-small ONNX call failed",
        sOrtMessage ? sOrtMessage : "unknown runtime error"
    );
    if ( pStatus && pCtx && pCtx->pApi ) {
        pCtx->pApi->ReleaseStatus(pStatus);
    }
    xllm__error_set(pError, XLLM_ERROR_INTERNAL, sBuffer);
    return XRT_NET_ERROR;
}

static int xllm__memory_e5_load_file_bytes(
    const char *sPath,
    unsigned char **ppData,
    size_t *piSize,
    xllm_error *pError
)
{
    FILE *pFile = NULL;
    long iFileSize;
    size_t iRead;
    unsigned char *pBuffer = NULL;

    if ( !sPath || !sPath[0] || !ppData || !piSize ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "multilingual-e5-small model path is invalid");
        return XRT_NET_ERROR;
    }

    *ppData = NULL;
    *piSize = 0u;

    pFile = fopen(sPath, "rb");
    if ( !pFile ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "failed to open multilingual-e5-small model");
        return XRT_NET_ERROR;
    }
    if ( fseek(pFile, 0, SEEK_END) != 0 ) {
        fclose(pFile);
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "failed to seek multilingual-e5-small model");
        return XRT_NET_ERROR;
    }
    iFileSize = ftell(pFile);
    if ( iFileSize <= 0 ) {
        fclose(pFile);
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "failed to stat multilingual-e5-small model");
        return XRT_NET_ERROR;
    }
    if ( fseek(pFile, 0, SEEK_SET) != 0 ) {
        fclose(pFile);
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "failed to rewind multilingual-e5-small model");
        return XRT_NET_ERROR;
    }

    pBuffer = (unsigned char *)xrtCalloc((size_t)iFileSize, sizeof(unsigned char));
    if ( !pBuffer ) {
        fclose(pFile);
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate multilingual-e5-small model buffer");
        return XRT_NET_ERROR;
    }
    iRead = fread(pBuffer, 1u, (size_t)iFileSize, pFile);
    fclose(pFile);
    if ( iRead != (size_t)iFileSize ) {
        xrtFree(pBuffer);
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "failed to read multilingual-e5-small model");
        return XRT_NET_ERROR;
    }

    *ppData = pBuffer;
    *piSize = iRead;
    return XRT_NET_OK;
}

static void xllm__memory_e5_free_names(char **ppNames, size_t iCount)
{
    size_t i;

    if ( !ppNames ) {
        return;
    }
    for ( i = 0u; i < iCount; ++i ) {
        xllm__free_cstr(&ppNames[i]);
    }
    xrtFree(ppNames);
}

static int xllm__memory_e5_load_io_names(
    xllm__memory_e5_onnx_ctx *pCtx,
    bool bInput,
    char ***pppNames,
    size_t *piCount,
    xllm_error *pError
)
{
    OrtStatus *pStatus = NULL;
    size_t iCount = 0u;
    char **ppNames = NULL;
    size_t i;

    if ( !pCtx || !pppNames || !piCount ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "multilingual-e5-small IO name output is invalid");
        return XRT_NET_ERROR;
    }

    pStatus = bInput
        ? pCtx->pApi->SessionGetInputCount(pCtx->pSession, &iCount)
        : pCtx->pApi->SessionGetOutputCount(pCtx->pSession, &iCount);
    if ( pStatus ) {
        return xllm__memory_e5_set_ort_error(
            pCtx,
            pStatus,
            bInput ? "SessionGetInputCount" : "SessionGetOutputCount",
            pError
        );
    }

    ppNames = (char **)xrtCalloc(iCount > 0u ? iCount : 1u, sizeof(*ppNames));
    if ( !ppNames ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate multilingual-e5-small IO names");
        return XRT_NET_ERROR;
    }

    for ( i = 0u; i < iCount; ++i ) {
        char *sOrtName = NULL;

        pStatus = bInput
            ? pCtx->pApi->SessionGetInputName(pCtx->pSession, i, pCtx->pAllocator, &sOrtName)
            : pCtx->pApi->SessionGetOutputName(pCtx->pSession, i, pCtx->pAllocator, &sOrtName);
        if ( pStatus ) {
            xllm__memory_e5_free_names(ppNames, iCount);
            return xllm__memory_e5_set_ort_error(
                pCtx,
                pStatus,
                bInput ? "SessionGetInputName" : "SessionGetOutputName",
                pError
            );
        }

        ppNames[i] = xllm__dup_cstr(sOrtName ? sOrtName : "");
        if ( sOrtName ) {
            pCtx->pApi->AllocatorFree(pCtx->pAllocator, sOrtName);
        }
        if ( !ppNames[i] ) {
            xllm__memory_e5_free_names(ppNames, iCount);
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to copy multilingual-e5-small IO name");
            return XRT_NET_ERROR;
        }
    }

    *pppNames = ppNames;
    *piCount = iCount;
    return XRT_NET_OK;
}

static void xllm__memory_builtin_e5_release_ctx(
    xllm__memory_e5_onnx_ctx *pCtx
)
{
    if ( !pCtx ) {
        return;
    }

    xllm__memory_e5_free_names(pCtx->ppInputNames, pCtx->iInputCount);
    xllm__memory_e5_free_names(pCtx->ppOutputNames, pCtx->iOutputCount);
    xllm__memory_e5_tokenizer_reset(&pCtx->tTokenizer);
    xllm__free_cstr(&pCtx->sRuntimeDllPath);
    xllm__free_cstr(&pCtx->sModelPath);
    xllm__free_cstr(&pCtx->sTokenizerPath);

    if ( pCtx->pCpuMemoryInfo && pCtx->pApi ) {
        pCtx->pApi->ReleaseMemoryInfo(pCtx->pCpuMemoryInfo);
    }
    if ( pCtx->pSession && pCtx->pApi ) {
        pCtx->pApi->ReleaseSession(pCtx->pSession);
    }
    if ( pCtx->pSessionOptions && pCtx->pApi ) {
        pCtx->pApi->ReleaseSessionOptions(pCtx->pSessionOptions);
    }
    if ( pCtx->pEnv && pCtx->pApi ) {
        pCtx->pApi->ReleaseEnv(pCtx->pEnv);
    }
    if ( pCtx->pMutex ) {
        xrtMutexDestroy(pCtx->pMutex);
    }
    if ( pCtx->hRuntime ) {
        FreeLibrary(pCtx->hRuntime);
    }
    xrtFree(pCtx);
}

static int xllm__memory_e5_fail_and_release_ctx(
    xllm__memory_e5_onnx_ctx *pCtx,
    OrtStatus *pStatus,
    const char *sContext,
    xllm_error *pError
)
{
    int iStatus = xllm__memory_e5_set_ort_error(pCtx, pStatus, sContext, pError);
    xllm__memory_builtin_e5_release_ctx(pCtx);
    return iStatus;
}

static int xllm__memory_builtin_e5_create_ctx(
    const xllm_memory_builtin_embedder_options *pOptions,
    const xllm_memory_builtin_embedder_probe *pProbe,
    void **ppCtx,
    xllm_error *pError
)
{
    xllm__memory_e5_onnx_ctx *pCtx = NULL;
    unsigned char *pModelBytes = NULL;
    size_t iModelByteCount = 0u;
    FARPROC pProc = NULL;
    xllm__memory_ort_get_api_base_fn pGetApiBase = NULL;
    const OrtApiBase *pApiBase = NULL;
    OrtStatus *pStatus = NULL;
    char *sTokenizerJsonPath = NULL;

    (void)pOptions;

    if ( !ppCtx || !pProbe ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "multilingual-e5-small context output is required");
        return XRT_NET_ERROR;
    }
    *ppCtx = NULL;

    pCtx = (xllm__memory_e5_onnx_ctx *)xrtCalloc(1u, sizeof(*pCtx));
    if ( !pCtx ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate multilingual-e5-small context");
        return XRT_NET_ERROR;
    }

    pCtx->iRefCount = 1;
    pCtx->pMutex = xrtMutexCreate();
    pCtx->sRuntimeDllPath = xllm__dup_cstr(pProbe->sResolvedRuntimeDllPath);
    pCtx->sModelPath = xllm__dup_cstr(pProbe->sResolvedModelPath);
    pCtx->sTokenizerPath = xllm__dup_cstr(pProbe->sResolvedTokenizerPath);
    if ( !pCtx->pMutex || !pCtx->sRuntimeDllPath || !pCtx->sModelPath || !pCtx->sTokenizerPath ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to initialize multilingual-e5-small context");
        xllm__memory_builtin_e5_release_ctx(pCtx);
        return XRT_NET_ERROR;
    }

    sTokenizerJsonPath = xllm__memory_e5_resolve_tokenizer_json(pCtx->sTokenizerPath);
    if ( !sTokenizerJsonPath ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "multilingual-e5-small tokenizer.json could not be resolved");
        xllm__memory_builtin_e5_release_ctx(pCtx);
        return XRT_NET_ERROR;
    }
    if ( xllm__memory_e5_tokenizer_init_from_json(&pCtx->tTokenizer, sTokenizerJsonPath, pError) != XRT_NET_OK ) {
        xllm__free_cstr(&sTokenizerJsonPath);
        xllm__memory_builtin_e5_release_ctx(pCtx);
        return XRT_NET_ERROR;
    }
    xllm__free_cstr(&pCtx->sTokenizerPath);
    pCtx->sTokenizerPath = sTokenizerJsonPath;

    pCtx->hRuntime = LoadLibraryA(pCtx->sRuntimeDllPath);
    if ( !pCtx->hRuntime ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "failed to load multilingual-e5-small onnxruntime.dll");
        xllm__memory_builtin_e5_release_ctx(pCtx);
        return XRT_NET_ERROR;
    }

    pProc = GetProcAddress(pCtx->hRuntime, "OrtGetApiBase");
    memcpy(&pGetApiBase, &pProc, sizeof(pGetApiBase));
    if ( !pGetApiBase ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "failed to resolve OrtGetApiBase");
        xllm__memory_builtin_e5_release_ctx(pCtx);
        return XRT_NET_ERROR;
    }

    pApiBase = pGetApiBase();
    if ( !pApiBase ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "OrtGetApiBase returned NULL");
        xllm__memory_builtin_e5_release_ctx(pCtx);
        return XRT_NET_ERROR;
    }
    pCtx->pApi = pApiBase->GetApi(ORT_API_VERSION);
    if ( !pCtx->pApi ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to get ONNX Runtime C API");
        xllm__memory_builtin_e5_release_ctx(pCtx);
        return XRT_NET_ERROR;
    }

    pStatus = pCtx->pApi->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "xllm-memory-e5", &pCtx->pEnv);
    if ( pStatus ) {
        return xllm__memory_e5_fail_and_release_ctx(pCtx, pStatus, "CreateEnv", pError);
    }
    pStatus = pCtx->pApi->CreateSessionOptions(&pCtx->pSessionOptions);
    if ( pStatus ) {
        return xllm__memory_e5_fail_and_release_ctx(pCtx, pStatus, "CreateSessionOptions", pError);
    }
    pStatus = pCtx->pApi->SetIntraOpNumThreads(pCtx->pSessionOptions, 1);
    if ( pStatus ) {
        return xllm__memory_e5_fail_and_release_ctx(pCtx, pStatus, "SetIntraOpNumThreads", pError);
    }
    pStatus = pCtx->pApi->GetAllocatorWithDefaultOptions(&pCtx->pAllocator);
    if ( pStatus ) {
        return xllm__memory_e5_fail_and_release_ctx(pCtx, pStatus, "GetAllocatorWithDefaultOptions", pError);
    }
    pStatus = pCtx->pApi->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &pCtx->pCpuMemoryInfo);
    if ( pStatus ) {
        return xllm__memory_e5_fail_and_release_ctx(pCtx, pStatus, "CreateCpuMemoryInfo", pError);
    }

    if ( xllm__memory_e5_load_file_bytes(pCtx->sModelPath, &pModelBytes, &iModelByteCount, pError) != XRT_NET_OK ) {
        xllm__memory_builtin_e5_release_ctx(pCtx);
        return XRT_NET_ERROR;
    }
    pStatus = pCtx->pApi->CreateSessionFromArray(
        pCtx->pEnv,
        pModelBytes,
        iModelByteCount,
        pCtx->pSessionOptions,
        &pCtx->pSession
    );
    xrtFree(pModelBytes);
    if ( pStatus ) {
        return xllm__memory_e5_fail_and_release_ctx(pCtx, pStatus, "CreateSessionFromArray", pError);
    }

    if ( xllm__memory_e5_load_io_names(pCtx, true, &pCtx->ppInputNames, &pCtx->iInputCount, pError) != XRT_NET_OK ||
         xllm__memory_e5_load_io_names(pCtx, false, &pCtx->ppOutputNames, &pCtx->iOutputCount, pError) != XRT_NET_OK ) {
        xllm__memory_builtin_e5_release_ctx(pCtx);
        return XRT_NET_ERROR;
    }

    *ppCtx = pCtx;
    return XRT_NET_OK;
}

static int xllm__memory_builtin_e5_clone_ctx(
    void *pCtx,
    void **ppClonedCtx,
    xllm_error *pError
)
{
    xllm__memory_e5_onnx_ctx *pEmbedCtx = (xllm__memory_e5_onnx_ctx *)pCtx;

    (void)pError;

    if ( !ppClonedCtx ) {
        return XRT_NET_ERROR;
    }
    *ppClonedCtx = NULL;
    if ( !pEmbedCtx ) {
        return XRT_NET_OK;
    }

    __xrtAtomicAddFetch32(&pEmbedCtx->iRefCount, 1);
    *ppClonedCtx = pEmbedCtx;
    return XRT_NET_OK;
}

static void xllm__memory_builtin_e5_dispose_ctx(void *pCtx)
{
    xllm__memory_e5_onnx_ctx *pEmbedCtx = (xllm__memory_e5_onnx_ctx *)pCtx;

    if ( !pEmbedCtx ) {
        return;
    }
    if ( __xrtAtomicAddFetch32(&pEmbedCtx->iRefCount, -1) == 0 ) {
        xllm__memory_builtin_e5_release_ctx(pEmbedCtx);
    }
}

static char *xllm__memory_e5_make_prefixed_text(
    xllm_memory_embed_task eTask,
    const char *sText
)
{
    xllm__memory_e5_string_builder tBuilder;
    const char *sPrefix =
        (eTask == XLLM_MEMORY_EMBED_QUERY)
            ? XLLM__MEMORY_E5_QUERY_PREFIX
            : XLLM__MEMORY_E5_DOCUMENT_PREFIX;

    memset(&tBuilder, 0, sizeof(tBuilder));
    if ( xllm__memory_e5_string_builder_append_cstr(&tBuilder, sPrefix) != XRT_NET_OK ||
         xllm__memory_e5_string_builder_append_cstr(&tBuilder, sText ? sText : "") != XRT_NET_OK ) {
        xllm__memory_e5_string_builder_free(&tBuilder);
        return NULL;
    }
    return tBuilder.sData;
}

static char *xllm__memory_e5_normalize_metaspace(const char *sText)
{
    static const char *sMetaspace = "\xE2\x96\x81";
    xllm__memory_e5_string_builder tBuilder;
    const unsigned char *pCursor = (const unsigned char *)sText;
    bool bLastWasSpace = true;

    memset(&tBuilder, 0, sizeof(tBuilder));
    if ( xllm__memory_e5_string_builder_append_cstr(&tBuilder, sMetaspace) != XRT_NET_OK ) {
        xllm__memory_e5_string_builder_free(&tBuilder);
        return NULL;
    }

    while ( pCursor && *pCursor ) {
        size_t iCharLength;

        if ( *pCursor < 0x80u && isspace(*pCursor) ) {
            if ( !bLastWasSpace ) {
                if ( xllm__memory_e5_string_builder_append_cstr(&tBuilder, sMetaspace) != XRT_NET_OK ) {
                    xllm__memory_e5_string_builder_free(&tBuilder);
                    return NULL;
                }
                bLastWasSpace = true;
            }
            ++pCursor;
            continue;
        }

        iCharLength = xllm__memory_e5_utf8_char_length((const char *)pCursor);
        if ( xllm__memory_e5_string_builder_append_bytes(&tBuilder, (const char *)pCursor, iCharLength) != XRT_NET_OK ) {
            xllm__memory_e5_string_builder_free(&tBuilder);
            return NULL;
        }
        bLastWasSpace = false;
        pCursor += iCharLength;
    }

    return tBuilder.sData;
}

static int xllm__memory_e5_tokenize(
    const xllm__memory_e5_tokenizer *pTokenizer,
    const char *sText,
    int64 **ppTokenIds,
    size_t *piTokenCount,
    xllm_error *pError
)
{
    size_t iTextLength;
    size_t *piBoundaries = NULL;
    int32 *piBoundaryMap = NULL;
    size_t iBoundaryCount = 0u;
    size_t i;
    double *pfBest = NULL;
    uint32 *puBestTokenIds = NULL;
    uint32 *puNextBoundary = NULL;
    int64 *piTokenIds = NULL;
    size_t iTokenCount = 0u;

    if ( !pTokenizer || !sText || !ppTokenIds || !piTokenCount ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "multilingual-e5-small tokenization arguments are invalid");
        return XRT_NET_ERROR;
    }

    *ppTokenIds = NULL;
    *piTokenCount = 0u;
    iTextLength = strlen(sText);

    piBoundaries = (size_t *)xrtCalloc(iTextLength + 2u, sizeof(*piBoundaries));
    piBoundaryMap = (int32 *)xrtCalloc(iTextLength + 2u, sizeof(*piBoundaryMap));
    if ( !piBoundaries || !piBoundaryMap ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate multilingual-e5-small token boundaries");
        goto cleanup;
    }
    for ( i = 0u; i < iTextLength + 2u; ++i ) {
        piBoundaryMap[i] = -1;
    }

    for ( i = 0u; i < iTextLength; ) {
        size_t iCharLength = xllm__memory_e5_utf8_char_length(sText + i);
        piBoundaries[iBoundaryCount] = i;
        piBoundaryMap[i] = (int32)iBoundaryCount;
        ++iBoundaryCount;
        i += iCharLength;
    }
    piBoundaries[iBoundaryCount] = iTextLength;
    piBoundaryMap[iTextLength] = (int32)iBoundaryCount;
    ++iBoundaryCount;

    pfBest = (double *)xrtCalloc(iBoundaryCount, sizeof(*pfBest));
    puBestTokenIds = (uint32 *)xrtCalloc(iBoundaryCount, sizeof(*puBestTokenIds));
    puNextBoundary = (uint32 *)xrtCalloc(iBoundaryCount, sizeof(*puNextBoundary));
    if ( !pfBest || !puBestTokenIds || !puNextBoundary ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate multilingual-e5-small token search state");
        goto cleanup;
    }

    pfBest[iBoundaryCount - 1u] = 0.0;
    for ( i = iBoundaryCount - 1u; i-- > 0u; ) {
        size_t iPos = piBoundaries[i];
        unsigned char uFirstByte = (unsigned char)sText[iPos];
        uint32 iStart = pTokenizer->auBucketOffsets[uFirstByte];
        uint32 iEnd = pTokenizer->auBucketOffsets[uFirstByte + 1u];
        double fBestScore = -DBL_MAX;
        uint32 uBestTokenId = pTokenizer->uUnkId;
        uint32 uBestNextBoundary = (uint32)(i + 1u);
        uint32 j;

        for ( j = iStart; j < iEnd; ++j ) {
            const xllm__memory_e5_piece *pPiece = &pTokenizer->pPieces[pTokenizer->pBucketPieceIds[j]];
            size_t iEndPos = iPos + pPiece->uLength;
            int32 iNextBoundary;

            if ( pPiece->uLength == 0u || iEndPos > iTextLength ) {
                continue;
            }
            if ( memcmp(sText + iPos, pPiece->sPiece, pPiece->uLength) != 0 ) {
                continue;
            }
            iNextBoundary = piBoundaryMap[iEndPos];
            if ( iNextBoundary < 0 ) {
                continue;
            }
            if ( pPiece->fScore + pfBest[iNextBoundary] > fBestScore ) {
                fBestScore = pPiece->fScore + pfBest[iNextBoundary];
                uBestTokenId = pPiece->uId;
                uBestNextBoundary = (uint32)iNextBoundary;
            }
        }

        if ( fBestScore <= -DBL_MAX / 2.0 ) {
            fBestScore = -10.0 + pfBest[i + 1u];
            uBestTokenId = pTokenizer->uUnkId;
            uBestNextBoundary = (uint32)(i + 1u);
        }

        pfBest[i] = fBestScore;
        puBestTokenIds[i] = uBestTokenId;
        puNextBoundary[i] = uBestNextBoundary;
    }

    piTokenIds = (int64 *)xrtCalloc(
        (size_t)pTokenizer->uMaxInputTokens > 0u ? pTokenizer->uMaxInputTokens : 512u,
        sizeof(*piTokenIds)
    );
    if ( !piTokenIds ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate multilingual-e5-small token ids");
        goto cleanup;
    }

    piTokenIds[iTokenCount++] = (int64)pTokenizer->uBosId;
    for ( i = 0u; i + 1u < iBoundaryCount && iTokenCount + 1u < pTokenizer->uMaxInputTokens; ) {
        piTokenIds[iTokenCount++] = (int64)puBestTokenIds[i];
        i = puNextBoundary[i];
    }
    if ( iTokenCount < pTokenizer->uMaxInputTokens ) {
        piTokenIds[iTokenCount++] = (int64)pTokenizer->uEosId;
    } else {
        piTokenIds[pTokenizer->uMaxInputTokens - 1u] = (int64)pTokenizer->uEosId;
        iTokenCount = pTokenizer->uMaxInputTokens;
    }

    *ppTokenIds = piTokenIds;
    *piTokenCount = iTokenCount;
    piTokenIds = NULL;

cleanup:
    if ( piTokenIds ) {
        xrtFree(piTokenIds);
    }
    if ( puNextBoundary ) {
        xrtFree(puNextBoundary);
    }
    if ( puBestTokenIds ) {
        xrtFree(puBestTokenIds);
    }
    if ( pfBest ) {
        xrtFree(pfBest);
    }
    if ( piBoundaryMap ) {
        xrtFree(piBoundaryMap);
    }
    if ( piBoundaries ) {
        xrtFree(piBoundaries);
    }
    return *ppTokenIds ? XRT_NET_OK : XRT_NET_ERROR;
}

static void xllm__memory_e5_normalize_vector(float *pfValues, uint32 uValueCount)
{
    uint32 i;
    double fNorm = 0.0;

    if ( !pfValues || uValueCount == 0u ) {
        return;
    }

    for ( i = 0u; i < uValueCount; ++i ) {
        fNorm += (double)pfValues[i] * (double)pfValues[i];
    }
    if ( fNorm <= 0.0 ) {
        return;
    }
    fNorm = sqrt(fNorm);
    for ( i = 0u; i < uValueCount; ++i ) {
        pfValues[i] = (float)((double)pfValues[i] / fNorm);
    }
}

static int xllm__memory_e5_make_tensor(
    const xllm__memory_e5_onnx_ctx *pCtx,
    void *pData,
    size_t iBytes,
    const int64 *aiShape,
    size_t iShapeCount,
    ONNXTensorElementDataType eType,
    OrtValue **ppValue,
    xllm_error *pError
)
{
    OrtStatus *pStatus;

    pStatus = pCtx->pApi->CreateTensorWithDataAsOrtValue(
        pCtx->pCpuMemoryInfo,
        pData,
        iBytes,
        aiShape,
        iShapeCount,
        eType,
        ppValue
    );
    if ( pStatus ) {
        return xllm__memory_e5_set_ort_error(pCtx, pStatus, "CreateTensorWithDataAsOrtValue", pError);
    }
    return XRT_NET_OK;
}

static int xllm__memory_e5_pick_output_embedding(
    xllm__memory_e5_onnx_ctx *pCtx,
    OrtValue **ppOutputValues,
    size_t iTokenCount,
    xllm_memory_embedding *pEmbedding,
    xllm_error *pError
)
{
    size_t i;

    for ( i = 0u; i < pCtx->iOutputCount; ++i ) {
        OrtTensorTypeAndShapeInfo *pTensorInfo = NULL;
        OrtStatus *pStatus;
        ONNXTensorElementDataType eElemType = ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED;
        size_t iDimCount = 0u;
        int64 aiDims[8];
        float *pfData = NULL;
        size_t j;

        pStatus = pCtx->pApi->GetTensorTypeAndShape(ppOutputValues[i], &pTensorInfo);
        if ( pStatus ) {
            return xllm__memory_e5_set_ort_error(pCtx, pStatus, "GetTensorTypeAndShape", pError);
        }
        pStatus = pCtx->pApi->GetTensorElementType(pTensorInfo, &eElemType);
        if ( pStatus ) {
            pCtx->pApi->ReleaseTensorTypeAndShapeInfo(pTensorInfo);
            return xllm__memory_e5_set_ort_error(pCtx, pStatus, "GetTensorElementType", pError);
        }
        if ( eElemType != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ) {
            pCtx->pApi->ReleaseTensorTypeAndShapeInfo(pTensorInfo);
            continue;
        }
        pStatus = pCtx->pApi->GetDimensionsCount(pTensorInfo, &iDimCount);
        if ( pStatus ) {
            pCtx->pApi->ReleaseTensorTypeAndShapeInfo(pTensorInfo);
            return xllm__memory_e5_set_ort_error(pCtx, pStatus, "GetDimensionsCount", pError);
        }
        if ( iDimCount == 0u || iDimCount > sizeof(aiDims) / sizeof(aiDims[0]) ) {
            pCtx->pApi->ReleaseTensorTypeAndShapeInfo(pTensorInfo);
            continue;
        }
        pStatus = pCtx->pApi->GetDimensions(pTensorInfo, aiDims, iDimCount);
        if ( pStatus ) {
            pCtx->pApi->ReleaseTensorTypeAndShapeInfo(pTensorInfo);
            return xllm__memory_e5_set_ort_error(pCtx, pStatus, "GetDimensions", pError);
        }
        pStatus = pCtx->pApi->GetTensorMutableData(ppOutputValues[i], (void **)&pfData);
        if ( pStatus ) {
            pCtx->pApi->ReleaseTensorTypeAndShapeInfo(pTensorInfo);
            return xllm__memory_e5_set_ort_error(pCtx, pStatus, "GetTensorMutableData", pError);
        }

        if ( iDimCount == 2u && aiDims[0] == 1 && aiDims[1] > 0 ) {
            pEmbedding->pfValues = (float *)xrtCalloc((size_t)aiDims[1], sizeof(float));
            if ( !pEmbedding->pfValues ) {
                pCtx->pApi->ReleaseTensorTypeAndShapeInfo(pTensorInfo);
                xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate embedding vector");
                return XRT_NET_ERROR;
            }
            memcpy(pEmbedding->pfValues, pfData, (size_t)aiDims[1] * sizeof(float));
            pEmbedding->uValueCount = (uint32)aiDims[1];
            pCtx->uEmbeddingDim = (uint32)aiDims[1];
            pCtx->pApi->ReleaseTensorTypeAndShapeInfo(pTensorInfo);
            return XRT_NET_OK;
        }

        if ( iDimCount == 3u && aiDims[0] == 1 && aiDims[1] > 0 && aiDims[2] > 0 ) {
            size_t iSeqLength = (size_t)aiDims[1];
            size_t iHiddenSize = (size_t)aiDims[2];
            size_t iUsed = iTokenCount < iSeqLength ? iTokenCount : iSeqLength;

            pEmbedding->pfValues = (float *)xrtCalloc(iHiddenSize, sizeof(float));
            if ( !pEmbedding->pfValues ) {
                pCtx->pApi->ReleaseTensorTypeAndShapeInfo(pTensorInfo);
                xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate pooled embedding vector");
                return XRT_NET_ERROR;
            }
            for ( j = 0u; j < iUsed; ++j ) {
                const float *pfRow = pfData + (j * iHiddenSize);
                size_t k;
                for ( k = 0u; k < iHiddenSize; ++k ) {
                    pEmbedding->pfValues[k] += pfRow[k];
                }
            }
            if ( iUsed > 0u ) {
                for ( j = 0u; j < iHiddenSize; ++j ) {
                    pEmbedding->pfValues[j] = (float)((double)pEmbedding->pfValues[j] / (double)iUsed);
                }
            }
            pEmbedding->uValueCount = (uint32)iHiddenSize;
            pCtx->uEmbeddingDim = (uint32)iHiddenSize;
            pCtx->pApi->ReleaseTensorTypeAndShapeInfo(pTensorInfo);
            return XRT_NET_OK;
        }

        pCtx->pApi->ReleaseTensorTypeAndShapeInfo(pTensorInfo);
    }

    xllm__error_set(pError, XLLM_ERROR_PARSE, "multilingual-e5-small did not return a usable float embedding tensor");
    return XRT_NET_ERROR;
}

static int xllm__memory_builtin_e5_embed_text(
    void *pCtx,
    xllm_memory_embed_task eTask,
    const char *sText,
    xllm_memory_embedding *pEmbedding,
    xllm_error *pError
)
{
    xllm__memory_e5_onnx_ctx *pEmbedCtx = (xllm__memory_e5_onnx_ctx *)pCtx;
    char *sPrefixedText = NULL;
    char *sNormalizedText = NULL;
    int64 *piInputIds = NULL;
    int64 *piAttentionMask = NULL;
    int64 *piTokenTypeIds = NULL;
    int64 *piPositionIds = NULL;
    OrtValue **ppInputValues = NULL;
    OrtValue **ppOutputValues = NULL;
    const char **ppInputNames = NULL;
    const char **ppOutputNames = NULL;
    size_t iTokenCount = 0u;
    size_t i;
    int iStatus = XRT_NET_ERROR;

    if ( !pEmbedCtx || !pEmbedding || !sText || !sText[0] ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "multilingual-e5-small embed input is invalid");
        return XRT_NET_ERROR;
    }

    memset(pEmbedding, 0, sizeof(*pEmbedding));
    sPrefixedText = xllm__memory_e5_make_prefixed_text(eTask, sText);
    if ( !sPrefixedText ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to build multilingual-e5-small prefixed text");
        goto cleanup;
    }
    sNormalizedText = xllm__memory_e5_normalize_metaspace(sPrefixedText);
    if ( !sNormalizedText ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to normalize multilingual-e5-small input text");
        goto cleanup;
    }
    if ( xllm__memory_e5_tokenize(&pEmbedCtx->tTokenizer, sNormalizedText, &piInputIds, &iTokenCount, pError) != XRT_NET_OK ) {
        goto cleanup;
    }

    piAttentionMask = (int64 *)xrtCalloc(iTokenCount, sizeof(*piAttentionMask));
    ppInputValues = (OrtValue **)xrtCalloc(pEmbedCtx->iInputCount > 0u ? pEmbedCtx->iInputCount : 1u, sizeof(*ppInputValues));
    ppOutputValues = (OrtValue **)xrtCalloc(pEmbedCtx->iOutputCount > 0u ? pEmbedCtx->iOutputCount : 1u, sizeof(*ppOutputValues));
    ppInputNames = (const char **)xrtCalloc(pEmbedCtx->iInputCount > 0u ? pEmbedCtx->iInputCount : 1u, sizeof(*ppInputNames));
    ppOutputNames = (const char **)xrtCalloc(pEmbedCtx->iOutputCount > 0u ? pEmbedCtx->iOutputCount : 1u, sizeof(*ppOutputNames));
    if ( !piAttentionMask || !ppInputValues || !ppOutputValues || !ppInputNames || !ppOutputNames ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate multilingual-e5-small ONNX buffers");
        goto cleanup;
    }

    for ( i = 0u; i < iTokenCount; ++i ) {
        piAttentionMask[i] = 1;
    }
    for ( i = 0u; i < pEmbedCtx->iOutputCount; ++i ) {
        ppOutputNames[i] = pEmbedCtx->ppOutputNames[i];
    }

    {
        int64 aiShape[2];
        aiShape[0] = 1;
        aiShape[1] = (int64)iTokenCount;

        for ( i = 0u; i < pEmbedCtx->iInputCount; ++i ) {
            const char *sName = pEmbedCtx->ppInputNames[i];

            ppInputNames[i] = sName;
            if ( strstr(sName, "input_ids") != NULL ) {
                if ( xllm__memory_e5_make_tensor(
                        pEmbedCtx,
                        piInputIds,
                        iTokenCount * sizeof(*piInputIds),
                        aiShape,
                        2u,
                        ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64,
                        &ppInputValues[i],
                        pError
                     ) != XRT_NET_OK ) {
                    goto cleanup;
                }
            } else if ( strstr(sName, "attention_mask") != NULL ) {
                if ( xllm__memory_e5_make_tensor(
                        pEmbedCtx,
                        piAttentionMask,
                        iTokenCount * sizeof(*piAttentionMask),
                        aiShape,
                        2u,
                        ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64,
                        &ppInputValues[i],
                        pError
                     ) != XRT_NET_OK ) {
                    goto cleanup;
                }
            } else if ( strstr(sName, "token_type_ids") != NULL ) {
                piTokenTypeIds = (int64 *)xrtCalloc(iTokenCount, sizeof(*piTokenTypeIds));
                if ( !piTokenTypeIds ) {
                    xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate multilingual-e5-small token_type_ids");
                    goto cleanup;
                }
                if ( xllm__memory_e5_make_tensor(
                        pEmbedCtx,
                        piTokenTypeIds,
                        iTokenCount * sizeof(*piTokenTypeIds),
                        aiShape,
                        2u,
                        ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64,
                        &ppInputValues[i],
                        pError
                     ) != XRT_NET_OK ) {
                    goto cleanup;
                }
            } else if ( strstr(sName, "position_ids") != NULL ) {
                piPositionIds = (int64 *)xrtCalloc(iTokenCount, sizeof(*piPositionIds));
                if ( !piPositionIds ) {
                    xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate multilingual-e5-small position_ids");
                    goto cleanup;
                }
                {
                    size_t j;
                    for ( j = 0u; j < iTokenCount; ++j ) {
                        piPositionIds[j] = (int64)j;
                    }
                }
                if ( xllm__memory_e5_make_tensor(
                        pEmbedCtx,
                        piPositionIds,
                        iTokenCount * sizeof(*piPositionIds),
                        aiShape,
                        2u,
                        ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64,
                        &ppInputValues[i],
                        pError
                     ) != XRT_NET_OK ) {
                    goto cleanup;
                }
            } else {
                xllm__error_set(pError, XLLM_ERROR_UNSUPPORTED_CAPABILITY, "multilingual-e5-small model input is not supported by the builtin runner");
                goto cleanup;
            }
        }
    }

    xrtMutexLock(pEmbedCtx->pMutex);
    {
        OrtStatus *pStatus = pEmbedCtx->pApi->Run(
            pEmbedCtx->pSession,
            NULL,
            ppInputNames,
            (const OrtValue *const *)ppInputValues,
            pEmbedCtx->iInputCount,
            ppOutputNames,
            pEmbedCtx->iOutputCount,
            ppOutputValues
        );
        xrtMutexUnlock(pEmbedCtx->pMutex);
        if ( pStatus ) {
            iStatus = xllm__memory_e5_set_ort_error(pEmbedCtx, pStatus, "Run", pError);
            goto cleanup;
        }
    }

    if ( xllm__memory_e5_pick_output_embedding(pEmbedCtx, ppOutputValues, iTokenCount, pEmbedding, pError) != XRT_NET_OK ) {
        goto cleanup;
    }
    if ( XLLM__MEMORY_E5_NORMALIZE_OUTPUT ) {
        xllm__memory_e5_normalize_vector(pEmbedding->pfValues, pEmbedding->uValueCount);
    }
    iStatus = XRT_NET_OK;

cleanup:
    if ( ppOutputValues ) {
        for ( i = 0u; i < pEmbedCtx->iOutputCount; ++i ) {
            if ( ppOutputValues[i] ) {
                pEmbedCtx->pApi->ReleaseValue(ppOutputValues[i]);
            }
        }
        xrtFree(ppOutputValues);
    }
    if ( ppInputValues ) {
        for ( i = 0u; i < pEmbedCtx->iInputCount; ++i ) {
            if ( ppInputValues[i] ) {
                pEmbedCtx->pApi->ReleaseValue(ppInputValues[i]);
            }
        }
        xrtFree(ppInputValues);
    }
    if ( ppOutputNames ) {
        xrtFree(ppOutputNames);
    }
    if ( ppInputNames ) {
        xrtFree(ppInputNames);
    }
    if ( piPositionIds ) {
        xrtFree(piPositionIds);
    }
    if ( piTokenTypeIds ) {
        xrtFree(piTokenTypeIds);
    }
    if ( piAttentionMask ) {
        xrtFree(piAttentionMask);
    }
    if ( piInputIds ) {
        xrtFree(piInputIds);
    }
    xllm__free_cstr(&sNormalizedText);
    xllm__free_cstr(&sPrefixedText);
    if ( iStatus != XRT_NET_OK ) {
        xllm__memory_embedding_reset_with_embedder(NULL, pEmbedding);
    }
    return iStatus;
}
