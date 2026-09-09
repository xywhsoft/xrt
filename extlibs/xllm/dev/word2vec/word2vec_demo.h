#ifndef WORD2VEC_DEMO_H
#define WORD2VEC_DEMO_H

#include <stddef.h>

typedef struct w2vdemo_model w2vdemo_model;

int w2vdemo_model_load(const char *sModelPath, w2vdemo_model **ppModel);
void w2vdemo_model_destroy(w2vdemo_model *pModel);

int w2vdemo_model_dimension(const w2vdemo_model *pModel);
long long w2vdemo_model_vocab_size(const w2vdemo_model *pModel);

int w2vdemo_model_embed_text(
    const w2vdemo_model *pModel,
    const char *sText,
    float *pOutVector,
    size_t iOutVectorCount,
    size_t *piTokenCount,
    size_t *piMatchedTokenCount
);

int w2vdemo_model_compare_texts(
    const w2vdemo_model *pModel,
    const char *sLeftText,
    const char *sRightText,
    float *pfSimilarity,
    size_t *piLeftMatchedTokenCount,
    size_t *piRightMatchedTokenCount
);

float w2vdemo_cosine_similarity(
    const float *pLeft,
    const float *pRight,
    size_t iCount
);

const char *w2vdemo_last_error(void);

#endif
