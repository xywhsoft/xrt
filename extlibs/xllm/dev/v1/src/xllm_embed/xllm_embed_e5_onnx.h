#ifndef XLLM_EMBED_E5_ONNX_INTERNAL_H
#define XLLM_EMBED_E5_ONNX_INTERNAL_H

static int xllm__memory_builtin_e5_create_ctx(
    const xllm_memory_builtin_embedder_options *pOptions,
    const xllm_memory_builtin_embedder_probe *pProbe,
    void **ppCtx,
    xllm_error *pError
);

static int xllm__memory_builtin_e5_clone_ctx(
    void *pCtx,
    void **ppClonedCtx,
    xllm_error *pError
);

static void xllm__memory_builtin_e5_dispose_ctx(void *pCtx);

static int xllm__memory_builtin_e5_embed_text(
    void *pCtx,
    xllm_memory_embed_task eTask,
    const char *sText,
    xllm_memory_embedding *pEmbedding,
    xllm_error *pError
);

#endif
