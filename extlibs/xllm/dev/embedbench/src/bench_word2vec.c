#define _CRT_SECURE_NO_WARNINGS

#include "bench_common.h"
#include "../../word2vec/word2vec_demo.h"

#include <math.h>

typedef struct bench_word2vec_options {
    const char *pCorpusPath;
    const char *pCasesPath;
    const char *pTrainerPath;
    const char *pModelPath;
    int iWarmup;
    int iIterations;
} bench_word2vec_options;

static int
bench_word2vec_parse_options(int argc, char **argv, bench_word2vec_options *pOptions)
{
    int i;

    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->iWarmup = 50;
    pOptions->iIterations = 500;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--corpus") == 0 && i + 1 < argc) {
            pOptions->pCorpusPath = argv[++i];
        } else if (strcmp(argv[i], "--cases") == 0 && i + 1 < argc) {
            pOptions->pCasesPath = argv[++i];
        } else if (strcmp(argv[i], "--trainer") == 0 && i + 1 < argc) {
            pOptions->pTrainerPath = argv[++i];
        } else if (strcmp(argv[i], "--model") == 0 && i + 1 < argc) {
            pOptions->pModelPath = argv[++i];
        } else if (strcmp(argv[i], "--warmup") == 0 && i + 1 < argc) {
            pOptions->iWarmup = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--iterations") == 0 && i + 1 < argc) {
            pOptions->iIterations = atoi(argv[++i]);
        } else {
            fprintf(stderr, "Unknown or incomplete argument: %s\n", argv[i]);
            return -1;
        }
    }

    return pOptions->pCorpusPath != NULL &&
                   pOptions->pCasesPath != NULL &&
                   pOptions->pTrainerPath != NULL &&
                   pOptions->pModelPath != NULL
               ? 0
               : -1;
}

static int
bench_word2vec_train_if_needed(const bench_word2vec_options *pOptions)
{
    char szCommand[4096];
    int iResult;

    if (bench_file_exists(pOptions->pModelPath)) {
        return 0;
    }

    snprintf(
        szCommand,
        sizeof(szCommand),
        "cmd /c \"\"%s\" -train \"%s\" -output \"%s\" -size 32 -window 5 -sample 1e-4 -negative 5 -hs 0 -binary 1 -cbow 0 -iter 30 -min-count 1 -threads 1 -debug 1\"",
        pOptions->pTrainerPath,
        pOptions->pCorpusPath,
        pOptions->pModelPath);

    iResult = system(szCommand);
    return iResult == 0 ? 0 : -1;
}

int
main(int argc, char **argv)
{
    bench_word2vec_options sOptions;
    bench_case_list sCases;
    w2vdemo_model *pModel = NULL;
    float *pVector = NULL;
    double *pCaseScores = NULL;
    int iDim;
    size_t i;
    double fInitMs;
    double fEmbedMs;
    double fCompareMs;
    double fStart;
    double fRelatedSum = 0.0;
    double fUnrelatedSum = 0.0;
    size_t uRelatedCount = 0;
    size_t uUnrelatedCount = 0;

    if (bench_word2vec_parse_options(argc, argv, &sOptions) != 0) {
        fprintf(stderr, "usage: bench_word2vec --corpus <path> --cases <path> --trainer <path> --model <path> [--warmup N] [--iterations N]\n");
        return 1;
    }

    if (bench_word2vec_train_if_needed(&sOptions) != 0) {
        fprintf(stderr, "word2vec training failed\n");
        return 1;
    }

    fStart = bench_now_ms();
    if (w2vdemo_model_load(sOptions.pModelPath, &pModel) != 0) {
        fprintf(stderr, "word2vec load failed: %s\n", w2vdemo_last_error());
        return 1;
    }
    fInitMs = bench_now_ms() - fStart;

    if (bench_load_cases(sOptions.pCasesPath, &sCases) != 0) {
        fprintf(stderr, "failed to load benchmark cases\n");
        w2vdemo_model_destroy(pModel);
        return 1;
    }

    iDim = w2vdemo_model_dimension(pModel);
    if (iDim <= 0) {
        fprintf(stderr, "invalid word2vec vector dimension\n");
        bench_free_cases(&sCases);
        w2vdemo_model_destroy(pModel);
        return 1;
    }

    pVector = (float *)calloc((size_t)iDim, sizeof(float));
    pCaseScores = (double *)calloc(sCases.uCount, sizeof(double));
    if (pVector == NULL || pCaseScores == NULL) {
        fprintf(stderr, "out of memory\n");
        free(pVector);
        free(pCaseScores);
        bench_free_cases(&sCases);
        w2vdemo_model_destroy(pModel);
        return 1;
    }

    for (i = 0; i < sCases.uCount; ++i) {
        float fSimilarity = 0.0f;
        size_t uLeftMatched = 0;
        size_t uRightMatched = 0;
        if (w2vdemo_model_compare_texts(
                pModel,
                sCases.pItems[i].pLeft,
                sCases.pItems[i].pRight,
                &fSimilarity,
                &uLeftMatched,
                &uRightMatched) != 0) {
            fprintf(stderr, "word2vec compare failed: %s\n", w2vdemo_last_error());
            free(pVector);
            free(pCaseScores);
            bench_free_cases(&sCases);
            w2vdemo_model_destroy(pModel);
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
            size_t uTokenCount = 0;
            size_t uMatched = 0;
            (void)w2vdemo_model_embed_text(pModel, sCases.pItems[i].pLeft, pVector, (size_t)iDim, &uTokenCount, &uMatched);
            (void)w2vdemo_model_embed_text(pModel, sCases.pItems[i].pRight, pVector, (size_t)iDim, &uTokenCount, &uMatched);
        }
    }

    fStart = bench_now_ms();
    for (int it = 0; it < sOptions.iIterations; ++it) {
        for (i = 0; i < sCases.uCount; ++i) {
            size_t uTokenCount = 0;
            size_t uMatched = 0;
            (void)w2vdemo_model_embed_text(pModel, sCases.pItems[i].pLeft, pVector, (size_t)iDim, &uTokenCount, &uMatched);
            (void)w2vdemo_model_embed_text(pModel, sCases.pItems[i].pRight, pVector, (size_t)iDim, &uTokenCount, &uMatched);
        }
    }
    fEmbedMs = (bench_now_ms() - fStart) / (double)(sOptions.iIterations * (int)(sCases.uCount * 2u));

    for (int it = 0; it < sOptions.iWarmup; ++it) {
        for (i = 0; i < sCases.uCount; ++i) {
            float fSimilarity = 0.0f;
            size_t uLeftMatched = 0;
            size_t uRightMatched = 0;
            (void)w2vdemo_model_compare_texts(
                pModel,
                sCases.pItems[i].pLeft,
                sCases.pItems[i].pRight,
                &fSimilarity,
                &uLeftMatched,
                &uRightMatched);
        }
    }

    fStart = bench_now_ms();
    for (int it = 0; it < sOptions.iIterations; ++it) {
        for (i = 0; i < sCases.uCount; ++i) {
            float fSimilarity = 0.0f;
            size_t uLeftMatched = 0;
            size_t uRightMatched = 0;
            (void)w2vdemo_model_compare_texts(
                pModel,
                sCases.pItems[i].pLeft,
                sCases.pItems[i].pRight,
                &fSimilarity,
                &uLeftMatched,
                &uRightMatched);
        }
    }
    fCompareMs = (bench_now_ms() - fStart) / (double)(sOptions.iIterations * (int)sCases.uCount);

    printf("backend=word2vec\n");
    printf("model_path=%s\n", sOptions.pModelPath);
    printf("runtime_path=\n");
    printf("vector_dim=%d\n", iDim);
    printf("vocab_size=%lld\n", w2vdemo_model_vocab_size(pModel));
    printf("init_ms=%.6f\n", fInitMs);
    printf("embed_avg_ms=%.6f\n", fEmbedMs);
    printf("compare_avg_ms=%.6f\n", fCompareMs);
    printf("related_avg=%.6f\n", uRelatedCount ? fRelatedSum / (double)uRelatedCount : 0.0);
    printf("unrelated_avg=%.6f\n", uUnrelatedCount ? fUnrelatedSum / (double)uUnrelatedCount : 0.0);
    for (i = 0; i < sCases.uCount; ++i) {
        printf("case.%s=%.6f\n", sCases.pItems[i].pName, pCaseScores[i]);
    }

    free(pVector);
    free(pCaseScores);
    bench_free_cases(&sCases);
    w2vdemo_model_destroy(pModel);
    return 0;
}
