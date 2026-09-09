#define _CRT_SECURE_NO_WARNINGS

#include "../onnx_demo.h"

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "onnxruntime_c_api.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef const OrtApiBase *(ORT_API_CALL *onnx_demo_get_api_base_fn)(void);

typedef struct onnx_demo_string_builder {
    char *pData;
    size_t uLength;
    size_t uCapacity;
} onnx_demo_string_builder;

struct onnx_demo {
    HMODULE hRuntime;
    const OrtApi *pApi;
    OrtEnv *pEnv;
    OrtSessionOptions *pSessionOptions;
    OrtSession *pSession;
    OrtMemoryInfo *pMemoryInfo;
    char **ppVocab;
    size_t uVocabSize;
    size_t uEmbeddingDim;
    char szLastError[512];
};

static void
onnx_demo_set_error(onnx_demo *pDemo, const char *pMessage)
{
    if (pDemo == NULL) {
        return;
    }

    snprintf(pDemo->szLastError, sizeof(pDemo->szLastError), "%s", pMessage != NULL ? pMessage : "unknown error");
}

static int
onnx_demo_set_ort_error(onnx_demo *pDemo, OrtStatus *pStatus, const char *pContext)
{
    const char *pErrorMessage = "unknown runtime error";

    if (pStatus != NULL && pDemo != NULL && pDemo->pApi != NULL) {
        pErrorMessage = pDemo->pApi->GetErrorMessage(pStatus);
    }

    if (pDemo != NULL) {
        snprintf(
            pDemo->szLastError,
            sizeof(pDemo->szLastError),
            "%s: %s",
            pContext != NULL ? pContext : "onnxruntime call failed",
            pErrorMessage != NULL ? pErrorMessage : "unknown runtime error");
    }

    if (pStatus != NULL && pDemo != NULL && pDemo->pApi != NULL) {
        pDemo->pApi->ReleaseStatus(pStatus);
    }

    return -1;
}

static char *
onnx_demo_strdup(const char *pText)
{
    size_t uLength;
    char *pCopy;

    if (pText == NULL) {
        return NULL;
    }

    uLength = strlen(pText);
    pCopy = (char *)malloc(uLength + 1);
    if (pCopy == NULL) {
        return NULL;
    }

    memcpy(pCopy, pText, uLength + 1);
    return pCopy;
}

static int
onnx_demo_string_builder_reserve(onnx_demo_string_builder *pBuilder, size_t uNeeded)
{
    size_t uCapacity;
    char *pNewBuffer;

    if (pBuilder->uCapacity >= uNeeded) {
        return 0;
    }

    uCapacity = pBuilder->uCapacity == 0 ? 32 : pBuilder->uCapacity;
    while (uCapacity < uNeeded) {
        uCapacity *= 2;
    }

    pNewBuffer = (char *)realloc(pBuilder->pData, uCapacity);
    if (pNewBuffer == NULL) {
        return -1;
    }

    pBuilder->pData = pNewBuffer;
    pBuilder->uCapacity = uCapacity;
    return 0;
}

static int
onnx_demo_string_builder_append(onnx_demo_string_builder *pBuilder, char cValue)
{
    if (onnx_demo_string_builder_reserve(pBuilder, pBuilder->uLength + 2) != 0) {
        return -1;
    }

    pBuilder->pData[pBuilder->uLength++] = cValue;
    pBuilder->pData[pBuilder->uLength] = '\0';
    return 0;
}

static void
onnx_demo_string_builder_reset(onnx_demo_string_builder *pBuilder)
{
    pBuilder->uLength = 0;
    if (pBuilder->pData != NULL) {
        pBuilder->pData[0] = '\0';
    }
}

static void
onnx_demo_string_builder_free(onnx_demo_string_builder *pBuilder)
{
    free(pBuilder->pData);
    pBuilder->pData = NULL;
    pBuilder->uLength = 0;
    pBuilder->uCapacity = 0;
}

static size_t
onnx_demo_utf8_decode(const unsigned char *pData, size_t uRemaining, uint32_t *puCodepoint)
{
    if (uRemaining == 0) {
        return 0;
    }

    if (pData[0] < 0x80U) {
        *puCodepoint = pData[0];
        return 1;
    }

    if ((pData[0] & 0xE0U) == 0xC0U && uRemaining >= 2 &&
        (pData[1] & 0xC0U) == 0x80U) {
        *puCodepoint = ((uint32_t)(pData[0] & 0x1FU) << 6) |
                       (uint32_t)(pData[1] & 0x3FU);
        return 2;
    }

    if ((pData[0] & 0xF0U) == 0xE0U && uRemaining >= 3 &&
        (pData[1] & 0xC0U) == 0x80U &&
        (pData[2] & 0xC0U) == 0x80U) {
        *puCodepoint = ((uint32_t)(pData[0] & 0x0FU) << 12) |
                       ((uint32_t)(pData[1] & 0x3FU) << 6) |
                       (uint32_t)(pData[2] & 0x3FU);
        return 3;
    }

    if ((pData[0] & 0xF8U) == 0xF0U && uRemaining >= 4 &&
        (pData[1] & 0xC0U) == 0x80U &&
        (pData[2] & 0xC0U) == 0x80U &&
        (pData[3] & 0xC0U) == 0x80U) {
        *puCodepoint = ((uint32_t)(pData[0] & 0x07U) << 18) |
                       ((uint32_t)(pData[1] & 0x3FU) << 12) |
                       ((uint32_t)(pData[2] & 0x3FU) << 6) |
                       (uint32_t)(pData[3] & 0x3FU);
        return 4;
    }

    return 0;
}

static int
onnx_demo_is_cjk_codepoint(uint32_t uCodepoint)
{
    if (uCodepoint >= 0x3400U && uCodepoint <= 0x4DBFU) {
        return 1;
    }
    if (uCodepoint >= 0x4E00U && uCodepoint <= 0x9FFFU) {
        return 1;
    }
    if (uCodepoint >= 0xF900U && uCodepoint <= 0xFAFFU) {
        return 1;
    }
    if (uCodepoint >= 0x20000U && uCodepoint <= 0x2A6DFU) {
        return 1;
    }
    return 0;
}

static int
onnx_demo_lookup_token(const onnx_demo *pDemo, const char *pToken)
{
    size_t i;

    for (i = 0; i < pDemo->uVocabSize; ++i) {
        if (strcmp(pDemo->ppVocab[i], pToken) == 0) {
            return (int)i;
        }
    }

    return -1;
}

static void
onnx_demo_consume_token(
    const onnx_demo *pDemo,
    const char *pToken,
    float *pBow,
    size_t *puMatchedTokens,
    size_t *puTotalTokens)
{
    int iIndex;

    if (pToken == NULL || pToken[0] == '\0') {
        return;
    }

    (*puTotalTokens)++;
    iIndex = onnx_demo_lookup_token(pDemo, pToken);
    if (iIndex >= 0) {
        pBow[iIndex] += 1.0f;
        (*puMatchedTokens)++;
    }
}

static int
onnx_demo_flush_ascii_token(
    const onnx_demo *pDemo,
    onnx_demo_string_builder *pBuilder,
    float *pBow,
    size_t *puMatchedTokens,
    size_t *puTotalTokens)
{
    if (pBuilder->uLength == 0) {
        return 0;
    }

    onnx_demo_consume_token(pDemo, pBuilder->pData, pBow, puMatchedTokens, puTotalTokens);
    onnx_demo_string_builder_reset(pBuilder);
    return 0;
}

static int
onnx_demo_fill_bow(
    const onnx_demo *pDemo,
    const char *pText,
    float *pBow,
    size_t *puMatchedTokens,
    size_t *puTotalTokens)
{
    const char *pWhitespace = strpbrk(pText, " \t\r\n");
    const unsigned char *pCursor = (const unsigned char *)pText;
    onnx_demo_string_builder sAscii = {0};
    char szPrevCjk[8];
    size_t uPrevCjkLength = 0;

    if (pWhitespace != NULL) {
        const unsigned char *pSegmentCursor = (const unsigned char *)pText;
        onnx_demo_string_builder sSegment = {0};

        while (*pSegmentCursor != '\0') {
            if (isspace(*pSegmentCursor)) {
                if (sSegment.uLength > 0) {
                    onnx_demo_consume_token(pDemo, sSegment.pData, pBow, puMatchedTokens, puTotalTokens);
                    onnx_demo_string_builder_reset(&sSegment);
                }
                ++pSegmentCursor;
                continue;
            }

            if (onnx_demo_string_builder_reserve(&sSegment, sSegment.uLength + 5) != 0) {
                onnx_demo_string_builder_free(&sSegment);
                return -1;
            }

            if (*pSegmentCursor < 0x80U) {
                sSegment.pData[sSegment.uLength++] = (char)tolower(*pSegmentCursor);
                sSegment.pData[sSegment.uLength] = '\0';
                ++pSegmentCursor;
                continue;
            }

            {
                size_t uRemaining = strlen((const char *)pSegmentCursor);
                uint32_t uCodepoint = 0;
                size_t uConsumed = onnx_demo_utf8_decode(pSegmentCursor, uRemaining, &uCodepoint);
                if (uConsumed == 0) {
                    uConsumed = 1;
                }
                memcpy(sSegment.pData + sSegment.uLength, pSegmentCursor, uConsumed);
                sSegment.uLength += uConsumed;
                sSegment.pData[sSegment.uLength] = '\0';
                pSegmentCursor += uConsumed;
            }
        }

        if (sSegment.uLength > 0) {
            onnx_demo_consume_token(pDemo, sSegment.pData, pBow, puMatchedTokens, puTotalTokens);
        }

        onnx_demo_string_builder_free(&sSegment);
        return 0;
    }

    while (*pCursor != '\0') {
        size_t uRemaining = strlen((const char *)pCursor);
        uint32_t uCodepoint = 0;
        size_t uConsumed = 0;

        if (*pCursor < 0x80U && (isalnum(*pCursor) || *pCursor == '_' || *pCursor == '-')) {
            char cLower = (char)tolower(*pCursor);
            if (onnx_demo_string_builder_append(&sAscii, cLower) != 0) {
                onnx_demo_string_builder_free(&sAscii);
                return -1;
            }
            uPrevCjkLength = 0;
            ++pCursor;
            continue;
        }

        if (onnx_demo_flush_ascii_token(pDemo, &sAscii, pBow, puMatchedTokens, puTotalTokens) != 0) {
            onnx_demo_string_builder_free(&sAscii);
            return -1;
        }

        uConsumed = onnx_demo_utf8_decode(pCursor, uRemaining, &uCodepoint);
        if (uConsumed != 0 && onnx_demo_is_cjk_codepoint(uCodepoint)) {
            char szCurrentCjk[8];
            char szBigram[16];

            memcpy(szCurrentCjk, pCursor, uConsumed);
            szCurrentCjk[uConsumed] = '\0';
            onnx_demo_consume_token(pDemo, szCurrentCjk, pBow, puMatchedTokens, puTotalTokens);

            if (uPrevCjkLength > 0) {
                memcpy(szBigram, szPrevCjk, uPrevCjkLength);
                memcpy(szBigram + uPrevCjkLength, szCurrentCjk, uConsumed + 1);
                onnx_demo_consume_token(pDemo, szBigram, pBow, puMatchedTokens, puTotalTokens);
            }

            memcpy(szPrevCjk, szCurrentCjk, uConsumed + 1);
            uPrevCjkLength = uConsumed;
            pCursor += uConsumed;
            continue;
        }

        uPrevCjkLength = 0;
        pCursor += uConsumed == 0 ? 1 : uConsumed;
    }

    if (onnx_demo_flush_ascii_token(pDemo, &sAscii, pBow, puMatchedTokens, puTotalTokens) != 0) {
        onnx_demo_string_builder_free(&sAscii);
        return -1;
    }

    onnx_demo_string_builder_free(&sAscii);
    return 0;
}

static int
onnx_demo_load_text_lines(onnx_demo *pDemo, const char *pVocabPath)
{
    FILE *pFile;
    char szLine[256];
    size_t uCount = 0;
    char **ppItems = NULL;

    pFile = fopen(pVocabPath, "rb");
    if (pFile == NULL) {
        onnx_demo_set_error(pDemo, "failed to open vocab file");
        return -1;
    }

    while (fgets(szLine, sizeof(szLine), pFile) != NULL) {
        size_t uLength = strcspn(szLine, "\r\n");
        char *pItem;
        char **ppNewItems;

        szLine[uLength] = '\0';
        if (uLength == 0) {
            continue;
        }

        pItem = onnx_demo_strdup(szLine);
        if (pItem == NULL) {
            fclose(pFile);
            onnx_demo_set_error(pDemo, "failed to copy vocab token");
            return -1;
        }

        ppNewItems = (char **)realloc(ppItems, (uCount + 1) * sizeof(char *));
        if (ppNewItems == NULL) {
            free(pItem);
            fclose(pFile);
            onnx_demo_set_error(pDemo, "failed to grow vocab array");
            return -1;
        }

        ppItems = ppNewItems;
        ppItems[uCount++] = pItem;
    }

    fclose(pFile);

    pDemo->ppVocab = ppItems;
    pDemo->uVocabSize = uCount;
    if (uCount == 0) {
        onnx_demo_set_error(pDemo, "vocab file was empty");
        return -1;
    }

    return 0;
}

static int
onnx_demo_read_file_bytes(const char *pPath, unsigned char **ppData, size_t *puLength)
{
    FILE *pFile;
    long iLength;
    unsigned char *pData;

    pFile = fopen(pPath, "rb");
    if (pFile == NULL) {
        return -1;
    }

    if (fseek(pFile, 0, SEEK_END) != 0) {
        fclose(pFile);
        return -1;
    }

    iLength = ftell(pFile);
    if (iLength < 0) {
        fclose(pFile);
        return -1;
    }

    if (fseek(pFile, 0, SEEK_SET) != 0) {
        fclose(pFile);
        return -1;
    }

    pData = (unsigned char *)malloc((size_t)iLength);
    if (pData == NULL) {
        fclose(pFile);
        return -1;
    }

    if ((size_t)iLength != fread(pData, 1, (size_t)iLength, pFile)) {
        free(pData);
        fclose(pFile);
        return -1;
    }

    fclose(pFile);
    *ppData = pData;
    *puLength = (size_t)iLength;
    return 0;
}

static int
onnx_demo_infer_shapes(onnx_demo *pDemo)
{
    OrtStatus *pStatus;
    size_t uInputCount = 0;
    size_t uOutputCount = 0;
    OrtTypeInfo *pInputType = NULL;
    OrtTypeInfo *pOutputType = NULL;
    const OrtTensorTypeAndShapeInfo *pInputTensorInfo = NULL;
    const OrtTensorTypeAndShapeInfo *pOutputTensorInfo = NULL;
    size_t uInputDimCount = 0;
    size_t uOutputDimCount = 0;
    int64_t aiInputDims[4];
    int64_t aiOutputDims[4];

    pStatus = pDemo->pApi->SessionGetInputCount(pDemo->pSession, &uInputCount);
    if (pStatus != NULL) {
        return onnx_demo_set_ort_error(pDemo, pStatus, "SessionGetInputCount");
    }

    pStatus = pDemo->pApi->SessionGetOutputCount(pDemo->pSession, &uOutputCount);
    if (pStatus != NULL) {
        return onnx_demo_set_ort_error(pDemo, pStatus, "SessionGetOutputCount");
    }

    if (uInputCount != 1 || uOutputCount != 1) {
        onnx_demo_set_error(pDemo, "demo expects exactly one input and one output");
        return -1;
    }

    pStatus = pDemo->pApi->SessionGetInputTypeInfo(pDemo->pSession, 0, &pInputType);
    if (pStatus != NULL) {
        return onnx_demo_set_ort_error(pDemo, pStatus, "SessionGetInputTypeInfo");
    }

    pStatus = pDemo->pApi->CastTypeInfoToTensorInfo(pInputType, &pInputTensorInfo);
    if (pStatus != NULL) {
        pDemo->pApi->ReleaseTypeInfo(pInputType);
        return onnx_demo_set_ort_error(pDemo, pStatus, "CastTypeInfoToTensorInfo(input)");
    }

    pStatus = pDemo->pApi->GetDimensionsCount(pInputTensorInfo, &uInputDimCount);
    if (pStatus != NULL) {
        pDemo->pApi->ReleaseTypeInfo(pInputType);
        return onnx_demo_set_ort_error(pDemo, pStatus, "GetDimensionsCount(input)");
    }

    if (uInputDimCount > 4) {
        pDemo->pApi->ReleaseTypeInfo(pInputType);
        onnx_demo_set_error(pDemo, "demo input rank is unexpectedly large");
        return -1;
    }

    pStatus = pDemo->pApi->GetDimensions(pInputTensorInfo, aiInputDims, uInputDimCount);
    if (pStatus != NULL) {
        pDemo->pApi->ReleaseTypeInfo(pInputType);
        return onnx_demo_set_ort_error(pDemo, pStatus, "GetDimensions(input)");
    }

    pDemo->pApi->ReleaseTypeInfo(pInputType);

    if (uInputDimCount != 2 || aiInputDims[1] <= 0) {
        onnx_demo_set_error(pDemo, "demo input shape is not [1, vocab_size]");
        return -1;
    }

    if ((size_t)aiInputDims[1] != pDemo->uVocabSize) {
        onnx_demo_set_error(pDemo, "vocab size does not match model input width");
        return -1;
    }

    pStatus = pDemo->pApi->SessionGetOutputTypeInfo(pDemo->pSession, 0, &pOutputType);
    if (pStatus != NULL) {
        return onnx_demo_set_ort_error(pDemo, pStatus, "SessionGetOutputTypeInfo");
    }

    pStatus = pDemo->pApi->CastTypeInfoToTensorInfo(pOutputType, &pOutputTensorInfo);
    if (pStatus != NULL) {
        pDemo->pApi->ReleaseTypeInfo(pOutputType);
        return onnx_demo_set_ort_error(pDemo, pStatus, "CastTypeInfoToTensorInfo(output)");
    }

    pStatus = pDemo->pApi->GetDimensionsCount(pOutputTensorInfo, &uOutputDimCount);
    if (pStatus != NULL) {
        pDemo->pApi->ReleaseTypeInfo(pOutputType);
        return onnx_demo_set_ort_error(pDemo, pStatus, "GetDimensionsCount(output)");
    }

    if (uOutputDimCount > 4) {
        pDemo->pApi->ReleaseTypeInfo(pOutputType);
        onnx_demo_set_error(pDemo, "demo output rank is unexpectedly large");
        return -1;
    }

    pStatus = pDemo->pApi->GetDimensions(pOutputTensorInfo, aiOutputDims, uOutputDimCount);
    if (pStatus != NULL) {
        pDemo->pApi->ReleaseTypeInfo(pOutputType);
        return onnx_demo_set_ort_error(pDemo, pStatus, "GetDimensions(output)");
    }

    pDemo->pApi->ReleaseTypeInfo(pOutputType);

    if (uOutputDimCount != 2 || aiOutputDims[1] <= 0) {
        onnx_demo_set_error(pDemo, "demo output shape is not [1, embedding_dim]");
        return -1;
    }

    pDemo->uEmbeddingDim = (size_t)aiOutputDims[1];
    return 0;
}

int
onnx_demo_create(onnx_demo **ppDemo, const char *pDllPath, const char *pModelPath, const char *pVocabPath)
{
    onnx_demo *pDemo;
    onnx_demo_get_api_base_fn pGetApiBase = NULL;
    const OrtApiBase *pApiBase = NULL;
    FARPROC pFarProc = NULL;
    unsigned char *pModelBytes = NULL;
    size_t uModelBytes = 0;
    OrtStatus *pStatus;

    if (ppDemo == NULL || pDllPath == NULL || pModelPath == NULL || pVocabPath == NULL) {
        return -1;
    }

    *ppDemo = NULL;
    pDemo = (onnx_demo *)calloc(1, sizeof(*pDemo));
    if (pDemo == NULL) {
        return -1;
    }

    if (onnx_demo_load_text_lines(pDemo, pVocabPath) != 0) {
        onnx_demo_destroy(pDemo);
        return -1;
    }

    pDemo->hRuntime = LoadLibraryA(pDllPath);
    if (pDemo->hRuntime == NULL) {
        onnx_demo_set_error(pDemo, "failed to load onnxruntime.dll");
        onnx_demo_destroy(pDemo);
        return -1;
    }

    pFarProc = GetProcAddress(pDemo->hRuntime, "OrtGetApiBase");
    memcpy(&pGetApiBase, &pFarProc, sizeof(pGetApiBase));
    if (pGetApiBase == NULL) {
        onnx_demo_set_error(pDemo, "failed to resolve OrtGetApiBase");
        onnx_demo_destroy(pDemo);
        return -1;
    }

    pApiBase = pGetApiBase();
    if (pApiBase == NULL) {
        onnx_demo_set_error(pDemo, "OrtGetApiBase returned NULL");
        onnx_demo_destroy(pDemo);
        return -1;
    }

    pDemo->pApi = pApiBase->GetApi(ORT_API_VERSION);
    if (pDemo->pApi == NULL) {
        onnx_demo_set_error(pDemo, "failed to resolve requested ONNX Runtime API version");
        onnx_demo_destroy(pDemo);
        return -1;
    }

    pStatus = pDemo->pApi->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "onnxdemo", &pDemo->pEnv);
    if (pStatus != NULL) {
        onnx_demo_set_ort_error(pDemo, pStatus, "CreateEnv");
        onnx_demo_destroy(pDemo);
        return -1;
    }

    pStatus = pDemo->pApi->CreateSessionOptions(&pDemo->pSessionOptions);
    if (pStatus != NULL) {
        onnx_demo_set_ort_error(pDemo, pStatus, "CreateSessionOptions");
        onnx_demo_destroy(pDemo);
        return -1;
    }

    pStatus = pDemo->pApi->SetIntraOpNumThreads(pDemo->pSessionOptions, 1);
    if (pStatus != NULL) {
        onnx_demo_set_ort_error(pDemo, pStatus, "SetIntraOpNumThreads");
        onnx_demo_destroy(pDemo);
        return -1;
    }

    pStatus = pDemo->pApi->SetInterOpNumThreads(pDemo->pSessionOptions, 1);
    if (pStatus != NULL) {
        onnx_demo_set_ort_error(pDemo, pStatus, "SetInterOpNumThreads");
        onnx_demo_destroy(pDemo);
        return -1;
    }

    pStatus = pDemo->pApi->SetSessionGraphOptimizationLevel(pDemo->pSessionOptions, ORT_ENABLE_BASIC);
    if (pStatus != NULL) {
        onnx_demo_set_ort_error(pDemo, pStatus, "SetSessionGraphOptimizationLevel");
        onnx_demo_destroy(pDemo);
        return -1;
    }

    if (onnx_demo_read_file_bytes(pModelPath, &pModelBytes, &uModelBytes) != 0) {
        onnx_demo_set_error(pDemo, "failed to read model bytes");
        onnx_demo_destroy(pDemo);
        return -1;
    }

    pStatus = pDemo->pApi->CreateSessionFromArray(
        pDemo->pEnv,
        pModelBytes,
        uModelBytes,
        pDemo->pSessionOptions,
        &pDemo->pSession);
    free(pModelBytes);
    if (pStatus != NULL) {
        onnx_demo_set_ort_error(pDemo, pStatus, "CreateSessionFromArray");
        onnx_demo_destroy(pDemo);
        return -1;
    }

    pStatus = pDemo->pApi->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &pDemo->pMemoryInfo);
    if (pStatus != NULL) {
        onnx_demo_set_ort_error(pDemo, pStatus, "CreateCpuMemoryInfo");
        onnx_demo_destroy(pDemo);
        return -1;
    }

    if (onnx_demo_infer_shapes(pDemo) != 0) {
        onnx_demo_destroy(pDemo);
        return -1;
    }

    *ppDemo = pDemo;
    return 0;
}

void
onnx_demo_destroy(onnx_demo *pDemo)
{
    size_t i;

    if (pDemo == NULL) {
        return;
    }

    if (pDemo->pApi != NULL) {
        if (pDemo->pMemoryInfo != NULL) {
            pDemo->pApi->ReleaseMemoryInfo(pDemo->pMemoryInfo);
        }
        if (pDemo->pSession != NULL) {
            pDemo->pApi->ReleaseSession(pDemo->pSession);
        }
        if (pDemo->pSessionOptions != NULL) {
            pDemo->pApi->ReleaseSessionOptions(pDemo->pSessionOptions);
        }
        if (pDemo->pEnv != NULL) {
            pDemo->pApi->ReleaseEnv(pDemo->pEnv);
        }
    }

    if (pDemo->hRuntime != NULL) {
        FreeLibrary(pDemo->hRuntime);
    }

    for (i = 0; i < pDemo->uVocabSize; ++i) {
        free(pDemo->ppVocab[i]);
    }
    free(pDemo->ppVocab);
    free(pDemo);
}

const char *
onnx_demo_last_error(const onnx_demo *pDemo)
{
    if (pDemo == NULL) {
        return "onnx_demo is NULL";
    }
    return pDemo->szLastError;
}

size_t
onnx_demo_embedding_dim(const onnx_demo *pDemo)
{
    return pDemo != NULL ? pDemo->uEmbeddingDim : 0U;
}

int
onnx_demo_embed_text(
    onnx_demo *pDemo,
    const char *pText,
    float **ppVector,
    size_t *puDim,
    size_t *puMatchedTokens,
    size_t *puTotalTokens)
{
    float *pBow = NULL;
    float *pCopy = NULL;
    OrtStatus *pStatus;
    OrtValue *pInputValue = NULL;
    OrtValue *pOutputValue = NULL;
    OrtTensorTypeAndShapeInfo *pShapeInfo = NULL;
    int64_t aiInputShape[2];
    size_t uDimCount = 0;
    int64_t aiOutputDims[4];
    void *pOutputData = NULL;
    size_t uOutputElements;
    const char *apInputNames[1] = {"bow"};
    const char *apOutputNames[1] = {"embedding"};

    if (pDemo == NULL || pText == NULL || ppVector == NULL || puDim == NULL ||
        puMatchedTokens == NULL || puTotalTokens == NULL) {
        return -1;
    }

    *ppVector = NULL;
    *puDim = 0;
    *puMatchedTokens = 0;
    *puTotalTokens = 0;

    pBow = (float *)calloc(pDemo->uVocabSize, sizeof(float));
    if (pBow == NULL) {
        onnx_demo_set_error(pDemo, "failed to allocate bag-of-words buffer");
        return -1;
    }

    if (onnx_demo_fill_bow(pDemo, pText, pBow, puMatchedTokens, puTotalTokens) != 0) {
        free(pBow);
        onnx_demo_set_error(pDemo, "failed to tokenize text");
        return -1;
    }

    aiInputShape[0] = 1;
    aiInputShape[1] = (int64_t)pDemo->uVocabSize;
    pStatus = pDemo->pApi->CreateTensorWithDataAsOrtValue(
        pDemo->pMemoryInfo,
        pBow,
        pDemo->uVocabSize * sizeof(float),
        aiInputShape,
        2,
        ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
        &pInputValue);
    if (pStatus != NULL) {
        free(pBow);
        return onnx_demo_set_ort_error(pDemo, pStatus, "CreateTensorWithDataAsOrtValue");
    }

    pStatus = pDemo->pApi->Run(
        pDemo->pSession,
        NULL,
        apInputNames,
        (const OrtValue *const *)&pInputValue,
        1,
        apOutputNames,
        1,
        &pOutputValue);
    free(pBow);
    if (pStatus != NULL) {
        pDemo->pApi->ReleaseValue(pInputValue);
        return onnx_demo_set_ort_error(pDemo, pStatus, "Run");
    }

    pStatus = pDemo->pApi->GetTensorTypeAndShape(pOutputValue, &pShapeInfo);
    if (pStatus != NULL) {
        pDemo->pApi->ReleaseValue(pInputValue);
        pDemo->pApi->ReleaseValue(pOutputValue);
        return onnx_demo_set_ort_error(pDemo, pStatus, "GetTensorTypeAndShape");
    }

    pStatus = pDemo->pApi->GetDimensionsCount(pShapeInfo, &uDimCount);
    if (pStatus != NULL) {
        pDemo->pApi->ReleaseTensorTypeAndShapeInfo(pShapeInfo);
        pDemo->pApi->ReleaseValue(pInputValue);
        pDemo->pApi->ReleaseValue(pOutputValue);
        return onnx_demo_set_ort_error(pDemo, pStatus, "GetDimensionsCount(output)");
    }

    if (uDimCount > 4) {
        pDemo->pApi->ReleaseTensorTypeAndShapeInfo(pShapeInfo);
        pDemo->pApi->ReleaseValue(pInputValue);
        pDemo->pApi->ReleaseValue(pOutputValue);
        onnx_demo_set_error(pDemo, "output rank is unexpectedly large");
        return -1;
    }

    pStatus = pDemo->pApi->GetDimensions(pShapeInfo, aiOutputDims, uDimCount);
    if (pStatus != NULL) {
        pDemo->pApi->ReleaseTensorTypeAndShapeInfo(pShapeInfo);
        pDemo->pApi->ReleaseValue(pInputValue);
        pDemo->pApi->ReleaseValue(pOutputValue);
        return onnx_demo_set_ort_error(pDemo, pStatus, "GetDimensions(output)");
    }

    pDemo->pApi->ReleaseTensorTypeAndShapeInfo(pShapeInfo);
    if (uDimCount != 2 || aiOutputDims[1] <= 0) {
        pDemo->pApi->ReleaseValue(pInputValue);
        pDemo->pApi->ReleaseValue(pOutputValue);
        onnx_demo_set_error(pDemo, "unexpected output tensor shape");
        return -1;
    }

    uOutputElements = (size_t)aiOutputDims[1];
    pStatus = pDemo->pApi->GetTensorMutableData(pOutputValue, &pOutputData);
    if (pStatus != NULL) {
        pDemo->pApi->ReleaseValue(pInputValue);
        pDemo->pApi->ReleaseValue(pOutputValue);
        return onnx_demo_set_ort_error(pDemo, pStatus, "GetTensorMutableData");
    }

    pCopy = (float *)malloc(uOutputElements * sizeof(float));
    if (pCopy == NULL) {
        pDemo->pApi->ReleaseValue(pInputValue);
        pDemo->pApi->ReleaseValue(pOutputValue);
        onnx_demo_set_error(pDemo, "failed to copy embedding vector");
        return -1;
    }

    memcpy(pCopy, pOutputData, uOutputElements * sizeof(float));

    pDemo->pApi->ReleaseValue(pInputValue);
    pDemo->pApi->ReleaseValue(pOutputValue);

    *ppVector = pCopy;
    *puDim = uOutputElements;
    return 0;
}

void
onnx_demo_free_vector(float *pVector)
{
    free(pVector);
}

float
onnx_demo_cosine_similarity(const float *pLeft, const float *pRight, size_t uDim)
{
    size_t i;
    double fDot = 0.0;
    double fLeftNorm = 0.0;
    double fRightNorm = 0.0;

    if (pLeft == NULL || pRight == NULL || uDim == 0) {
        return 0.0f;
    }

    for (i = 0; i < uDim; ++i) {
        fDot += (double)pLeft[i] * (double)pRight[i];
        fLeftNorm += (double)pLeft[i] * (double)pLeft[i];
        fRightNorm += (double)pRight[i] * (double)pRight[i];
    }

    if (fLeftNorm <= 0.0 || fRightNorm <= 0.0) {
        return 0.0f;
    }

    return (float)(fDot / (sqrt(fLeftNorm) * sqrt(fRightNorm)));
}
