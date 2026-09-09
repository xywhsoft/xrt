# xwork integration and packaging

>Status: First draft in Chinese, awaiting review.

This article explains xwork¡¯s current source-level integration methods, dependency snapshots, and update rules.

## Public Surface

| Path | Description |
| --- | --- |
| `xwork.h` | canonical public API header. |
| `xwork.c` | aggregate implementation translation unit. |
| `src/xwork_*/*.c` | Implements slicing internally, included by `xwork.c`. |
| `lib/` | Current source code build dependency snapshot. |
| `include/` | Reserved for future installed-header layout, not currently a canonical include path. |

Consumers should not compile both `xwork.c` and `src/xwork_*/*.c` unless explicitly replacing the aggregate model.

## Minimal source code level integration

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -c xwork.c
```

Single-header package:

```powershell
.\build_single_head.bat
```

This writes `singlehead/xwork.h`. The file contains the public API plus the
xwork implementation when `XWORK_IMPLEMENTATION` is defined. It intentionally
does not embed external dependencies; keep `xrt.h`, `xllm.h`,
`xllm-session.h`, `xllm-memory.h`, and sqlite available to the consumer build.

Minimal single-header smoke:

```c
#define XRT_IMPLEMENTATION
#include "xrt.h"
#define XLLM_SESSION_IMPLEMENTATION
#include "xllm-session.h"
#define XLLM_MEMORY_IMPLEMENTATION
#include "xllm-memory.h"
#define XWORK_IMPLEMENTATION
#include "xwork.h"
```

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -Ilib\sqlite tests\xwork_orchestrator_smoke.c lib\sqlite\sqlite3.c -o tests\xwork_orchestrator_smoke.exe -lws2_32 -liphlpapi
```

## Dependency snapshot

| Component | Version / Snapshot | Location | Notes |
| --- | --- | --- | --- |
| xwork | 0.1.0 | `xwork.h` | `XWORK_VERSION_MAJOR/MINOR/PATCH` |
| xllm | 0.1.0 | `lib/xllm.h`, `lib/xllm-session.h`, `lib/xllm-memory.h` |
| xrt | vendored snapshot | `lib/xrt.h` | Currently tracked by copy snapshot. |
| sqlite | vendored amalgamation | `lib/sqlite/sqlite3.c`, `lib/sqlite/sqlite3.h` | for file persistence tests/examples. |

## Version rules

public version£º

- `XWORK_VERSION_MAJOR`
- `XWORK_VERSION_MINOR`
- `XWORK_VERSION_PATCH`
- `xwork_version()`

The `0.x` stage is a source code compatible snapshot. Changes to public struct layout, enum value, or function signature may require minor bumps; patch bumps are reserved for compatibility fixes, documentation, testing, or internal behavior fixes.

Persistence format is tracked separately by `XWORK_PERSISTENCE_FORMAT_VERSION`.

Remote protocol is tracked separately by `XWORK_REMOTE_PROTOCOL_VERSION_CURRENT`.

## Update dependency rules

After refreshing `xrt` or `xllm` in `lib/`:

1. Copy dependency files to `lib/`.
2. Update the dependency snapshot information in this file.
3. Run the default smoke set.
4. Run `gcc -std=c11 -Wall -Wextra -pedantic -c xwork.c`.
5. Document behavioral impacts to `dev/CHANGELOG.md` or release notes.

## Related documents

- [API Reference Index](api/README.md)
- [Provider Smoke](guide/provider-smoke-intro.md)
- [Internal packaging notes](../dev/docs/PACKAGING.md)
- [Internal compatibility notes](../dev/docs/COMPATIBILITY.md)
