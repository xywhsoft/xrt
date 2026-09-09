# Tutorial code snippet maintenance instructions

>Status: First draft in Chinese, awaiting review.

This document records how to maintain the key C fragments in the tutorial to avoid the document fragments being disconnected from `xwork.h` and `examples/`.

## Fragment type

| Type | Marking method | Maintenance rules |
| --- | --- | --- |
| Compilable complete program | Links to `examples/*.c`, documentation for build commands | Must be verified with `examples/README.md` and CI example targets. |
| C calling fragment | Use fenced code block `c` | You must use the real type name, function name and field name in the current `xwork.h`. |
| Calling order | Use fenced code block `text` | Only express life cycle order, not declared as compilable C. |
| JSON contract | Use fenced code block `json` | Fields must come from host tool contract or current implementation. |

## Current source of truth

- Minimal compilable program: [`examples/first_xwork_program.c`](../../examples/first_xwork_program.c).
- Example build command: [`examples/README.md`](../../examples/README.md).
- Public C API: [`xwork.h`](../../xwork.h).
- host tool JSON contract:[`docs/api/api-host-tools.md`](../api/api-host-tools.md).
- Document review rule: [`docs/DOCS_REVIEW_CHECKLIST.md`](../DOCS_REVIEW_CHECKLIST.md).

## Update rules

- After modifying the `xwork.h` public API, check all `c` fragments.
- After modifying example, check the corresponding guide/case page and `examples/README.md`.
- When adding concept fragments that cannot be compiled, use `text` first instead of marking it as `c`.
- Running `tools/check_docs.ps1` locally verifies links, version constants, schema names, and built-in tool names.
