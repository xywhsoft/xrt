#ifndef XLLM_EMBED_BUILTIN_INTERNAL_H
#define XLLM_EMBED_BUILTIN_INTERNAL_H

static void xllm__memory_builtin_embedder_options_init(
    xllm_memory_builtin_embedder_options *pOptions
);

static void xllm__memory_builtin_embedder_probe_reset(
    xllm_memory_builtin_embedder_probe *pProbe
);

static int xllm__memory_probe_builtin_embedder(
    const xllm_memory_builtin_embedder_options *pOptions,
    xllm_memory_builtin_embedder_probe *pProbe,
    xllm_error *pError
);

static int xllm__memory_make_builtin_embedder(
    const xllm_memory_builtin_embedder_options *pOptions,
    xllm_memory_embedder *pEmbedder,
    xllm_error *pError
);

#endif
