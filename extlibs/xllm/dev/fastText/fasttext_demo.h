#ifndef FASTTEXT_DEMO_H
#define FASTTEXT_DEMO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ftdemo_model ftdemo_model;

int ftdemo_train_default_model(
    const char *sInputPath,
    const char *sOutputPrefix
);

int ftdemo_model_load(
    const char *sModelPath,
    ftdemo_model **ppModel
);

void ftdemo_model_destroy(ftdemo_model *pModel);

int ftdemo_model_dimension(const ftdemo_model *pModel);

int ftdemo_model_embed_text(
    ftdemo_model *pModel,
    const char *sText,
    float *pOutVector,
    size_t iOutVectorCount
);

int ftdemo_model_compare_texts(
    ftdemo_model *pModel,
    const char *sLeftText,
    const char *sRightText,
    float *pfSimilarity
);

float ftdemo_cosine_similarity(
    const float *pLeft,
    const float *pRight,
    size_t iCount
);

const char *ftdemo_last_error(void);

#ifdef __cplusplus
}
#endif

#endif
