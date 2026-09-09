#include "../word2vec_demo.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct w2vdemo_model {
    long long iVocabSize;
    int iDim;
    char **ppWords;
    float *pVectors;
};

static char g_sLastError[256];

static void w2vdemo__set_error(const char *sMessage)
{
    if ( !sMessage ) {
        sMessage = "unknown error";
    }

    strncpy(g_sLastError, sMessage, sizeof(g_sLastError) - 1u);
    g_sLastError[sizeof(g_sLastError) - 1u] = '\0';
}

static void w2vdemo__set_errorf(const char *sPrefix, const char *sPath)
{
    size_t iLen;

    if ( !sPrefix ) {
        sPrefix = "error";
    }
    if ( !sPath ) {
        sPath = "";
    }

    iLen = strlen(sPrefix);
    if ( iLen >= sizeof(g_sLastError) - 1u ) {
        iLen = sizeof(g_sLastError) - 1u;
    }
    memcpy(g_sLastError, sPrefix, iLen);
    if ( iLen < sizeof(g_sLastError) - 1u ) {
        g_sLastError[iLen++] = ':';
    }
    if ( iLen < sizeof(g_sLastError) - 1u ) {
        g_sLastError[iLen++] = ' ';
    }
    strncpy(g_sLastError + iLen, sPath, sizeof(g_sLastError) - iLen - 1u);
    g_sLastError[sizeof(g_sLastError) - 1u] = '\0';
}

static char *w2vdemo__dup_token(const char *sStart, size_t iLen)
{
    char *sToken;

    sToken = (char *)calloc(iLen + 1u, sizeof(char));
    if ( !sToken ) {
        return NULL;
    }

    memcpy(sToken, sStart, iLen);
    sToken[iLen] = '\0';
    return sToken;
}

static int w2vdemo__read_word(FILE *pFile, char *sWord, size_t iWordCap)
{
    int iCh;
    size_t iLen = 0u;

    if ( !pFile || !sWord || iWordCap == 0u ) {
        return 0;
    }

    do {
        iCh = fgetc(pFile);
        if ( iCh == EOF ) {
            return 0;
        }
    } while ( iCh == ' ' || iCh == '\n' || iCh == '\r' || iCh == '\t' );

    while ( iCh != EOF && iCh != ' ' && iCh != '\n' && iCh != '\r' && iCh != '\t' ) {
        if ( iLen + 1u < iWordCap ) {
            sWord[iLen++] = (char)iCh;
        }
        iCh = fgetc(pFile);
    }

    sWord[iLen] = '\0';
    return 1;
}

static int w2vdemo__lookup_word(const w2vdemo_model *pModel, const char *sWord)
{
    long long i;

    if ( !pModel || !sWord || !sWord[0] ) {
        return -1;
    }

    for ( i = 0; i < pModel->iVocabSize; ++i ) {
        if ( strcmp(pModel->ppWords[i], sWord) == 0 ) {
            return (int)i;
        }
    }

    return -1;
}

int w2vdemo_model_load(const char *sModelPath, w2vdemo_model **ppModel)
{
    FILE *pFile;
    long long i;
    w2vdemo_model *pModel;
    char sWord[256];

    if ( !ppModel ) {
        w2vdemo__set_error("output model pointer is null");
        return -1;
    }
    *ppModel = NULL;

    if ( !sModelPath || !sModelPath[0] ) {
        w2vdemo__set_error("model path is empty");
        return -1;
    }

    pFile = fopen(sModelPath, "rb");
    if ( !pFile ) {
        w2vdemo__set_errorf("failed to open model", sModelPath);
        return -1;
    }

    pModel = (w2vdemo_model *)calloc(1u, sizeof(*pModel));
    if ( !pModel ) {
        fclose(pFile);
        w2vdemo__set_error("out of memory allocating model");
        return -1;
    }

    if ( fscanf(pFile, "%lld %d", &pModel->iVocabSize, &pModel->iDim) != 2 ) {
        fclose(pFile);
        free(pModel);
        w2vdemo__set_error("failed to parse model header");
        return -1;
    }

    if ( pModel->iVocabSize <= 0 || pModel->iDim <= 0 ) {
        fclose(pFile);
        free(pModel);
        w2vdemo__set_error("invalid model header");
        return -1;
    }

    pModel->ppWords = (char **)calloc((size_t)pModel->iVocabSize, sizeof(char *));
    pModel->pVectors = (float *)calloc((size_t)pModel->iVocabSize * (size_t)pModel->iDim, sizeof(float));
    if ( !pModel->ppWords || !pModel->pVectors ) {
        fclose(pFile);
        w2vdemo_model_destroy(pModel);
        w2vdemo__set_error("out of memory loading vectors");
        return -1;
    }

    for ( i = 0; i < pModel->iVocabSize; ++i ) {
        if ( !w2vdemo__read_word(pFile, sWord, sizeof(sWord)) ) {
            fclose(pFile);
            w2vdemo_model_destroy(pModel);
            w2vdemo__set_error("unexpected EOF while reading vocabulary");
            return -1;
        }

        pModel->ppWords[i] = w2vdemo__dup_token(sWord, strlen(sWord));
        if ( !pModel->ppWords[i] ) {
            fclose(pFile);
            w2vdemo_model_destroy(pModel);
            w2vdemo__set_error("out of memory copying word");
            return -1;
        }

        if ( fread(pModel->pVectors + ((size_t)i * (size_t)pModel->iDim), sizeof(float), (size_t)pModel->iDim, pFile) != (size_t)pModel->iDim ) {
            fclose(pFile);
            w2vdemo_model_destroy(pModel);
            w2vdemo__set_error("unexpected EOF while reading vectors");
            return -1;
        }

        (void)fgetc(pFile);
    }

    fclose(pFile);
    g_sLastError[0] = '\0';
    *ppModel = pModel;
    return 0;
}

void w2vdemo_model_destroy(w2vdemo_model *pModel)
{
    long long i;

    if ( !pModel ) {
        return;
    }

    if ( pModel->ppWords ) {
        for ( i = 0; i < pModel->iVocabSize; ++i ) {
            free(pModel->ppWords[i]);
        }
    }

    free(pModel->ppWords);
    free(pModel->pVectors);
    free(pModel);
}

int w2vdemo_model_dimension(const w2vdemo_model *pModel)
{
    return pModel ? pModel->iDim : -1;
}

long long w2vdemo_model_vocab_size(const w2vdemo_model *pModel)
{
    return pModel ? pModel->iVocabSize : -1;
}

int w2vdemo_model_embed_text(
    const w2vdemo_model *pModel,
    const char *sText,
    float *pOutVector,
    size_t iOutVectorCount,
    size_t *piTokenCount,
    size_t *piMatchedTokenCount
)
{
    const char *pCur;
    size_t iMatched = 0u;
    size_t iTokens = 0u;
    size_t i;

    if ( piTokenCount ) {
        *piTokenCount = 0u;
    }
    if ( piMatchedTokenCount ) {
        *piMatchedTokenCount = 0u;
    }

    if ( !pModel || !sText || !pOutVector ) {
        w2vdemo__set_error("invalid embed arguments");
        return -1;
    }
    if ( iOutVectorCount != (size_t)pModel->iDim ) {
        w2vdemo__set_error("output vector size does not match model dimension");
        return -1;
    }

    for ( i = 0u; i < iOutVectorCount; ++i ) {
        pOutVector[i] = 0.0f;
    }

    pCur = sText;
    while ( *pCur ) {
        const char *pStart;
        size_t iLen;
        char *sToken;
        int iWordIndex;

        while ( *pCur && isspace((unsigned char)*pCur) ) {
            ++pCur;
        }
        if ( !*pCur ) {
            break;
        }

        pStart = pCur;
        while ( *pCur && !isspace((unsigned char)*pCur) ) {
            ++pCur;
        }

        iLen = (size_t)(pCur - pStart);
        if ( iLen == 0u ) {
            continue;
        }

        ++iTokens;
        sToken = w2vdemo__dup_token(pStart, iLen);
        if ( !sToken ) {
            w2vdemo__set_error("out of memory tokenizing text");
            return -1;
        }

        iWordIndex = w2vdemo__lookup_word(pModel, sToken);
        free(sToken);
        if ( iWordIndex < 0 ) {
            continue;
        }

        for ( i = 0u; i < iOutVectorCount; ++i ) {
            pOutVector[i] += pModel->pVectors[(size_t)iWordIndex * iOutVectorCount + i];
        }
        ++iMatched;
    }

    if ( iMatched == 0u ) {
        w2vdemo__set_error("no known tokens found in text");
        if ( piTokenCount ) {
            *piTokenCount = iTokens;
        }
        return -1;
    }

    for ( i = 0u; i < iOutVectorCount; ++i ) {
        pOutVector[i] /= (float)iMatched;
    }

    if ( piTokenCount ) {
        *piTokenCount = iTokens;
    }
    if ( piMatchedTokenCount ) {
        *piMatchedTokenCount = iMatched;
    }

    g_sLastError[0] = '\0';
    return 0;
}

float w2vdemo_cosine_similarity(
    const float *pLeft,
    const float *pRight,
    size_t iCount
)
{
    double fDot = 0.0;
    double fLeftNorm = 0.0;
    double fRightNorm = 0.0;
    size_t i;

    if ( !pLeft || !pRight || iCount == 0u ) {
        return 0.0f;
    }

    for ( i = 0u; i < iCount; ++i ) {
        double fL = (double)pLeft[i];
        double fR = (double)pRight[i];
        fDot += fL * fR;
        fLeftNorm += fL * fL;
        fRightNorm += fR * fR;
    }

    if ( fLeftNorm <= 0.0 || fRightNorm <= 0.0 ) {
        return 0.0f;
    }

    return (float)(fDot / (sqrt(fLeftNorm) * sqrt(fRightNorm)));
}

int w2vdemo_model_compare_texts(
    const w2vdemo_model *pModel,
    const char *sLeftText,
    const char *sRightText,
    float *pfSimilarity,
    size_t *piLeftMatchedTokenCount,
    size_t *piRightMatchedTokenCount
)
{
    float *pLeft;
    float *pRight;
    size_t iDim;
    size_t iLeftMatched = 0u;
    size_t iRightMatched = 0u;
    int iResult = -1;

    if ( piLeftMatchedTokenCount ) {
        *piLeftMatchedTokenCount = 0u;
    }
    if ( piRightMatchedTokenCount ) {
        *piRightMatchedTokenCount = 0u;
    }

    if ( !pModel || !pfSimilarity ) {
        w2vdemo__set_error("invalid compare arguments");
        return -1;
    }

    iDim = (size_t)pModel->iDim;
    pLeft = (float *)calloc(iDim, sizeof(float));
    pRight = (float *)calloc(iDim, sizeof(float));
    if ( !pLeft || !pRight ) {
        free(pLeft);
        free(pRight);
        w2vdemo__set_error("out of memory comparing texts");
        return -1;
    }

    if ( w2vdemo_model_embed_text(pModel, sLeftText, pLeft, iDim, NULL, &iLeftMatched) != 0 ) {
        goto cleanup;
    }
    if ( w2vdemo_model_embed_text(pModel, sRightText, pRight, iDim, NULL, &iRightMatched) != 0 ) {
        goto cleanup;
    }

    *pfSimilarity = w2vdemo_cosine_similarity(pLeft, pRight, iDim);
    if ( piLeftMatchedTokenCount ) {
        *piLeftMatchedTokenCount = iLeftMatched;
    }
    if ( piRightMatchedTokenCount ) {
        *piRightMatchedTokenCount = iRightMatched;
    }
    g_sLastError[0] = '\0';
    iResult = 0;

cleanup:
    free(pLeft);
    free(pRight);
    return iResult;
}

const char *w2vdemo_last_error(void)
{
    return g_sLastError;
}
