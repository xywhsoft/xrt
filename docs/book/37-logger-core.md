---
num: 37
slug: logger-core
title: 日志（上）：记录、级别与结构化字段
volume: 卷五 系统服务
type: practice
lead: Logger 与 Sink 分离、自定义输出目的地、结构化字段直达——日志系统的地基。
api: logger, error
---

## 导读

日志是服务的第一可观测性。XRT 的日志模型核心是一对分离：**Logger**（`xrtLogCreate`，命名与级别门槛）与 **Sink**（`xrtLogSinkCreate`，输出目的地）——一个 Logger 可挂多个 Sink，一个 Sink 可被多个 Logger 复用；记录经 Logger 的级别过滤后广播到全部 Sink，**结构化字段**（`xrtLogFieldInt` 等）随记录直达 Sink 由其自行消费。本章讲清这对分离与自定义 Sink 的完整写法——它是理解下一章五类内置 Sink 的地基。

## 引入

三个朴素需求戳破 printf 日志的天花板。需求一：开发期要 DEBUG 级别刷屏、生产只要 WARN 以上——级别过滤不能靠注释掉 printf 重编译。需求二：同一条日志既要进控制台又要进文件（格式不同）——输出目的地不能焊死在打日志的地方。需求三：`request complete` 这条消息最好带上 `request_id=42`——排查时"哪次请求"比"什么消息"更值钱，而 printf 只能手工拼字符串。

三个需求指向同一个结构：**级别过滤**（Logger 的门槛）、**输出分离**（Sink 的广播）、**结构化字段**（字段随记录走，由 Sink 决定怎么呈现——文本 Sink 打印、JSON Sink 进 fields 对象）。Logger/Sink 分离的额外收益在后两章展开：内置五类 Sink 开箱即用（第 38 章）、异步 Sink 把写盘移出热路径（第 38 章）。

## 概念

### Logger 与 Sink 的分工

```diagram flow
- Logger：命名（http/db/auth）+ 级别门槛（低于门槛的记录在此丢弃）
- Sink：名字 + 自己的级别门槛 + 写入函数 + 用户数据
- 组合：xrtLogAttach 把 Sink 挂进 Logger——一挂多、多复用
- 提交：xrtLog(x) / xrtLogFields(带字段) → 过滤后广播到全部 Sink
```

两级门槛各司其职：Logger 的门槛是**模块策略**（db 模块开 DEBUG、其他保持 INFO），Sink 的门槛是**目的地策略**（控制台只要 ERROR、文件全收）。一条记录要过两道门才落盘——`xrtLogSinkSetLevel` 运行时可调（第 38 章的运维话题）。

### 自定义 Sink：四要素与三纪律

`xlogsinkconfig` 四要素：**名字**（运维可读）、**级别门槛**、**写入函数**（`xlogresult (const xlogrecord*, ptr)` 签名）、**用户数据**（输出目标的载体——FILE*、socket、任何东西）。写入函数的三条纪律：**记录是借用的**——回调内不得保留引用（记录的内存归提交路径，回调返回即失效）；**返回值语义**——`XLOG_RESULT_WRITTEN`/`_ERROR` 由 Logger 汇总上报（一个 Sink 失败不拖垮其他 Sink）；**零中间分配**——热路径的写函数直接格式化写出，不要为每条日志分配（示例的 fprintf 直写就是范本）。

### 结构化字段：值跟着记录走

| 构造 | 字段类型 |
| --- | --- |
| `xrtLogFieldInt` / `Float` | 数值字段 |
| `xrtLogFieldString` | 字符串字段（视图） |
| `xrtLogFieldTime` | 时间字段（微秒值） |
| `xrtLogFieldError` | **错误字段**——第 4 章 xerror 直达日志 |
| `xrtLogFieldNull` | 空值哨兵 |

字段的价值在**消费端的自由**：文本 Sink 可以忽略字段或打印键值对、JSON Sink 把字段收进 `fields` 对象（第 38 章 json 范例的输出形态）、你的自定义 Sink 可以只挑关心的字段做告警。`xrtLogFieldError` 值得点名——错误对象进日志，原因链（第 4 章）随之完整保留，排障时"错误是什么 + 为什么"一次看全。

### 级别体系

`XLOG_TRACE/DEBUG/INFO/WARN/ERROR` 五级（数值递增）。级别的语义约定：TRACE 是调试图纸（进"详细到不好意思"的细节）、DEBUG 是开发期诊断、INFO 是业务里程碑、WARN 是可自动恢复的异常、ERROR 是需要人工介入的故障。级别门槛的设置经验：**生产 Logger 保持 INFO、按模块下开 DEBUG**——全局 DEBUG 是日志洪水，模块定向才是诊断。

## 示例

### 完整程序：自定义 Sink 与结构化字段

来自仓库范例 `examples/logging/core/main.c`——约 60 行走完 Logger/Sink/字段全流程：

```embed path="examples/logging/core/main.c" title="examples/logging/core/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/logging/core/main.c -lws2_32 -liphlpapi
[INFO] example: request complete
```

**刚才发生了什么。** ① `exampleWrite` 是自定义 Sink 的全部：fprintf 按 `[级别] 名字: 消息` 格式直写、`xrtLogLevelName` 把枚举转可读名、`ferror` 决定返回 WRITTEN 还是 ERROR——三纪律（借用/返回值/零分配）全部落实。注意它**没消费字段**——request_id 被这个 Sink 忽略了，这正是"字段由 Sink 自行决定消费方式"的体现。② `Config.UserData = stdout`——同一个写函数换成 stderr 或文件指针就是另一个目的地，用户数据是 Sink 的万能插槽。③ `xrtLogCreate("example", XLOG_DEBUG)` 建 Logger、`xrtLogSinkCreate` 建 Sink、`xrtLogAttach` 组合——三步装配。④ `xrtLogFields` 提交带一个整数字段的记录，返回值核对 WRITTEN——提交路径的失败同样要检查（Sink 满了、关了）。⑤ 收尾顺序：先 `SinkFree` 再 `LogFree`——Sink 是被引用的资源，Logger 释放前 Sink 可独立回收。

### 完整程序：printf 便捷层

来自 `examples/logging/printf/main.c`，格式化提交的便捷形态：

```embed path="examples/logging/printf/main.c" title="examples/logging/printf/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/logging/printf/main.c -lws2_32 -liphlpapi
request=42 status=200
```

**刚才发生了什么。** printf 风格的提交（`xrtLogPrintf` 族，格式串接受 C 字符串而非视图）内部走第 25 章格式化——`"request=%u status=%u"` 与参数在提交口一步拼成消息文本。**便捷层与结构化字段的选择**：给机器消费的字段永远用 `LogFields`（字段保类型、JSON Sink 能收进对象）；只给人看的拼接消息用 printf 层——混用时记住"进了消息文本的字段就不再是结构化数据"。

## 契约

- **借用记录**：Sink 写函数内不保留记录引用；字段视图同样时效止于回调。
- **返回值**：WRITTEN/ERROR 由 Logger 汇总；单 Sink 失败不影响其他 Sink。
- **零分配**：写函数直写输出，不为每条记录分配。
- **两级门槛**：Logger 门槛是模块策略、Sink 门槛是目的地策略；记录过两道门才落盘。
- **字段保类型**：结构化字段进 `LogFields`；printf 层只拼展示文本。
- **错误字段**：`xrtLogFieldError` 携带第 4 章错误对象——原因链完整进日志。

### 从示例到工程：日志的三个宿主

**库代码**（写给别人用的模块）：不建全局 Logger——接受调用方注入的 Logger 或按模块名建独立实例（`xrt/db`、`xrt/http` 前缀天然分域）；库永远不直接碰 Sink（输出策略是应用的特权）。**应用服务**：启动时装配——按配置建 Logger 树与 Sink 集（第 38 章五类内置 + 自定义告警 Sink）、级别从环境变量或配置读入、优雅停机时 Flush 后释放（异步 Sink 的收尾纪律在下一章）。**工具类程序**：一个控制台 Logger + 一个文本 Sink 足矣——printf 的升级版，五分钟装配终身受用。三宿主的共同纪律：**日志装配集中在 main 附近**——散落在业务代码里的 Logger 创建是配置漂移的起点。

### 一个习惯：写日志前先想检索

日志的第一消费者是"三天后凌晨被叫醒的你"。写每条日志前默念三个问题：**能用它定位问题吗**（有 request_id 之类的关联字段吗）；**级别对吗**（这是里程碑 INFO 还是故障 ERROR）；**量级对吗**（这条日志在高峰期每秒多少条、磁盘受得住吗）。三个问题过不关的日志先别写——日志写出来就是负债，能检索的才是资产。结构化字段是这个习惯的技术支撑：关联字段进 LogFields、级别按语义表选、量级在压测时观测——第 38 章的统计与轮转是这个习惯的运维延伸。

## 避坑

### 坑 1：Sink 回调里保留记录指针

症状：后续使用保存的记录或字段指针读到悬空内容——与第 18/32 章迭代借用坑同族。

原因：记录在提交栈上、回调返回即回收；保存引用是把它当成了拥有式对象。

```c bad
static xlogresult slowWrite(const xlogrecord* pRecord, ptr pData)
{
	gPending = pRecord;          /* 存全局"稍后处理"——回调返回即悬空 */
	return XLOG_RESULT_WRITTEN;
}
```

```c good
static xlogresult slowWrite(const xlogrecord* pRecord, ptr pData)
{
	queue_push(pData, pRecord);  /* 队列做深拷贝：字段与消息复制后才入队 */
	return XLOG_RESULT_WRITTEN;
}
```

### 坑 2：生产全局开 DEBUG

症状：日志洪水——磁盘被刷爆、真正的 ERROR 被淹没；性能随日志量下滑。

原因：级别门槛是策略不是默认——全局 DEBUG 把"开发期诊断"开成了"永久状态"。

```c bad
xlogger* gLog = xrtLogCreate(XRT_STR_LITERAL("app"), XLOG_DEBUG);
/* 全模块 DEBUG：每条 SQL、每次缓存命中都写盘 */
```

```c good
xlogger* gLog = xrtLogCreate(XRT_STR_LITERAL("app"), XLOG_INFO);
xlogger* gDbLog = xrtLogCreate(XRT_STR_LITERAL("db"), XLOG_DEBUG);
/* 只有 db 模块定向开 DEBUG——诊断范围精确、日志量可控 */
```

## 练习

### 基础：双门槛验证

建一个 DEBUG Logger + 一个 WARN 门槛 Sink，依次提交 TRACE/DEBUG/INFO/WARN/ERROR 五条——数一数几条真正到达写函数。

### 进阶：告警 Sink

写一个自定义 Sink：只关心 ERROR 级别且带错误字段的记录，命中时把字段提取出来发到你的"告警通道"（printf 模拟即可）；其他记录返回 WRITTEN 但不写——字段消费的选择权演示。

### 挑战：内存环形 Sink

实现 Ring Sink：固定 N 条的环形缓冲，Write 复制记录（消息+字段深拷贝，第 22 章池或 arena 管内存）；暴露 `dump()` 把最近 N 条导出成文本。验收标准：提交 1000 条后 dump 恰好是最后 N 条；零泄漏（第 6 章统计验证）；写函数自身零分配（拷贝用的内存在 Sink 创建时预备）。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 分工 | Logger（命名+门槛）/ Sink（目的地+门槛）；一挂多、多复用 |
| 四要素 | 名字 / 级别 / 写函数 / 用户数据——自定义 Sink 的全部 |
| 三纪律 | 记录借用不保留 / 返回值语义 / 写函数零分配 |
| 字段族 | Int/Float/String/Time/Error/Null——保类型直达 Sink |
| 便捷层 | printf 风格提交；给机器的字段永远走 LogFields |
| 级别语义 | TRACE 图纸 / DEBUG 诊断 / INFO 里程碑 / WARN 可恢复 / ERROR 人工介入 |
| 门槛策略 | 生产 Logger INFO + 模块定向 DEBUG |
