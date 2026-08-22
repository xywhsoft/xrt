# XRT Case Study: Streaming LLM API

> A case-study view of how the current XRT mainline can support a streaming LLM request chain.

[中文](streaming-llm-api.md) | [Back to Case Studies](README.en.md)

---

## Mainline Chain

This case is about the full chain, not only “sending one HTTP request”.

The current recommended chain is:

- runtime
- `xhttp`
- `xtlssession`
- `xvalue + json`
- future / coroutine / wait-source

This is what makes the “Internet + AI” direction concrete in XRT.

---

## Why This Case Matters

A streaming LLM integration is a very good test for whether a library is really coherent.

It needs:

- transport
- TLS
- structured payloads
- async orchestration
- streaming result handling

The current XRT mainline is explicitly being shaped so these capabilities can cooperate instead of remaining disconnected modules.

---

## Related Reading

- [XHTTP](../api/api-xhttp.en.md)
- [Network TLS](../api/api-network-tls.en.md)
- [Future / Task / Promise](../api/api-future-task-promise.en.md)
- [Guide: Streaming LLM API Intro](../guide/streaming-llm-api-intro.en.md)
