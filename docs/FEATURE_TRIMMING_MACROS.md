# XRT Feature Trimming Macros

> 当前多文件源码主线的裁剪宏说明。本文档只描述当前仍然有效、且已经对照 `xrt.h / xrt.c` 校准过的裁剪面。

[English](FEATURE_TRIMMING_MACROS.en.md) | [返回文档中心](README.md)

---

## 1. 适用范围

本文档描述的是当前正式源码主线里的功能裁剪规则：

- 以多文件版本 `xrt.h + xrt.c + lib/*` 为准
- 单头文件分发版应使用同一组宏；单头文件重新生成后会自然同步
- “支持裁剪”指两件事同时成立：
	- 公共声明会被隐藏
	- 对应实现不会继续被编译进来

本轮已经重点核对并补齐了新增模块的裁剪闭环：

- `subprocess`
- `file_async`
- `xson`
- `queue_wait` 的文档缺口


## 2. 当前有效裁剪宏

### 2.1 运行时与基础能力

```c
#define XRT_NO_TIME
#define XRT_NO_FILE
#define XRT_NO_FILE_ASYNC
#define XRT_NO_THREAD
#define XRT_NO_QUEUE
#define XRT_NO_QUEUE_WAIT
#define XRT_NO_COROUTINE
#define XRT_NO_CRYPTO
#define XRT_NO_XID
#define XRT_NO_BUFFER
#define XRT_NO_REGEX
#define XRT_NO_SUBPROCESS
```

说明：

- `XRT_NO_FILE_ASYNC` 现在是独立裁剪面，不再只是隐式跟随别的模块
- `XRT_NO_QUEUE_WAIT` 是真实存在的裁剪宏，本轮已补进说明文档
- `XRT_NO_SUBPROCESS` 现在会同时隐藏公共 API，并阻止 `lib/subprocess.h` 实现进入编译


### 2.2 网络主线

```c
#define XRT_NO_NETWORK
#define XRT_NO_NETTLS
#define XRT_NO_XURL
#define XRT_NO_HTTP_UTIL
#define XRT_NO_XCODEC
#define XRT_NO_XHTTP
#define XRT_NO_XHTTPD
#define XRT_NO_XWS
```

说明：

- `XRT_NO_NETWORK` 是网络根裁剪面
- `xnet_proxy` 当前仍跟随网络根模块一起裁剪，不是独立裁剪面


### 2.3 内存与容器

```c
#define XRT_NO_ARRAY
#define XRT_NO_BSMN
#define XRT_NO_MEMUNIT
#define XRT_NO_MEMPOOL_FS
#define XRT_NO_STACK
#define XRT_NO_AVLTREE
#define XRT_NO_MEMPOOL
#define XRT_NO_DICT
#define XRT_NO_LIST
```


### 2.4 值类型、文本与结构化数据

```c
#define XRT_NO_VALUE
#define XRT_NO_JNUM
#define XRT_NO_JSON
#define XRT_NO_XSON
#define XRT_NO_TEMPLATE
```

说明：

- `XRT_NO_XSON` 现在是独立裁剪面
- `XSON` 仍然依赖 `JSON`，但不再只靠“跟着 JSON 一起消失”这种隐式行为


## 3. 自动收口规则

当前源码里，下面这些宏会自动带出子裁剪面：

### 3.1 `XRT_MINIMAL`

```c
#define XRT_MINIMAL
```

当前会直接带出：

```c
XRT_NO_TIME
XRT_NO_FILE
XRT_NO_FILE_ASYNC
XRT_NO_THREAD
XRT_NO_QUEUE
XRT_NO_COROUTINE
XRT_NO_NETWORK
XRT_NO_CRYPTO
XRT_NO_NETTLS
XRT_NO_XID
XRT_NO_BUFFER
XRT_NO_ARRAY
XRT_NO_BSMN
XRT_NO_MEMUNIT
XRT_NO_MEMPOOL_FS
XRT_NO_STACK
XRT_NO_AVLTREE
XRT_NO_MEMPOOL
XRT_NO_DICT
XRT_NO_LIST
XRT_NO_VALUE
XRT_NO_JNUM
XRT_NO_JSON
XRT_NO_XSON
XRT_NO_TEMPLATE
XRT_NO_REGEX
XRT_NO_SUBPROCESS
```


### 3.2 自动派生关系

```c
XRT_NO_NETWORK    -> XRT_NO_FILE_ASYNC, XRT_NO_XURL, XRT_NO_HTTP_UTIL, XRT_NO_XCODEC, XRT_NO_XHTTP, XRT_NO_XHTTPD, XRT_NO_XWS
XRT_NO_QUEUE      -> XRT_NO_QUEUE_WAIT
XRT_NO_FILE       -> XRT_NO_FILE_ASYNC
XRT_NO_JSON       -> XRT_NO_XSON
```

这几条里最重要的是：

- `file_async` 同时依赖 `FILE` 和 `NETWORK`
- `xson` 在架构上依赖 `JSON`


## 4. 当前仍按父模块裁剪的子能力

下面这些能力当前还没有单独开放新的裁剪宏，而是继续跟随父模块：

| 能力 | 当前裁剪方式 | 说明 |
|------|--------------|------|
| `ptrarray` | `XRT_NO_ARRAY` | 仍属于数组族的一部分 |
| `dynstack` | `XRT_NO_STACK` | 仍属于栈族的一部分 |
| `xnet_proxy` | `XRT_NO_NETWORK` | 仍深度依附网络主线 |

这不代表以后永远不能拆开，只是当前源码主线还没有把它们整理成稳定的独立裁剪面。


## 5. 本轮核对结论

这次对源码做的结论可以直接概括成下面几条：

- `subprocess` 之前缺少独立裁剪面，现在已经补齐
- `file_async` 之前缺少明确的独立裁剪面，现在已经补齐
- `xson` 之前只有“随 JSON 一起消失”的隐式效果，现在已经补齐独立裁剪面
- `queue_wait` 宏本来就在代码里，但之前文档漏写了

已用下面这些配置对 `xrt.c` 做过裁剪编译验证：

```c
XRT_NO_SUBPROCESS
XRT_NO_FILE_ASYNC
XRT_NO_XSON
XRT_NO_SUBPROCESS + XRT_NO_FILE_ASYNC + XRT_NO_XSON + XRT_NO_QUEUE_WAIT
```

验证结果：

- 多文件主线可编译
- 对应符号不会继续出现在目标文件里
- 当前没有发现“定义裁剪宏后实现仍被硬编进去”的新增漏口


## 6. 推荐配置方式

### 6.1 关闭新增三块独立模块

```c
#define XRT_NO_SUBPROCESS
#define XRT_NO_FILE_ASYNC
#define XRT_NO_XSON
#include "xrt.h"
```

适用：

- 不需要进程拉起
- 不需要异步文件 I/O
- 只需要 JSON，不需要 XSON 扩展语法


### 6.2 构造“无网络、无异步文件”的纯本地运行时

```c
#define XRT_NO_NETWORK
#define XRT_NO_NETTLS
#include "xrt.h"
```

这会顺带裁掉：

- `file_async`
- `xurl`
- `http_util`
- `xcodec`
- `xhttp`
- `xhttpd`
- `xws`


### 6.3 保留 JSON，关闭 XSON

```c
#define XRT_NO_XSON
#include "xrt.h"
```

适用：

- 只想保留标准 JSON
- 不需要 `time/list/coll/class` 这些 XSON 扩展表示


## 7. 不再建议使用的历史思路

旧文档里出现过的旧网络模块族裁剪面，例如：

```c
XRT_NO_NETSOCK
XRT_NO_NETPOLL
XRT_NO_NETLOOP
XRT_NO_NETTCP
XRT_NO_NETUDP
XRT_NO_NETHTTP
```

都不应再作为当前正式裁剪方案继续使用。当前正式主线只看本页列出的宏集合。
