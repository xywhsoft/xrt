---
num: 32
slug: json
title: JSON 读写
volume: 卷四 文本与结构化数据
type: practice
lead: DOM、SAX 事件流、增量 Writer 三条路径——配置解析、大文件扫描、流式序列化各取所需。
api: json, value
---

## 导读

JSON 是当代数据交换的通用语，XRT 给它三条完整路径：**DOM**（`xrtJsonParse` 解析成 xvalue 树——配置与小型文档的默认选择）、**SAX**（`xrtJsonVisit` 事件回调流式遍历——大文件零中间树）、**Writer**（`xrtJsonWriterCreate` 增量构建——流式序列化不攒整树）。三条路径共享同一套严格性：重复键拒绝、资源边界（第 3 章三闸）防深嵌套炸弹、错误带行列位置（`xrtJsonErrorLocation`）。本章把三条路径各自的适用场景讲清，并给"选路径"的决策规则。

## 引入

三个场景三条路径。**配置加载**：启动时读一份几百行的 JSON——DOM 路径三行代码（Parse、按名取、用完 Release），值树直接进第 30 章的合并/读取体系。**日志扫描**：遍历一个 2GB 的 JSON Lines 日志找异常事件——DOM 要把整文件变成树（内存爆炸），SAX 逐事件回调、零中间结构、内存与文件大小无关。**响应序列化**：服务端把查询结果写成 JSON 响应——数据本来就在业务结构里，先搭值树再序列化是浪费；Writer 直接增量写：开对象、写键值、闭对象——边生成边输出。

三条路径不是三选一的玄学，是**三个数据形态问题**的答案：需要随机访问与反复读取 → DOM；单遍扫描 → SAX；边生成边输出 → Writer。一个系统里三者共存是常态——配置走 DOM、日志走 SAX、响应走 Writer，同一个项目三个都用在各自最合适的位置。

## 概念

### DOM：解析、访问、序列化

```diagram flow
- 解析：xrtJsonParse(文本) → 拥有式根值（Release 释放）
- 访问：ObjectGet 按名取成员（借用）→ 精确读取（第 30 章）
- 序列化：xrtJsonStringify(值, bPretty) → 拥有式文本
- 严格性：重复键拒绝；资源边界三闸；错误带行列位置
```

DOM 的价值是"解析一次、随机访问"：值树在内存里，按路径任意跳转。`xrtJsonParse` 失败时线程错误槽有结构化信息，`xrtJsonErrorLocation` 取出行列——"第 3 行第 14 列少了逗号"直接指到字符。浮点序列化用第 26 章的最短往返——读进来的 `0.1` 写回去还是 `0.1`，往返零漂移。

### SAX：事件回调流

`xrtJsonVisit(文本, 回调, 用户数据)` 把解析过程变成事件序列：进入对象/数组、成员名、标量值、容器结束。回调拿到 `xjsonevent` 结构（HasName/Name/类型/值），返回 false 中止。**零中间树**是它的形态优势：内存占用就是回调自己的栈；配合第 27 章流式解码可以做到"边收边解析"。SAX 的纪律：回调里不要做重活（它在解析路径上）、不要保存事件里的借用指针过回调期（第 30 章时效纪律）。

### Writer：增量构建

```c
xjsonwriter* W = xrtJsonWriterCreate(NULL);
xrtJsonWriterObject(W);          /* 开对象 */
xrtJsonWriterName(W, "name");
xrtJsonWriterString(W, ...);
xrtJsonWriterEnd(W);             /* 闭对象 */
str s = xrtJsonWriterFinish(W);  /* 校验配对完整性并取走 */
```

开闭配对由 `Finish` 校验——少个 End 在这里暴露，不是产出残缺 JSON。Writer 支持输出到内存（Take 拥有式串）或直接写 sink（第 35 章 Logger 的 sink 体系同构）——后者让序列化直连文件/网络。

### 路径选择规则

| 场景 | 路径 | 理由 |
| --- | --- | --- |
| 配置/小型文档 | DOM | 随机访问、代码最短 |
| 大文件单遍扫描 | SAX | 内存与文件大小无关 |
| 边生成边输出 | Writer | 不搭中间树 |
| 需要修改再序列化 | DOM | 树上编辑后 Stringify |

### 资源边界与严格性

不受信任 JSON 的三道闸（第 3 章 `xrtresourcelimits`）：嵌套深度、条目数、总字节数——恶意文档在第 128 层被拒，不把栈吃穿。重复键拒绝（配置文件的"同名键覆盖"是事故不是特性）；数字解析走第 26 章严格语义。**信任边界两侧的解析配置不同**：本地生成的配置默认边界即可，外部输入收紧三闸。

## 示例

### 完整程序：三条路径一次走完

来自仓库范例 `examples/data/json/main.c`——同一份数据用 DOM 读、SAX 扫、Writer 写：

```embed path="examples/data/json/main.c" title="examples/data/json/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/data/json/main.c -lws2_32 -liphlpapi
name = xrt
{
  "name": "xrt",
  "features": [
    "json",
    "http"
  ]
}
member: code
member: ok
{"code":200,"message":"OK"}
```

**刚才发生了什么。** ① DOM 段：`Parse` 得到根值，`ObjectGet("name")` 按名取成员（借用不转移所有权），`GetString` 读出视图打印——三步就是配置读取的日常形状。② 序列化段：`Stringify(值, true)` 美化输出——缩进格式供人看；机器交互用 false（紧凑），体积更小。③ SAX 段：`Visit` 逐事件回调，输出 `member: code/ok`——事件流里每个成员名都到过回调的手上，全程没有中间树。④ Writer 段在文件后半：增量构建与 `Finish` 校验——最后一行 `{"code":200,"message":"OK"}` 就是 Writer 的产物；读代码时注意每个 Object/Name/Int/String 都与 End 配对。⑤ 巡礼示例的第五行验证错误位置：构造缺逗号的输入，`error-location line=1` 精确到行——第 4 章"错误自带定位"的又一次兑现。

### 完整程序：全接口巡礼（文件与流式）

来自 `examples/data/json_tour/main.c`，覆盖文件解析、文件写出、流式与 sink 写：

```embed path="examples/data/json_tour/main.c" title="examples/data/json_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/data/json_tour/main.c -lws2_32 -liphlpapi
json: read+valid ok file-parse/read ok
json: write-file/stringify-file(pretty) ok
json: stream write=23 quote=9
json: sink-writer [[1,true,null,2.5,"v"]] ok
json: error-location line=1 ok
```

**刚才发生了什么。** ① 文件路径：Parse 与 Stringify 直接对接文件——配置加载与导出的文件形态。② 流式写：Writer 逐元素写、计数 23 个元素 9 个引号——"边生成边输出"的实证。③ sink 写：Writer 输出直接接目标（缓冲/文件）——序列化与 IO 的组合形态，省掉中间字符串。四种姿势覆盖了 JSON 与文件系统（第 45 章）的全部交汇点。

## 契约

- **三路径**：DOM 随机访问 / SAX 单遍零树 / Writer 增量输出；按数据形态选，不按喜好选。
- **严格性**：重复键拒绝；数字走第 26 章严格语义；错误带行列位置（`xrtJsonErrorLocation`）。
- **资源边界**：不受信任输入必设三闸（深度/条目/字节）——防线在解析入口不在业务层。
- **往返精度**：浮点序列化用最短往返——读写闭环零漂移。
- **所有权**：Parse 根值 Release 释放整树；ObjectGet 借用；Stringify 产物 `xrtFree`。
- **Writer 配对**：开闭由 Finish 校验；输出可取走（内存）或直连 sink。

### 从示例到工程：JSON 的三个宿主

**配置层**：启动加载 → DOM 值树 → 第 30 章合并体系（默认+用户覆盖）→ 全程只读。配置的三个纪律：三闸按业务收紧（配置文件不会有万层嵌套）、字段缺失与类型不符都要报告（第 30 章精确读取）、热更新时旧树 Release 新树替换（引用计数保证在用请求安全读完）。**协议层**：每个请求的 JSON 体解析——SAX 或 DOM 按体量选，资源边界必设（外部输入），错误位置写进 400 响应帮助客户端自查。**数据交换层**：模块间与跨进程传递——值树直接传（引用计数配平，见第 30 章三宿主）或序列化后走通道（最短往返保证精度）。三层用同一套严格性，差异只在边界宽度与错误报告的去向。

### 一个对照：三路径的代码形状

同一个"取用户名字段"的需求在三路径下的形状，值得并排看一遍。DOM：`Get(Get(Root,"user"),"name")` 两跳取到——最短、可反复访问。SAX：回调里维护"当前在 user 对象内"的状态机、遇到 name 事件记录——代码长但内存恒定。Writer：反向问题（生成而不是读取），Name+String 两步——没有"取"只有"给"。三段代码并排读一次，路径选择的直觉就有了：**访问模式决定代码形状，形状别扭通常意味着选错了路径**。

## 避坑

### 坑 1：外部 JSON 不设资源边界

症状：处理用户上传的 JSON 时栈溢出或内存暴涨——万层嵌套的数组文本把递归解析器的栈吃穿。

原因：默认资源边界宽松（面向可信配置）；外部输入必须收紧三闸——防线在解析入口。

```c bad
xvalue* pRoot = xrtJsonParse(Text, NULL);   /* 默认边界——万层嵌套直接爆栈 */
```

```c good
xrtresourcelimits Limits;
xrtResourceLimitsInit(&Limits);
Limits.iMaxDepth = 64;      /* 业务最深 3 层，给 64 已是十倍余量 */
xvalue* pRoot = xrtJsonParse(Text, &Limits);
if ( pRoot == NULL ) {
	size_t iLine, iColumn;
	xrtJsonErrorLocation(xrtGetError(), &iLine, &iColumn);
	Reject(iLine, iColumn);   /* 超限即拒绝——错误还带位置 */
}
```

### 坑 2：SAX 回调里保存借用指针

症状：扫描完成后使用保存的字符串指针，读到悬空或被复用内容——与第 18 章坑 1 同族。

原因：事件里的字符串是借用视图，时效止于本次回调——解析器复用缓冲。

```c bad
static bool onEvent(const xjsonevent* pEvent, ptr pData)
{
	if ( pEvent->Type == XJSON_EVENT_STRING ) {
		gSaved = pEvent->String;   /* 借用视图存全局——下次回调即悬空 */
	}
	return true;
}
```

```c good
static bool onEvent(const xjsonevent* pEvent, ptr pData)
{
	if ( pEvent->Type == XJSON_EVENT_STRING ) {
		Process(pEvent->String);   /* 回调期内用完即弃 */
		/* 需要保留：Dup 一份再保存 */
	}
	return true;
}
```

## 练习

### 基础：配置读取器（DOM 三步姿势）

用 DOM 路径读取 `{ "host": "0.0.0.0", "port": 8080, "debug": true }`：按名取出三个字段并打印（端口与开关走第 30 章精确读取）。

### 进阶：JSON Lines 扫描器

用 SAX 路径扫一个 `.jsonl` 文件（每行一个 JSON 对象）：逐行解析、回调里统计每种事件类型的次数、找到 `"level":"error"` 的行号。提示：行分隔即"每行调用一次 Visit"；配合第 25 章的行迭代。

### 挑战：流式响应生成器

实现 `write_page(Writer, 页号, 数据数组)`：Writer 增量写出一个分页响应对象（元数据 + 数据数组），数据元素来自业务结构（不搭值树）；输出同时接内存与文件 sink 两路。验收标准：产出 JSON 通过 Parse 校验（往返合法）；100 万元素的生成峰值内存与元素数无关（流式实证）；文件与内存两路产物逐字节一致。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 三路径 | DOM（Parse/Get/Stringify）/ SAX（Visit 事件流）/ Writer（增量构建） |
| 选路规则 | 随机访问→DOM / 单遍扫描→SAX / 边生成边输出→Writer |
| 严格性 | 重复键拒绝；错误带行列（`xrtJsonErrorLocation`） |
| 三闸 | 深度/条目/字节——外部输入必设，超限整体拒绝并带位置报告 |
| 浮点 | 最短往返序列化——读写闭环零漂移 |
| Writer | 开闭配对 Finish 校验；Take 取走或直连 sink |
| 所有权 | 根值 Release 整树释放；成员访问借用；热更新旧树安全退场 |
| 三宿主 | 配置层（合并体系）/ 协议层（三闸+错误回显）/ 数据交换层（值树或序列化） |
| 形状判据 | 访问模式决定代码形状——形状别扭通常意味着路径选错 |
