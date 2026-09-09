# xllm Release API / 工具说明

> 状态：中文初稿已生成，待审阅。  
> 相关文件：`VERSION`、`xllm.h`、`build.bat`、`RELEASE_NOTES.md`

本页不是运行时 API 参考，而是面向使用者解释 xllm 发布包和发布验证工具。你会学到如何确认版本、如何理解 release bundle、如何验证校验和、以及 release gate 在保护什么。

## 版本信息

xllm 的版本来自两个地方：

```c
#define XLLM_VERSION_MAJOR 0
#define XLLM_VERSION_MINOR 1
#define XLLM_VERSION_PATCH 0
```

运行时可以调用：

```c
XLLM_API const char *xllm_version(void);
```

发布前要求 `VERSION` 文件、`xllm.h` 中的版本宏、`xllm_version()` 返回值和 `RELEASE_NOTES.md` 保持一致。

## 发布包包含什么

当前 Windows 发布包通常包含：

| 内容 | 用途 |
| --- | --- |
| `xllm-windows.zip` | 面向下游使用者的压缩包。 |
| `include/` | 对外头文件，例如 `xllm.h`、`xllm-session.h`、`xllm-memory.h`。 |
| `src/` | single-header / implementation 依赖的实现文件。 |
| `lib/` 或第三方依赖 | xrt、SQLite 等必要依赖。 |
| `BUNDLE_SHA256SUMS.txt` | bundle 内文件校验和。 |
| `SHA256SUMS.txt` | release 输出根目录校验和。 |
| `release_metadata.json` | 发布元数据。 |
| verify scripts | 验证包是否完整、可编译、校验和是否匹配。 |

## 常用发布命令

### verify-version

**功能**：检查版本声明是否一致。

```bat
cmd /c .\build.bat verify-version
```

如果你看到版本不一致，需要同步修改 `VERSION`、头文件版本宏和 release notes。

### singlehead

**功能**：验证 single-header / implementation 组合是否仍可用。

```bat
cmd /c .\build.bat singlehead
```

这个步骤能发现 include 顺序、implementation 宏和头文件拆分导致的问题。

### static-analysis

**功能**：运行静态检查和发布参数相关检查。

```bat
cmd /c .\build.bat static-analysis
```

### release-bundle

**功能**：生成发布包。

```bat
cmd /c .\build.bat release-bundle -VerifyCompile
```

`-VerifyCompile` 会在生成包后做编译验证，建议发布前始终开启。

### verify-artifact

**功能**：验证已经生成的发布产物。

```bat
cmd /c .\build.bat verify-artifact -RootDir build\release_bundle -VerifyCompile
```

它会检查校验和、元数据、包结构，并可验证编译。

### downstream-smoke

**功能**：像下游使用者一样解压发布包、编译最小程序并运行 memory ingest/search 流程。

```bat
cmd /c .\build.bat downstream-smoke
```

这个步骤能发现“仓库内能编译，但发布包缺文件”的问题。

### release-gate

**功能**：执行发布前总闸。

```bat
cmd /c .\build.bat release-gate -RunDownstream
```

`-RunDownstream` 会把下游 smoke 纳入最终发布流程。

## 校验和与签名

当前发布工具的基线是校验和验证：

- release 输出根目录生成 `SHA256SUMS.txt`。
- bundle 内生成 `BUNDLE_SHA256SUMS.txt`。
- verify 脚本检查文件内容是否匹配。

校验和能发现文件损坏或意外修改，但不能证明发布者身份。文档中的签名策略建议分阶段演进：

1. 本地工程发布继续使用 checksum-only gate。
2. 内部候选发布增加可选 detached signature。
3. Windows 分发时按需要加入 Authenticode。
4. IDE 或自动更新场景必须验证发布者签名和校验和。

## 发布说明分类

`RELEASE_NOTES.md` 建议使用以下分类：

| 分类 | 内容 |
| --- | --- |
| `core` | runtime、request/response、errors、stream、tools、结构化输出。 |
| `session` | history、compact、summary、state import/export、session bridge。 |
| `memory` | ingest/search/list/remove、typed memory、diagnostics、SQLite、watcher。 |
| `provider` | adapter 行为、能力矩阵、真实 provider 探测结果。 |
| `release` | bundle layout、artifact verification、downstream integration、versioning。 |
| `security` | 敏感默认值、无遥测保证、删除/导出治理、prompt injection 指南。 |
| `known gaps` | 已知限制。 |

## 发布前最小清单

1. 更新 `VERSION`、`xllm.h` 版本宏、`RELEASE_NOTES.md`。
2. 运行 `cmd /c .\build.bat verify-version`。
3. 运行 `cmd /c .\build.bat singlehead`。
4. 运行相关 smoke，并记录报告路径。
5. 运行 `cmd /c .\build.bat static-analysis`。
6. 运行 `cmd /c .\build.bat release-bundle -VerifyCompile`。
7. 运行 `cmd /c .\build.bat verify-artifact -RootDir build\release_bundle -VerifyCompile`。
8. 运行 `cmd /c .\build.bat downstream-smoke`。
9. 按发布渠道执行签名策略。
10. 时间允许时运行 `cmd /c .\build.bat release-gate -RunDownstream`。

## 学习者常见问题

### 我只想使用 xllm，需要运行 release gate 吗？

不需要。release gate 是维护者发布包时使用的。使用者只需要确认下载的 bundle 通过校验，并按教程编译即可。

### 为什么下游 smoke 很重要？

因为它模拟“别人拿到 zip 后如何使用”。它能发现 include 缺失、源码漏打包、SQLite 文件缺失、示例链接失败等问题。

### 现在发布包是否已经签名？

当前策略说明中，默认基线是 checksum 验证；签名是后续供应链加固阶段。公开分发或 IDE 自动更新前，应按签名策略补齐。

## 相关文档

- [Diagnostics API](api-diagnostics.md)
- [Core API](api-core.md)
- [返回 API 索引](README.md)
