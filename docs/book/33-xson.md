---
num: 33
slug: xson
title: XSON 扩展序列化
volume: 卷四 文本与结构化数据
type: practice
lead: JSON 的严格超集——bytes/time/set/intmap 四种一等类型，配置与内部数据的全类型往返。
api: xson, value
---

## 导读

JSON 表达不了的东西太多了：二进制要 Base64 手工转、时间戳要字符串约定、集合与整数键映射要退化成数组——每个缺口都是一层自定义约定，每层约定都是互操作的地雷。**XSON 是 JSON 的严格超集**：合法 JSON 天然是合法 XSON，同时多出四种一等类型——`bytes("...")`（内联 Base64）、`time("...")`（ISO 时间戳）、`set[...]`（去重集合）、`intmap{...}`（整数键映射）。解析器与第 31 章同一套（事件回调按类型字段区分，扩展类型不再退化为字符串），序列化把四种类型原样写回——**全类型往返零约定成本**。什么时候用 XSON、什么时候坚持 JSON，本章给出决策规则。

## 引入

一个内部配置的真实形状：证书私钥是二进制（JSON 里只能 Base64 字符串，消费方要知道"这串要解码"）；过期时间是时间戳（写成字符串还是数字？时区算谁的？）；允许的端口是一个集合（JSON 数组带重复怎么办？消费方要去重？）；连接参数是端口号到说明的映射（JSON 对象的键只能是字符串，"80" 与 80 的转换谁做？）。四个字段四种约定，文档写了两页，还是有人弄错。

XSON 的答案是让这四种形状成为**一等类型**：`bytes("AAEC...")` 自带编码声明、`time("2026-07-31T00:00:00Z")` 统一 UTC 微秒、`set[80, 443]` 语义即去重、`intmap{80: "http"}` 整数键原生的映射——值树的 `xvalue`（第 30 章）本来就有这四种类型，XSON 只是让序列化不再把它们拍扁。**超集设计**保证平滑：已有的 JSON 工具链照常工作，XSON 文档里的 JSON 部分对任何 JSON 解析器都合法。

## 概念

### 四种扩展类型

```diagram flow
- bytes("AAEC/w==")：二进制内联 Base64——免外部引用，读回即 bytes 值
- time("2026-07-31T00:00:00Z")：ISO 8601——解析归一 UTC，事件里给微秒整数
- set[...]：去重集合——语义即第 19 章集合，序列化原样往返
- intmap{ 80: "http" }：整数键映射——键不再被迫转字符串
```

四种类型在值树（第 30 章）里都有对应：Bytes/Time 值、set/intmap 容器——XSON 序列化只是"值树 ↔ 文本"的完整映射，不丢类型信息。事件流（SAX）同样区分：`xxsonevent` 的 Type 字段直接标出扩展类型，消费方不需要猜"这个字符串是不是其实是个时间"。

### 严格超集的含义

三条性质值得分开说。**正向兼容**：任何合法 JSON 文档是合法 XSON——把现有 `.json` 配置改名为 `.xson` 零成本。**反向不兼容**：带扩展类型的 XSON 文档对纯 JSON 解析器不合法（`bytes(` 不是 JSON 语法）——XSON 只用于"两端都是 XRT 或约定了 XSON"的通道。**互不干扰**：解析器同一套，按文档实际内容解析——JSON 文档解析出的值树不含扩展类型，XSON 文档可能含；配置系统不需要为两种格式维护两套代码。

### 与 JSON 的选型规则

| 场景 | 选择 | 理由 |
| --- | --- | --- |
| 对外开放 API | JSON | 通用语，任何客户端可解析 |
| 内部配置 | XSON | 二进制/时间/集合原生表达，零约定 |
| 内部服务间数据 | XSON | 两端可控，全类型往返 |
| 需要人工编辑的配置 | JSON 或 XSON | 时间戳人写 ISO 字符串比微秒数字友好 |

规则的核心变量是**接收方是否可控**：可控 → 用 XSON 拿全类型；不可控 → JSON 保互操作。混合形态也常见：对外 API 的字段用 JSON 类型，内部处理的中间表示用值树（本来就有全类型）——边界处转换，内部不将就。

### 错误定位与严格性

XSON 继承第 31 章的全部严格性：资源边界三闸、重复键拒绝、错误带行列位置。扩展类型的解析错误同样定位到字符——`time("2026-13-01...")` 的非法月份直接指出位置。

## 示例

### 完整程序：全类型往返

来自仓库范例 `examples/data/xson/main.c`——四种扩展类型一次写读：

```embed path="examples/data/xson/main.c" title="examples/data/xson/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/data/xson/main.c -lws2_32 -liphlpapi
{
  "blob": bytes("AAEC/w=="),
  "updated": time("2026-07-31T00:00:00Z"),
  "roles": set[ ... ],
  "ports": intmap{ 80: "http", 443: "https" }
}
bytes = 4
time = 1785456000000000
{"code":200,"tags":set["xrt"]}
```

**刚才发生了什么。** ① 序列化段：值树里的四种类型原样写出——`bytes("AAEC/w==")` 带编码声明、`time(...)` 是 ISO 字符串、`set[...]` 与 `intmap{...}` 各用自己的括号语义。**类型信息在文本里可见**——读文档的人不需要查约定表。② 解析段：`bytes = 4`——Base64 解码回 4 字节二进制；`time = 1785456000000000`——ISO 字符串归一成 UTC 微秒整数（第 3 章 `xtime` 口径）。③ 最后一行是紧凑序列化（不美化）——`set["xrt"]` 在一行里的形态；机器通道用紧凑、落盘审阅用美化，与 JSON 的选择一致。

### 完整程序：文件往返与错误定位

来自 `examples/data/xson_tour/main.c`：

```embed path="examples/data/xson_tour/main.c" title="examples/data/xson_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/data/xson_tour/main.c -lws2_32 -liphlpapi
xson: valid +/- and read(config) ok
xson: error location line=1 ok
xson: file roundtrip parse/read -> write/stringify ok
xson: sync write via callback ok
xson: sink writer full value domain ok
```

**刚才发生了什么。** ① 校验与读取：XSON 文档的合法性校验与配置字段读取一次走完。② 错误定位：构造缺逗号的输入，`line=1` 精确报告——第 31 章 JSON 的定位能力原样继承。③ 文件往返：解析 → 读字段 → 写回 → 序列化，闭环验证类型不丢。④ 回调式写出：序列化输出经回调逐块交出——流式落盘/直连 sink 的形态，与第 31 章 Writer 的 sink 一致。⑤ sink 写出覆盖全部值域：标量与四种扩展类型都经 sink 走一遍——全类型不只 DOM 能表达，流式写出同样不丢类型。五个"ok"合起来就是"XSON 与 JSON 同一套工程能力，只是类型更全"的实证。

## 契约

- **严格超集**：合法 JSON 必是合法 XSON；解析器同一套；JSON 文档不含扩展类型。
- **四类型往返**：bytes/time/set/intmap 序列化与解析原样往返，零约定成本。
- **时间归一**：`time(...)` 解析归一 UTC 微秒（`xtime` 口径）；序列化写 ISO 8601。
- **选型规则**：接收方可控 → XSON 全类型；不可控 → JSON 互操作；边界转换、内部不将就。
- **严格性继承**：三闸、重复键拒绝、错误行列定位——与 JSON 完全一致。
- **紧凑与美化**：机器通道紧凑、落盘审阅美化——与 JSON 的选择规则相同。

### 从示例到工程：XSON 的三个宿主

**内部配置**（主战场）：启动加载 → 值树 → 第 30 章合并——二进制密钥、过期时间、权限集合、端口映射全部一等类型，消费代码零转换。这是 XSON 设计的直接目标场景。**内部服务总线**：服务间消息序列化——两端都是 XRT，全类型往返省掉两侧的编解码层；与第 21 章队列配合时，值树直接传（免序列化）与 XSON 序列化后传（跨进程）按通道选。**对外边界的降级出口**：需要对外输出 JSON 时按约定降级（坑 1 的 good 形态）——降级逻辑集中在边界模块，内部值树保持全类型。三个宿主的共同点：**类型信息在内部永远一等，只在明确标注的边界降级**。

### 一个对照：同一份配置的两种写法

把"证书配置"用两种格式写一遍，差异立现。JSON 形态：`{"key": "AAEC...", "expire": "2026-07-31T00:00:00Z", "ports": [80, 443, 80]}`——key 要文档说明是 Base64、expire 要约定 ISO 与时区、ports 数组带重复且消费方要去重。XSON 形态：`{"key": bytes("AAEC..."), "expire": time("2026-07-31T00:00:00Z"), "ports": set[80, 443]}`——三个字段自解释，消费代码各一行精确读取。对照的要点不是"XSON 更短"（可能更长），而是**约定从文档移进了语法**——文档会失忆、会失传，语法则由解析器强制执行。这就是"一等类型"的全部含义。

## 避坑

### 坑 1：把 XSON 发给不可控的接收方

症状：外部集成方报告"你的 JSON 格式非法"——他们的解析器在 `bytes(` 或 `set[` 处报语法错误。

原因：XSON 的扩展语法对纯 JSON 解析器不合法——超集的"超"只对 XRT 与约定方有效。

```c bad
/* 对外 API 直接返回 XSON 序列化 */
str sBody = xrtXsonStringify(pResponse, false, NULL);
HttpReply(200, sBody);   /* 客户端 JSON 解析器：syntax error */
```

```c good
/* 对外通道一律 JSON：扩展类型在边界显式转换 */
str sBody = xrtJsonStringify(pResponse, false, NULL);
/* bytes/time 在出口前转成约定的 JSON 形态（字段文档写清） */
```

### 坑 2：把 time 值当字符串读

症状：读时间字段得到失败或空值——"明明序列化里是字符串 `time("...")`"。

原因：XSON 的 time 是**一等类型**——值树里的类型是 Time 不是 String；`GetString` 按第 30 章精确读取语义失败（类型不符）。

```c bad
xstrview When;
xrtValueGetString(pUpdated, &When);   /* 类型是 Time——精确读取失败 */
```

```c good
xtime When;
xrtValueGetTime(pUpdated, &When);     /* GetTime 读微秒整数 */
/* 要展示格式：自己格式化（第 41 章时间模块） */
```

## 练习

### 基础：类型识别

对包含四种扩展类型各一个的 XSON 文档解析，用 `xrtValueType` 打印每个字段的类型名；再序列化回文本对比类型标注原样保留。

### 进阶：配置迁移

把一份 JSON 配置（含 Base64 字符串字段、ISO 时间字符串字段、端口数组）迁移成 XSON 的全类型形态：字段类型升级（字符串→bytes/time、数组→set/intmap）、解析代码改用精确读取、序列化对比前后体积与可读性。提示：迁移的价值要在"消费方代码变简单"上体现——数一数删掉了多少转换代码。

### 挑战：双格式导出器

实现 `export(值树, 格式)`：XSON 输出全类型；JSON 输出时扩展类型按约定降级（bytes→Base64 字符串、time→ISO 字符串、set→数组、intmap→字符串键对象），并在文档头部字段声明降级约定。验收标准：XSON 导入导出往返逐类型相等；JSON 导出可被任意 JSON 解析器解析；两格式的字段对照表自动生成。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 超集关系 | 合法 JSON ⊂ 合法 XSON；同一套解析器与严格性 |
| 四类型 | bytes 内联 Base64 / time ISO 归一 UTC 微秒 / set 去重 / intmap 整数键 |
| 选型 | 接收方可控→XSON；不可控→JSON；边界转换内部不将就 |
| 时间读取 | `GetTime` 读微秒整数——time 是一等类型不是字符串 |
| 错误定位 | 行列位置与 JSON 同源——`line=1` 级精确 |
| 通道形态 | 紧凑机器通道 / 美化落盘审阅 / 回调流式写出 |
