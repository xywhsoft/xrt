# XRT Guide: Streaming LLM API Intro

> A guide-level overview of how the current XRT mainline can be used to organize streaming LLM API calls.

[中文](streaming-llm-api-intro.md) | [Back to Guides](README.en.md)

---

## Why This Matters

“Internet + AI” is not just a slogan for XRT.

A real LLM integration path usually needs:

- HTTP client
- TLS
- JSON request / response handling
- streaming output handling
- async orchestration

The current XRT mainline already provides the foundations for that chain.

---

## Recommended Mainline

Think about the chain this way:

- `xhttp` sends the request
- `xtlssession` provides transport security
- `xvalue + json` build and parse structured payloads
- `future / coroutine / wait-source` organize waiting and continuation

That is the recommended XRT path for streaming LLM integrations.

---

## What to Build Around

For this scenario, prioritize:

- `xvalue` as the internal structured data model
- JSON as the exchange format
- future / task / coroutine for orchestration
- HTTP streaming and response handling on top of the network mainline

This avoids splitting the program into multiple incompatible data and async models.

---

## Related Reading

- [XHTTP](../api/api-xhttp.en.md)
- [Network TLS](../api/api-network-tls.en.md)
- [JSON](../api/api-json.en.md)
- [Future / Task / Promise](../api/api-future-task-promise.en.md)
- [Case: Streaming LLM API](../case/streaming-llm-api.en.md)
