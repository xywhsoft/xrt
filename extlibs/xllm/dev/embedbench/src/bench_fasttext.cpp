#include "bench_common.h"

extern "C" {
#include "../../fastText/fasttext_demo.h"
}

#include <string>
#include <vector>

typedef struct bench_fasttext_options {
    const char *pCorpusPath;
    const char *pCasesPath;
    const char *pModelPrefix;
    int iWarmup;
    int iIterations;
} bench_fasttext_options;

static int
bench_fasttext_parse_options(int argc, char **argv, bench_fasttext_options *pOptions)
{
    int i;

    pOptions->pCorpusPath = NULL;
    pOptions->pCasesPath = NULL;
    pOptions->pModelPrefix = NULL;
    pOptions->iWarmup = 50;
    pOptions->iIterations = 500;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--corpus") == 0 && i + 1 < argc) {
            pOptions->pCorpusPath = argv[++i];
        } else if (strcmp(argv[i], "--cases") == 0 && i + 1 < argc) {
            pOptions->pCasesPath = argv[++i];
        } else if (strcmp(argv[i], "--model-prefix") == 0 && i + 1 < argc) {
            pOptions->pModelPrefix = argv[++i];
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
                   pOptions->pModelPrefix != NULL
               ? 0
               : -1;
}

int
main(int argc, char **argv)
{
    bench_fasttext_options sOptions;
    bench_case_list sCases;
    ftdemo_model *pModel = NULL;
    std::string sModelPath;
    std::vector<float> aVector;
    std::vector<double> aCaseScores;
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

    if (bench_fasttext_parse_options(argc, argv, &sOptions) != 0) {
        fprintf(stderr, "usage: bench_fasttext --corpus <path> --cases <path> --model-prefix <path> [--warmup N] [--iterations N]\n");
        return 1;
    }

    sModelPath = std::string(sOptions.pModelPrefix) + ".bin";
    if (!bench_file_exists(sModelPath.c_str())) {
        if (ftdemo_train_default_model(sOptions.pCorpusPath, sOptions.pModelPrefix) != 0) {
            fprintf(stderr, "fastText training failed: %s\n", ftdemo_last_error());
            return 1;
        }
    }

    fStart = bench_now_ms();
    if (ftdemo_model_load(sModelPath.c_str(), &pModel) != 0) {
        fprintf(stderr, "fastText load failed: %s\n", ftdemo_last_error());
        return 1;
    }
    fInitMs = bench_now_ms() - fStart;

    if (bench_load_cases(sOptions.pCasesPath, &sCases) != 0) {
        fprintf(stderr, "failed to load benchmark cases\n");
        ftdemo_model_destroy(pModel);
        return 1;
    }

    iDim = ftdemo_model_dimension(pModel);
    if (iDim <= 0) {
        fprintf(stderr, "invalid fastText vector dimension\n");
        bench_free_cases(&sCases);
        ftdemo_model_destroy(pModel);
        return 1;
    }

    aVector.resize((size_t)iDim);
    aCaseScores.resize(sCases.uCount);

    for (i = 0; i < sCases.uCount; ++i) {
        float fSimilarity = 0.0f;
        if (ftdemo_model_compare_texts(pModel, sCases.pItems[i].pLeft, sCases.pItems[i].pRight, &fSimilarity) != 0) {
            fprintf(stderr, "fastText compare failed: %s\n", ftdemo_last_error());
            bench_free_cases(&sCases);
            ftdemo_model_destroy(pModel);
            return 1;
        }

        aCaseScores[i] = (double)fSimilarity;
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
            (void)ftdemo_model_embed_text(pModel, sCases.pItems[i].pLeft, aVector.data(), aVector.size());
            (void)ftdemo_model_embed_text(pModel, sCases.pItems[i].pRight, aVector.data(), aVector.size());
        }
    }

    fStart = bench_now_ms();
    for (int it = 0; it < sOptions.iIterations; ++it) {
        for (i = 0; i < sCases.uCount; ++i) {
            (void)ftdemo_model_embed_text(pModel, sCases.pItems[i].pLeft, aVector.data(), aVector.size());
            (void)ftdemo_model_embed_text(pModel, sCases.pItems[i].pRight, aVector.data(), aVector.size());
        }
    }
    fEmbedMs = (bench_now_ms() - fStart) / (double)(sOptions.iIterations * (int)(sCases.uCount * 2u));

    for (int it = 0; it < sOptions.iWarmup; ++it) {
        for (i = 0; i < sCases.uCount; ++i) {
            float fSimilarity = 0.0f;
            (void)ftdemo_model_compare_texts(pModel, sCases.pItems[i].pLeft, sCases.pItems[i].pRight, &fSimilarity);
        }
    }

    fStart = bench_now_ms();
    for (int it = 0; it < sOptions.iIterations; ++it) {
        for (i = 0; i < sCases.uCount; ++i) {
            float fSimilarity = 0.0f;
            (void)ftdemo_model_compare_texts(pModel, sCases.pItems[i].pLeft, sCases.pItems[i].pRight, &fSimilarity);
        }
    }
    fCompareMs = (bench_now_ms() - fStart) / (double)(sOptions.iIterations * (int)sCases.uCount);

    printf("backend=fasttext\n");
    printf("model_path=%s\n", sModelPath.c_str());
    printf("runtime_path=\n");
    printf("vector_dim=%d\n", iDim);
    printf("vocab_size=-1\n");
    printf("init_ms=%.6f\n", fInitMs);
    printf("embed_avg_ms=%.6f\n", fEmbedMs);
    printf("compare_avg_ms=%.6f\n", fCompareMs);
    printf("related_avg=%.6f\n", uRelatedCount ? fRelatedSum / (double)uRelatedCount : 0.0);
    printf("unrelated_avg=%.6f\n", uUnrelatedCount ? fUnrelatedSum / (double)uUnrelatedCount : 0.0);
    for (i = 0; i < sCases.uCount; ++i) {
        printf("case.%s=%.6f\n", sCases.pItems[i].pName, aCaseScores[i]);
    }

    bench_free_cases(&sCases);
    ftdemo_model_destroy(pModel);
    return 0;
}
