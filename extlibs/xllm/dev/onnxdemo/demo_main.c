#define _CRT_SECURE_NO_WARNINGS

#include "onnx_demo.h"

#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct demo_case {
    const char *pName;
    const char *pLeft;
    const char *pRight;
} demo_case;

static int
file_exists(const char *pPath)
{
    DWORD dwAttributes = GetFileAttributesA(pPath);
    return dwAttributes != INVALID_FILE_ATTRIBUTES &&
           (dwAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static void
print_vector_preview(const float *pVector, size_t uDim)
{
    size_t i;
    size_t uPreview = uDim < 6 ? uDim : 6;

    printf("vector preview:");
    for (i = 0; i < uPreview; ++i) {
        printf(" %.4f", pVector[i]);
    }
    if (uDim > uPreview) {
        printf(" ...");
    }
    printf("\n");
}

int
main(void)
{
    const char *pDllPath = "build\\onnxruntime.dll";
    const char *pOrtModelPath = "build\\model\\ort\\toy_text_embedder.ort";
    const char *pOnnxModelPath = "build\\model\\toy_text_embedder.onnx";
    const char *pVocabPath = "build\\model\\vocab.txt";
    const char *pModelPath = file_exists(pOrtModelPath) ? pOrtModelPath : pOnnxModelPath;
    onnx_demo *pDemo = NULL;
    size_t i;
    demo_case aCases[] = {
        {
            "zh related",
            "向量检索帮助长期记忆找到相关片段",
            "本地记忆系统使用向量索引召回相关知识"
        },
        {
            "zh unrelated",
            "向量检索帮助长期记忆找到相关片段",
            "烘焙面包需要面粉酵母和发酵时间"
        },
        {
            "en related",
            "local memory uses vector search for related notes",
            "semantic retrieval looks up relevant notes from a local knowledge base"
        },
        {
            "en unrelated",
            "local memory uses vector search for related notes",
            "pasta with tomato garlic olive oil tastes bright"
        }
    };

    printf("runtime dll : %s\n", pDllPath);
    printf("model path  : %s\n", pModelPath);
    printf("vocab path  : %s\n\n", pVocabPath);

    if (onnx_demo_create(&pDemo, pDllPath, pModelPath, pVocabPath) != 0) {
        fprintf(stderr, "failed to initialize onnx demo: %s\n", onnx_demo_last_error(pDemo));
        onnx_demo_destroy(pDemo);
        return 1;
    }

    printf("embedding dim: %zu\n\n", onnx_demo_embedding_dim(pDemo));

    for (i = 0; i < sizeof(aCases) / sizeof(aCases[0]); ++i) {
        float *pLeftVector = NULL;
        float *pRightVector = NULL;
        size_t uLeftDim = 0;
        size_t uRightDim = 0;
        size_t uLeftMatched = 0;
        size_t uRightMatched = 0;
        size_t uLeftTotal = 0;
        size_t uRightTotal = 0;
        float fSimilarity = 0.0f;

        if (onnx_demo_embed_text(
                pDemo,
                aCases[i].pLeft,
                &pLeftVector,
                &uLeftDim,
                &uLeftMatched,
                &uLeftTotal) != 0) {
            fprintf(stderr, "left embed failed: %s\n", onnx_demo_last_error(pDemo));
            onnx_demo_destroy(pDemo);
            return 1;
        }

        if (onnx_demo_embed_text(
                pDemo,
                aCases[i].pRight,
                &pRightVector,
                &uRightDim,
                &uRightMatched,
                &uRightTotal) != 0) {
            fprintf(stderr, "right embed failed: %s\n", onnx_demo_last_error(pDemo));
            onnx_demo_free_vector(pLeftVector);
            onnx_demo_destroy(pDemo);
            return 1;
        }

        fSimilarity = onnx_demo_cosine_similarity(
            pLeftVector,
            pRightVector,
            uLeftDim < uRightDim ? uLeftDim : uRightDim);

        printf("[%s]\n", aCases[i].pName);
        printf("left matched tokens : %zu / %zu\n", uLeftMatched, uLeftTotal);
        printf("right matched tokens: %zu / %zu\n", uRightMatched, uRightTotal);
        printf("cosine similarity   : %.5f\n", fSimilarity);
        print_vector_preview(pLeftVector, uLeftDim);
        printf("\n");

        onnx_demo_free_vector(pLeftVector);
        onnx_demo_free_vector(pRightVector);
    }

    onnx_demo_destroy(pDemo);
    return 0;
}
