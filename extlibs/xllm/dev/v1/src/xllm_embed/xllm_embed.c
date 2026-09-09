static void xllm__memory_embedding_reset_with_embedder(
    const xllm_memory_embedder *pEmbedder,
    xllm_memory_embedding *pEmbedding
)
{
    if ( !pEmbedding ) {
        return;
    }

    if ( pEmbedding->pfValues ) {
        if ( pEmbedder && pEmbedder->pfnResetEmbedding ) {
            pEmbedder->pfnResetEmbedding(pEmbedder->pCtx, pEmbedding);
        } else {
            xrtFree(pEmbedding->pfValues);
            pEmbedding->pfValues = NULL;
            pEmbedding->uValueCount = 0u;
        }
    }
    memset(pEmbedding, 0, sizeof(*pEmbedding));
}

static int xllm__memory_embed_text(
    const xllm_memory_embedder *pEmbedder,
    xllm_memory_embed_task eTask,
    const char *sText,
    xllm_memory_embedding *pEmbedding,
    xllm_error *pError
)
{
    if ( !pEmbedding ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "embedding output is required");
        return XRT_NET_ERROR;
    }

    memset(pEmbedding, 0, sizeof(*pEmbedding));
    if ( !pEmbedder || !pEmbedder->pfnEmbedText ) {
        return XRT_NET_OK;
    }
    if ( !sText || !sText[0] ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "embedding text is required");
        return XRT_NET_ERROR;
    }
    if ( pEmbedder->pfnEmbedText(pEmbedder->pCtx, eTask, sText, pEmbedding, pError) != XRT_NET_OK ) {
        xllm__memory_embedding_reset_with_embedder(pEmbedder, pEmbedding);
        return XRT_NET_ERROR;
    }
    if ( !pEmbedding->pfValues || pEmbedding->uValueCount == 0u ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "embedder returned an empty vector");
        xllm__memory_embedding_reset_with_embedder(pEmbedder, pEmbedding);
        return XRT_NET_ERROR;
    }
    return XRT_NET_OK;
}

static double xllm__memory_embedding_cosine_score(
    const float *pfLeft,
    const float *pfRight,
    uint32 uValueCount
)
{
    uint32 i;
    double fDot = 0.0;
    double fLeftNorm = 0.0;
    double fRightNorm = 0.0;
    double fCosine;

    if ( !pfLeft || !pfRight || uValueCount == 0u ) {
        return 0.0;
    }

    for ( i = 0u; i < uValueCount; ++i ) {
        double fLeft = (double)pfLeft[i];
        double fRight = (double)pfRight[i];
        fDot += fLeft * fRight;
        fLeftNorm += fLeft * fLeft;
        fRightNorm += fRight * fRight;
    }
    if ( fLeftNorm <= 0.0 || fRightNorm <= 0.0 ) {
        return 0.0;
    }

    fCosine = fDot / (sqrt(fLeftNorm) * sqrt(fRightNorm));
    if ( fCosine < -1.0 ) {
        fCosine = -1.0;
    } else if ( fCosine > 1.0 ) {
        fCosine = 1.0;
    }
    return (fCosine + 1.0) * 0.5;
}
