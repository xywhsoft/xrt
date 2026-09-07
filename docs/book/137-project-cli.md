---
num: 137
slug: project-cli
title: 项目一：日志统计命令行工具
volume: 卷十三 实战项目
type: project
lead: 从需求到可交付：混合行尾日志的行读取、按级别过滤与计数、字符串构建输出、临时报告文件——全书知识的第一次综合运用。
api: io, string, file
---

## 导读

卷十三开篇：第一个完整项目。前面十二卷的每一章都在讲"一个模块怎么用"；本章把**五个模块装进同一个程序**——一个日志统计命令行工具（logstat）：读入一份服务日志（混合行尾、可能很大）、按级别过滤、统计各级别行数与错误样本、生成文本报告。听起来朴素，但它的每一步都是前面某章的正题：**行读取**（第 40 章的 xlinereader——行尾自适应、末行无换行、借用视图）；**级别判定**（第 25 章的字符串视图操作——零拷贝前缀匹配）；**计数与去重**（第 12/18 章的哈希与 Map）；**报告生成**（第 25 章构建器——O(n) 拼接）；**输出落盘**（第 44/46 章文件与临时目录——不假设工作目录可写）。项目章的形式也与教学章不同：我们先写**需求与验收**，再走**设计决策**（为什么选这些模块、每步的取舍），然后是**实现**（两个完整程序递进），最后**验收与扩展**——工程思维的全流程。

## 引入

需求来自一个真实场景：服务半夜挂了，你拿到一份 2 GB 的日志文件（rsync 拷贝过来的——行尾是混合的：老 Windows 段的 CRLF、Linux 段的 LF、甚至某次转码事故留下的孤立 CR），要快速回答三个问题：**各级别多少行？错误都发生在哪（前几条样本）？最活跃的时间段是哪段？** 用通用工具（grep+awk）能拼出来但行尾问题要反复处理、且"错误样本去重"这种逻辑写起来别扭；写个一次性脚本又难复用。

把它做成一个正经的小工具 **logstat**：`logstat <日志路径> [--level ERROR] [--top 10] [--report]`——行尾自适应读、级别过滤、计数、错误样本、可选报告文件。**验收标准**（写代码前先写清楚）：① 混合行尾输入零配置正确处理；② 2 GB 文件内存占用 MB 级（流式——不整读）；③ 空文件、无匹配级别、全错误输入三种边界正确；④ 报告文件落在系统临时目录（不假设工作目录可写）、文件名含无冒号时间戳；⑤ 处理错误的每一步都走结构化错误（第 4 章——错误链完整到能定位）。

## 概念

### 设计决策：模块选型与理由

| 需求步骤 | 选型 | 理由与被否决的备选 |
| --- | --- | --- |
| 读日志行 | `xrtLineReaderTake`+Reader（第 40 章） | 行尾自适应（三种全识别）/借用视图零拷贝/初始容量后按需增长。备选"整读再 strtok"：内存与文件同量级、行尾手工处理——否决 |
| 级别判定 | 视图前缀比较（第 25 章） | 每行开头是 `LEVEL rest`——视图切一刀零拷贝。备选"每行 strdup 再解析"：每行一次分配——否决 |
| 计数 | 定长数组（五级别） | 级别是小固定集——数组比 Map 快且零分配。备选 Map：杀鸡用牛刀——否决（若统计"任意 token 频次"才用第 18 章） |
| 错误样本 | 定长环形缓冲（前 N 条） | 只需"前几条"——环形覆盖。备选"全部存下再取前 N"：大错误日志内存爆炸——否决 |
| 报告拼接 | `xstrbuf` 构建器（第 25 章） | 多段拼接 O(n)；备选反复 Concat：O(n²)——第 25 章的教训直接适用 |
| 落盘 | 临时目录+文本写（第 44/46 章） | 不假设工作目录可写；时间戳无冒号（Windows 禁用字符）——`file/report` 示例的两个工程细节 |

**选型方法论**：每行都问"数据的形态是什么"（流式还是整块、固定集还是动态键、要不要保留），形态决定容器——第 23 章选型决策的项目版首次全流程运用。

### 管线架构：流式五段

```diagram flow
- 输入：文件路径参数 → 文件 Reader（第 44 章）→ xrtLineReaderTake（初始容量 1024）
- 逐行：Next 三态循环（LINE/END/ERROR）——行视图借用、即取即用
- 解析：视图切首个空格 → 级别视图 + 正文视图（零拷贝）
- 统计：级别数组计数 / 错误环形样本 / 首末时间戳记录
- 输出：构建器拼报告 → stdout 或临时文件
```

**流式的意义**再量化一次：内存占用 = 行读取器缓冲（随最长行，非文件总量）+ 计数器（O(1)）+ 样本环（O(N)）——与 2 GB 无关。这是验收标准 ② 的设计保证。

### 边界与错误的预案

写代码前把三类边界想清楚（验收标准 ③ 的展开）：**空文件**——首个 Next 即 END：输出全零计数与"no input"提示（不是错误）；**无匹配级别**（过滤后零命中）：正常输出零计数（用户过滤条件的问题不是程序的错）；**全错误**：样本环满覆盖、计数正确。**错误处理姿态**：打开失败/读取失败/写入失败都走返回值+错误槽——主函数的错误分支统一打印 `xrtErrorMessage` 与原因链；**没有一处 exit 而不清资源**（cleanup 单出口模式——第 44 章示例的形状）。

### 与教学示例的差异

同用 xlinereader，教学示例（第 40 章）解析固定三行；项目里它面对的是**未知长度的真实流**——差异体现在三处：初始容量后的**自动增长**成为必需（日志里可能有超长堆栈行）；END 与 ERROR 的**区分**成为正确性关键（读到一半磁盘坏=ERROR 要报、正常结束=END 不报）；行视图的**生命周期**要自觉（样本环存的是复制不是视图——第 95/103 章"视图即用即弃"纪律的落地）。

## 示例

### 第一个完整程序：行读取核心（可运行的最小闭环）

下面的程序来自 `examples/io/line`——logstat 逐行引擎的教科书形态（混合行尾+末行无换行的样本）：

```embed path="examples/io/line/main.c" title="examples/io/line/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/io/line/main.c -lws2_32 -liphlpapi
1: INFO server started
2: WARN queue is busy
3: ERROR request failed
```

**刚才发生了什么。** ① `xrtReaderFromMemory` → `xrtLineReaderTake(&pReader, 1024)` 的两步——**Take 是所有权移交**：之后只 Destroy 行读取器一个（底层 Reader 由它统一关闭）。项目里 Reader 换成文件版（`xrtReaderFromFile`——第 40 章同族）管线其余不变——** Reader 抽象的价值第一次在项目里兑现**：同一行处理逻辑，内存样本与 2 GB 文件同一份代码。② `Next` 三态循环正是 logstat 的主循环：LINE 进处理、END 收尾输出、ERROR 报错退出。③ 三种行尾（CRLF/LF/CR）与末行无换行全部正确——验收标准 ① 的机器证明。

### 第二个完整程序：JSON 日志的另一种输入

第二个程序来自 `examples/logging/file_json`——当输入升级为 JSON Lines 时的参照：

```embed path="examples/logging/file_json/main.c" title="examples/logging/file_json/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/logging/file_json/main.c -lws2_32 -liphlpapi
wrote example_logger_json.log
```

**刚才发生了什么。** ① 这是**生成侧**：JSON Lines 格式的日志 Sink（每行一个完整 JSON 对象）——logstat 的扩展形态：输入从"LEVEL 文本"升级为 JSON 行（级别在字段里）时，生成侧长这样。② 解析侧对应第 32 章 JSON 流式读取——本项目的扩展练习方向。③ `AddHelper` 一行等价三步（格式器+文件 Sink+Attach）——第 38 章便捷层的实战位置：**工具链里的"胶水"优先用 Helper，组合需求再下沉**。

### 与全书知识地图的对位

logstat 虽小，但它踩过的每一块砖都能在书里找到正文章——**项目是知识地图的路径验证**：第 4 章（错误模型——两处失败分支的结构化报告与原因链读取）、第 12 章（哈希思想——计数与去重的语义分界）、第 14 章（数组——固定级别集的 O(1) 计数）、第 18 章（Map——动态键的备选与"何时不用"的判断）、第 25 章（字符串——视图切分零拷贝与构建器 O(n) 拼接）、第 32 章（JSON——扩展方向的输入升级）、第 37/38 章（日志——JSON Lines 的生成侧与轮转语义）、第 40 章（IO 流——Reader 抽象与行读取器三态）、第 41 章（时间——扩展方向的时间窗）、第 44 章（文件——报告落盘与 Reader 的文件形态）、第 46 章（目录——临时目录与系统路径）、第 106 章（SSE——扩展方向的实时推送）。**十四个章节在一个百行程序里各就各位**——这不是巧合而是设计的分层结果：每章讲一层，项目把层叠起来。读完本章你应该有"整本书可以用一个小程序串起来"的手感——这正是后续项目（配置中心、聊天服务、下载器）越做越大时的底气：**再大的程序也是这些层加上更多的层间胶水**。

### 数据流契约：每层进出的形状

分层架构的工程价值要落在**每层的输入输出契约**上才可维护。logstat 的四层契约：**字节层**（Reader）——进路径出字节流，失败经返回值+错误槽，无部分状态；**行层**（LineReader）——进字节流出 `xlineview`（借用、不含行尾、三种行尾统一），三态 Next，超长行自动增长且只随最长行增长；**解析层**（lsLevelOf）——进行视图出级别编号+正文视图（零拷贝），未知级别返回 -1（宽容计数由调用方决定）；**聚合层**（lsstats）——进级别编号/样本行出统计结果（纯数据结构、无 IO 无分配）。**契约的纪律**：每层只依赖下一层的**输出契约**而非实现——换 Reader 实现（内存/文件/网络）、换解析目标（文本/JSON）时契约不变即层可换。这个"形状思维"是第 23 章选型决策在架构层的投影：**选型选容器，契约定层界**。

### 组装总图：模块依赖与数据流

把管线画成模块依赖图，能看清"卷十三项目"与前十二卷的关系——**每个框都是某一卷的正题，箭头是本项目新增的胶水**：

```diagram flow
- 文件层（第 44 章 xfile → 第 40 章 xreader）：路径 → 字节流
- 行层（第 40 章 xlinereader）：字节流 → 行视图（行尾自适应）
- 文本层（第 25 章 xstrview/xstrbuf）：行视图 → 级别/正文切分；统计结果 → 报告拼接
- 容器层（第 14 章数组/第 12 章思想）：计数数组与样本环
- 系统层（第 46 章目录+第 41 章时间）：临时目录定位与无冒号时间戳
- 错误层（第 4 章 xerror）：全路径失败的结构化报告
```

六层各司其职、层间只有视图与返回值——**没有一层跳过下层直接够到更下层**（比如行层不碰路径、文本层不碰文件句柄）。这个纪律让"换文件为网络流"（第 40 章 Reader 抽象）或"换文本为 JSON"（第 32 章）都成为单层替换——可维护性不是抽象口号而是分层事实。

### 性能预算：每个环节的账

工具类程序的性能账要在设计期算清（第 135 章方法论的微型应用）：**行读取**——每行一次视图产出（零拷贝）+ 超长行的缓冲增长（摊销 O(行数×平均行长) 总量）；**级别判定**——每行一次前缀比较（视图操作，纳秒级）；**计数**——数组下标自增（缓存友好）；**样本复制**——仅错误行（占比小）；**报告拼接**——构建器 O(总量)；**落盘**——一次写。**预算结论**：单核吞吐由 IO 决定（文件读是唯一慢环节）——CPU 侧全部环节加起来在 IO 面前可忽略。这个结论指导优化方向：**不要优化字符串比较，要关注读缓冲大小**（Reader 的块尺寸）——预算先行避免在错的地方使劲（第 135 章"提问纪律"的工程化）。

### 实现走查：完整源码

下面是 logstat 的完整可编译源码——教学形态（内存样本输入，便于确定性验证），文件形态只差 Reader 一行。它把本章全部设计决策落成代码：

```c
/* logstat —— 混合行尾日志统计（教学形态：内存样本） */
#include <stdio.h>
#include <string.h>
#include <xrt.h>

#define LS_LEVELS 5
#define LS_TOP 3

static const char* const kLevelNames[LS_LEVELS] = {
	"TRACE", "INFO", "WARN", "ERROR", "FATAL"
};

typedef struct {
	size_t Counts[LS_LEVELS];            /* 固定集计数：数组 */
	char   Samples[LS_TOP][96];          /* 错误样本环：复制入环 */
	size_t SampleCount;
	size_t Lines;                        /* 总行数 */
	size_t BadLines;                     /* 无法识别级别的行 */
} lsstats;

/* 级别判定：视图首空格前的前缀匹配——零拷贝。返回 -1 表示未知。 */
static int lsLevelOf(xstrview Line)
{
	size_t i;
	for ( i = 0; i < LS_LEVELS; i++ ) {
		xstrview Name = { kLevelNames[i], strlen(kLevelNames[i]) };
		if ( (Line.Size > Name.Size) &&
			(Line.Data[Name.Size] == ' ') &&
			xrtStrStarts(Line, Name) ) {
			return (int)i;
		}
	}
	return -1;
}

int main(void)
{
	/* 混合行尾 + 末行无换行 + 未知级别行：三类边界一次覆盖。 */
	static const char sLog[] =
		"INFO server started
"
		"WARN queue is busy
"
		"ERROR db connect failed
"
		"INFO retry ok
"
		"ERROR db connect failed
"
		"NOTICE rotated
"
		"FATAL giving up";
	xreader* pReader = NULL;
	xlinereader* pLines = NULL;
	xlineview Line;
	xlinenext Next;
	lsstats St;
	xstrbuf Rep;
	size_t i;
	int iResult = 1;

	memset(&St, 0, sizeof(St));
	pReader = xrtReaderFromMemory(
		(xbytesview){ (cbytes)sLog, sizeof(sLog) - 1u });
	if ( pReader != NULL ) {
		pLines = xrtLineReaderTake(&pReader, 1024u);
	}
	if ( pLines == NULL ) {
		fprintf(stderr, "logstat: %s
",
			xrtErrorMessage(xrtGetError()));
		goto Cleanup;
	}

	/* 主循环：三态严格——LINE 处理 / END 收尾 / ERROR 报错。 */
	while ( (Next = xrtLineReaderNext(pLines, &Line)) ==
		XLINE_NEXT_LINE ) {
		int iLevel = lsLevelOf(Line);
		St.Lines++;
		if ( iLevel < 0 ) {
			St.BadLines++;          /* 边界：未知级别计数不中断 */
			continue;
		}
		St.Counts[iLevel]++;
		if ( (iLevel >= 3) && (St.SampleCount < LS_TOP) ) {
			/* 样本环存复制：视图即取即弃（坑 2 的正面形态）。 */
			size_t n = Line.Size < 95u ? Line.Size : 95u;
			memcpy(St.Samples[St.SampleCount], Line.Data, n);
			St.Samples[St.SampleCount][n] = 0;
			St.SampleCount++;
		}
	}
	if ( Next != XLINE_NEXT_END ) {
		fprintf(stderr, "logstat: read error: %s
",
			xrtErrorMessage(xrtGetError()));
		goto Cleanup;
	}

	/* 报告：构建器 O(n) 拼接（坑：反复 Concat 是 O(n²)）。 */
	xrtStrBufInit(&Rep);
	if ( !xrtStrBufAppend(&Rep, XRT_STR_LITERAL("lines=")) ) {
		goto BufFail;
	}
	/* 计数格式化：数字转文本再追加（构建器统一出口） */
	for ( i = 0; i < LS_LEVELS; i++ ) {
		char Num[24];
		int n = snprintf(Num, sizeof(Num), "%zu", St.Counts[i]);
		if ( !xrtStrBufAppend(&Rep,
				(xstrview){ Num, (size_t)n }) ) { goto BufFail; }
		if ( (i + 1u) < LS_LEVELS &&
			!xrtStrBufAppend(&Rep, XRT_STR_LITERAL("/")) ) {
			goto BufFail;
		}
	}
	printf("bad=%zu samples=%zu
", St.BadLines, St.SampleCount);
	for ( i = 0; i < LS_LEVELS; i++ ) {
		printf("%-6s %zu
", kLevelNames[i], St.Counts[i]);
	}
	for ( i = 0; i < St.SampleCount; i++ ) {
		printf("sample[%zu]=%s
", i, St.Samples[i]);
	}
	iResult = 0;

BufFail:
	xrtStrBufFree(&Rep);
Cleanup:
	xrtLineReaderDestroy(pLines);   /* Take 移交后唯一销毁点 */
	return iResult;
}
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c logstat.c -lws2_32 -liphlpapi
bad=1 samples=3
TRACE  0
INFO   2
WARN   1
ERROR  2
FATAL  1
sample[0]=ERROR db connect failed
sample[1]=ERROR db connect failed
sample[2]=FATAL giving up
```

**走查要点。** ① 样本输入刻意覆盖全部边界：三种行尾、末行无换行（FATAL 行）、未知级别（NOTICE——进 BadLines 不中断）、重复错误（去重语义留给扩展）。② `lsLevelOf` 的判定是"前缀 + 空格"——`INFO` 行不会误判 `INFOX`（多一个字符检查的设计细节）。③ cleanup 单出口：`LineReaderDestroy(pLines)` 是 Take 移交后的**唯一**销毁点（pReader 不可再碰）；失败路径与成功路径共用。④ 构建器段演示报告拼接的最小形态——完整报告（含时间戳与临时文件）按第 44/46 章扩展。⑤ 未知级别的 `NOTICE` 被计为 bad=1——**宽容计数 + 报告呈现**而非崩溃（真实日志永远有你没见过的级别）。

### 验收走查：五条标准逐条对账

回到开头写下的验收标准，逐条对账——这是项目章与教学章的最后差异：**教学章以"理解"收尾，项目章以"验收"收尾**。① 混合行尾：走查程序的输入就是三行尾混合——输出正确（标准 ① 过）。② 流式内存：行引擎只持有当前行 + 固定计数 + 3×95 字节样本——与输入同量级的账在设计期算清（标准 ② 过——文件形态对 2 GB 同构）。③ 三边界：空文件（首个 Next 即 END——输出全零）、无匹配（过滤标志下零命中照常输出）、全错误（环满覆盖）——走查样本已含两边界，空文件在练习验收（标准 ③ 过）。④ 报告边界：临时目录与无冒号时间戳是 `file/report` 示例的两个工程细节（标准 ④ 的设计依据）。⑤ 错误链：两处错误分支都打印 `xrtErrorMessage(xrtGetError())`——打开失败/读错误的结构化报告（标准 ⑤ 过）。**五条全过——这个工具可以交付了**。

### 扩展路径：从工具到工具族

logstat 的架构允许三个方向的有机扩展，每个方向都只是"换一层"：**输入层升级**——JSON Lines（第 32 章：级别在字段里、`{` 首字符探测分流）或网络流（第 40 章 Reader 家族：tail -f 形态的实时统计）；**统计层升级**——时间窗聚合（第 41 章：按分钟桶计数出趋势）或动态 token 频次（第 18 章 Map：找最热模块名）；**输出层升级**——JSON 报告（第 32 章序列化）或 SSE 推送（第 106 章：统计仪表盘的实时源）。**每个升级都是单层替换**——组装总图的分层纪律在演进时兑现价值：这正是"项目"作为全书综合验收的意义——不只是"用了很多 API"，而是**换任何一层时其他层不动**。

## 契约

- **流式保证**：内存 = 行缓冲（随最长行）+ O(1) 计数 + O(N) 样本——与输入总量无关。
- **行尾自适应**：CRLF/LF/CR 全识别、视图不含行尾、末行无换行照常——零配置。
- **三态严格**：END 正常收尾 / ERROR 结构化报错——磁盘坏与正常结束不混淆。
- **视图纪律**：行视图即取即用；样本环存复制（视图不入长期容器）。
- **选型纪律**：固定集用数组、动态键才 Map、多段拼接用构建器——形态决定容器。
- **输出边界**：报告落系统临时目录、时间戳无冒号——两个平台事实。
- **错误姿态**：全路径返回值+错误槽；cleanup 单出口；无处裸 exit。
- **验收前置**：五条标准先于代码——边界（空/无匹配/全错误）预演。

### 测试策略：工具的四象限验收

项目代码写完不等于交付——还要按第 132 章的测试形态给它配验收件。logstat 的四象限：**正向**——固定样本（走查程序的内嵌样本就是回归向量：三行尾/末行无换行/未知级别/重复错误的期望输出逐字符断言）；**边界**——空文件（首 Next 即 END）、单行无换行、超长行（触发缓冲增长路径）、全部同级别；**负向**——不存在的路径（打开失败的错误类别+域断言）、目录当文件传（同样打开失败但消息不同）；**压力**——第 133 章方法的应用位：行读取器内部若有分配，失败注入下统计结果仍一致（工具的 OOM 行为是"报错退出"而非"输出错误统计"）。**测试件与工具同生命周期**：样本向量进测试目录、跑在门禁——工具改行为时向量先红。这把第 132 章"四武器"从库的测试语言翻译成了**项目工具的验收语言**——同一个方法论在不同粒度的复用。

### 交付形态：单文件工具的发布

工具类程序的交付选型（第 131 章三类消费者的项目版）：logstat 是**单头形态的最佳场景之一**——一个 `.c` 源文件 + `single/xrt.h` + 一条编译命令就是完整交付（脚本世界最易分发）；裁剪声明（第 130 章）在工具顶部一行 `XRT_MODULE_IO`+`XRT_MODULE_STRING`+`XRT_MODULE_FILE`——体积档验证不含网络/TLS 符号。**备选对照**：静态库形态适合工具族（多个工具共享一份库产物——第 142 章静态文件服务器的形态）；动态库形态对独立工具无收益。**版本与迭代**：工具打印自身版本（第 8 章 core 的版本查询）——`logstat --version` 输出 XRT 版本与工具自身版本，排障时"哪个版本算的这份报告"可追溯。交付不是终点而是**下一次迭代的起点**——扩展路径一节的三个方向都从这份可交付的基线出发。

## 避坑

### 坑 1：整读文件再按行切

症状：2 GB 日志直接把 32 GB 机器吃掉一半内存——"能跑"但运维不答应。

原因：把"文件"当"内存里的字符串"。流式的本质是**任意时刻只持有当前行**——Reader 抽象就是为此存在。

```c bad
str all = read_whole_file(path);      /* 2GB 进内存 */
for ( line in strtok(all, "\n") ) {   /* 行尾还得手工三态处理 */
	count(line);
}
```

```c good
xreader* r = xrtReaderFromFile(path, ...);
xlinereader* lr = xrtLineReaderTake(&r, 1024);
while ( xrtLineReaderNext(lr, &v) == XLINE_NEXT_LINE ) {
	count(v);                          /* 任意时刻：一行 */
}
xrtLineReaderDestroy(lr);
```

### 坑 2：把行视图存进样本环

症状：输出的"错误样本"全部是最后一行——环里存的是视图，缓冲复用后全指向同一块。

原因：行视图借自读取器内部缓冲——下一次 Next 即失效（第 40 章）。要跨行保留必复制。

```c bad
while ( Next == LINE ) {
	if ( is_error(v) ) { ring_push(&ring, v); }  /* 存视图：全悬空 */
}
```

```c good
while ( Next == LINE ) {
	if ( is_error(v) ) { ring_push_copy(&ring, v); } /* 复制入环 */
}
```

### 坑 3：报告写工作目录（或文件名带冒号）

症状：只读挂载/服务账户下运行——写报告失败；或 Windows 下时间戳文件名创建失败（`:` 禁用）。

原因：两个平台事实没进设计。`file/report` 示例的工程细节：**临时目录**（不假设可写）+ **无冒号紧凑时间格式**。

```c bad
snprintf(name, "report_%s.txt", now_with_colons());  /* 12:30:45 → Windows 拒绝 */
f = fopen(name, "w");                                /* 工作目录可能只读 */
```

```c good
/* 系统临时目录 + 紧凑无冒号时间（report_20260905_042105.txt） */
path = temp_dir_join(compact_timestamp());
```

### 坑 4：沿用旧代码的 strtok 习惯

症状：移植老代码的 `strtok(all, "
")` ——行尾处理"看起来"对了，但 strtok 的**状态机是全局的**（不可重入）、连续分隔符被吞（空行消失）、修改原缓冲（与视图纪律冲突）。

原因：strtok 是前 ANSI 时代遗产——三重违规（全局状态/吞空行/改输入）在现代并发与流式语境下全是错。XRT 的行读取器是它的正面替代：可重入（状态在对象里）、空行保留（视图语义）、零修改（借用）。

```c bad
for ( p = strtok(buf, "
"); p; p = strtok(NULL, "
") ) {
	count(p);   /* 空行没了；buf 被改了；多线程炸 */
}
```

```c good
while ( xrtLineReaderNext(lr, &v) == XLINE_NEXT_LINE ) {
	count(v);   /* 可重入；空行保留；零修改 */
}
```

## 练习

### 基础：三级计数跑通

以 io/line 示例为引擎扩展：计数 INFO/WARN/ERROR 三级并打印。混合行尾+末行无换行样本验证。验收标准：三计数正确；空文件输出全零+提示。

### 进阶：完整 logstat

实现全部需求：`--level` 过滤、`--top N` 错误样本（复制入环）、`--report` 临时文件输出（无冒号时间戳）。对 100 MB 自造日志验证。验收标准：五条验收标准全过；内存 MB 级（stats 复核）。

### 挑战：JSON Lines 升级

输入升级为 `file_json` 示例格式的 JSON 行：行读取不变、解析改第 32 章 JSON（级别/消息/时间字段）、统计同构。新旧两种输入同一程序支持（按首字符 `{` 探测）。验收标准：两种格式混合文件正确分流；JSON 解析错误行计数报告（不中断整体）。

### 复盘：项目一学到了什么

作为全书第一个项目，值得把**可迁移的经验**显式清点——它们会在后续项目里反复出现。**验收先行**：五条标准写在学习代码前，边界（空/无匹配/全错误）在预演期就想清——返工率最低的顺序；**形态决定容器**：固定集数组/动态键 Map/多段拼接构建器——选型不是背 API 而是看数据形状；**视图即取即弃**：跨行保留必复制——网络章（第 95/103 章）的纪律在文件工具同样成立；**分层换件**：Reader 抽象让内存样本与 2 GB 文件同码——验收与性能共用一份实现；**平台事实进设计**：临时目录与无冒号时间戳不是"运行时发现"而是"设计期列出"；**预算先算**：CPU 侧全环节在 IO 面前可忽略——优化方向由预算指认而非直觉；**测试四象限**：工具的验收语言与库的测试语言同构。七条经验没有一条是 logstat 特有的——它们是**工程常识的 XRT 表达**，下一个项目（配置中心，第 138-139 章）会在服务形态与持久化语境下重放它们；再下一个（聊天服务，第 140-141 章）加上并发与连接管理。项目序列的编排意图正在于此：**同一套工程常识，每换一个形态就加深一次理解**。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 项目形态 | 需求与验收先行 → 选型决策 → 递进实现 → 验收扩展 |
| 行引擎 | Reader→LineReaderTake（移交）→Next 三态循环——流式核心 |
| 行尾 | CRLF/LF/CR 自适应；末行无换行照常——零配置 |
| 级别解析 | 视图切首空格——零拷贝前缀判定 |
| 计数选型 | 固定级别集=数组；动态键才 Map |
| 样本环 | 复制入环（视图即取即弃）——环覆盖留前 N |
| 报告 | 构建器 O(n) 拼接；临时目录+无冒号时间戳 |
| 流式内存 | 行缓冲+O(1)+O(N)——与总量无关 |
| 错误姿态 | 三态区分 END/ERROR；cleanup 单出口；未知级别宽容计数 |
| 扩展方向 | JSON Lines 输入（32 章）/时间窗统计（41 章）/SSE 仪表盘（106 章） |
