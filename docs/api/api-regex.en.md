# Regex Mainline

> The current regular-expression mainline integrated from `bbre`, aimed at lightweight, high-performance, embeddable text-matching scenarios.

[Back to Index](README.en.md)

---

## 1. Positioning

XRT's current regex module is built on [bbre](https://github.com/max-nurzia/bbre) and folded directly into the mainline.

It is suitable for:

- text matching
- substring search
- capture extraction
- multi-pattern matching
- rule checks over config, logs, HTTP text, and template fragments

The goal is not to build the heaviest “scripting-language regex ecosystem”, but to provide:

- a light footprint
- embeddability
- cross-platform use
- direct usefulness to XRT text and network application layers

---

## 2. Core Capability Areas

The current mainline provides:

- single-pattern compile and match
- capture extraction
- regex cloning for parallel use
- multi-pattern set matching

---

## 3. Official API

```c
XXAPI bbre* bbre_init_pattern(const char* pat_nt);
XXAPI int bbre_init(bbre** preg, const bbre_builder* build, const bbre_alloc* alloc);
XXAPI void bbre_destroy(bbre* reg);
XXAPI int bbre_is_match(bbre* reg, const char* text, size_t text_size);
XXAPI int bbre_find(bbre* reg, const char* text, size_t text_size, bbre_span* out_bounds);
XXAPI int bbre_captures(bbre* reg, const char* text, size_t text_size, bbre_span* out_captures, unsigned int out_captures_size);
XXAPI int bbre_clone(bbre** pout, const bbre* reg, const bbre_alloc* alloc);
XXAPI bbre_set* bbre_set_init_patterns(const char* const* ppats_nt, size_t num_pats);
XXAPI int bbre_set_matches(bbre_set* set, const char* text, size_t text_size, unsigned int* out_idxs, unsigned int out_idxs_size, unsigned int* out_num_idxs);
XXAPI void bbre_set_destroy(bbre_set* set);
```

---

## 4. Recommended Use

- use `bbre_init_pattern` for simple one-off rules
- use `bbre_builder` when you need explicit flags
- use `bbre_clone` when different threads or contexts should not share one regex object
- use `bbre_set` for blacklists, whitelists, and multi-rule scans

---

## 5. Relationship to Other Modules

- `string` handles construction, slicing, and formatting
- `regex` handles rule matching and extraction
- upper HTTP / WebSocket / template / log processing can use regex as a lightweight rule layer

---

## 6. Suggested Reading

- [String](api-string.en.md)
- [Template](api-template.en.md)
- [XHTTP](api-xhttp.en.md)
