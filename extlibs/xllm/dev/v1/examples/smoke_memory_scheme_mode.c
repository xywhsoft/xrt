#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xllm-memory.h"

typedef struct {
    uint32 uDimensions;
} smoke_embedder_ctx;

static int require_true(int condition, const char *message)
{
    if ( !condition ) {
        fprintf(stderr, "%s\n", message);
        return 1;
    }
    return 0;
}

static int smoke_embed_text(
    void *pCtx,
    xllm_memory_embed_task eTask,
    const char *sText,
    xllm_memory_embedding *pEmbedding,
    xllm_error *pError
)
{
    smoke_embedder_ctx *pState = (smoke_embedder_ctx *)pCtx;
    float *pfValues;
    size_t i;
    uint32 j;

    (void)eTask;
    if ( !pState || !pEmbedding ) {
        if ( pError ) {
            pError->eCode = XLLM_ERROR_INVALID_REQUEST;
        }
        return XRT_NET_ERROR;
    }

    pfValues = (float *)calloc((size_t)pState->uDimensions, sizeof(*pfValues));
    if ( !pfValues ) {
        if ( pError ) {
            pError->eCode = XLLM_ERROR_INTERNAL;
        }
        return XRT_NET_ERROR;
    }

    if ( sText ) {
        for ( i = 0u; sText[i] != '\0'; ++i ) {
            unsigned char ch = (unsigned char)sText[i];
            uint32 uBucket = (uint32)((i + (size_t)ch) % (size_t)pState->uDimensions);
            pfValues[uBucket] += 1.0f;
            for ( j = 0u; j < pState->uDimensions; ++j ) {
                pfValues[j] += ((float)((ch + (unsigned char)(j * 13u)) % 17u)) * 0.0005f;
            }
        }
    }

    pEmbedding->pfValues = pfValues;
    pEmbedding->uValueCount = pState->uDimensions;
    return XRT_NET_OK;
}

static void smoke_reset_embedding(void *pCtx, xllm_memory_embedding *pEmbedding)
{
    (void)pCtx;
    if ( !pEmbedding ) {
        return;
    }
    if ( pEmbedding->pfValues ) {
        free(pEmbedding->pfValues);
    }
    pEmbedding->pfValues = NULL;
    pEmbedding->uValueCount = 0u;
}

static int smoke_clone_ctx(void *pCtx, void **ppClonedCtx, xllm_error *pError)
{
    smoke_embedder_ctx *pClone;

    if ( !pCtx || !ppClonedCtx ) {
        if ( pError ) {
            pError->eCode = XLLM_ERROR_INVALID_REQUEST;
        }
        return XRT_NET_ERROR;
    }

    pClone = (smoke_embedder_ctx *)calloc(1u, sizeof(*pClone));
    if ( !pClone ) {
        if ( pError ) {
            pError->eCode = XLLM_ERROR_INTERNAL;
        }
        return XRT_NET_ERROR;
    }

    *pClone = *(const smoke_embedder_ctx *)pCtx;
    *ppClonedCtx = pClone;
    return XRT_NET_OK;
}

static void smoke_dispose_ctx(void *pCtx)
{
    free(pCtx);
}

static int make_stub_embedder(
    xllm_memory_embedder *pEmbedder,
    xllm_error *pError
)
{
    smoke_embedder_ctx *pCtx;

    if ( !pEmbedder ) {
        if ( pError ) {
            pError->eCode = XLLM_ERROR_INVALID_REQUEST;
        }
        return XRT_NET_ERROR;
    }

    pCtx = (smoke_embedder_ctx *)calloc(1u, sizeof(*pCtx));
    if ( !pCtx ) {
        if ( pError ) {
            pError->eCode = XLLM_ERROR_INTERNAL;
        }
        return XRT_NET_ERROR;
    }

    pCtx->uDimensions = 16u;
    xllm_memory_embedder_init(pEmbedder);
    pEmbedder->pfnEmbedText = smoke_embed_text;
    pEmbedder->pfnResetEmbedding = smoke_reset_embedding;
    pEmbedder->pfnCloneCtx = smoke_clone_ctx;
    pEmbedder->pfnDisposeCtx = smoke_dispose_ctx;
    pEmbedder->pCtx = pCtx;
    return XRT_NET_OK;
}

static int expect_scheme(
    xllm_runtime *pRuntime,
    const xllm_memory_options *pOptions,
    xllm_memory_scheme eExpectedScheme,
    const char *sExpectedProfile,
    const char *sLabel
)
{
    xllm_memory *pMemory = NULL;
    int iStatus;
    int iRc = 1;

    iStatus = xllm_memory_create(pRuntime, pOptions, &pMemory);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "%s create failed: %d\n", sLabel, iStatus);
        return 1;
    }

    if ( require_true(xllm_memory_get_scheme(pMemory) == eExpectedScheme, sLabel) != 0 ) {
        goto cleanup;
    }
    if ( require_true(
            xllm_memory_get_profile_id(pMemory) != NULL &&
            strcmp(xllm_memory_get_profile_id(pMemory), sExpectedProfile) == 0,
            "resolved memory profile id mismatch"
         ) != 0 ) {
        goto cleanup;
    }

    iRc = 0;

cleanup:
    xllm_memory_destroy(pMemory);
    return iRc;
}

static int expect_create_failure(
    xllm_runtime *pRuntime,
    const xllm_memory_options *pOptions,
    const char *sLabel
)
{
    xllm_memory *pMemory = NULL;
    int iStatus = xllm_memory_create(pRuntime, pOptions, &pMemory);

    if ( iStatus == XRT_NET_OK ) {
        fprintf(stderr, "%s unexpectedly succeeded\n", sLabel);
        xllm_memory_destroy(pMemory);
        return 1;
    }
    return 0;
}

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_error tError;
    int iStatus;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_memory_options_init(&tMemoryOptions);
    xllm_error_init(&tError);

    iStatus = xllm_runtime_create(&tRuntimeOptions, &pRuntime);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "runtime create failed: %d\n", iStatus);
        xllm_error_free(&tError);
        return 1;
    }

#if XLLM_MEMORY_SCHEME_MODE == XLLM_MEMORY_SCHEME_MODE_ALL
    tMemoryOptions.sNamespace = "scheme-mode-auto-sparse";
    if ( expect_scheme(
            pRuntime,
            &tMemoryOptions,
            XLLM_MEMORY_SCHEME_BUILTIN_SPARSE,
            "builtin_sparse.v1",
            "auto sparse"
         ) != 0 ) {
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 2;
    }

    xllm_memory_options_init(&tMemoryOptions);
    tMemoryOptions.sNamespace = "scheme-mode-auto-custom";
    iStatus = make_stub_embedder(&tMemoryOptions.tEmbedder, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "stub embedder create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 3;
    }
    if ( expect_scheme(
            pRuntime,
            &tMemoryOptions,
            XLLM_MEMORY_SCHEME_CUSTOM,
            "custom.v1",
            "auto custom"
         ) != 0 ) {
        xllm_memory_embedder_reset(&tMemoryOptions.tEmbedder);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 4;
    }
    xllm_memory_embedder_reset(&tMemoryOptions.tEmbedder);

    xllm_memory_options_init(&tMemoryOptions);
    tMemoryOptions.sNamespace = "scheme-mode-explicit-sparse";
    tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
    tMemoryOptions.sMemoryProfileId = "builtin_sparse.v1";
    if ( expect_scheme(
            pRuntime,
            &tMemoryOptions,
            XLLM_MEMORY_SCHEME_BUILTIN_SPARSE,
            "builtin_sparse.v1",
            "explicit sparse"
         ) != 0 ) {
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 5;
    }

    xllm_memory_options_init(&tMemoryOptions);
    tMemoryOptions.sNamespace = "scheme-mode-explicit-custom";
    tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_CUSTOM;
    tMemoryOptions.sMemoryProfileId = "custom.v1";
    iStatus = make_stub_embedder(&tMemoryOptions.tEmbedder, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "stub embedder create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 6;
    }
    if ( expect_scheme(
            pRuntime,
            &tMemoryOptions,
            XLLM_MEMORY_SCHEME_CUSTOM,
            "custom.v1",
            "explicit custom"
         ) != 0 ) {
        xllm_memory_embedder_reset(&tMemoryOptions.tEmbedder);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 7;
    }
    xllm_memory_embedder_reset(&tMemoryOptions.tEmbedder);

    xllm_memory_options_init(&tMemoryOptions);
    tMemoryOptions.sNamespace = "scheme-mode-mismatch";
    tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
    tMemoryOptions.sMemoryProfileId = "custom.v1";
    if ( expect_create_failure(pRuntime, &tMemoryOptions, "profile mismatch") != 0 ) {
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 8;
    }

#if XLLM__MEMORY_HAS_SCHEME_ONNX_E5
    {
        xllm_memory_builtin_embedder_options tProbeOptions;
        xllm_memory_builtin_embedder_probe tProbe;

        xllm_memory_builtin_embedder_options_init(&tProbeOptions);
        memset(&tProbe, 0, sizeof(tProbe));
        tProbeOptions.eKind = XLLM_MEMORY_BUILTIN_EMBEDDER_MULTILINGUAL_E5_SMALL_ONNX;
        iStatus = xllm_memory_probe_builtin_embedder(&tProbeOptions, &tProbe, &tError);
        if ( iStatus != XRT_NET_OK ) {
            fprintf(stderr, "onnx probe failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
            xllm_runtime_destroy(pRuntime);
            xllm_error_free(&tError);
            return 9;
        }
        if ( tProbe.bReady ) {
            xllm_memory_options_init(&tMemoryOptions);
            tMemoryOptions.sNamespace = "scheme-mode-explicit-onnx";
            tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_ONNX_E5;
            tMemoryOptions.sMemoryProfileId = "onnx_e5.v1";
            if ( expect_scheme(
                    pRuntime,
                    &tMemoryOptions,
                    XLLM_MEMORY_SCHEME_ONNX_E5,
                    "onnx_e5.v1",
                    "explicit onnx"
                 ) != 0 ) {
                xllm_memory_builtin_embedder_probe_reset(&tProbe);
                xllm_runtime_destroy(pRuntime);
                xllm_error_free(&tError);
                return 10;
            }
        } else {
            printf("smoke_memory_scheme_mode note: onnx_e5 assets unavailable, skip explicit onnx create\n");
        }
        xllm_memory_builtin_embedder_probe_reset(&tProbe);
    }
#endif
#elif XLLM_MEMORY_SCHEME_MODE == XLLM_MEMORY_SCHEME_MODE_CUSTOM
    tMemoryOptions.sNamespace = "scheme-mode-auto-custom-only";
    if ( expect_scheme(
            pRuntime,
            &tMemoryOptions,
            XLLM_MEMORY_SCHEME_CUSTOM,
            "custom.v1",
            "auto custom only"
         ) != 0 ) {
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 11;
    }

    xllm_memory_options_init(&tMemoryOptions);
    tMemoryOptions.sNamespace = "scheme-mode-explicit-custom-only";
    tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_CUSTOM;
    tMemoryOptions.sMemoryProfileId = "custom.v1";
    iStatus = make_stub_embedder(&tMemoryOptions.tEmbedder, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "stub embedder create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 12;
    }
    if ( expect_scheme(
            pRuntime,
            &tMemoryOptions,
            XLLM_MEMORY_SCHEME_CUSTOM,
            "custom.v1",
            "explicit custom only"
         ) != 0 ) {
        xllm_memory_embedder_reset(&tMemoryOptions.tEmbedder);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 13;
    }
    xllm_memory_embedder_reset(&tMemoryOptions.tEmbedder);

    xllm_memory_options_init(&tMemoryOptions);
    tMemoryOptions.sNamespace = "scheme-mode-unavailable-sparse";
    tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
    tMemoryOptions.sMemoryProfileId = "builtin_sparse.v1";
    if ( expect_create_failure(pRuntime, &tMemoryOptions, "custom-only sparse request") != 0 ) {
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 14;
    }

    xllm_memory_options_init(&tMemoryOptions);
    tMemoryOptions.sNamespace = "scheme-mode-unavailable-onnx";
    tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_ONNX_E5;
    tMemoryOptions.sMemoryProfileId = "onnx_e5.v1";
    if ( expect_create_failure(pRuntime, &tMemoryOptions, "custom-only onnx request") != 0 ) {
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 15;
    }
#else
    printf("smoke_memory_scheme_mode skipped: build mode %d not covered by this smoke\n", XLLM_MEMORY_SCHEME_MODE);
#endif

    printf("smoke_memory_scheme_mode ok\n");

    xllm_runtime_destroy(pRuntime);
    xllm_error_free(&tError);
    return 0;
}
