# Module Name API

> Explain in one sentence what problem this module solves.

[Back to API Index](README.en.md) | [Related Tutorials](../guide/README.en.md) | [Related Cases](../case/README.en.md)

---

## Table of Contents

- [Constants and Macros](#constants-and-macros)
- [Public Types](#public-types)
- [Lifecycle](#lifecycle)
- [Feature Group One](#feature-group-one)
- [Common Usage](#common-usage)
- [Common Mistakes](#common-mistakes)
- [Related Examples](#related-examples)

---

## Constants and Macros

| Name | Value | Description | When to Use |
| --- | --- | --- | --- |
| `NAME` | `value` | Description | Usage scenario |

## Public Types

### `type_name`

Explain what this type represents, and whether callers need to create or free it directly.

| Field / Value | Type / Value | Default | Description | Ownership |
| --- | --- | --- | --- | --- |
| `field` | `type` | `default` | Field meaning | borrowed / owned / caller-owned |

---

## Lifecycle

Describe the standard call order for this module in a few steps:

1. Initialize options or input objects.
2. Create or register resources.
3. Call the main API.
4. Read the result.
5. Reset/free/destroy.

---

## Feature Group One

### `function_name`

Explain this function in one sentence.

**Purpose:**

Explain what this function does, which scenarios it fits, and which problems it does not solve.

**Prototype:**

```c
XLLM_API int function_name(type *pArg, xllm_error *pError);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pArg` | input / output | no | Meaning, lifecycle, ownership, valid range |
| `pError` | output | optional | Receives error details on failure; callers clean internal strings with `xllm_error_free` |

**Return Value:**

- `0`: success.
- Non-`0`: failure; if `pError` is provided, error code and message are written into `pError`.

**Resource Ownership:**

- Explain whether this function allocates resources.
- Explain which reset/free/destroy function callers should use.
- Explain whether returned pointers are borrowed or caller-owned.

**Notes:**

- Call-order requirements.
- Thread-safety and reentrancy notes.
- Provider / scheme / profile differences.
- Compatibility and version notes.

**Example Code:**

```c
#include "xllm.h"

int main(void) {
    return 0;
}
```

**Related APIs:**

- `related_function`

---

## Common Usage

Explain the 2-3 most common combinations for this module.

## Common Mistakes

| Problem | Cause | Fix |
| --- | --- | --- |
| Symptom | Common cause | Correct approach |

## Related Examples

- `examples\...`
- `build.bat smoke -Filter "..."`
