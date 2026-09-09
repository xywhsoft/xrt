#ifndef XLLM_EMBED_INTERNAL_H
#define XLLM_EMBED_INTERNAL_H

static void xllm__memory_embedding_reset_with_embedder(
    const xllm_memory_embedder *pEmbedder,
    xllm_memory_embedding *pEmbedding
);

static int xllm__memory_embed_text(
    const xllm_memory_embedder *pEmbedder,
    xllm_memory_embed_task eTask,
    const char *sText,
    xllm_memory_embedding *pEmbedding,
    xllm_error *pError
);

static double xllm__memory_embedding_cosine_score(
    const float *pfLeft,
    const float *pfRight,
    uint32 uValueCount
);

#endif
