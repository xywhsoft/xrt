#include <stdio.h>

#define XLLM__MEMORY_E5_RUNTIME_ENV "XLLM_MEMORY_E5_RUNTIME_DLL"
#define XLLM__MEMORY_E5_MODEL_ENV "XLLM_MEMORY_E5_MODEL_PATH"
#define XLLM__MEMORY_E5_TOKENIZER_ENV "XLLM_MEMORY_E5_TOKENIZER_PATH"

#include "xllm_embed_e5_onnx.h"

typedef struct {
    const char *sModelId;
    const char *sRepoId;
    const char *sQueryPrefix;
    const char *sDocumentPrefix;
    const char *sPoolingMode;
    bool bNormalize;
    uint32 uMaxInputTokens;
    uint32 uEmbeddingDimensions;
} xllm__memory_builtin_model_profile;

typedef struct {
    uint32 uDimensions;
} xllm__memory_builtin_hash_ctx;

static const xllm__memory_builtin_model_profile XLLM__MEMORY_BUILTIN_E5_PROFILE = {
    "multilingual-e5-small",
    "WiseIntelligence/multilingual-e5-small-Optimum-ONNX-Quantized-AVX2",
    "query: ",
    "passage: ",
    "mean",
    true,
    512u,
    384u
};

static bool xllm__memory_builtin_path_exists(const char *sPath)
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

static char *xllm__memory_builtin_dup_existing_path(const char *sPath)
{
    if ( !xllm__memory_builtin_path_exists(sPath) ) {
        return NULL;
    }
    return xllm__dup_cstr(sPath);
}

static bool xllm__memory_builtin_dir_exists(const char *sPath)
{
    return sPath && sPath[0] && xrtDirExists((str)sPath);
}

static char *xllm__memory_builtin_join_and_dup_existing_path(
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
    return xllm__memory_builtin_dup_existing_path(sBuffer);
}

static char *xllm__memory_builtin_resolve_repo_file(
    const char *sMaybePath,
    const char *const *pLeafNames,
    size_t iLeafCount
)
{
    size_t i;

    if ( !sMaybePath || !sMaybePath[0] ) {
        return NULL;
    }
    if ( !xllm__memory_builtin_dir_exists(sMaybePath) ) {
        return xllm__memory_builtin_dup_existing_path(sMaybePath);
    }

    for ( i = 0u; i < iLeafCount; ++i ) {
        char *sResolved = xllm__memory_builtin_join_and_dup_existing_path(sMaybePath, pLeafNames[i]);
        if ( sResolved ) {
            return sResolved;
        }
    }
    return NULL;
}

static char *xllm__memory_builtin_try_candidates(
    const char *const *pPrefixes,
    size_t iPrefixCount,
    const char *const *pSuffixes,
    size_t iSuffixCount
)
{
    size_t i;
    size_t j;

    for ( i = 0u; i < iPrefixCount; ++i ) {
        for ( j = 0u; j < iSuffixCount; ++j ) {
            char sBuffer[1024];
            int iWritten = snprintf(
                sBuffer,
                sizeof(sBuffer),
                "%s%s",
                pPrefixes[i] ? pPrefixes[i] : "",
                pSuffixes[j] ? pSuffixes[j] : ""
            );
            if ( iWritten <= 0 || (size_t)iWritten >= sizeof(sBuffer) ) {
                continue;
            }
            if ( xllm__memory_builtin_path_exists(sBuffer) ) {
                return xllm__dup_cstr(sBuffer);
            }
        }
    }

    return NULL;
}

static bool xllm__memory_builtin_sibling_exists(const char *sPath, const char *sLeaf)
{
    char sBuffer[1024];
    const char *sSlash;
    const char *sBackslash;
    size_t iDirLen;
    int iWritten;

    if ( !sPath || !sPath[0] || !sLeaf || !sLeaf[0] ) {
        return false;
    }

    sSlash = strrchr(sPath, '/');
    sBackslash = strrchr(sPath, '\\');
    if ( !sSlash || (sBackslash && sBackslash > sSlash) ) {
        sSlash = sBackslash;
    }
    if ( !sSlash ) {
        return xllm__memory_builtin_path_exists(sLeaf);
    }

    iDirLen = (size_t)(sSlash - sPath) + 1u;
    if ( iDirLen >= sizeof(sBuffer) ) {
        return false;
    }
    memcpy(sBuffer, sPath, iDirLen);
    sBuffer[iDirLen] = '\0';
    iWritten = snprintf(sBuffer + iDirLen, sizeof(sBuffer) - iDirLen, "%s", sLeaf);
    if ( iWritten <= 0 || (size_t)iWritten >= sizeof(sBuffer) - iDirLen ) {
        return false;
    }
    return xllm__memory_builtin_path_exists(sBuffer);
}

static char *xllm__memory_builtin_resolve_default_runtime(void)
{
    static const char *const sPrefixes[] = {
        "",
        ".\\",
        "..\\",
        "..\\..\\",
        "..\\..\\..\\",
        "..\\..\\..\\..\\"
    };
    static const char *const sSuffixes[] = {
        "build\\onnxruntime_nupkg\\runtimes\\win-x64\\native\\onnxruntime.dll",
        "lib\\onnxruntime\\onnxruntime.dll",
        "dev\\minionnx\\build\\onnxruntime.dll"
    };
    size_t i;
    size_t j;

    for ( i = 0u; i < (sizeof(sPrefixes) / sizeof(sPrefixes[0])); ++i ) {
        for ( j = 0u; j < (sizeof(sSuffixes) / sizeof(sSuffixes[0])); ++j ) {
            char sBuffer[1024];
            int iWritten = snprintf(
                sBuffer,
                sizeof(sBuffer),
                "%s%s",
                sPrefixes[i] ? sPrefixes[i] : "",
                sSuffixes[j] ? sSuffixes[j] : ""
            );
            if ( iWritten <= 0 || (size_t)iWritten >= sizeof(sBuffer) ) {
                continue;
            }
            if ( !xllm__memory_builtin_path_exists(sBuffer) ) {
                continue;
            }
            if ( strstr(sBuffer, "lib\\onnxruntime\\") != NULL &&
                 !xllm__memory_builtin_sibling_exists(sBuffer, "onnxruntime_providers_shared.dll") ) {
                continue;
            }
            return xllm__dup_cstr(sBuffer);
        }
    }

    return NULL;
}

static char *xllm__memory_builtin_resolve_default_model(void)
{
    static const char *const sPrefixes[] = {
        "",
        ".\\",
        "..\\",
        "..\\..\\",
        "..\\..\\..\\",
        "..\\..\\..\\..\\"
    };
    static const char *const sSuffixes[] = {
        "dev\\minionnx\\build\\multilingual-e5-small\\ort\\model.ort",
        "dev\\embedbench\\build\\real_onnx\\model_cache\\WiseIntelligence__multilingual-e5-small-Optimum-ONNX-Quantized-AVX2\\model.onnx"
    };

    return xllm__memory_builtin_try_candidates(
        sPrefixes,
        sizeof(sPrefixes) / sizeof(sPrefixes[0]),
        sSuffixes,
        sizeof(sSuffixes) / sizeof(sSuffixes[0])
    );
}

static char *xllm__memory_builtin_resolve_default_tokenizer(void)
{
    static const char *const sPrefixes[] = {
        "",
        ".\\",
        "..\\",
        "..\\..\\",
        "..\\..\\..\\",
        "..\\..\\..\\..\\"
    };
    static const char *const sSuffixes[] = {
        "dev\\embedbench\\build\\real_onnx\\model_cache\\WiseIntelligence__multilingual-e5-small-Optimum-ONNX-Quantized-AVX2\\tokenizer.json",
        "dev\\embedbench\\build\\real_onnx\\model_cache\\WiseIntelligence__multilingual-e5-small-Optimum-ONNX-Quantized-AVX2\\vocab.txt"
    };

    return xllm__memory_builtin_try_candidates(
        sPrefixes,
        sizeof(sPrefixes) / sizeof(sPrefixes[0]),
        sSuffixes,
        sizeof(sSuffixes) / sizeof(sSuffixes[0])
    );
}

static char *xllm__memory_builtin_resolve_default_model_dir(void)
{
    static const char *const sPrefixes[] = {
        "",
        ".\\",
        "..\\",
        "..\\..\\",
        "..\\..\\..\\",
        "..\\..\\..\\..\\"
    };
    static const char *const sSuffixes[] = {
        "dev\\minionnx\\build\\multilingual-e5-small",
        "dev\\embedbench\\build\\real_onnx\\model_cache\\WiseIntelligence__multilingual-e5-small-Optimum-ONNX-Quantized-AVX2"
    };

    return xllm__memory_builtin_try_candidates(
        sPrefixes,
        sizeof(sPrefixes) / sizeof(sPrefixes[0]),
        sSuffixes,
        sizeof(sSuffixes) / sizeof(sSuffixes[0])
    );
}

static char *xllm__memory_builtin_make_message(
    const char *sPrefix,
    const char *sDetail
)
{
    char sBuffer[1024];
    int iWritten = snprintf(
        sBuffer,
        sizeof(sBuffer),
        "%s%s%s",
        sPrefix ? sPrefix : "",
        (sPrefix && sPrefix[0] && sDetail && sDetail[0]) ? ": " : "",
        sDetail ? sDetail : ""
    );
    if ( iWritten <= 0 || (size_t)iWritten >= sizeof(sBuffer) ) {
        return xllm__dup_cstr(sPrefix ? sPrefix : "memory embedder");
    }
    return xllm__dup_cstr(sBuffer);
}

static int xllm__memory_builtin_hash_embed_text(
    void *pCtx,
    xllm_memory_embed_task eTask,
    const char *sText,
    xllm_memory_embedding *pEmbedding,
    xllm_error *pError
)
{
    const xllm__memory_builtin_hash_ctx *pHash = (const xllm__memory_builtin_hash_ctx *)pCtx;
    uint32 uDim = (pHash && pHash->uDimensions > 0u) ? pHash->uDimensions : 16u;
    const char *p;

    (void)eTask;
    (void)pError;

    if ( !sText || !pEmbedding || uDim == 0u ) {
        return XRT_NET_ERROR;
    }

    pEmbedding->pfValues = (float *)xrtCalloc((size_t)uDim, sizeof(float));
    if ( !pEmbedding->pfValues ) {
        return XRT_NET_ERROR;
    }
    pEmbedding->uValueCount = uDim;

    for ( p = sText; *p; ++p ) {
        unsigned char c = (unsigned char)tolower((unsigned char)*p);
        uint32 uSlot = (uint32)(c % uDim);
        pEmbedding->pfValues[uSlot] += 1.0f;
    }

    return XRT_NET_OK;
}

static int xllm__memory_builtin_hash_clone_ctx(
    void *pCtx,
    void **ppClonedCtx,
    xllm_error *pError
)
{
    const xllm__memory_builtin_hash_ctx *pSource = (const xllm__memory_builtin_hash_ctx *)pCtx;
    xllm__memory_builtin_hash_ctx *pClone;

    (void)pError;

    if ( !ppClonedCtx ) {
        return XRT_NET_ERROR;
    }
    *ppClonedCtx = NULL;
    if ( !pSource ) {
        return XRT_NET_OK;
    }

    pClone = (xllm__memory_builtin_hash_ctx *)xrtCalloc(1u, sizeof(*pClone));
    if ( !pClone ) {
        return XRT_NET_ERROR;
    }
    *pClone = *pSource;
    *ppClonedCtx = pClone;
    return XRT_NET_OK;
}

static void xllm__memory_builtin_hash_dispose_ctx(void *pCtx)
{
    if ( pCtx ) {
        xrtFree(pCtx);
    }
}

static void xllm__memory_builtin_embedder_options_init(
    xllm_memory_builtin_embedder_options *pOptions
)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->eKind = XLLM_MEMORY_BUILTIN_EMBEDDER_MULTILINGUAL_E5_SMALL_ONNX;
    pOptions->bAutoDiscoverAssets = true;
    pOptions->uHashDimensions = 16u;
}

static void xllm__memory_builtin_embedder_probe_reset(
    xllm_memory_builtin_embedder_probe *pProbe
)
{
    if ( !pProbe ) {
        return;
    }

    xllm__free_cstr(&pProbe->sResolvedRuntimeDllPath);
    xllm__free_cstr(&pProbe->sResolvedModelPath);
    xllm__free_cstr(&pProbe->sResolvedTokenizerPath);
    xllm__free_cstr(&pProbe->sMessage);
    xllm__xvalue_release(&pProbe->tVendorExtra);
    memset(pProbe, 0, sizeof(*pProbe));
}

static const char *xllm__memory_builtin_embedder_kind_name(
    xllm_memory_builtin_embedder_kind eKind
)
{
    switch ( eKind ) {
        case XLLM_MEMORY_BUILTIN_EMBEDDER_HASH:
            return "builtin_hash.v1";
        case XLLM_MEMORY_BUILTIN_EMBEDDER_MULTILINGUAL_E5_SMALL_ONNX:
            return "multilingual-e5-small.onnx.v1";
        case XLLM_MEMORY_BUILTIN_EMBEDDER_NONE:
        default:
            return "none.v1";
    }
}

static xvalue xllm__memory_builtin_make_asset_extra(
    xvalue tUserExtra,
    xllm_memory_builtin_embedder_kind eKind,
    const xllm_memory_builtin_embedder_probe *pProbe,
    uint32 uDimensions
)
{
    xvalue tExtra;

    tExtra = xllm__memory_create_shared_table_copy(tUserExtra);
    if ( !tExtra ) {
        return 0;
    }

    if ( !xvoTableSetInt(tExtra, (str)XLLM__MEMORY_EMBED_EXTRA_BUILTIN_KIND, 0u, (int64)eKind) ||
         !xllm__memory_table_set_owned_text(
            tExtra,
            XLLM__MEMORY_EMBED_EXTRA_EMBEDDER_KIND,
            xllm__memory_builtin_embedder_kind_name(eKind)
         ) ) {
        xvoUnref(tExtra);
        return 0;
    }

    switch ( eKind ) {
        case XLLM_MEMORY_BUILTIN_EMBEDDER_HASH:
            if ( !xllm__memory_table_set_owned_text(tExtra, XLLM__MEMORY_EMBED_EXTRA_PROFILE_ID, "builtin_hash.v1") ||
                 !xllm__memory_table_set_owned_text(tExtra, XLLM__MEMORY_EMBED_EXTRA_MODEL_ID, "builtin-hash") ||
                 !xvoTableSetInt(tExtra, (str)XLLM__MEMORY_EMBED_EXTRA_DIMENSIONS, 0u, (int64)uDimensions) ) {
                xvoUnref(tExtra);
                return 0;
            }
            break;

        case XLLM_MEMORY_BUILTIN_EMBEDDER_MULTILINGUAL_E5_SMALL_ONNX:
            if ( !xllm__memory_table_set_owned_text(tExtra, XLLM__MEMORY_EMBED_EXTRA_PROFILE_ID, "multilingual-e5-small.onnx.v1") ||
                 !xllm__memory_table_set_owned_text(tExtra, XLLM__MEMORY_EMBED_EXTRA_MODEL_ID, XLLM__MEMORY_BUILTIN_E5_PROFILE.sModelId) ||
                 !xllm__memory_table_set_owned_text(tExtra, XLLM__MEMORY_EMBED_EXTRA_REPO_ID, XLLM__MEMORY_BUILTIN_E5_PROFILE.sRepoId) ||
                 !xllm__memory_table_set_owned_text(tExtra, XLLM__MEMORY_EMBED_EXTRA_RUNTIME_DLL_PATH, pProbe ? pProbe->sResolvedRuntimeDllPath : NULL) ||
                 !xllm__memory_table_set_owned_text(tExtra, XLLM__MEMORY_EMBED_EXTRA_MODEL_PATH, pProbe ? pProbe->sResolvedModelPath : NULL) ||
                 !xllm__memory_table_set_owned_text(tExtra, XLLM__MEMORY_EMBED_EXTRA_TOKENIZER_PATH, pProbe ? pProbe->sResolvedTokenizerPath : NULL) ||
                 !xllm__memory_table_set_owned_text(tExtra, XLLM__MEMORY_EMBED_EXTRA_QUERY_PREFIX, XLLM__MEMORY_BUILTIN_E5_PROFILE.sQueryPrefix) ||
                 !xllm__memory_table_set_owned_text(tExtra, XLLM__MEMORY_EMBED_EXTRA_DOCUMENT_PREFIX, XLLM__MEMORY_BUILTIN_E5_PROFILE.sDocumentPrefix) ||
                 !xllm__memory_table_set_owned_text(tExtra, XLLM__MEMORY_EMBED_EXTRA_POOLING_MODE, XLLM__MEMORY_BUILTIN_E5_PROFILE.sPoolingMode) ||
                 !xvoTableSetInt(tExtra, (str)XLLM__MEMORY_EMBED_EXTRA_NORMALIZE, 0u, XLLM__MEMORY_BUILTIN_E5_PROFILE.bNormalize ? 1 : 0) ||
                 !xvoTableSetInt(tExtra, (str)XLLM__MEMORY_EMBED_EXTRA_MAX_INPUT_TOKENS, 0u, (int64)XLLM__MEMORY_BUILTIN_E5_PROFILE.uMaxInputTokens) ||
                 !xvoTableSetInt(tExtra, (str)XLLM__MEMORY_EMBED_EXTRA_DIMENSIONS, 0u, (int64)uDimensions) ) {
                xvoUnref(tExtra);
                return 0;
            }
            break;

        case XLLM_MEMORY_BUILTIN_EMBEDDER_NONE:
        default:
            break;
    }

    return tExtra;
}

static int xllm__memory_probe_builtin_embedder(
    const xllm_memory_builtin_embedder_options *pOptions,
    xllm_memory_builtin_embedder_probe *pProbe,
    xllm_error *pError
)
{
    xllm_memory_builtin_embedder_options tDefaultOptions;
    const xllm_memory_builtin_embedder_options *pUseOptions = pOptions;

    if ( !pProbe ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "builtin embedder probe output is required");
        return XRT_NET_ERROR;
    }

    if ( !pUseOptions ) {
        xllm__memory_builtin_embedder_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }

    xllm__memory_builtin_embedder_probe_reset(pProbe);
    pProbe->eKind = pUseOptions->eKind;
    pProbe->tVendorExtra = pUseOptions->tVendorExtra;
    xllm__xvalue_addref(pProbe->tVendorExtra);

    switch ( pUseOptions->eKind ) {
        case XLLM_MEMORY_BUILTIN_EMBEDDER_HASH:
            pProbe->bImplemented = true;
            pProbe->bReady = true;
            pProbe->sMessage = xllm__memory_builtin_make_message(
                "hash embedder ready",
                "deterministic lexical hash embedder is available"
            );
            return XRT_NET_OK;

        case XLLM_MEMORY_BUILTIN_EMBEDDER_MULTILINGUAL_E5_SMALL_ONNX:
            {
                static const char *const sModelLeaves[] = {
                    "model.ort",
                    "model.onnx",
                    "ort\\model.ort"
                };
                static const char *const sTokenizerLeaves[] = {
                    "tokenizer.json",
                    "vocab.txt",
                    "sentencepiece.bpe.model"
                };
                char *sDefaultModelDir = NULL;

            if ( pUseOptions->sRuntimeDllPath && pUseOptions->sRuntimeDllPath[0] ) {
                pProbe->sResolvedRuntimeDllPath =
                    xllm__memory_builtin_dup_existing_path(pUseOptions->sRuntimeDllPath);
            } else {
                const char *sEnv = getenv(XLLM__MEMORY_E5_RUNTIME_ENV);
                if ( sEnv && sEnv[0] ) {
                    pProbe->sResolvedRuntimeDllPath = xllm__memory_builtin_dup_existing_path(sEnv);
                }
            }
            if ( !pProbe->sResolvedRuntimeDllPath && pUseOptions->bAutoDiscoverAssets ) {
                pProbe->sResolvedRuntimeDllPath = xllm__memory_builtin_resolve_default_runtime();
            }
            pProbe->bRuntimeFound = pProbe->sResolvedRuntimeDllPath != NULL;

            if ( pUseOptions->sModelPath && pUseOptions->sModelPath[0] ) {
                pProbe->sResolvedModelPath =
                    xllm__memory_builtin_resolve_repo_file(
                        pUseOptions->sModelPath,
                        sModelLeaves,
                        sizeof(sModelLeaves) / sizeof(sModelLeaves[0])
                    );
            } else {
                const char *sEnv = getenv(XLLM__MEMORY_E5_MODEL_ENV);
                if ( sEnv && sEnv[0] ) {
                    pProbe->sResolvedModelPath =
                        xllm__memory_builtin_resolve_repo_file(
                            sEnv,
                            sModelLeaves,
                            sizeof(sModelLeaves) / sizeof(sModelLeaves[0])
                        );
                }
            }
            if ( !pProbe->sResolvedModelPath && pUseOptions->bAutoDiscoverAssets ) {
                pProbe->sResolvedModelPath = xllm__memory_builtin_resolve_default_model();
            }
            pProbe->bModelFound = pProbe->sResolvedModelPath != NULL;

            if ( pUseOptions->sTokenizerPath && pUseOptions->sTokenizerPath[0] ) {
                pProbe->sResolvedTokenizerPath =
                    xllm__memory_builtin_resolve_repo_file(
                        pUseOptions->sTokenizerPath,
                        sTokenizerLeaves,
                        sizeof(sTokenizerLeaves) / sizeof(sTokenizerLeaves[0])
                    );
            } else {
                const char *sEnv = getenv(XLLM__MEMORY_E5_TOKENIZER_ENV);
                if ( sEnv && sEnv[0] ) {
                    pProbe->sResolvedTokenizerPath =
                        xllm__memory_builtin_resolve_repo_file(
                            sEnv,
                            sTokenizerLeaves,
                            sizeof(sTokenizerLeaves) / sizeof(sTokenizerLeaves[0])
                        );
                }
            }
            if ( !pProbe->sResolvedModelPath && pUseOptions->bAutoDiscoverAssets ) {
                sDefaultModelDir = xllm__memory_builtin_resolve_default_model_dir();
                if ( sDefaultModelDir ) {
                    pProbe->sResolvedModelPath =
                        xllm__memory_builtin_resolve_repo_file(
                            sDefaultModelDir,
                            sModelLeaves,
                            sizeof(sModelLeaves) / sizeof(sModelLeaves[0])
                        );
                }
            }
            if ( !pProbe->sResolvedTokenizerPath && pUseOptions->bAutoDiscoverAssets ) {
                pProbe->sResolvedTokenizerPath = xllm__memory_builtin_resolve_default_tokenizer();
            }
            if ( !pProbe->sResolvedTokenizerPath && sDefaultModelDir ) {
                pProbe->sResolvedTokenizerPath =
                    xllm__memory_builtin_resolve_repo_file(
                        sDefaultModelDir,
                        sTokenizerLeaves,
                        sizeof(sTokenizerLeaves) / sizeof(sTokenizerLeaves[0])
                    );
            }
            pProbe->bTokenizerFound = pProbe->sResolvedTokenizerPath != NULL;
            pProbe->bImplemented = true;
            pProbe->bReady = pProbe->bRuntimeFound && pProbe->bModelFound && pProbe->bTokenizerFound;

            if ( pProbe->bRuntimeFound && pProbe->bModelFound && pProbe->bTokenizerFound ) {
                pProbe->sMessage = xllm__memory_builtin_make_message(
                    "multilingual-e5-small assets found",
                    "profile is fixed (query/passages + mean + normalize), builtin ONNX inference is available"
                );
            } else {
                char sMissing[256];
                size_t iOffset = 0u;

                sMissing[0] = '\0';
                if ( !pProbe->bRuntimeFound ) {
                    iOffset += (size_t)snprintf(sMissing + iOffset, sizeof(sMissing) - iOffset, "%sruntime_dll", iOffset ? "," : "");
                }
                if ( !pProbe->bModelFound && iOffset < sizeof(sMissing) ) {
                    iOffset += (size_t)snprintf(sMissing + iOffset, sizeof(sMissing) - iOffset, "%smodel", iOffset ? "," : "");
                }
                if ( !pProbe->bTokenizerFound && iOffset < sizeof(sMissing) ) {
                    iOffset += (size_t)snprintf(sMissing + iOffset, sizeof(sMissing) - iOffset, "%stokenizer", iOffset ? "," : "");
                }
                pProbe->sMessage = xllm__memory_builtin_make_message(
                    "multilingual-e5-small assets missing",
                    sMissing[0] ? sMissing : "unknown"
                );
            }
            xllm__free_cstr(&sDefaultModelDir);
            return XRT_NET_OK;
            }

        case XLLM_MEMORY_BUILTIN_EMBEDDER_NONE:
        default:
            pProbe->sMessage = xllm__memory_builtin_make_message(
                "builtin embedder disabled",
                "no builtin embedder was selected"
            );
            return XRT_NET_OK;
    }
}

static int xllm__memory_make_builtin_embedder(
    const xllm_memory_builtin_embedder_options *pOptions,
    xllm_memory_embedder *pEmbedder,
    xllm_error *pError
)
{
    xllm_memory_builtin_embedder_options tDefaultOptions;
    const xllm_memory_builtin_embedder_options *pUseOptions = pOptions;

    if ( !pEmbedder ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "builtin embedder output is required");
        return XRT_NET_ERROR;
    }

    if ( !pUseOptions ) {
        xllm__memory_builtin_embedder_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }

    xllm_memory_embedder_reset(pEmbedder);

    switch ( pUseOptions->eKind ) {
        case XLLM_MEMORY_BUILTIN_EMBEDDER_HASH: {
            xllm__memory_builtin_hash_ctx *pHash =
                (xllm__memory_builtin_hash_ctx *)xrtCalloc(1u, sizeof(*pHash));
            if ( !pHash ) {
                xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate hash embedder context");
                return XRT_NET_ERROR;
            }
            pHash->uDimensions = pUseOptions->uHashDimensions > 0u ? pUseOptions->uHashDimensions : 16u;
            pEmbedder->pfnEmbedText = xllm__memory_builtin_hash_embed_text;
            pEmbedder->pfnCloneCtx = xllm__memory_builtin_hash_clone_ctx;
            pEmbedder->pfnDisposeCtx = xllm__memory_builtin_hash_dispose_ctx;
            pEmbedder->pCtx = pHash;
            pEmbedder->tVendorExtra = xllm__memory_builtin_make_asset_extra(
                pUseOptions->tVendorExtra,
                XLLM_MEMORY_BUILTIN_EMBEDDER_HASH,
                NULL,
                pHash->uDimensions
            );
            if ( !pEmbedder->tVendorExtra ) {
                xllm_memory_embedder_reset(pEmbedder);
                xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate hash embedder asset metadata");
                return XRT_NET_ERROR;
            }
            return XRT_NET_OK;
        }

        case XLLM_MEMORY_BUILTIN_EMBEDDER_MULTILINGUAL_E5_SMALL_ONNX: {
            xllm_memory_builtin_embedder_probe tProbe;
            int iStatus;
            void *pCtx = NULL;

            memset(&tProbe, 0, sizeof(tProbe));
            iStatus = xllm__memory_probe_builtin_embedder(pUseOptions, &tProbe, pError);
            if ( iStatus != XRT_NET_OK ) {
                xllm__memory_builtin_embedder_probe_reset(&tProbe);
                return iStatus;
            }
            if ( !tProbe.bRuntimeFound || !tProbe.bModelFound || !tProbe.bTokenizerFound ) {
                xllm__error_set(
                    pError,
                    XLLM_ERROR_INVALID_REQUEST,
                    tProbe.sMessage ? tProbe.sMessage : "multilingual-e5-small assets are not ready"
                );
                xllm__memory_builtin_embedder_probe_reset(&tProbe);
                return XRT_NET_ERROR;
            }
            if ( xllm__memory_builtin_e5_create_ctx(pUseOptions, &tProbe, &pCtx, pError) != XRT_NET_OK ) {
                xllm__memory_builtin_embedder_probe_reset(&tProbe);
                return XRT_NET_ERROR;
            }
            pEmbedder->pfnEmbedText = xllm__memory_builtin_e5_embed_text;
            pEmbedder->pfnCloneCtx = xllm__memory_builtin_e5_clone_ctx;
            pEmbedder->pfnDisposeCtx = xllm__memory_builtin_e5_dispose_ctx;
            pEmbedder->pCtx = pCtx;
            pEmbedder->tVendorExtra = xllm__memory_builtin_make_asset_extra(
                pUseOptions->tVendorExtra,
                XLLM_MEMORY_BUILTIN_EMBEDDER_MULTILINGUAL_E5_SMALL_ONNX,
                &tProbe,
                XLLM__MEMORY_BUILTIN_E5_PROFILE.uEmbeddingDimensions
            );
            if ( !pEmbedder->tVendorExtra ) {
                xllm__memory_builtin_embedder_probe_reset(&tProbe);
                xllm_memory_embedder_reset(pEmbedder);
                xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate multilingual-e5-small asset metadata");
                return XRT_NET_ERROR;
            }
            xllm__memory_builtin_embedder_probe_reset(&tProbe);
            return XRT_NET_OK;
        }

        case XLLM_MEMORY_BUILTIN_EMBEDDER_NONE:
        default:
            xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "no builtin embedder was selected");
            return XRT_NET_ERROR;
    }
}
