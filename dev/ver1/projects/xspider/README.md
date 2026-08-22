# XSpider

一个基于 **XRT** 开发的教学型并发爬虫项目。  
A teaching-oriented concurrent crawler project built on **XRT**.

它的定位不是“最大最全的爬虫系统”，而是一个结构良好、便于阅读、能够快速证明 `xrt` 能力的完整项目。  
It is designed to prove what `xrt` can do through a small but complete crawler project.


## 项目定位 / Positioning

`xspider` 的目标是把 `xrt` 的核心能力串成一条完整主线：

- 多线程调度
- 多协程并发
- 异步 HTTP 抓取
- URL 规范化与相对链接解析
- 同站 / 子域名过滤
- 去重与 frontier 调度
- 页面回调与结果落盘
- TLS 失败诊断

它更接近一个 **MVP + 教学项目**，而不是生产级、分布式、强策略化的爬虫系统。


## 展示了哪些 XRT 能力 / What XRT Features Are Demonstrated

- `thread`
	使用 `xrtThreadCreate()` 创建多线程抓取执行层
- `coroutine`
	每个线程内部创建协程调度器，多协程并发处理抓取任务
- `xnet-v2 / xhttp`
	使用 `xrtHttpExecuteAsync()` 发起异步 HTTP 请求
- `xnetfuture`
	使用 `xrtNetFutureWaitCoTimeout()` 在协程中等待异步请求完成
- `xurl`
	使用 `xrtUrlResolveTo()`、`xrtUrlParseView()` 做相对链接解析、同站与子域名过滤
- `dict`
	使用 `xrtDictCreate(..., XRT_OBJMODE_SHARED)` 做去重集合
- `mutex`
	使用 `xrtMutexCreate()` 保护 frontier、统计和输出文件
- `file / dir / hash`
	使用 `xrtDirCreateAll()`、`xrtFileWriteAll()`、`xrtHash64()` 保存抓取结果


## 吸收了哪些同类产品的思路 / Inspirations

- 类似 `Colly`
	提供 `ShouldVisit / OnDiscover / OnPage / OnError` 这类回调接口
- 类似 `crawler4j`
	支持线程数、深度、最大页面数等直接配置
- 类似 `Scrapy`
	按 frontier、抓取、抽取、回调、统计分层组织
- 参考 `Heritrix`
	围绕同站范围控制和链接发现流程组织抓取逻辑


## 快速开始 / Quick Start

### 1. Windows GCC 构建

```bat
build_GCC_x64.bat
```

### 2. Windows TCC 构建

```bat
build_TCC_x64.bat
```

### 3. Linux / macOS 构建

```sh
sh build.sh
```

### 4. 本地站点快速验证

在 `demo_site` 目录启动一个静态站点：

```bat
python -m http.server 18901 --bind 127.0.0.1
```

然后运行：

```bat
bin\xspider.exe http://127.0.0.1:18901/index.html 4 64 2 6
```

### 5. 抓取公开站点

严格 TLS 校验：

```bat
bin\xspider.exe https://xxrpa.com 6 512 4 12
```

关闭 TLS 校验后抓取整站：

```bat
bin\xspider.exe https://xxrpa.com 6 512 4 12 --insecure --delay-ms 0
```


## 命令行参数 / CLI

基础用法：

```text
xspider <root_url> [max_depth] [max_pages] [threads] [coroutines_per_thread] [options]
```

位置参数：

| 参数 | 含义 | 默认值 |
| --- | --- | --- |
| `root_url` | 根网址 | 必填 |
| `max_depth` | 最大抓取深度 | `4` |
| `max_pages` | 最大抓取页面数 | `256` |
| `threads` | 工作线程数 | `2` |
| `coroutines_per_thread` | 每线程协程数 | `8` |

可选参数：

| 参数 | 含义 | 默认值 |
| --- | --- | --- |
| `--insecure` | 关闭 TLS 证书校验 | 关闭后生效 |
| `--strict-tls` | 启用 TLS 证书校验 | `on` |
| `--delay-ms <n>` | 设置全局礼貌延迟毫秒数 | `200` |
| `--subdomains` | 包含子域名 | `on` |
| `--no-subdomains` | 不包含子域名 | `off` |

说明：

- `--delay-ms` 是 **全局礼貌延迟**，不是按 host 分别限速
- 当前 demo 默认包含子域名
- 每次运行前会清空 `output/` 目录，重新生成结果


## 常用命令示例 / Common Commands

抓本地教学站点：

```bat
bin\xspider.exe http://127.0.0.1:18901/index.html 4 64 2 6
```

抓一个 HTTPS 站点并保持严格 TLS 校验：

```bat
bin\xspider.exe https://example.com 4 256 4 12 --strict-tls
```

抓整站并关闭 TLS 校验：

```bat
bin\xspider.exe https://xxrpa.com 6 512 4 12 --insecure
```

做吞吐测试，不加礼貌延迟：

```bat
bin\xspider.exe https://xxrpa.com 6 512 4 12 --insecure --delay-ms 0
```

只抓主域名，不抓子域名：

```bat
bin\xspider.exe https://xxrpa.com 6 512 4 12 --no-subdomains
```


## 核心架构 / Architecture

`xspider` 的运行流程可以概括为：

1. `seed`
	从根 URL 创建第一个任务
2. `frontier`
	将新 URL 规范化、去重、入队
3. `thread + coroutine workers`
	工作线程不断从 frontier 取任务，每个线程内部由多个协程并发执行抓取
4. `fetcher`
	通过 `xrtHttpExecuteAsync()` 发起请求，在协程中等待结果
5. `extractor`
	对 HTML 页面提取链接，并继续调度新任务
6. `callbacks`
	在发现链接、抓到页面、发生错误时调用回调
7. `output`
	把页面保存到 `output/pages/`，并把元数据写入 `output/manifest.tsv`

对应模块：

- [main.c](/D:/git/xrt/projects/xspider/main.c)
	CLI、demo 回调、结果输出
- [xspider.h](/D:/git/xrt/projects/xspider/src/xspider.h)
	爬虫核心逻辑、frontier、worker、HTTP 抓取、统计
- [xspider_html.h](/D:/git/xrt/projects/xspider/src/xspider_html.h)
	轻量 HTML 链接提取
- [tls_probe.c](/D:/git/xrt/projects/xspider/tls_probe.c)
	HTTPS/TLS 诊断探针


## 回调接口 / Callback Surface

`xspider` 当前提供四类回调：

- `ShouldVisit`
	决定一个 URL 是否允许进入 frontier
- `OnDiscover`
	每发现一个新链接就触发一次
- `OnPage`
	每抓到一个页面就触发一次
- `OnError`
	抓取失败时触发，并尽量提供错误阶段和错误文本

这些接口定义位于 [xspider.h](/D:/git/xrt/projects/xspider/src/xspider.h)。


## 输出结果 / Outputs

运行结束后会在 `output/` 下得到：

- `output/pages/`
	保存抓取到的页面正文，文件名为 URL 的 `xrtHash64()` 值
- `output/manifest.tsv`
	保存页面清单

`manifest.tsv` 的字段如下：

| 字段 | 含义 |
| --- | --- |
| `url` | 页面 URL |
| `depth` | 抓取深度 |
| `status` | HTTP 状态码 |
| `links` | 该页发现的新链接数 |
| `file` | 页面正文保存路径 |


## 项目结构 / Layout

```text
projects/xspider/
├─ main.c
├─ README.md
├─ tls_probe.c
├─ build_GCC_x64.bat
├─ build_TCC_x64.bat
├─ build.sh
├─ src/
│  ├─ xspider.h
│  └─ xspider_html.h
├─ demo_site/
│  ├─ index.html
│  ├─ about.html
│  ├─ contact.html
│  ├─ guide/
│  │  ├─ getting-started.html
│  │  └─ concurrency.html
│  └─ blog/
│     ├─ post-1.html
│     └─ post-2.html
├─ bin/                 (构建产物 / generated binaries)
└─ output/              (抓取结果 / generated crawl outputs)
```


## 性能实测 / Performance Snapshot

以下数据为 **2026-04-01** 在当前开发机上的实际测试结果，目标站点为 `https://xxrpa.com`，使用 `--insecure` 以绕过其错误证书配置。

| 命令 | 页面数 | 耗时 | 近似吞吐 |
| --- | ---: | ---: | ---: |
| `4 threads x 12 coroutines --delay-ms 200` | `158` | `32.53s` | `4.86 页/秒` |
| `4 threads x 12 coroutines --delay-ms 0` | `158` | `1.679s` | `94.10 页/秒` |
| `4 threads x 12 coroutines --delay-ms 0` 第二次复测 | `158` | `1.683s` | `93.88 页/秒` |
| `8 threads x 24 coroutines --delay-ms 0` | `158` | `1.774s` | `89.06 页/秒` |

结论：

- 默认 `200ms` 礼貌延迟会把全局吞吐限制在大约 `5 请求/秒`
- 对这个站点来说，去掉延迟后当前实现的有效吞吐大约在 **90-95 页/秒**
- 把并发从 `4x12` 提高到 `8x24` 没有继续提速，说明此时瓶颈更接近目标站点响应、连接管理或本地调度开销，而不是协程数量本身


## HTTPS / TLS 诊断案例 / HTTPS Diagnostic Case

`xxrpa.com` 是一个很适合用来展示 `xrt` HTTPS 诊断能力的案例。

### 现象

- 浏览器可以打开页面，但会提示连接不完全安全
- 严格 TLS 校验下，`xspider` 和 `tls_probe` 都会失败
- 关闭校验后，`xrt` 可以正常抓到页面数据

### 当前实测结论

站点证书关键信息如下：

- `Common Name = server`
- `Issuer = Mongoose`
- `SAN = localhost`

因此，`https://xxrpa.com` 在严格 TLS 校验下失败的直接原因是：

> 证书主机名与请求主机名不匹配

现在 `xrt` 会明确输出：

```text
TLS server certificate hostname does not match the requested host.
```

而不是只返回一个模糊的 `XRT_NET_CLOSED(-4)`。

### 如何复现

严格模式：

```bat
bin\xspider.exe https://xxrpa.com 1 8 2 4
```

或使用探针：

```bat
bin\tls_probe.exe https://xxrpa.com/ 1
```

关闭校验后：

```bat
bin\tls_probe.exe https://xxrpa.com/ 0
```


## `tls_probe.c` 的用途 / Purpose of `tls_probe.c`

`tls_probe.c` 是一个更小的 HTTP/TLS 验证程序，适合在不运行完整爬虫的情况下检查：

- TLS 是否能握手成功
- 严格校验是否通过
- 错误文本是否准确
- 关闭校验后是否能取到页面正文

它不是主程序的一部分，而是一个辅助诊断文件。

在 Windows 上可单独编译：

```bat
gcc -std=c11 -O2 -Wall -Wextra -I ..\..\singlehead tls_probe.c -o bin\tls_probe.exe -lws2_32 -lwinmm -lbcrypt -liphlpapi
```


## 当前边界 / Current Limits

- 当前是 whole-body 抓取模型，更适合页面型 / 文档型站点
- 链接提取使用轻量扫描器，适合 demo 和中等复杂页面
- 当前礼貌延迟是 **全局** 限速，不是按 host 分别限速
- 当前没有实现 `robots.txt`
- 当前没有实现断点续跑和持久化 frontier
- 当前没有实现分布式调度
- 当前没有实现内容去重、正文抽取、结构化数据管道


## 这个项目证明了什么 / What This Project Proves

`xspider` 已经证明，`xrt` 不只是一个“基础运行库”，它已经具备快速构建一个结构化并发爬虫项目所需的关键能力：

- 线程层并发
- 协程层并发
- 异步网络 I/O
- HTTP 客户端
- URL 工具链
- 共享容器
- 文件系统输出
- 错误传播与 TLS 诊断

也就是说，`xrt` 已经可以支撑一个真正可运行、可教学、可验证性能的爬虫项目，而不是只能停留在零散示例阶段。
