# xwork 集成与打包

> 状态：中文初稿，待审阅。

本文说明 xwork 当前的源码级集成方式、依赖快照和更新规则。

## Public Surface

| 路径 | 说明 |
| --- | --- |
| `xwork.h` | canonical public API header。 |
| `xwork.c` | aggregate implementation translation unit。 |
| `src/xwork_*/*.c` | 内部实现切片，由 `xwork.c` include。 |
| `lib/` | 当前源码构建依赖快照。 |
| `include/` | 预留未来 installed-header layout，目前不是 canonical include path。 |

消费者不应同时编译 `xwork.c` 和 `src/xwork_*/*.c`，除非明确要替换聚合模型。

## 最小源码级集成

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -c xwork.c
```

需要 sqlite-backed file persistence 的示例或测试通常额外编译 sqlite amalgamation：

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -Ilib\sqlite tests\xwork_orchestrator_smoke.c lib\sqlite\sqlite3.c -o tests\xwork_orchestrator_smoke.exe -lws2_32 -liphlpapi
```

## 依赖快照

| Component | Version / Snapshot | Location | Notes |
| --- | --- | --- | --- |
| xwork | 0.1.0 | `xwork.h` | `XWORK_VERSION_MAJOR/MINOR/PATCH` |
| xllm | 0.1.0 | `lib/xllm.h`, `lib/xllm-session.h`, `lib/xllm-memory.h` | `XLLM_VERSION_MAJOR/MINOR/PATCH` |
| xrt | vendored snapshot | `lib/xrt.h` | 当前按复制快照跟踪。 |
| sqlite | vendored amalgamation | `lib/sqlite/sqlite3.c`, `lib/sqlite/sqlite3.h` | 用于 file persistence tests/examples。 |

## 版本规则

public version：

- `XWORK_VERSION_MAJOR`
- `XWORK_VERSION_MINOR`
- `XWORK_VERSION_PATCH`
- `xwork_version()`

`0.x` 阶段是源码兼容快照。public struct layout、enum value 或 function signature 变化可能需要 minor bump；patch bump 保留给兼容修复、文档、测试或内部行为修正。

Persistence format 由 `XWORK_PERSISTENCE_FORMAT_VERSION` 单独跟踪。

Remote protocol 由 `XWORK_REMOTE_PROTOCOL_VERSION_CURRENT` 单独跟踪。

## 更新依赖规则

刷新 `lib/` 中的 `xrt` 或 `xllm` 后：

1. 复制 dependency files 到 `lib/`。
2. 更新本文件中的依赖快照信息。
3. 运行默认 smoke set。
4. 运行 `gcc -std=c11 -Wall -Wextra -pedantic -c xwork.c`。
5. 将行为影响记录到 `dev/CHANGELOG.md` 或 release notes。

## 相关文档

- [API 文档索引](api/README.md)
- [Provider Smoke](guide/provider-smoke-intro.md)
- [内部 packaging 记录](../dev/docs/PACKAGING.md)
- [内部 compatibility 记录](../dev/docs/COMPATIBILITY.md)
