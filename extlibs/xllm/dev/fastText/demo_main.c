#include "fasttext_demo.h"

#include <stdio.h>
#include <stdlib.h>

static int demo_file_exists(const char *sPath)
{
    FILE *pFile;

    if ( !sPath || !sPath[0] ) {
        return 0;
    }

    pFile = fopen(sPath, "rb");
    if ( !pFile ) {
        return 0;
    }

    fclose(pFile);
    return 1;
}

static void demo_print_vector_preview(
    const char *sLabel,
    const float *pVector,
    size_t iCount
)
{
    size_t i;
    size_t iPreview = iCount < 8u ? iCount : 8u;

    printf("%s [%zu dims]:", sLabel, iCount);
    for ( i = 0; i < iPreview; ++i ) {
        printf(" %.5f", pVector[i]);
    }
    if ( iPreview < iCount ) {
        printf(" ...");
    }
    printf("\n");
}

static int demo_embed_and_print(
    ftdemo_model *pModel,
    const char *sText
)
{
    int iDim;
    float *pVector;
    int iResult;

    iDim = ftdemo_model_dimension(pModel);
    if ( iDim <= 0 ) {
        fprintf(stderr, "Invalid model dimension.\n");
        return 1;
    }

    pVector = (float *)calloc((size_t)iDim, sizeof(float));
    if ( !pVector ) {
        fprintf(stderr, "Out of memory.\n");
        return 1;
    }

    iResult = ftdemo_model_embed_text(pModel, sText, pVector, (size_t)iDim);
    if ( iResult != 0 ) {
        fprintf(stderr, "Embed failed: %s\n", ftdemo_last_error());
        free(pVector);
        return 1;
    }

    printf("Text: %s\n", sText);
    demo_print_vector_preview("Vector preview", pVector, (size_t)iDim);
    free(pVector);
    return 0;
}

static int demo_compare_and_print(
    ftdemo_model *pModel,
    const char *sLeft,
    const char *sRight
)
{
    float fSimilarity = 0.0f;

    if ( ftdemo_model_compare_texts(pModel, sLeft, sRight, &fSimilarity) != 0 ) {
        fprintf(stderr, "Compare failed: %s\n", ftdemo_last_error());
        return 1;
    }

    printf("A: %s\n", sLeft);
    printf("B: %s\n", sRight);
    printf("Cosine similarity: %.5f\n", fSimilarity);
    return 0;
}

static void demo_run_builtin_examples(ftdemo_model *pModel)
{
    static const struct {
        const char *sName;
        const char *sLeft;
        const char *sRight;
    } kPairs[] = {
        {
            "related zh",
            "向量 检索 文本 切分 语义 索引",
            "语义 检索 向量 索引 chunk 记忆"
        },
        {
            "unrelated zh",
            "向量 检索 文本 切分 语义 索引",
            "咖啡 早餐 厨房 杯子 烘焙"
        },
        {
            "related en",
            "vector retrieval text chunk memory index",
            "semantic search vector index chunk knowledge"
        },
        {
            "unrelated en",
            "vector retrieval text chunk memory index",
            "coffee breakfast kitchen mug bakery"
        }
    };
    int i;

    printf("Built-in fastText demo pairs:\n\n");
    for ( i = 0; i < (int)(sizeof(kPairs) / sizeof(kPairs[0])); ++i ) {
        printf("[%s]\n", kPairs[i].sName);
        (void)demo_compare_and_print(pModel, kPairs[i].sLeft, kPairs[i].sRight);
        printf("\n");
    }

    printf("Vector preview examples:\n\n");
    (void)demo_embed_and_print(pModel, "向量 检索 文本 切分 语义 索引");
    printf("\n");
    (void)demo_embed_and_print(pModel, "vector retrieval text chunk memory index");
    printf("\n");
}

int main(int argc, char **argv)
{
    const char *sCorpusPath = "demo_corpus.txt";
    const char *sModelPrefix = "build\\demo_model";
    const char *sModelPath = "build\\demo_model.bin";
    ftdemo_model *pModel = NULL;
    int iResult = 0;

    if ( !demo_file_exists(sModelPath) ) {
        printf("Training demo model from %s ...\n", sCorpusPath);
        if ( ftdemo_train_default_model(sCorpusPath, sModelPrefix) != 0 ) {
            fprintf(stderr, "Training failed: %s\n", ftdemo_last_error());
            return 1;
        }
    }

    if ( ftdemo_model_load(sModelPath, &pModel) != 0 ) {
        fprintf(stderr, "Load failed: %s\n", ftdemo_last_error());
        return 1;
    }

    printf("Loaded model: %s\n", sModelPath);
    printf("Vector dimension: %d\n\n", ftdemo_model_dimension(pModel));

    if ( argc >= 3 ) {
        iResult = demo_compare_and_print(pModel, argv[1], argv[2]);
    } else if ( argc == 2 ) {
        iResult = demo_embed_and_print(pModel, argv[1]);
    } else {
        demo_run_builtin_examples(pModel);
    }

    ftdemo_model_destroy(pModel);
    return iResult;
}
