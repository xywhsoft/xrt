---
num: 36
slug: text-pipeline
title: 文本管线组合：JSON 配置 → 模板渲染 → 协议输出
volume: 卷四 文本与结构化数据 · 卷四收官
type: composition
lead: 把前六章的全部工具串成一条真实管线——提取、解析、合并、渲染、编码、压缩各就各位。
api: json, template, value, regex, codec, compress, html
---

## 导读

卷四收官章做一件与前几章不同的事：**不再教新工具，而是把工具串成管线**。一条真实的数据管线——"日志/半结构化文本 → 正则提取 → 值树加工 → JSON 读取合并 → 模板渲染 → 编码压缩输出"——每一个环节都是前面某一章的主角。组合章的价值在于暴露"单章教学看不见的问题"：环节之间的数据契约怎么定、错误在哪一层报告、性能在哪个环节卡住、边界严格性如何在整条链上传递。读完本章，卷四从"六个工具箱"变成"一条可组装的生产线"。

## 引入

一个真实需求：运维平台要从多种来源生成"每日服务报告"。来源一：服务的 JSON Lines 日志（错误计数、耗时统计在字段里）；来源二：一段半结构化的状态文本（版本号、连接数是 `key=value` 形态——正则的领地）；来源三：平台默认配置（JSON）加用户覆盖（XSON 全类型）。产出：HTML 邮件正文（模板渲染）+ 矑文本摘要（另一模板）+ 归档副本（gzip 压缩落盘）。

这条管线六种环节六种工具，任何一环单独看都是"学过的"，连起来就出现新问题：正则提取的字符串怎么进值树（类型转换在哪做）？三份数据怎么合并成模板的输入（第 31 章 ObjectMerge 的嵌套语义）？渲染两个模板要两份数据吗（一份值树两个模板）？归档压缩在哪一步做（渲染后、落盘前）？错误报告怎么定位到"哪个来源的哪个字段"？——本章的主体就是沿着这条管线逐站回答。

## 概念

### 管线全景图

```diagram flow
- 来源 A：JSON Lines 日志 → 逐行 JSON 解析（SAX/DOM）→ 统计值树
- 来源 B：状态文本 → 正则命名捕获提取 → 转值树（字符串→数字在这里做）
- 来源 C：默认 JSON + 用户 XSON → 各自解析 → ObjectMerge 合并
- 汇聚：三份值树 ObjectMerge 成渲染数据（各占一个顶层键，避免键冲突）
- 渲染：HTML 模板与纯文本模板各渲染一次（同一份数据）
- 输出：邮件正文直发；归档副本 gzip 压缩（一次性 DeflateAll）落盘
```

### 环节间的数据契约

管线设计的第一件事不是选函数，是**定义环节间的契约**——每个环节的输出是什么形状、什么类型。三个纪律：**契约以值树为货币**——所有环节的输入输出都是 `xvalue`，类型在第 31 章的类型族里（不要发明"字符串化的数字"这类中间形态）；**转换在提取层完成**——正则捕获的天然是字符串，"看起来是数字"的转换在提取环节显式做（第 26 章严格解析），不把类型犹豫留给下游；**合并用命名空间**——三份来源各自挂进顶层对象的一个键下（`stats`/`runtime`/`config`），来源之间的键冲突从结构上消失，合并策略只需处理"同来源的覆盖"。

### 错误在哪一层报告

整条链上任何环节都可能失败，报告的归属原则：**在拥有上下文的那层报告**。JSON 解析失败——日志层知道是哪一行哪个文件（第 32 章行列定位 + 来源标注）；正则提取失败——提取层知道模式与原文（提取不到值返回"缺失"而不是报错——"字段缺失"是数据状态不是错误）；合并冲突——配置层知道键名与两份来源；模板渲染取不到路径——渲染层按配置决定报错还是空串。整链错误最终经第 4 章原因链上报：`报告生成失败 → 来源 B 提取失败 → 第 3 行缺 version 字段`——每一层贡献自己的那节链。

### 管线终点的一环：HTML 实体转义（html）

当渲染产物要进 HTML 页面或邮件正文，链尾还差最后一站：把文本里的 `<`、`&`、引号转成实体——**任何进入 HTML 上下文的动态文本都必须过这一站**，漏掉它就是注入漏洞。html 模块小而克制，三个函数一条龙：`xrtHtmlEscapeSize` 严格校验 UTF-8 并返回转义后的精确字节数（两遍法的第一遍）；`xrtHtmlEscapeWrite` 写入调用方缓冲（容量须含结尾零）；`xrtHtmlEscape` 直接产出由 `xrtFree` 释放的零结尾串。一个必须选对的参数是**上下文**：`XHTML_ESCAPE_TEXT` 用于元素内容，`XHTML_ESCAPE_ATTRIBUTE` 用于**由引号包围的**属性值——两处要转的字符集不同，上下文选错正是跨站脚本注入的经典变种。它依赖 UNICODE 能力（第 27 章字符集的用武之地），转义前对输入做严格 UTF-8 校验——畸形输入在这里被拦下而不是带进产物。顺序纪律与压缩一致：**渲染 → 转义 → 压缩**，转义永远在编码环节、绝不掺进模板。

```c
/* 输出侧最后一站：动态文本进 HTML 前的转义（上下文决定字符集） */
size_t iSize = 0;
if ( !xrtHtmlEscapeSize(UserBio, XHTML_ESCAPE_TEXT, &iSize) ) { return fail(); }
str sSafe = xrtHtmlEscape(UserBio, XHTML_ESCAPE_TEXT, NULL);
/* sSafe 进模板产物；用毕 xrtFree——拥有式串（第 25 章） */
```

### 性能与边界在链上的分布

性能分布不均：**解析与渲染是常数大头**（每字节都要过一遍），提取与合并是线性小头；优化顺序先渲染（模板编译一次）、再解析（日志用 SAX 流式）、最后才考虑微优化。边界严格性同样分层：**外部输入口（日志文件、用户配置）全部上闸**——JSON 三闸、正则的输入长度上限、压缩解压上限（第 29 章防线）；**内部环节之间信任**——值树从提取到渲染不再重复校验（校验在入口一次完成）。"外紧内松"是管线的边界哲学：把严格性预算花在不可控的入口，不浪费在自家环节之间。

### 可测试性：管线即拼图

管线化的意外收益是可测试性：每个环节是纯函数（值树进、值树出），可以独立测试；环节之间用最小契约数据（一两条记录的值树）做集成测试；端到端测试用固定输入跑全链、断言最终文本（第 12 章哈希给大输出做指纹断言）。对比"一整坨处理函数"的测试方式——管线化让每一环的失败都能定位到站，而不是在整坨里猜。

## 示例

### 完整程序：状态文本提取到值树

管线来源 B 的最小实现——正则提取 + 类型转换 + 值树构造，来自第 30 章示例的组合运用：

```c
/* pipeline_extract.c —— 来源 B：状态文本 → 值树 */
#define XRT_MODULE_ALL
#define XRT_IMPLEMENTATION
#include <xrt.h>
#include <stdio.h>
#include <string.h>

static xregex* gPair;

void extract_init(void)
{
	gPair = xrtRegexCompile(XRT_STR_LITERAL(
		"(?<name>[A-Za-z_]+)=(?<value>\\d+)"));
}

xvalue* extract_status(xstrview Text)
{
	xvalue* pRoot = xrtValueObject();
	xregexmatcher* M = xrtRegexMatcherCreate(gPair, Text);
	xstrview Name;
	xstrview Value;

	while ( xrtRegexMatcherNext(M) ) {
		int64 iNumber;
		xrtRegexMatcherCaptureNamed(M, XRT_STR_LITERAL("name"), &Name, NULL);
		xrtRegexMatcherCaptureNamed(M, XRT_STR_LITERAL("value"), &Value, NULL);
		/* 类型转换在提取层：严格解析（第 26 章），失败按缺失跳过 */
		if ( xrtIntParse(Value, 10, 0, &iNumber) ) {
			xvalue* pInt = xrtValueInt(iNumber);
			xrtValueObjectSet(pRoot, Name, pInt);
			xrtValueRelease(pInt);
		}
	}
	xrtRegexMatcherFree(M);
	return pRoot;   /* 拥有式值树，归调用方释放 */
}

int main(void)
{
	xvalue* pStatus;
	int64 iVersion;
	int64 iConnections;

	extract_init();
	pStatus = extract_status(
		XRT_STR_LITERAL("version=204 connections=32 uptime=98765"));
	if ( pStatus == NULL ) {
		return 1;
	}
	if ( xrtValueObjectGet(pStatus, XRT_STR_LITERAL("version")) == NULL ) {
		return 2;   /* 版本字段缺失：提取环节的失败在此暴露 */
	}
	printf("extracted keys=%zu
", xrtValueCount(pStatus));
	xrtValueRelease(pStatus);
	xrtRegexRelease(gPair);
	return 0;
}
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -DXRT_IMPLEMENTATION -I single pipeline_extract.c -lws2_32 -liphlpapi
$ ./a.exe
extracted keys=3
```

**这段代码的站位。** 三章工具同框：第 30 章正则的两层模型（全局编译、每次建匹配器）+ 命名捕获；第 26 章严格解析做显式类型转换（"看起来是数字"变成"确认是数字"）；第 31 章值树构造与引用配平（Set 后 Release）。这个 30 行的函数是整条管线最典型的环节形态——**值树进、值树出、自己管好自己的资源**。

### 完整程序：三源合并与双模板渲染

管线的汇聚与输出段——三份值树合并、两个模板渲染：

```c
/* pipeline_render.c —— 汇聚 → 渲染 → 输出 */
#define XRT_MODULE_ALL
#define XRT_IMPLEMENTATION
#include <xrt.h>
#include <stdio.h>

static xtemplate* gHtml;
static xtemplate* gPlain;

void render_init(xstrview HtmlTpl, xstrview PlainTpl)
{
	gHtml = xrtTemplateCompile(HtmlTpl);
	gPlain = xrtTemplateCompile(PlainTpl);
}

int render_report(xvalue* pStats, xvalue* pRuntime, xvalue* pConfig,
	FILE* pArchive)
{
	xvalue* pData = xrtValueObject();
	xvalue* pHtml;
	str sPlain;
	int iResult = 0;

	/* 命名空间合并：各来源占一个顶层键——冲突从结构上消失 */
	if ( pData != NULL ) {
		xrtValueObjectSet(pData, XRT_STR_LITERAL("stats"), pStats);
		xrtValueObjectSet(pData, XRT_STR_LITERAL("runtime"), pRuntime);
		xrtValueObjectSet(pData, XRT_STR_LITERAL("config"), pConfig);
	}
	pHtml = xrtTemplateRender(gHtml, pData, NULL, NULL);
	sPlain = xrtTemplateRender(gPlain, pData, NULL, NULL);
	if ( (pHtml == NULL) || (sPlain == NULL) ) {
		iResult = 1;
		goto Cleanup;
	}
	printf("%s\n", sPlain);
	/* 归档：紧凑渲染的结果整块压缩（一次性路径）——第 29 章 */
	{
		xbytesview View = { (cbytes)sPlain, strlen(sPlain) };
		size_t iGzSize = 0;
		bytes pGz = xrtDeflateAll(View, NULL, &iGzSize);
		if ( pGz != NULL ) {
			fwrite(pGz, 1, iGzSize, pArchive);
			xrtFree(pGz);
		}
	}
Cleanup:
	xrtFree(sPlain);
	xrtFree(pHtml);
	xrtValueRelease(pData);
	return iResult;
}

int main(void)
{
	xvalue* pStats;
	xvalue* pRuntime;
	xvalue* pConfig;
	FILE* pArchive;
	int iResult;

	render_init(XRT_STR_LITERAL("errors={%stats.errors}"),
		XRT_STR_LITERAL("plain: {+$config.name}"));
	pStats = xrtValueObject();
	pRuntime = xrtValueObject();
	pConfig = xrtValueObject();
	if ( (pStats == NULL) || (pRuntime == NULL) || (pConfig == NULL) ) {
		return 1;
	}
	{
		xvalue* pErrors = xrtValueInt(3);
		xvalue* pName = xrtValueString(XRT_STR_LITERAL("daily"));
		xrtValueObjectSet(pStats, XRT_STR_LITERAL("errors"), pErrors);
		xrtValueObjectSet(pConfig, XRT_STR_LITERAL("name"), pName);
		xrtValueRelease(pErrors);
		xrtValueRelease(pName);
	}
	pArchive = fopen("report.gz", "wb");
	if ( pArchive == NULL ) {
		return 2;
	}
	iResult = render_report(pStats, pRuntime, pConfig, pArchive);
	fclose(pArchive);
	xrtValueRelease(pStats);
	xrtValueRelease(pRuntime);
	xrtValueRelease(pConfig);
	xrtTemplateRelease(gHtml);
	xrtTemplateRelease(gPlain);
	return iResult;
}
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -DXRT_IMPLEMENTATION -I single pipeline_render.c -lws2_32 -liphlpapi
$ ./a.exe
plain: daily
$ ls -l report.gz
（归档副本已生成：紧凑渲染 + gzip 一次性压缩）
```

**这段代码的站位。** ① 命名空间合并——三个来源各自挂键，`ObjectMerge` 的覆盖语义根本不需要出场（跨来源无冲突）。② 一份数据两个模板——HTML 与纯文本共享同一个 `pData`，模板差异只在前缀与控制结构。③ 归档在渲染后、落盘前——一次性压缩正好匹配"整块文本"的形态。④ 引用配平清晰：pData 挂完三个来源即可释放（容器持引用），两个渲染产物各自 `xrtFree`。这段 50 行代码把第 31/35/29 章的主角按管线次序全部请上了台。

### 环节设计的四个检查项

写出每个环节函数时过一遍四个检查项，管线质量就有了底线保障。**检查一：进出都是值树吗**——环节签名应该是 `xvalue* f(输入视图或值树)`，出现 `char**`、`struct` 专有类型就意味着契约破了口子。**检查二：资源自理吗**——环节内部创建的匹配器、值树、缓冲在环节内配平；输出之外的中间产物不泄漏给调用方（调用方只接住返回的拥有式产物）。**检查三：失败路径同构吗**——每个环节的失败都走同一种形态（返回 NULL 加错误槽），编排层不需要为不同环节写不同的失败处理。**检查四：可单测吗**——给环节喂最小输入（一两条记录）能独立运行与断言。四个检查项过完，环节就是一颗可替换的螺丝钉——编排层换顺序、并行化、加缓存都不影响环节本身。

### 编排层：管线的指挥家

环节之外的编排层同样有设计原则。**顺序声明化**——环节的执行次序写在编排函数里一眼可见（顺序即代码），不要藏在环节的副作用里；**数据流单向**——值树从上一环节流向下一环节，不回流、不共享可变状态（并行化的前提）；**早退与部分成功**——决定哪些环节失败即中止（配置读不了就别渲染）、哪些允许部分成功（日志统计失败但状态文本可用，报告标注"统计缺失"继续生成）——这个决策属于业务，写在编排层且写进注释。编排层应该短到可以一口气读完：一个正常人能在三十秒内看清"数据从哪来、经过哪几站、到哪去"。

### 一个真实案例的数字

给这条管线一组参考量级，建立工程直觉：日报告场景——日志 50MB（SAX 流式解析，峰值内存 MB 级）、状态文本 2KB（正则提取）、配置 10KB（双格式合并）、渲染两份输出约 200KB（模板编译一次）、归档 gzip 后约 30KB。整条管线单机串行耗时秒级，其中解析占七成、渲染占两成——性能画像与"先渲染后解析"的优化次序并不矛盾：渲染优化的意思是编译一次（省的是重复编译），解析优化的意思是流式（省的是内存），两者都不改变"解析是耗时大头"的事实。你的管线第一步永远是量出自己的数字，再用本章的次序观排优先级。

## 契约

- **值树是货币**：环节间输入输出一律 `xvalue`；类型转换在提取层显式完成。
- **命名空间合并**：多来源各占顶层键；覆盖语义只处理同来源冲突。
- **错误归属**：在拥有上下文的层报告；整链经原因链逐层上报；"缺失"是数据状态不是错误。
- **边界哲学**：外紧内松——外部入口全上闸（三闸/长度/解压上限），内部环节信任。
- **性能次序**：先渲染（编译一次）、再解析（流式）、最后微优化。
- **资源配平**：每环节管好自己的资源（匹配器、值树、产物），失败路径同构。
- **HTML 转义上下文**：TEXT 元素内容 / ATTRIBUTE 引号属性值——上下文选错字符集即漏转义；转义在渲染后、压缩前。

### 演进路径：从直译到管线

管线不是设计出来的第一版，而是重构出来的第二版——认识这条演进路径，比一步到位更有实操价值。**第一版（直译）**：一个大函数从输入干到输出，跑通了业务——它不是错误，是起点，价值在于验证需求。**第二版（环节化）**：把大函数按"数据形态转换"切站——每个 `char*` 到 `char*` 的段落变成"视图到值树"或"值树到文本"的环节——坑 2 的重构就是这个动作。**第三版（契约固化）**：环节签名统一、错误形态统一、单测补齐——本章"四个检查项"的落实。**第四版（编排进化）**：环节不变，编排层升级——日志解析与状态提取并行（两站无数据依赖）、配置加载提前到启动（不变数据不重复读）、渲染加缓存（同数据同模板的输出指纹缓存）。每一版改的都是连接方式而非环节本身——这正是管线的本义：**环节稳定、编排演进**。

### 与卷五~卷九的接口预告

这条管线不是孤例，它是全书后续卷的预演。卷五（第 38 章起）的 Logger 是"渲染后的文本去哪"的下一站（sink 输出）；卷七网络引擎把"JSON 解析 + 模板渲染"装进请求回调——同样的管线跑在连接的上下文里；卷九 HTTP 的内容协商决定"渲染产物要不要压缩、用什么编码"——本章的 gzip+Base64 组合在协议层再次出现。带着管线的思维读后续各卷：每个新模块都是某个环节的深度展开，而你已经在组合章见过它们站位的全景。

## 避坑

### 坑 1：类型转换散落在下游

症状：模板渲染处处报"类型不符"；或下游到处出现"如果是字符串就再转一次"的防御代码——同一份数据被转换了三次。

原因：提取层偷懒把字符串原样放进值树，类型责任被推给所有下游——契约失守。

```c bad
/* 提取层：把数字当字符串放进值树 */
xvalue* pVal = xrtValueString(Value);   /* "128" 是字符串值 */
/* 下游模板：{%runtime.port} 类型不符——报错或到处转换 */
```

```c good
/* 提取层完成转换：进值树的就是最终类型 */
int64 iNumber;
if ( xrtIntParse(Value, 10, 0, &iNumber) ) {
	xvalue* pInt = xrtValueInt(iNumber);
	xrtValueObjectSet(pRoot, Name, pInt);
	xrtValueRelease(pInt);
}
/* 类型不确定的字段在提取层就标注（或不进树），下游零防御 */
```

### 坑 2：整链一个大函数

症状：`generate_report()` 三百行——日志解析、正则提取、合并、渲染、压缩全在一个函数体里；任何一环出错都要在全函数里定位；测试只能端到端。

原因：管线没有环节化——"能跑就行"的直译写法把六个环节焊死在一起。

```c bad
str generate_report(void)
{
	/* 300 行：解析日志 → 提取状态 → 读配置 → 合并 → 渲染 → 压缩…… */
	/* 中间量全是局部变量，任何一段都抽不出来单独测试 */
}
```

```c good
/* 每个环节一个函数：值树进、值树出、资源自理 */
xvalue* collect_stats(xstrview LogText);      /* 环节 1 */
xvalue* extract_status(xstrview StatusText);  /* 环节 2 */
xvalue* load_config(void);                    /* 环节 3 */
int render_report(xvalue* Data, FILE* Out);   /* 环节 4+5 */
/* 主流程只剩编排——每个环节可单测、可替换、可复用 */
```

### 逐站精讲：来源 A 的统计环节

来源 A（JSON Lines 日志统计）是管线的性能大头，值得单独展开。形态选择：日志文件可能几十 MB——DOM 逐行解析（每行一个 JSON 对象、行级 Parse）是标准姿势；行迭代用第 25 章的行分割，每行独立解析互不影响（一行损坏不影响其他行的统计——部分成功的语义在环节内成立）。统计的累积值放进值树：错误计数是 int、耗时分布是数组、按模块的分组是对象——`ObjectSetNew` 族边解析边累积。两道闸就位：行长度上限（畸形日志行不撑爆内存）与条目数上限（循环次数有界）。这个环节的输出契约：`{errors: int, p50/p95: int, modules: {name: count}}`——下游模板按这个形状取值，形状变更时模板的冒烟测试立刻报警。

### 逐站精讲：渲染与输出的分离决策

渲染与输出（压缩、落盘、发送）在管线里是两站还是一站？取决于输出目标的多样性。**单一输出**（只落盘）——渲染函数末尾直接压缩写文件，一站收工；**多输出**（邮件正文 + 归档副本，本章场景）——渲染只产出文本、输出环节各自消费——因为两个输出对文本的加工不同（邮件直接用、归档要压缩），合并成一站会让"输出格式"的选择污染渲染函数。判断口诀：**输出的加工方式有几种，输出环节就有几站**。同一原则也回答"压缩在渲染前还是渲染后"：压缩的对象是最终文本（渲染后），不是值树（渲染前没有可压的文本）——这个问题在架构评审里被问到的频率高得惊人。

### 测试金字塔的展开

可测试性小节给了三层金字塔的骨架，这里展开每层的具体做法。**环节单测**（底座）：每个环节函数配最小输入集——正常输入、空输入、畸形输入、边界输入各一；断言输出值树的形状与关键值（第 31 章精确读取做断言、`ValueHash` 做整树指纹）。**契约集成测试**（中层）：相邻两站串联——上一环节的真实输出喂给下一环节，验证契约两侧的理解一致（提取环节说"port 是 int"，模板环节也这么认为——串联测试让分歧当场暴露）。**端到端指纹测试**（顶层）：固定输入跑全链，最终文本的 `Hash64` 写进断言——任何环节的任何变更改变了输出，测试立刻红。三层的比例大约 7:2:1——底座最厚、顶层最薄，修改时红的层次告诉你问题在哪级（环节坏了底座红、契约变了中层红、整体输出变了顶层红）。

## 练习

### 基础：两站管线

实现"JSON 配置 → 模板渲染"最小管线：解析配置（含 2 个字段）、构造值树、渲染一个三前缀模板。两个环节各自独立成函数。

### 进阶：三源汇聚

复现组合章的汇聚段：硬编码三份来源值树（各自 2~3 个键）、命名空间合并、两个模板（HTML 与纯文本）渲染同一份数据。验证两个输出的字段一致性。

### 挑战：完整报告管线

实现引入场景的完整管线：JSON Lines 日志统计（SAX 计数）+ 状态文本提取（正则）+ 配置合并（JSON+XSON）+ 双模板渲染 + gzip 归档。验收标准：五个环节各自为独立函数且各有单测；端到端固定输入产出确定文本（哈希断言）；三处边界闸全部就位；第 6 章统计验证零泄漏；总代码不超过 300 行——管线的意义就在这个数字里。

### 从组合章到卷四收官

本章是卷四的收官，也是"组合章"这个新章节类型的第一次亮相——它的定位与单模块章不同：**不教新 API，教组装能力**。全书的后续卷还会有组合章（卷五的"调试与诊断组合"、卷六的"调度实战"），它们的共同结构都是"全景图 → 数据契约 → 逐站精讲 → 演进路径"——本章建立的这个结构会被复用。对读者的最后建议：把本章的两个完整程序亲手跑一遍，然后把挑战练习的完整管线做出来——三百行以内的约束不是刁难，是提醒：**管线的价值在于环节清晰，不在于代码量**；写超了，说明有环节该拆或该删。

### 卷四总回顾

十章走完，卷四的资产清单：字符串（视图管线/构建器）、数值（严格/最短往返/格式串）、字符集（转码/标量操作）、编解码（三座桥）、压缩（两姿势/账本）、正则（线性/两层）、值树（货币/引用计数）、JSON（三路径）、XSON（全类型）、模板（编译渲染分离）——十个模块加上本章的组装能力，文本与结构化数据的处理链路完整闭环。下一卷进入系统服务（日志、IO、时间、文件、进程）——数据层的下一层，管线的输出端与配置端都将在那里找到归宿。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 货币 | 值树贯穿全链；类型转换在提取层一次性显式完成 |
| 汇聚 | 多来源命名空间合并（各占一个顶层键）；覆盖语义只管同来源冲突 |
| 错误 | 在拥有上下文的层报告；原因链逐层上报；缺失不等于错误 |
| 边界 | 外紧内松：入口全上闸（三闸/长度/解压上限），内部环节信任 |
| 性能 | 渲染编译一次 → 解析流式 → 最后才轮得到微优化 |
| 结构 | 每环节一个函数：值树进出、资源自理、失败同构、可单测可替换 |
| 测试 | 环节单测 + 契约集成 + 端到端指纹——三层金字塔，比例约 7:2:1 |
| 环节检查 | 值树进出 / 资源自理 / 失败同构 / 可单测——四项过完才是螺丝钉 |
| 编排原则 | 顺序声明化 / 数据流单向 / 早退与部分成功显式决策 |
| 渲染输出分离 | 输出加工方式有几种，输出环节就有几站；压缩在渲染后 |
| HTML 转义 | 动态文本进 HTML 必转义；TEXT/ATTRIBUTE 两上下文；严格 UTF-8 校验前置 |
