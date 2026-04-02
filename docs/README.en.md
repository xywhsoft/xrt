# XRT Documentation Hub

> Official entry pages for XRT users. Start with `guide/` when you need the learning path, switch to `api/` for exact contracts, and use `case/` for end-to-end composition examples.

[中文](README.md) | [Project Overview](../README.en.md)

---

## Quick Entry Points

- [Project Overview](../README.en.md)
- [API Index](api/README.en.md)
- [Guide Entry](guide/README.en.md)
- [Case Entry](case/README.en.md)
- [Examples](EXAMPLES.en.md)
- [Architecture](ARCHITECTURE.en.md)
- [Feature Trimming Macros](FEATURE_TRIMMING_MACROS.en.md)
- [Best Practices](BEST_PRACTICES.en.md)
- [FAQ](FAQ.en.md)
- [Migration Notes](MIGRATION.en.md)
- [Performance Notes](PERFORMANCE.en.md)


## Read by Goal

### First Time with XRT

1. [Project Overview](../README.en.md)
2. [Architecture](ARCHITECTURE.en.md)
3. [Guide Entry](guide/README.en.md)
4. [First XRT Program](guide/first-xrt-program.en.md)
5. [Case Entry](case/README.en.md)

### Ready to Write Code

1. [API Index](api/README.en.md)
2. [Base Runtime](api/api-base.en.md)
3. [Value Dynamic Type System](api/api-value.en.md)
4. [JSON Processing](api/api-json.en.md)
5. [Thread Runtime](api/api-thread.en.md)
6. [Queue](api/api-queue.en.md)
7. [Coroutine Runtime](api/api-coroutine.en.md)
8. [Future / Task / Promise](api/api-future-task-promise.en.md)
9. [XURL](api/api-xurl.en.md)
10. [HTTP Util](api/api-http-util.en.md)
11. [XNet V2](api/api-xnet-v2.en.md)
12. [A Local Console Service that Combines Config, Logging, Tasks, Networking, and Templates](case/local-console-service.en.md)

### Want a Runnable Sample First

1. [Examples](EXAMPLES.en.md)
2. [First XRT Program](guide/first-xrt-program.en.md)
3. [Minimal HTTP Service with XRT](case/minimal-http-service.en.md)


## Documentation Areas

- `api/`
	Exact function signatures, types, return values, and module boundaries.
- `guide/`
	When to use a module, why the mainline is shaped this way, and how to move from a tiny program to a working service.
- `case/`
	How multiple modules fit together to solve one complete problem.


## Common Starting Topics

- Runtime and structured data
	- [Base Runtime](api/api-base.en.md)
	- [xvalue and JSON Intro](guide/xvalue-json-intro.en.md)
- Concurrency and async
	- [Multitasking Overview](guide/multitask-overview.en.md)
	- [Queue Intro](guide/queue-intro.en.md)
	- [Coroutine, Future, and Task Intro](guide/coroutine-future-task-intro.en.md)
- Networking and services
	- [XURL Intro](guide/xurl-intro.en.md)
	- [HTTP Util Intro](guide/http-util-intro.en.md)
	- [XWS Intro](guide/xws-intro.en.md)
- Full applications
	- [Full HTTP + JSON + Template Service Chain](case/http-json-template-chain.en.md)
	- [A Local Console Service that Combines Config, Logging, Tasks, Networking, and Templates](case/local-console-service.en.md)


## Usage Advice

- Read `guide/` first when you are still deciding which model or module to use.
- Jump to `api/` when you already know the module and need exact details.
- Use `case/` when you need a full working composition instead of isolated snippets.
- When one page is Chinese-only, the English entry pages link to it directly so you can still follow the practical reading path.

