# 发布包消费和验证

本案例展示下游项目怎样验证并消费 xllm release bundle，避免“仓库里能编译，发布包里不能用”的问题。

[返回范例解析](README.md) | [Release Gate 入门](../guide/release-gate-intro.md) | [Release API / 工具说明](../api/api-release.md)

## 问题

你拿到一个 xllm 发布包后，需要确认三件事：

- 这个包没有损坏或被意外修改。
- 头文件、源码和依赖都完整。
- 下游项目能在干净目录里编译一个最小程序。

这和在 xllm 仓库里跑测试不同。发布包消费验证要模拟真实使用者，只依赖 bundle 内提供的内容和公开文档。

## 角色和检查深度

| 角色 | 建议检查 |
| --- | --- |
| 普通使用者 | checksum + 最小编译 |
| 团队集成者 | artifact verify + downstream smoke |
| 发布维护者 | release gate + downstream smoke |

如果你只是学习 xllm，先做最小编译即可。如果你要把发布包交给其他项目使用，应跑完整 artifact 验证。

## 步骤 1：生成或取得发布包

如果你在 xllm 仓库内生成发布包：

```bat
cmd /c .\build.bat release-bundle -VerifyCompile
```

如果你是下载发布包，先把 zip 和 checksum 文件放到同一个目录，并记录版本号。

## 步骤 2：验证版本一致性

发布维护者应先运行：

```bat
cmd /c .\build.bat verify-version
```

它用于确认：

- `VERSION`
- `xllm.h` 中的版本宏
- `xllm_version()` 返回值
- `RELEASE_NOTES.md`

都指向同一个版本。

普通使用者可以在最小程序里打印：

```c
printf("xllm version: %s\n", xllm_version());
```

## 步骤 3：验证 Artifact

发布目录生成后运行：

```bat
cmd /c .\build.bat verify-artifact -RootDir build\release_bundle -VerifyCompile
```

这个检查关注：

- 发布目录结构是否完整。
- `release_metadata.json` 是否存在且版本正确。
- `SHA256SUMS.txt` 是否匹配。
- bundle 内 `BUNDLE_SHA256SUMS.txt` 是否匹配。
- 是否能用发布产物完成编译验证。

如果 checksum 不匹配，不要继续消费该包。先重新生成或重新下载。

## 步骤 4：解压到干净目录

下游验证应使用干净目录，避免误用仓库内文件。例如：

```powershell
New-Item -ItemType Directory -Force build\downstream-check | Out-Null
Expand-Archive build\release_bundle\xllm-windows.zip build\downstream-check -Force
```

当前文档中的 `xllm-windows.zip` 是发布包布局约定名；如果本地 release 脚本输出了带版本号的平台包名，例如 `xllm-0.1.0-windows.zip`，请以 `release_metadata.json` 和 `SHA256SUMS.txt` 中记录的文件名为准。关键是：编译时 include 和源码路径都来自解压目录。

## 步骤 5：编译最小程序

最小程序只做三件事：include xllm、打印版本、创建 runtime。

```c
#define XRT_IMPLEMENTATION
#define XLLM_IMPLEMENTATION
#include "xllm.h"

#include <stdio.h>

int main(void)
{
    xllm_runtime *pRuntime = NULL;

    xrtInit();
    printf("xllm version: %s\n", xllm_version());

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "runtime create failed\n");
        return 1;
    }

    xllm_runtime_destroy(pRuntime);
    return 0;
}
```

编译命令要按发布包目录调整。Windows/gcc 形式通常类似：

```bat
gcc -std=c11 -Wall -Wextra -Iinclude -Ilib ^
    app.c ^
    -o app.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32
```

如果你的发布包要求把 SQLite 源码一起编译，参考仓库示例中的 `lib\sqlite\sqlite3.c` 用法。

## 步骤 6：验证 Memory 可用

如果你的下游项目会使用 memory，应再编译一个 ingest/search 小程序：

```c
#define XRT_IMPLEMENTATION
#define XLLM_IMPLEMENTATION
#include "xllm-memory.h"

int main(void)
{
    xllm_runtime *pRuntime = NULL;
    xllm_memory *pMemory = NULL;
    xllm_memory_options tOptions;

    xrtInit();
    xllm_runtime_create(NULL, &pRuntime);

    xllm_memory_options_init(&tOptions);
    tOptions.sNamespace = "downstream-check";
    tOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;

    if ( xllm_memory_create(pRuntime, &tOptions, &pMemory) != XRT_NET_OK ) {
        return 1;
    }

    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    return 0;
}
```

这一步能发现发布包是否漏掉 `xllm-memory.h`、memory 实现文件或 SQLite 依赖。

## 步骤 7：运行 Downstream Smoke

仓库内提供下游 smoke：

```bat
cmd /c .\build.bat downstream-smoke
```

它的意义是模拟真实使用者。失败时优先检查：

- bundle 是否漏文件。
- include 路径是否和文档一致。
- implementation 宏是否正确。
- SQLite 或 xrt 依赖是否缺失。
- checksum 或 metadata 是否不一致。

## 步骤 8：运行 Release Gate

发布维护者在准备公开发布前运行：

```bat
cmd /c .\build.bat release-gate -RunDownstream
```

这个总闸应在所有发布说明、版本、bundle、校验和和 downstream smoke 都准备好后运行。

## 消费方目录建议

下游项目可以把发布包内容组织为：

```text
third_party/
  xllm/
    include/
    src/
    lib/
    release_metadata.json
```

然后在自己的构建系统里加入：

- xllm include 路径。
- xrt include 路径。
- SQLite include/source，如果使用 memory。
- Windows 网络和系统库链接项。

## 关键检查清单

1. 校验 zip 或 release 目录 hash。
2. 确认 `release_metadata.json` 版本正确。
3. 编译只含 `xllm.h` 的最小程序。
4. 如果使用 session，编译 `xllm-session.h`。
5. 如果使用 memory，编译 `xllm-memory.h` 并包含 SQLite 依赖。
6. 如果使用 provider，设置 API key 后运行最小聊天。
7. 记录失败时的 `xllm_version()`、编译命令、include 路径和错误输出。

## 常见问题

不要从 xllm 仓库路径偷偷 include 文件。下游验证必须只用发布包内容。

不要忽略 checksum 失败。hash 不匹配时继续使用会让后续问题无法定位。

不要只测 core。如果你的产品使用 memory 或 session，就要分别编译这些头文件路径。

不要把签名和 checksum 混为一谈。checksum 验证完整性，签名验证发布者身份。公开分发或自动更新时还需要签名策略。

## 下一步

- 想了解发布命令，读 [Release Gate 入门](../guide/release-gate-intro.md)。
- 想看 API 层说明，读 [Release API / 工具说明](../api/api-release.md)。
- 想验证最小聊天，读 [最小聊天调用](minimal-chat.md)。
