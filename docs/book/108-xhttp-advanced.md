---
num: 108
slug: xhttp-advanced
title: 服务端收官：压缩、Range 与综合
volume: 卷十 扩展库：xhttp · 卷十收官
type: practice
lead: 内容协商驱动的响应压缩、Range 组合、代理头族与卷十全景——服务端主题的收官与卷十一预告。
api: xhttp-http_server, xhttp-http_compress, xhttp-http_cache_range
---

## 导读

卷十收官。三章服务端（103–107）之后，剩余的高级主题在此合拢：**响应压缩**（`reply_compress`/`server_compress`——按客户端 `Accept-Encoding` 协商选编码、透明压缩响应正文——第 92 章解码的镜像侧）；**Range 与组合响应**（`range_multipart`——多区间请求的 multipart/byteranges 响应，与第 100 章客户端 Range 组合对偶）；**代理头族**（`Via`/`X-Forwarded-*` 的规范化替代 `Forwarded`、`proxy_status` 代理状态——网关写作的词汇）。最后以"一次请求穿越卷十全栈"的全景图收卷：从 easy 调用到压缩 Range 响应的每一层。第 109 章起进入卷十一（xws/xruntime/xmail/xssh 扩展库家族）。

## 引入

压缩是带宽与 CPU 的交易：JSON API 响应压缩率常达 70-90%，但"永远 gzip"会撞上两类客户端——不支持的不解（必须 identity 回退）、已预压缩的内容不该再压（图片再压只会更大更慢）。所以压缩必须是**协商驱动**：读 `Accept-Encoding` 的 q 值（第 91 章）、按内容类型与大小决策、选双方最优编码。Range 组合是另一笔交易：客户端要三个区间的字节——一次回源逐段拿，还是服务端/代理直接拼一个 multipart 响应？后者省往返，但拼装与边界（第 90 章走私防御的 multipart 版）要正确。这两件事共同的特点是：**正确的实现要读的上下文多**（协商头、内容元数据、缓存状态），xhttp 把它们做成"配置即策略"的层，业务路由无感。

## 概念

### 响应压缩：协商驱动

```diagram flow
- 协商：读请求 Accept-Encoding（q 千分位——第 91 章模型）
- 决策：内容类型（可压？JSON/文本是/图片否）、大小（小响应不值得）、已有编码（不再压）
- 编码：按 q 与服务端能力选最优（gzip/deflate/identity）
- 应用：压缩变换正文（第 107 章变换来源）+ Content-Encoding 头 + 长度更新
```

`xhttpreplycompressconfig` 承载策略（最小压缩长度、内容类型白名单、编码偏好）；`reply_compress` 层在 Reply 构建后应用压缩——业务代码写的是未压缩正文，压缩是"出关检查"。**恒等回退**：客户端不支持或策略判不可压——identity 原样，绝不强制。服务端运行时的 `server_compress` 是同一能力的服务形态（中间件或运行时层启用）。

### Range 与 multipart/byteranges

单区间（`Range: bytes=0-99`）→ 206 + `Content-Range`；多区间（`bytes=0-99,200-299`）→ 206 + `multipart/byteranges`（每部件自带 Content-Range）。`range_multipart` 处理边界拼装——与第 90 章"读侧多区间"对偶的**写侧组合**。第 100 章客户端缓存的本地 Range 组合（片段+回源）最终也走这套写侧词汇重建响应。

### 代理头族：网关的语法

代理链的每一跳都要说话：`Via`（经过的代理与协议）、`Forwarded`（`X-Forwarded-For/Proto/Host` 的 RFC 7239 规范化替代——`Forwarded: for=192.0.2.60;proto=https`）、`proxy_status`（代理错误与上游状态的标准化报告）。`xhttp` 的 proxy 族模块（`forward`/`forwarded`/`via`/`proxy_alias`/`proxy_status` 及各自 `_write`）提供读写两端——第 69 章正向代理的 HTTP 头部配套。

### 一次请求穿越卷十全栈

```diagram flow
- 调用：xrtHttpClientGetSync（第 97 章 easy）→ 运行时冻结（98）
- 连接：池分片取连接/复用（98）→ TLS（卷八）→ 请求写出（卷九 89）
- 自动行为：重定向/Cookie/重试（99）→ 缓存查找/验证（100）
- URL/查询：解析与构建（101/102）→ 认证注入（105）
- 服务端：事件链（103）→ 中间件洋葱（104）→ 静态/路由
- 响应：正文来源（107）→ 压缩协商（本章）→ 分帧（卷九 90）→ 传输
- 客户端收：解码（卷九 92）→ 缓存入库（100）→ 结果快照（97）
```

每一站都是本卷一章——这就是 xhttp 的积木结构验收图。

## 示例

### 第一个完整程序：协商压缩的决策与产出

下面的程序来自 `examples/http/reply_compress`——按客户端能力压缩响应：

```embed path="extlibs/xhttp/examples/http/reply_compress/main.c" title="extlibs/xhttp/examples/http/reply_compress/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/reply_compress/main.c -lws2_32 -liphlpapi
（输出压缩协商与产出响应的自检结果）
```

**刚才发生了什么。** ① `xrtHttpAcceptEncodingInit/Add("gzip, identity;q=0.5")` 构造客户端能力声明——q 千分位（gzip 全分、identity 半分）——第 91 章协商模型的直接使用。② 普通 `ReplyCreate/SetBytes` 构建未压缩 JSON 响应——**业务代码不感知压缩**。③ 压缩层读能力+策略做决策并产出压缩版 Reply——高 q 的 gzip 胜出、变换正文（第 107 章来源）替换原正文、头字段更新。④ 对照实验路径：把能力换成 `identity` 重跑——恒等回退原样；配合客户端 `client_decompress`（第 92 章解码）就是完整的压缩闭环。

### 第二个完整程序：多区间组合响应

第二个程序来自 `examples/http/range_multipart`——multipart/byteranges 的写侧：

```embed path="extlibs/xhttp/examples/http/range_multipart/main.c" title="extlibs/xhttp/examples/http/range_multipart/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/range_multipart/main.c -lws2_32 -liphlpapi
（输出多区间响应组合的自检结果）
```

**刚才发生了什么。** ① 多区间请求的区间集合解析（第 90 章 Range 词汇）→ 每区间从正文切出 → 按边界拼 multipart/byteranges：边界行、每部件的 Content-Range、部件正文、终止边界——**写侧的边界纪律与第 90 章读侧走私防御对称**（边界错一位就是走私窗口）。② 与第 100 章呼应：客户端缓存的本地组合（已有片段+回源补齐）产出的正是这种响应——一章写侧、一章读侧，合起来是完整的 Range 体系。③ 配套巡检：`vary`（Vary 驱动的缓存分键——第 100 章 Vary 字段的协议面）、`proxy_status`/`proxy_status_write`（代理状态报告）、`via`/`forwarded`（代理链头族）——网关写作的完整词表。

### 三条读者的收官走法

**后端服务开发者**：103→104→105→107 是主干（事件链、洋葱、认证、正文），SSE（106）按业务取需——本章的压缩与 Range 是"上线前再回来"的优化章。**客户端/工具开发者**：97→98→99→101→102 已在 P18 就绪，本章只需压缩协商一节（配合第 92 章解码）。**网关作者**：本章代理头族 + 第 103 章服务端 + 第 98 章客户端运行时的组合是你的全部原料——`exchange`/`forward` 示例演示同一进程内"收请求再转发"的双形态组装。无论哪条走法，收官的自检标准只有一个：**卷十全景图的每一站你都能指到章**——能指到，说明积木在位；指不到，回到那章补——这是比"读完"更诚实的验收。

## 契约

- **压缩协商**：读 Accept-Encoding 的 q（千分位）；按内容类型/大小/已有编码决策；恒等回退绝不强制；产出变换正文+头更新。
- **压缩策略**：最小长度/类型白名单/编码偏好归配置；业务路由无感（出关检查形态）。
- **Range 写侧**：单区间 206+Content-Range；多区间 206+multipart/byteranges；边界拼装的原子性与正确性是走私防御的对称面。
- **代理头族**：Via/Forwarded（RFC 7239）/proxy_status 读写两端；X-Forwarded-* 的规范化替代。
- **Vary**：响应声明的分键字段——缓存主键成分（第 100 章）；`Vary: *` 不可缓存。
- **组合纪律**：压缩层不改语义（Content-Length 更新/分帧适配自动）；Range 与压缩可叠加（先 Range 后压或反之——策略显式）。
- **裁剪**：`HTTP_COMPRESS`/Range/代理族独立宏——不压缩的部署零成本。

## 避坑

### 坑 1：对已压缩内容再压缩

症状：JPEG/MP4 响应压完变大、CPU 白烧——压缩率 0 还倒贴开销。

原因：内容类型白名单是策略的核心成分——图片/视频/已带 `Content-Encoding` 的内容跳过。压缩层默认白名单不含它们，但自作聪明"全压"就撞上。

```c bad
Config.MinLength = 0;   /* 全压：JPEG 也压——变大变慢 */
```

```c good
/* 类型白名单默认只含可压文本族；长度阈值挡小响应 */
/* 自定义时显式排除 image/* video/* 与已有编码 */
```

### 坑 2：Range 响应把边界字符串写错（走私窗口）

症状：自拼 multipart 边界与正文撞车——对端把正文当边界解析，区间错位。

原因：边界唯一性是 multipart 的安全前提（第 102 章练习的 boundary 要求同样适用）——`range_multipart` 的边界生成内建防撞；手拼字符串没有。

```c bad
snprintf(Out, "----boundary\r\nContent-Range: ...");  /* 固定边界：正文含同串即撞 */
```

```c good
/* 用 Range 组合层：边界生成/转义/终止内建正确 */
/* 自拼时：随机边界 + 正文扫描防撞 + 正确的 "--" 前后缀 */
```

### 坑 3：代理链里丢了 Forwarded 语义

症状：多层代理后服务端看到的客户端 IP 是上一跳代理的——审计与限流全部失真。

原因：每跳代理应追加（不是覆盖）Forwarded/X-Forwarded-For；覆盖即丢历史。`forwarded` 模块的追加语义内建——手拼容易写成覆盖。

```c bad
set_header("X-Forwarded-For", current_peer_ip);   /* 覆盖：前面的历史蒸发 */
```

```c good
/* 追加当前跳，保留既有链 */
forwarded_append(reply, current_peer_ip, proto);
```

## 练习

### 基础：压缩协商矩阵

构造四种客户端能力（gzip 全分/identity/不支持头/`*;q=0`），对同一 JSON 响应走压缩层，记录四种产出。验收标准：协商结果与 q 推导一致；不支持时恒等回退。

### 进阶：多区间下载器

客户端请求三区间 → 服务端 `range_multipart` 响应 → 客户端解析 multipart 拼回文件（第 90 章 Body + 第 91 章参数层）。验收标准：拼回文件与原文件哈希一致；边界处理无重叠无缺失。

### 挑战：两级代理网关

中间代理（服务端+客户端形态组合：收请求→Forwarded 追加→转发上游→收响应→Via 追加→回客户端）。验收标准：两跳后服务端可见完整 Forwarded 链；Via 按序累积；上游 5xx 时 proxy_status 规范报告——用 curl 逐头验证。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 压缩协商 | Accept-Encoding q（千分位）驱动；恒等回退绝不强制；出关检查形态 |
| 压缩策略 | 最小长/类型白名单/编码偏好归配置；业务无感；已压内容跳过 |
| Range 写侧 | 单区间 206+Content-Range；多区间 multipart/byteranges；边界防撞 |
| 代理头族 | Via 追加/Forwarded（RFC 7239）/proxy_status——每跳追加不覆盖 |
| Vary | 响应声明的缓存分键；`Vary: *` 不可缓存（第 100 章） |
| 组合纪律 | 压缩自动更新长度与分帧；Range×压缩可叠且策略显式 |
| 卷十全景 | easy→运行时→池→TLS→自动行为→缓存→URL→服务端链→正文→压缩——章章对应 |
| 裁剪 | COMPRESS/Range/代理族独立宏 |
