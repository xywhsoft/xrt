#ifndef XLLM_TEST_PROVIDER_GOLDEN_H
#define XLLM_TEST_PROVIDER_GOLDEN_H

typedef struct xllm_test_provider_golden {
    const char* sName;
    xllm_provider eProvider;
    const char* sDialect;
    const char* sPathSuffix;
    const char* sRequestJson;
    const char* sResponseJson;
} xllm_test_provider_golden;

static const xllm_test_provider_golden XLLM_TEST_PROVIDER_GOLDENS[] = {
    {
        "openai-compatible",
        XLLM_PROVIDER_OPENAI_COMPAT,
        "completions",
        "/chat/completions",
        "{\"model\":\"golden-model\",\"messages\":[{\"role\":\"user\",\"content\":\"ping\"}],"
            "\"store\":false,\"max_tokens\":32,\"reasoning_effort\":\"max\"}",
        "{\"id\":\"chat-1\",\"model\":\"golden-model\",\"choices\":[{\"index\":0,"
            "\"message\":{\"role\":\"assistant\",\"content\":\"pong\"},\"finish_reason\":\"stop\"}],"
            "\"usage\":{\"prompt_tokens\":2,\"completion_tokens\":1,\"total_tokens\":3}}"
    },
    {
        "glm-completions",
        XLLM_PROVIDER_GLM,
        "completions",
        "/chat/completions",
        "{\"model\":\"golden-model\",\"messages\":[{\"role\":\"user\",\"content\":\"ping\"}],"
            "\"store\":false,\"max_tokens\":32,\"thinking\":{\"type\":\"enabled\","
            "\"clear_thinking\":false}}",
        "{\"id\":\"chat-1\",\"model\":\"golden-model\",\"choices\":[{\"index\":0,"
            "\"message\":{\"role\":\"assistant\",\"content\":\"pong\"},\"finish_reason\":\"stop\"}],"
            "\"usage\":{\"prompt_tokens\":2,\"completion_tokens\":1,\"total_tokens\":3}}"
    },
    {
        "openai-responses",
        XLLM_PROVIDER_OPENAI_RESPONSES,
        "responses",
        "/responses",
        "{\"model\":\"golden-model\",\"store\":false,\"max_output_tokens\":32,"
            "\"reasoning\":{\"effort\":\"max\"},\"input\":["
            "{\"type\":\"message\",\"role\":\"user\",\"content\":\"ping\"}]}",
        "{\"id\":\"resp-1\",\"model\":\"golden-model\",\"status\":\"completed\",\"output\":["
            "{\"type\":\"message\",\"role\":\"assistant\",\"content\":[{\"type\":\"output_text\","
            "\"text\":\"pong\"}]}],\"usage\":{\"input_tokens\":2,\"output_tokens\":1,\"total_tokens\":3}}"
    },
    {
        "anthropic-messages",
        XLLM_PROVIDER_ANTHROPIC,
        "anthropic",
        "/v1/messages",
        "{\"model\":\"golden-model\",\"max_tokens\":32,\"messages\":[{\"role\":\"user\","
            "\"content\":\"ping\"}]}",
        "{\"id\":\"msg-1\",\"model\":\"golden-model\",\"type\":\"message\",\"role\":\"assistant\","
            "\"content\":[{\"type\":\"text\",\"text\":\"pong\"}],\"stop_reason\":\"end_turn\","
            "\"usage\":{\"input_tokens\":2,\"output_tokens\":1}}"
    }
};

typedef struct xllm_test_stream_golden {
    const char* sName;
    xllm_provider eProvider;
    const char* sWire;
    const char* sContent;
} xllm_test_stream_golden;

/* Small deterministic corpus: it intentionally mixes LF/CRLF, comments,
 * empty events, a multi-byte UTF-8 scalar, and a final line without '\n'. */
static const xllm_test_stream_golden XLLM_TEST_STREAM_GOLDENS[] = {
    {
        "completions-stream",
        XLLM_PROVIDER_OPENAI_COMPAT,
        ": keep-alive\r\n\r\n"
        "data: {\"choices\":[{\"index\":0,\"delta\":{\"content\":\"p\xE7\xAD\x94ng\"},"
            "\"finish_reason\":\"stop\"}],\"usage\":{\"prompt_tokens\":2,\"completion_tokens\":1,"
            "\"total_tokens\":3}}\r\n\r\n"
        "data: [DONE]",
        "p\xE7\xAD\x94ng"
    },
    {
        "responses-stream",
        XLLM_PROVIDER_OPENAI_RESPONSES,
        "\n"
        "data: {\"type\":\"response.output_text.delta\",\"delta\":\"p\xE7\xAD\x94ng\"}\n\n"
        "data: {\"type\":\"response.completed\",\"response\":{\"usage\":{\"input_tokens\":2,"
            "\"output_tokens\":1,\"total_tokens\":3}}}\n\n",
        "p\xE7\xAD\x94ng"
    },
    {
        "anthropic-stream",
        XLLM_PROVIDER_ANTHROPIC,
        "event: content_block_delta\r\n"
        "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\","
            "\"text\":\"p\xE7\xAD\x94ng\"}}\r\n\r\n"
        "event: message_delta\r\n"
        "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\"},"
            "\"usage\":{\"input_tokens\":2,\"output_tokens\":1}}\r\n\r\n"
        "event: message_stop\r\n"
        "data: {\"type\":\"message_stop\"}",
        "p\xE7\xAD\x94ng"
    }
};

#endif
