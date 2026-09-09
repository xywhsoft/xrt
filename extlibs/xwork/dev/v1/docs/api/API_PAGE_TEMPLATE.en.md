#Module name API

> Use one sentence to explain what problem this module solves.

[Back to API Index](README.md) | [Related Tutorials](../guide/README.md) | [Related Cases](../case/README.md)

---

## Table of contents

- [Module Positioning](#Module Positioning)
- [Constants and Macros](#Constants and Macros)
- [Public Types](#Public Types)
- [Standard Call Order](#Standard Call Order)
- [Function Group One](#Function Group One)
- [Common Usage](#Common Usage)
- [Common Mistakes](#Common Mistakes)
- [Related Examples](#Related Examples)

---

## Module positioning

Explain what problems this module solves, what problems it does not solve, and its boundaries with other modules such as runtime/workspace/run/tool/persistence.

---

## Constants and macros

| name | value | description | when to use |
| --- | --- | --- | --- |
| `NAME` | `value` | Description | Usage scenarios |

---

## Public type

### `type_name`

Indicates what this type represents and whether the caller needs to create, initialize, reset, or destroy it directly.

| Field/Value | Type/Value | Default Value | Description | Ownership |
| --- | --- | --- | --- | --- |
| `field` | `type` |

---

## Standard calling sequence

1. Initialize options or input objects.
2. Create or register resources.
3. Call the main API.
4. Read the results.
5. reset/free/destroy.

---

## Function group one

### `xwork_function_name`

Describe this function in one sentence.

**Function:**

Explain what this function does, in which scenarios it is suitable to be used, and what problems it is not suitable to solve.

**Function prototype:**

```c
XWORK_API xwork_status xwork_function_name(type *pArg);
```

**parameter:**

| Parameters | Direction | Whether it can be `NULL` | Description |
| --- | --- | --- | --- |
| `pArg` | Input/Output | No | Parameter meaning, lifetime, ownership, valid range, units and default values |

**Return value:**

- `XWORK_OK`: Success.
- `XWORK_ERROR_INVALID_ARGUMENT`: Invalid parameter.
- Other error codes are listed by function semantics.

**Resource ownership:**

- Indicates whether this function allocates resources.
- Specify which `reset` / `destroy` function the caller should use to clean up.
- Indicates whether the returned pointer is borrowed, owned, or managed by the runtime.

**Additional Note:**

- Calling order requirements.
- Thread safety and re-entry considerations.
- persistence/recovery boundaries.
- profile, workspace, host tool or replay differences.
- Compatibility and version notes.

**Example code:**

```c
#include "xwork.h"

int main(void)
{
    return 0;
}
```

**Related API:**

- `related_function`

---

## Common usage

Explain the 2-3 most common combinations of this module.

## Common mistakes

| Problem | Cause | Solution |
| --- | --- | --- |
| Error phenomena | Common causes | Correct practices |

## Related examples

- `examples\...`
