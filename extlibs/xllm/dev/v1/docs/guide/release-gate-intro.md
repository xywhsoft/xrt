# Release Gate 入门

Release gate 是发布前的总检查。它确认版本、single-header、静态检查、发布包结构、校验和和下游 smoke 都能通过。

[返回教程入口](README.md) | [Release API / 工具说明](../api/api-release.md) | [Diagnostics 入门](diagnostics-intro.md)

## 你会学到什么

本文面向要消费或验证 xllm 发布包的学习者，解释：

- release gate 保护什么。
- 常用发布验证命令怎么运行。
- 如何理解 release bundle。
- 如何验证 checksum、metadata 和编译检查。
- 下游使用者遇到问题时应先看什么。

## 谁需要运行 Release Gate

如果你只是把 xllm 作为依赖使用，通常不需要运行完整 release gate。你只需要确认下载的发布包完整，并能编译你的最小程序。

如果你要发布 xllm，或者要把 xllm 发布包交给团队其他项目使用，就应该运行 release gate。

| 身份 | 建议 |
| --- | --- |
| 普通使用者 | 验证 checksum，编译最小示例 |
| 集成负责人 | 运行 artifact 验证和 downstream smoke |
| 发布维护者 | 运行完整 release gate |

## Release Gate 检查什么

完整发布前至少要覆盖：

| 检查 | 目的 |
| --- | --- |
| 版本一致性 | `VERSION`、头文件宏、release notes、运行时版本一致 |
| single-header 验证 | 头文件和 implementation include 模式可用 |
| 静态检查 | 基础质量、发布参数、脚本检查 |
| release bundle | 产物结构完整 |
| 校验和 | 文件未损坏、未意外修改 |
| 编译验证 | 发布包在下游环境可编译 |
| downstream smoke | 像真实使用者一样消费发布包 |

## 常用命令

所有命令都从仓库根目录运行。

### 验证版本

```bat
cmd /c .\build.bat verify-version
```

如果失败，先同步这些位置：

- `VERSION`
- `xllm.h` 中的 `XLLM_VERSION_MAJOR/MINOR/PATCH`
- `xllm_version()` 返回值
- `RELEASE_NOTES.md`

### 验证 single-header

```bat
cmd /c .\build.bat singlehead
```

这个检查能发现头文件 include 顺序、`XLLM_IMPLEMENTATION`、`XLLM_SESSION_IMPLEMENTATION`、`XLLM_MEMORY_IMPLEMENTATION` 组合问题。

### 静态检查

```bat
cmd /c .\build.bat static-analysis
```

它用于发布前质量检查。失败时先看脚本输出的具体检查项，不要直接跳过。

### 生成发布包

```bat
cmd /c .\build.bat release-bundle -VerifyCompile
```

`-VerifyCompile` 会在生成包后执行编译验证。发布前建议始终开启。

### 验证发布产物

```bat
cmd /c .\build.bat verify-artifact -RootDir build\release_bundle -VerifyCompile
```

它会检查发布目录、metadata、checksum，并可再次编译验证。

### 下游 smoke

```bat
cmd /c .\build.bat downstream-smoke
```

这个步骤模拟“用户拿到 zip 后如何使用”。它能发现仓库内编译没问题但发布包缺文件的问题。

### 总闸

```bat
cmd /c .\build.bat release-gate -RunDownstream
```

`-RunDownstream` 表示把下游 smoke 纳入总闸。准备真正发布时应使用它。

## Release Bundle 怎么看

发布包通常包含：

| 内容 | 用途 |
| --- | --- |
| `include/` | 对外头文件 |
| `src/` | implementation 所需源码 |
| `lib/` 或依赖目录 | xrt、SQLite 等必要依赖 |
| `release_metadata.json` | 版本、构建时间、文件布局等元数据 |
| `SHA256SUMS.txt` | 发布目录校验和 |
| `BUNDLE_SHA256SUMS.txt` | bundle 内文件校验和 |
| 示例或 smoke | 下游编译验证材料 |

学习者拿到发布包后，先看 include 是否完整，再看 checksum 是否通过，最后编译最小程序。

## 校验和验证

Checksum 能发现文件损坏或意外修改。它不能证明发布者身份，但它是 release gate 的基础。

验证时关注：

- `SHA256SUMS.txt` 是否覆盖发布目录中的关键文件。
- `BUNDLE_SHA256SUMS.txt` 是否覆盖 zip 内关键文件。
- verify 脚本是否报告缺失、额外或 hash 不匹配的文件。

如果 checksum 失败，不要继续使用该包。先重新生成或重新下载发布产物。

## Metadata 验证

`release_metadata.json` 应能回答：

- 这个包的 xllm 版本是什么。
- 包含哪些头文件和源码。
- 生成时间和目标平台是什么。
- 校验和文件在哪里。
- 是否执行过 compile verify。

如果 metadata 版本和头文件版本不一致，说明发布流程没有正确同步版本。

## 下游编译验证

下游验证的意义是：不依赖仓库内部路径，只使用发布包提供的文件完成编译和运行。

一个合格的 downstream smoke 应至少覆盖：

- include `xllm.h` 并打印 `xllm_version()`。
- 创建 `xllm_runtime`。
- 编译 session 或 memory 相关头文件。
- 运行一个 memory ingest/search 的小流程。

如果 downstream smoke 失败，优先怀疑：

- 发布包漏掉头文件或源码。
- include 路径和文档不一致。
- xrt 或 SQLite 依赖没有正确打包。
- single-header implementation 宏组合没有被测试。

## 使用者最小验证

如果你只是消费发布包，可以做一个更轻量的验证：

1. 校验下载文件的 hash。
2. 解压到干净目录。
3. 编译一个只调用 `xllm_version()` 和 `xllm_runtime_create` 的程序。
4. 如果要用 memory，再运行一次 ingest/search。
5. 如果要用 provider，再用你的 API key 跑一个最小请求。

这样能快速区分“包本身有问题”和“你的业务集成有问题”。

## 常见错误

不要只在仓库内编译成功就发布。发布包可能漏文件，必须做 artifact 和 downstream 验证。

不要忽略版本不一致。版本错会让使用者无法判断 bug 属于哪个发布。

不要把 checksum 当作签名。公开分发或自动更新场景还需要发布者签名策略。

不要把 release gate 失败当作脚本噪声。总闸失败通常代表发布包或文档承诺存在真实风险。

## 下一步

- 想看每个发布命令的说明，读 [Release API / 工具说明](../api/api-release.md)。
- 想排查失败输出，读 [Diagnostics 入门](diagnostics-intro.md)。
- 想学习第一个程序，读 [第一个 xllm 程序](first-xllm-program.md)。
