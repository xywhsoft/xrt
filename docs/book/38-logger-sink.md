---
num: 38
slug: logger-sink
title: 日志（下）：五类 Sink、轮转与异步
volume: 卷五 系统服务
type: practice
lead: 控制台/文件/文本/JSON/异步五类内置 Sink、运行时轮转重开、热路径写盘的异步化。
api: logger, io
---

## 导读

上一章写了自定义 Sink 的全部零件，本章组装整机：**五类内置 Sink**（控制台、文件、文本格式化、JSON 格式化、异步包装）覆盖绝大多数输出需求，配置器风格统一（`ConfigInit` + 定制 + 创建）。运维三件套——**轮转**（`xrtLogFileRotate`）、**重开**（`xrtLogFileReopen`，外部截断后恢复）、**统计**（`xrtLogFileStats`）让文件 Sink 适配长期运行。**异步 Sink** 是热路径的关键：提交线程只入队、专职线程写盘——日志不再拖慢业务。

## 引入

日志上线的三个坎。坎一：日志要 JSON 格式进采集管道（ELK/Loki 吃结构化）——自定义 Sink 手写 JSON 拼装？字段转义、时间格式、嵌套错误链全是细节坑。坎二：日志文件无限增长——磁盘三个月被刷爆；要按大小轮转、要压缩归档、要在 logrotate 外部截断后还能恢复写入。坎三：高峰期每秒几万条日志，写盘的 IO 延迟直接加在请求耗时上——日志成了性能税。

三个坎对应三组答案：JSON Sink 开箱即用（字段自动进对象、错误链自动展开、时间微秒戳）；文件族的 Rotate/Reopen/Stats（内置轮转语义 + 外部截断恢复）；异步 Sink 把写盘挪出提交路径（队列缓冲 + 专职线程刷盘）。它们共同的前提是第 37 章的地基——一切 Sink 都实现同一个写函数契约。

## 概念

### 五类内置 Sink 一张表

| Sink | 创建入口 | 典型用途 |
| --- | --- | --- |
| 控制台 | `xrtLogConsoleConfigInit` + 定制 | 开发期终端、容器 stdout |
| 文件 | `xrtLogTextFile` / `xrtLogJsonFile` | 落盘归档、按格式选 |
| 文本格式化 | `xrtLogTextWrite` 族 | 自定义文本布局（复用格式化引擎） |
| JSON 格式化 | json 配置器 | 采集管道、结构化检索 |
| 异步包装 | 异步配置器 | 热路径提交不阻塞 |

配置器风格全库统一：`ConfigInit` 填默认 → 按需改字段 → 一步创建。JSON Sink 的输出形态（json 范例可见）：顶层 `time`（微秒）/`level`/`logger`/`message` + `fields` 对象收纳结构化字段——字段名与类型原样保留，采集管道零解析成本。

### 文件运维三件套

```diagram flow
- Rotate：触发立即轮转——当前文件改名归档、原路径开新文件继续写
- Reopen：外部截断（logrotate move）后重开——句柄失效自动恢复
- Stats：累计写入量/丢弃量等运行时统计——容量规划与告警的数据源
```

轮转的触发分内置策略（按大小自动）与手动触发（`xrtLogFileRotate` 运维接口——按时间轮转由 cron 调它即可）；`xrtLogFilePath` 查询当前文件路径——轮转后路径变化，监控要知道当前写在哪。

### 异步 Sink：队列 + 专职线程

提交路径的真相：同步写文件 Sink 时，每条日志的 IO 延迟（毫秒级）直接加在业务调用上。异步 Sink 的结构：**提交线程**只做"格式化进队列"（微秒级，无锁队列批量接口——第 21 章的 MPMC 正是它的底座）；**专职线程**批量出队写盘（批量 32 条的摊还效率）。代价与纪律：停机必须 Flush（`xrtLogFlush`——队列里的记录落盘完才算干净退出，丢的是崩溃前最后几百毫秒）；队列容量是背压阀（满时按策略丢弃或阻塞——丢弃计数进 Stats，"日志洪水牺牲日志"是正确取舍）。

### 格式化复用：TextWrite 与字段 printf

`xrtLogTextWrite` 族让自定义目的地复用文本格式化引擎（你的 Sink 只管"写去哪"，格式化交给库）；`xrtLogFieldsPrintf` 族是字段值的 printf 拼装（便捷层，第 37 章的选择规则照旧）。校验器（`xrtLogTextConfigValidate` / `JsonConfigValidate` / `RecordValidate`）在配置加载口把关——配置写错的格式串在启动时暴露，不是运行时炸雷。

## 示例

### 完整程序：JSON 结构化输出

来自仓库范例 `examples/logging/json/main.c`——字段自动进对象、错误链展开：

```embed path="examples/logging/json/main.c" title="examples/logging/json/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/logging/json/main.c -lws2_32 -liphlpapi
{"time":1788575322193322,"level":"INFO","logger":"http",
 "message":"request completed","fields":{"request_id":42,"cached":false}}
```

**刚才发生了什么。** ① 一条 `LogFields` 提交（整数字段 42、布尔字段 false）——JSON Sink 自动把它们收进 `fields` 对象：**字段保类型直达**（42 是数字不是 `"42"`、false 是布尔），采集管道的解析零成本。② `time` 是 Unix 微秒（第 3 章 xtime 口径）——机器可排序、人可格式化，格式选择权在消费端。③ 输出里 message 与 fields 分离——给人看的消息与给机器检索的字段各归各位，这是"结构化日志"的本义。对比第 37 章的文本 Sink（忽略字段只打消息）：同一个 Logger 挂两种 Sink，文本进控制台、JSON 进采集——双格式并行就是这个模型的价值。

### 完整程序：异步 Sink

来自 `examples/logging/async/main.c`——提交线程零阻塞、专职线程刷盘：

```embed path="examples/logging/async/main.c" title="examples/logging/async/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/logging/async/main.c -lws2_32 -liphlpapi
asynchronous sink ready
```

**刚才发生了什么。** ① 异步 Sink 包装一个真实目的地（文件）——对 Logger 而言它就是普通 Sink，提交接口不变。② 主线程提交记录只入队（`asynchronous sink ready` 立即打印——提交没有被写盘阻塞）；专职线程在后台批量消费。③ 停机路径的 Flush：队列清空、文件句柄关闭——干净退出。把 sink_tour 范例留作运维接口的完全体阅读：它把 Ref/Name/Level/SetLevel/Count/Stats/Detach 族与文件专用族（Path/Rotate/Reopen/Stats）全部走了一遍，九个 ok 对应九组接口。

## 契约

- **配置器统一**：`ConfigInit` → 定制 → 创建；校验器在配置口把关。
- **JSON 形态**：time 微秒/level/logger/message + fields 对象——字段保类型、时间机器序。
- **轮转语义**：Rotate 改名归档开新文件；按大小自动 + 手动触发；Path 查当前路径。
- **截断恢复**：外部 move/截断后 Reopen 重开——logrotate 协作的内置支持。
- **异步纪律**：停机 Flush；队列容量是背压阀，丢弃计数进 Stats；提交微秒级、刷盘批量摊还。
- **格式化复用**：自定义目的地用 TextWrite 族；给机器的字段仍走 LogFields。

### 从示例到工程：Sink 装配的三个模式

**开发环境**：一个控制台 Sink（文本格式、DEBUG 门槛）——终端即时可读，五分钟装配。**生产标准**：JSON 文件 Sink（INFO 门槛、按大小轮转）+ 异步包装（队列背压）+ 控制台 ERROR Sink（容器环境的 stderr 告警通道）——三个 Sink 各司其职，一次装配。**采集管道**：JSON 格式直连 stdout（容器 stdout 采集）或轮转文件 + Filebeat 类代理——格式已在 Sink 层统一，管道零适配。三个模式的共同形状：装配代码集中在启动函数（第 37 章纪律），配置来自第 32 章 JSON 配置文件（Sink 类型、路径、级别全部可配）——"日志系统的日志配置"是配置驱动装配的标准练习题。

### 一个运维观：日志是数据流

把日志当成"打印"是开发视角；上线后它是**数据流**——有源头（业务代码）、有管道（Sink 与队列）、有归宿（文件/采集系统）、有容量（磁盘与队列）、有生命周期（轮转与归档）。用数据流的视角做运维决策：容量规划看 Stats 的写入速率 × 保留周期；洪水防护靠队列丢弃策略 + 级别动态下调（`SetLevel` 运行时可调）；故障恢复验证 Reopen（演练外部截断）；质量监控对账"提交量 vs 落盘量"（异步丢弃在容忍范围）。这个视角也解释了第 35 章管线思想的再次适用——日志系统本身就是"记录 → 格式化 → 队列 → 落盘"的管线，你在卷四学的组装能力在这里直接上岗。

## 避坑

### 坑 1：异步 Sink 停机不 Flush

症状：崩溃复现时"最后一段日志不见了"——排障最需要的那几百毫秒恰好丢失；优雅停机路径没有丢但 kill -9 场景必丢。

原因：异步队列里的记录还没落盘进程就退了——队列是易失的，Flush 是落盘的承诺。

```c bad
/* 停机 */
xrtLogSinkFree(pAsyncSink);   /* 直接释放——队列内容丢弃 */
```

```c good
/* 停机：先 Flush 队列再释放 */
xrtLogFlush(pLogger);          /* 或异步 Sink 专属 Flush 接口 */
xrtLogSinkFree(pAsyncSink);
```

### 坑 2：每条日志一个文件 Sink

症状：文件句柄暴涨；轮转互相踩踏；进程句柄上限告警。

原因：把"打日志的地方"当成了"建 Sink 的地方"——Sink 是全局装配的资源，不是随手建的一次性对象。

```c bad
void handle_request(void)
{
	xlogsink* pSink = create_file_sink("app.log");   /* 每请求建一个 */
	xrtLogAttach(gLog, pSink);
	xrtLog(gLog, XLOG_INFO, ...);
	xrtLogSinkFree(pSink);                            /* 拆了建、建了拆 */
}
```

```c good
/* 启动时装配一次，全局复用（第 37 章"装配集中"纪律） */
static xlogsink* gFileSink;
void init_logging(void) { gFileSink = create_file_sink("app.log"); xrtLogAttach(gLog, gFileSink); }
void handle_request(void) { xrtLog(gLog, XLOG_INFO, ...); }   /* 只提交，不装配 */
```

## 练习

### 基础：双格式并行（同一记录两种呈现）

一个 Logger 挂文本 Sink（控制台）+ JSON Sink（文件），同一条 LogFields 提交——验证文本行与 JSON 行的字段各自呈现。

### 进阶：定时轮转器

用 `xrtLogFileRotate` 实现"每小时轮转"：后台定时触发、轮转后把归档文件 gzip（第 29 章一次性压缩）、`xrtLogFilePath` 输出当前路径。提示：定时器可用简单的轮询线程（卷六有正经定时器）。

### 挑战：日志洪水实验

构造每秒十万条提交的压力：同步文件 Sink 与异步 Sink 各跑一分钟，测量提交侧平均耗时、丢弃计数（Stats）、磁盘写入总量三组对照。验收标准：异步版提交侧耗时比同步版低两个数量级以上；丢弃策略下洪水期不阻塞业务；实验报告含三组数字与分析。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 五类 Sink | 控制台 / 文件（文本/JSON）/ 格式化复用 / 异步包装 |
| JSON 形态 | time 微秒 + level + logger + message + fields 对象（字段保类型） |
| 运维三件 | Rotate 轮转 / Reopen 截断恢复 / Stats 统计；Path 查当前路径 |
| 异步纪律 | 停机 Flush；队列=背压阀；提交微秒、刷盘批量 |
| 装配纪律 | Sink 启动装配一次全局复用；业务代码只提交 |
| 校验器 | 配置加载口把关——格式错误启动时暴露 |
