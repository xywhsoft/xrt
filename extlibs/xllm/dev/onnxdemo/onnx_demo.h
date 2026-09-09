#ifndef ONNX_DEMO_H
#define ONNX_DEMO_H

#include <stddef.h>

typedef struct onnx_demo onnx_demo;

int onnx_demo_create(onnx_demo **ppDemo, const char *pDllPath, const char *pModelPath, const char *pVocabPath);
void onnx_demo_destroy(onnx_demo *pDemo);

const char *onnx_demo_last_error(const onnx_demo *pDemo);
size_t onnx_demo_embedding_dim(const onnx_demo *pDemo);

int onnx_demo_embed_text(
    onnx_demo *pDemo,
    const char *pText,
    float **ppVector,
    size_t *puDim,
    size_t *puMatchedTokens,
    size_t *puTotalTokens);

void onnx_demo_free_vector(float *pVector);
float onnx_demo_cosine_similarity(const float *pLeft, const float *pRight, size_t uDim);

#endif
