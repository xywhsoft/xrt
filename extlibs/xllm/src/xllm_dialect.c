#include "xllm_internal.h"

/* Dialect registry: providers resolve to a wire-dialect vtable. GLM rides
 * the completions dialect with provider tweak flags; Responses and Anthropic
 * register their own tables as they land. */

const xllm_dialect_ops* xllm__dialect_ops_for(xllm_provider eProvider)
{
    switch ( eProvider ) {
        case XLLM_PROVIDER_GLM:
        case XLLM_PROVIDER_OPENAI_COMPAT:
            return xllm__dialect_completions();
        case XLLM_PROVIDER_OPENAI_RESPONSES:
            return xllm__dialect_responses();
        case XLLM_PROVIDER_ANTHROPIC:
            return xllm__dialect_anthropic();
        default:
            return NULL;
    }
}
