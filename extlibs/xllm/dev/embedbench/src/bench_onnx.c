#define _CRT_SECURE_NO_WARNINGS

#include "bench_common.h"
#include "../../onnxdemo/onnx_demo.h"

typedef struct bench_onnx_options {
    const char *pCasesPath;
    const char *pRuntimeDll;
    const char *pModelPath;
    const char *pVocabPath;
    int iWarmup;
    int iIterations;
} bench_onnx_options;

static int
bench_onnx_parse_options(int argc, char **argv, bench_onnx_options *pOptions)
{
    int i;

    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->iWarmup = 50;
    pOptions->iIterations = 500;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--cases") == 0 && i + 1 < argc) {
            pOptions->pCasesPath = argv[++i];
        } else if (strcmp(argv[i], "--runtime-dll") == 0 && i + 1 < argc) {
            pOptions->pRuntimeDll = argv[++i];
        } else if (strcmp(argv[i], "--model") == 0 && i + 1 < argc) {
            pOptions->pModelPath = argv[++i];
        } else if (strcmp(argv[i], "--vocab") == 0 && i + 1 < argc) {
            pOptions->pVocabPath = argv[++i];
        } else if (strcmp(argv[i], "--warmup") == 0 && i + 1 < argc) {
            pOptions->iWarmup = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--iterations") == 0 && i + 1 < argc) {
            pOptions->iIterations = atoi(argv[++i]);
        } else {
            fprintf(stderr, "Unknown or incomplete argument: %s\n", argv[i]);
            return -1;
        }
    }

    return pOptions->pCasesPath != NULL &&
                   pOptions->pRuntimeDll != NULL &&
                   pOptions->pModelPath != NULL &&
                   pOptions->pVocabPath != NULL
               ? 0
               : -1;
}

static int
bench_onnx_compare_pair(
    onnx_demo *pDemo,
    const char *pLeft,
    const char *pRight,
    float *pfSimilarity)
{
    float *pLeftVector = NULL;
    float *pRightVector = NULL;
    size_t uLeftDim = 0;
    size_t uRightDim = 0;
    size_t uMatched = 0;
    size_t uTotal = 0;

    if (onnx_demo_embed_text(pDemo, pLeft, &pLeftVector, &uLeftDim, &uMatched, &uTotal) != 0) {
        return -1;
    }

    if (onnx_demo_embed_text(pDemo, pRight, &pRightVector, &uRightDim, &uMatched, &uTotal) != 0) {
        onnx_demo_free_vector(pLeftVector);
        return -1;
    }

    *pfSimilarity = onnx_demo_cosine_similarity(
        pLeftVector,
        pRightVector,
        uLeftDim < uRightDim ? uLeftDim : uRightDim);

    onnx_demo_free_vector(pLeftVector);
    onnx_demo_free_vector(pRightVector);
    return 0;
}

int
main(int argc, char **argv)
{
    bench_onnx_options sOptions;
    bench_case_list sCases;
    onnx_demo *pDemo = NULL;
    double *pCaseScores = NULL;
    size_t i;
    double fInitMs;
    double fEmbedMs;
    double fCompareMs;
    double fStart;
    double fRelatedSum = 0.0;
    double fUnrelatedSum = 0.0;
    size_t uRelatedCount = 0;
    size_t uUnrelatedCount = 0;

    if (bench_onnx_parse_options(argc, argv, &sOptions) != 0) {
        fprintf(stderr, "usage: bench_onnx --cases <path> --runtime-dll <path> --model <path> --vocab <path> [--warmup N] [--iterations N]\n");
        return 1;
    }

    fStart = bench_now_ms();
    if (onnx_demo_create(&pDemo, sOptions.pRuntimeDll, sOptions.pModelPath, sOptions.pVocabPath) != 0) {
        fprintf(stderr, "onnx init failed: %s\n", onnx_demo_last_error(pDemo));
        onnx_demo_destroy(pDemo);
        return 1;
    }
    fInitMs = bench_now_ms() - fStart;

    if (bench_load_cases(sOptions.pCasesPath, &sCases) != 0) {
        fprintf(stderr, "failed to load benchmark cases\n");
        onnx_demo_destroy(pDemo);
        return 1;
    }

    pCaseScores = (double *)calloc(sCases.uCount, sizeof(double));
    if (pCaseScores == NULL) {
        fprintf(stderr, "out of memory\n");
        bench_free_cases(&sCases);
        onnx_demo_destroy(pDemo);
        return 1;
    }

    for (i = 0; i < sCases.uCount; ++i) {
        float fSimilarity = 0.0f;
        if (bench_onnx_compare_pair(pDemo, sCases.pItems[i].pLeft, sCases.pItems[i].pRight, &fSimilarity) != 0) {
            fprintf(stderr, "onnx compare failed: %s\n", onnx_demo_last_error(pDemo));
            free(pCaseScores);
            bench_free_cases(&sCases);
            onnx_demo_destroy(pDemo);
            return 1;
        }

        pCaseScores[i] = (double)fSimilarity;
        if (bench_is_related_case(&sCases.pItems[i])) {
            fRelatedSum += fSimilarity;
            ++uRelatedCount;
        } else {
            fUnrelatedSum += fSimilarity;
            ++uUnrelatedCount;
        }
    }

    for (int it = 0; it < sOptions.iWarmup; ++it) {
        for (i = 0; i < sCases.uCount; ++i) {
            float *pVector = NULL;
            size_t uDim = 0;
            size_t uMatched = 0;
            size_t uTotal = 0;
            (void)onnx_demo_embed_text(pDemo, sCases.pItems[i].pLeft, &pVector, &uDim, &uMatched, &uTotal);
            onnx_demo_free_vector(pVector);
            pVector = NULL;
            (void)onnx_demo_embed_text(pDemo, sCases.pItems[i].pRight, &pVector, &uDim, &uMatched, &uTotal);
            onnx_demo_free_vector(pVector);
        }
    }

    fStart = bench_now_ms();
    for (int it = 0; it < sOptions.iIterations; ++it) {
        for (i = 0; i < sCases.uCount; ++i) {
            float *pVector = NULL;
            size_t uDim = 0;
            size_t uMatched = 0;
            size_t uTotal = 0;
            (void)onnx_demo_embed_text(pDemo, sCases.pItems[i].pLeft, &pVector, &uDim, &uMatched, &uTotal);
            onnx_demo_free_vector(pVector);
            pVector = NULL;
            (void)onnx_demo_embed_text(pDemo, sCases.pItems[i].pRight, &pVector, &uDim, &uMatched, &uTotal);
            onnx_demo_free_vector(pVector);
        }
    }
    fEmbedMs = (bench_now_ms() - fStart) / (double)(sOptions.iIterations * (int)(sCases.uCount * 2u));

    for (int it = 0; it < sOptions.iWarmup; ++it) {
        for (i = 0; i < sCases.uCount; ++i) {
            float fSimilarity = 0.0f;
            (void)bench_onnx_compare_pair(pDemo, sCases.pItems[i].pLeft, sCases.pItems[i].pRight, &fSimilarity);
        }
    }

    fStart = bench_now_ms();
    for (int it = 0; it < sOptions.iIterations; ++it) {
        for (i = 0; i < sCases.uCount; ++i) {
            float fSimilarity = 0.0f;
            (void)bench_onnx_compare_pair(pDemo, sCases.pItems[i].pLeft, sCases.pItems[i].pRight, &fSimilarity);
        }
    }
    fCompareMs = (bench_now_ms() - fStart) / (double)(sOptions.iIterations * (int)sCases.uCount);

    printf("backend=onnx\n");
    printf("model_path=%s\n", sOptions.pModelPath);
    printf("runtime_path=%s\n", sOptions.pRuntimeDll);
    printf("vector_dim=%zu\n", onnx_demo_embedding_dim(pDemo));
    printf("vocab_size=-1\n");
    printf("init_ms=%.6f\n", fInitMs);
    printf("embed_avg_ms=%.6f\n", fEmbedMs);
    printf("compare_avg_ms=%.6f\n", fCompareMs);
    printf("related_avg=%.6f\n", uRelatedCount ? fRelatedSum / (double)uRelatedCount : 0.0);
    printf("unrelated_avg=%.6f\n", uUnrelatedCount ? fUnrelatedSum / (double)uUnrelatedCount : 0.0);
    for (i = 0; i < sCases.uCount; ++i) {
        printf("case.%s=%.6f\n", sCases.pItems[i].pName, pCaseScores[i]);
    }

    free(pCaseScores);
    bench_free_cases(&sCases);
    onnx_demo_destroy(pDemo);
    return 0;
}
