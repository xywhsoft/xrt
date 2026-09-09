#include "word2vec_demo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static int demo_run_training_if_needed(void)
{
    const char *sModelPath = "build\\demo_model.bin";
    const char *sTrainerPath = "build\\word2vec_train.exe";
    const char *sCmd =
        "\"build\\word2vec_train.exe\""
        " -train demo_corpus.txt"
        " -output build\\demo_model.bin"
        " -size 32"
        " -window 5"
        " -sample 1e-4"
        " -negative 5"
        " -hs 0"
        " -binary 1"
        " -cbow 0"
        " -iter 30"
        " -min-count 1"
        " -threads 1"
        " -debug 1";
    int iResult;

    if ( demo_file_exists(sModelPath) ) {
        return 0;
    }

    if ( !demo_file_exists(sTrainerPath) ) {
        fprintf(stderr, "Trainer executable not found: %s\n", sTrainerPath);
        return 1;
    }

    printf("Training demo model from demo_corpus.txt ...\n");
    iResult = system(sCmd);
    if ( iResult != 0 ) {
        fprintf(stderr, "word2vec training command failed: %d\n", iResult);
        return 1;
    }

    return 0;
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
    for ( i = 0u; i < iPreview; ++i ) {
        printf(" %.5f", pVector[i]);
    }
    if ( iPreview < iCount ) {
        printf(" ...");
    }
    printf("\n");
}

static int demo_embed_and_print(
    const w2vdemo_model *pModel,
    const char *sText
)
{
    int iDim;
    float *pVector;
    size_t iTokens = 0u;
    size_t iMatchedTokens = 0u;
    int iResult;

    iDim = w2vdemo_model_dimension(pModel);
    if ( iDim <= 0 ) {
        fprintf(stderr, "Invalid model dimension.\n");
        return 1;
    }

    pVector = (float *)calloc((size_t)iDim, sizeof(float));
    if ( !pVector ) {
        fprintf(stderr, "Out of memory.\n");
        return 1;
    }

    iResult = w2vdemo_model_embed_text(
        pModel,
        sText,
        pVector,
        (size_t)iDim,
        &iTokens,
        &iMatchedTokens
    );
    if ( iResult != 0 ) {
        fprintf(stderr, "Embed failed: %s\n", w2vdemo_last_error());
        free(pVector);
        return 1;
    }

    printf("Text: %s\n", sText);
    printf("Matched tokens: %zu / %zu\n", iMatchedTokens, iTokens);
    demo_print_vector_preview("Vector preview", pVector, (size_t)iDim);
    free(pVector);
    return 0;
}

static int demo_compare_and_print(
    const w2vdemo_model *pModel,
    const char *sLeft,
    const char *sRight
)
{
    float fSimilarity = 0.0f;
    size_t iLeftMatched = 0u;
    size_t iRightMatched = 0u;

    if ( w2vdemo_model_compare_texts(
        pModel,
        sLeft,
        sRight,
        &fSimilarity,
        &iLeftMatched,
        &iRightMatched
    ) != 0 ) {
        fprintf(stderr, "Compare failed: %s\n", w2vdemo_last_error());
        return 1;
    }

    printf("A: %s\n", sLeft);
    printf("B: %s\n", sRight);
    printf("Matched tokens: left=%zu right=%zu\n", iLeftMatched, iRightMatched);
    printf("Cosine similarity: %.5f\n", fSimilarity);
    return 0;
}

static void demo_run_builtin_examples(const w2vdemo_model *pModel)
{
    static const struct {
        const char *sName;
        const char *sLeft;
        const char *sRight;
    } kPairs[] = {
        {
            "related zh",
            "向量 检索 文本 切分 语义 索引",
            "语义 检索 向量 索引 记忆 知识"
        },
        {
            "unrelated zh",
            "向量 检索 文本 切分 语义 索引",
            "咖啡 早餐 厨房 杯子 烘焙 面包"
        },
        {
            "related en",
            "vector retrieval text chunk memory index",
            "semantic search vector index memory knowledge"
        },
        {
            "unrelated en",
            "vector retrieval text chunk memory index",
            "coffee breakfast kitchen mug bakery bread"
        }
    };
    size_t i;

    printf("Built-in word2vec demo pairs:\n\n");
    for ( i = 0u; i < sizeof(kPairs) / sizeof(kPairs[0]); ++i ) {
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
    const char *sModelPath = "build\\demo_model.bin";
    w2vdemo_model *pModel = NULL;
    int iResult = 0;

    if ( demo_run_training_if_needed() != 0 ) {
        return 1;
    }

    if ( w2vdemo_model_load(sModelPath, &pModel) != 0 ) {
        fprintf(stderr, "Load failed: %s\n", w2vdemo_last_error());
        return 1;
    }

    printf("Loaded model: %s\n", sModelPath);
    printf("Vocabulary size: %lld\n", w2vdemo_model_vocab_size(pModel));
    printf("Vector dimension: %d\n\n", w2vdemo_model_dimension(pModel));

    if ( argc >= 3 ) {
        iResult = demo_compare_and_print(pModel, argv[1], argv[2]);
    } else if ( argc == 2 ) {
        iResult = demo_embed_and_print(pModel, argv[1]);
    } else {
        demo_run_builtin_examples(pModel);
    }

    w2vdemo_model_destroy(pModel);
    return iResult;
}
