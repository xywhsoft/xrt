# Template 入门：什么时候该用模板，而不是拼字符串

> 目标：把 `template` 从“看起来方便的语法糖”讲成一条真正稳定的输出主线。你应该在读完这页后明确知道：什么时候该继续用 `snprintf()`，什么时候该切到 `template`，以及 `xvalue + template` 应该怎样配合。

[返回教学文档](README.md)

---

## 1. 为什么需要模板

很多程序最后都会遇到“把结构化数据变成文本”的问题，例如：

- 生成一个 HTML 页面
- 生成一段邮件正文
- 生成一份报表
- 生成一个提示词片段

如果输出非常短、结构非常固定，直接 `snprintf()` 完全没问题。  
但一旦同时出现下面任意两条，手工拼字符串就会开始变脆：

- 条件分支变多
- 列表要循环输出
- 页面结构变长
- 数据字段来自对象树，而不是几个简单变量
- 同一份数据还要继续复用到 JSON、HTTP 或配置

这时候模板的价值就不是“少写几个引号”，而是：

- 让结构层和数据层分开
- 让输出逻辑可读
- 让同一份 `xvalue` 能服务多个出口


## 2. 模板和相邻方案的区别

先把边界讲清楚：

### 2.1 模板 vs `snprintf()`

`snprintf()` 更适合：

- 一两行短文本
- 字段非常少
- 没有循环和复杂条件

模板更适合：

- 结构已经像一个页面、片段或报表
- 需要 `if / else / foreach`
- 输出会继续增长

### 2.2 模板 vs JSON

JSON 解决的是“交换格式”问题。  
模板解决的是“展示格式”问题。

也就是说：

- 要给程序、接口、配置系统交换数据，优先 JSON
- 要给人看、给页面看、给文本结构看，优先模板

它们不是替代关系，而是经常共用同一份 `xvalue`。

### 2.3 模板 vs 业务逻辑

模板不应该接管业务计算。  
真正该做的分工是：

1. 业务层先准备好 `xvalue`
2. 模板层只决定怎么展示它

如果你把业务整理也塞进模板里，后面一样会乱。


## 3. 这条主线的核心模型

在 XRT 当前主线里，模板最好按下面 4 个动作理解：

1. 用 `xvalue` 组织数据
2. 用 `xteParseEx()` 预编译模板
3. 用 `xteMake()` 基于 `xvalue` 渲染字符串
4. 自己负责释放渲染结果，并决定把它写文件、回 HTTP，还是继续传下游

这里最关键的两个认知是：

- 模板源码通常是稳定的，应该先编译
- `xteMake()` 返回的是新字符串，必须 `xrtFree()`


## 4. 最小 DEMO：先把一页最简单的 HTML 渲染出来

先不要一上来就写循环和条件。  
第一步只做一件事：把一份最小 `xvalue` 渲染成一段 HTML。

```c
#include "xrt.h"
#include <stdio.h>

int main(void)
{
	xteengine hEngine = NULL;
	xtetemplate hTemplate = NULL;
	XTE_Error tError = {0};
	xvalue vPage = NULL;
	char* sHtml = NULL;
	size_t iHtmlSize = 0;
	int iRet = 1;

	xrtInit();

	hEngine = xteCreateEngine();
	if ( hEngine == NULL ) {
		fprintf(stderr, "create template engine failed\n");
		goto cleanup;
	}

	hTemplate = xteParseEx(
		hEngine,
		"<h1>{$title}</h1><p>{$message}</p>",
		0u,
		NULL,
		&tError
	);
	if ( hTemplate == NULL ) {
		fprintf(
			stderr,
			"parse template failed: %s (%u:%u)\n",
			tError.sDesc ? tError.sDesc : "unknown",
			(unsigned)tError.iLine,
			(unsigned)tError.iColumn
		);
		goto cleanup;
	}

	vPage = xvoCreateTable();
	if ( vPage == NULL ) {
		fprintf(stderr, "create page model failed\n");
		goto cleanup;
	}

	xvoTableSetText(vPage, "title", 0, "XRT Demo", 0, FALSE);
	xvoTableSetText(vPage, "message", 0, "hello template", 0, FALSE);

	sHtml = xteMake(hTemplate, vPage, NULL, NULL, &iHtmlSize);
	if ( sHtml == NULL ) {
		fprintf(stderr, "render html failed\n");
		goto cleanup;
	}

	printf("%.*s\n", (int)iHtmlSize, sHtml);
	iRet = 0;

cleanup:
	if ( sHtml != NULL ) {
		xrtFree(sHtml);
	}
	if ( vPage != NULL ) {
		xvoUnref(vPage);
	}
	if ( hTemplate != NULL ) {
		xteDestroyTemplate(hTemplate);
	}
	if ( hEngine != NULL ) {
		xteDestroyEngine(hEngine);
	}

	xrtUnit();
	return iRet;
}
```

这个 DEMO 的重点不是 HTML 本身，而是先把调用顺序记牢：

- `xteCreateEngine()`
- `xteParseEx()`
- 组织 `xvalue`
- `xteMake()`
- `xrtFree()`


## 5. 升级 DEMO：加入 `if` 和 `foreach`

只会替换变量还不够，因为真实模板最常见的升级就是：

- 有些区块要按条件显示
- 有些数据要循环输出

下面这个例子把这两件事一起讲掉：

```c
#include "xrt.h"
#include <stdio.h>

static const char g_sTemplate[] =
	"<h1>{$title}</h1>\n"
	"{#if:show_cards}\n"
	"<ul>\n"
	"{#foreach:cards}\n"
	"\t<li>{$title}: {$value_text}</li>\n"
	"{#end}\n"
	"</ul>\n"
	"{#else}\n"
	"<p>empty</p>\n"
	"{#end}\n";

int main(void)
{
	xteengine hEngine = NULL;
	xtetemplate hTemplate = NULL;
	XTE_Error tError = {0};
	xvalue vPage = NULL;
	xvalue vCards = NULL;
	xvalue vCard = NULL;
	char* sHtml = NULL;
	size_t iHtmlSize = 0;
	int iRet = 1;

	xrtInit();

	hEngine = xteCreateEngine();
	hTemplate = xteParseEx(hEngine, g_sTemplate, 0u, NULL, &tError);
	if ( (hEngine == NULL) || (hTemplate == NULL) ) {
		fprintf(stderr, "prepare template failed\n");
		goto cleanup;
	}

	vPage = xvoCreateTable();
	if ( vPage == NULL ) {
		goto cleanup;
	}

	xvoTableSetText(vPage, "title", 0, "Dashboard", 0, FALSE);
	xvoTableSetBool(vPage, "show_cards", 0, TRUE);

	xvoTableSetArray(vPage, "cards", 0);
	vCards = xvoTableGetValue(vPage, "cards", 0);
	if ( vCards == NULL ) {
		goto cleanup;
	}

	vCard = xvoCreateTable();
	if ( vCard == NULL ) {
		goto cleanup;
	}
	xvoTableSetText(vCard, "title", 0, "workers", 0, FALSE);
	xvoTableSetText(vCard, "value_text", 0, "1", 0, FALSE);
	if ( !xvoArrayAppendValue(vCards, vCard, TRUE) ) {
		xvoUnref(vCard);
		goto cleanup;
	}

	vCard = xvoCreateTable();
	if ( vCard == NULL ) {
		goto cleanup;
	}
	xvoTableSetText(vCard, "title", 0, "mode", 0, FALSE);
	xvoTableSetText(vCard, "value_text", 0, "guide-demo", 0, FALSE);
	if ( !xvoArrayAppendValue(vCards, vCard, TRUE) ) {
		xvoUnref(vCard);
		goto cleanup;
	}

	sHtml = xteMake(hTemplate, vPage, NULL, NULL, &iHtmlSize);
	if ( sHtml == NULL ) {
		goto cleanup;
	}

	printf("%.*s\n", (int)iHtmlSize, sHtml);
	iRet = 0;

cleanup:
	if ( sHtml != NULL ) {
		xrtFree(sHtml);
	}
	if ( vPage != NULL ) {
		xvoUnref(vPage);
	}
	if ( hTemplate != NULL ) {
		xteDestroyTemplate(hTemplate);
	}
	if ( hEngine != NULL ) {
		xteDestroyEngine(hEngine);
	}

	xrtUnit();
	return iRet;
}
```

这一步真正该学会的是：

- `bool` 字段负责控制区块显示
- `array` 字段负责列表输出
- 列表项最好直接做成 `table`

这样模板层就能天然读懂对象树，而不是靠你在字符串里继续硬拼。


## 6. 再升级一步：什么时候该把结果写文件，什么时候该接 HTTP

当你已经能稳定渲染字符串后，后面通常就分成两条线：

### 6.1 离线文件输出

适合：

- 静态页面生成
- 报表导出
- 本地 dashboard

这时你会在 `xteMake()` 之后接：

- `xrtDirCreateAll()`
- `xrtFileWriteAll()`

### 6.2 在线 HTTP 输出

适合：

- 浏览器请求页面
- 管理后台
- HTML + JSON 同时存在的服务

这时你会在 `xteMake()` 之后接：

- `xrtHttpdResponseSetBodyCopy()`

也就是说，模板主线真正稳定的地方，不是“输出到了哪里”，而是：

- 数据先变成 `xvalue`
- 模板再把它渲染成字符串

出口只是最后一步。


## 7. 什么时候不该先上模板

也要把边界说清楚。下面这些场景，不需要一开始就引入模板：

- 只有一行短文本
- 只是拼一个命令参数
- 没有循环、没有条件、没有层次结构

这种时候继续用：

- `snprintf()`
- `xrtStringBuilder` 风格输出

会更直接。

不要为了“看起来统一”把所有文本都塞进模板。  
模板真正适合的是“结构已经像一个页面或报表”的输出。


## 8. 常见错误

### 错误一：每次渲染都重新解析模板

模板源码通常是稳定的。  
如果结构不变，就应该先 `xteParseEx()`，后面反复 `xteMake()`。

### 错误二：模板里写太多业务判断

模板应该负责展示结构，不应该替业务层做大量计算。  
该在 C 代码里整理好的字段，就先整理好再交给模板。

### 错误三：页面上下文不是对象树，而是一堆零散变量

如果你不给模板一棵完整 `xvalue` 树，而是东塞一个字段、西塞一个字段，页面一复杂，模型还是会散掉。

### 错误四：忘了释放 `xteMake()` 返回的字符串

这类问题很隐蔽，但会持续泄漏。  
`xteMake()` 的返回值要明确 `xrtFree()`。

### 错误五：把模板当 JSON 用

模板负责展示，不负责交换格式。  
机器读写的数据仍然应该优先走 JSON。


## 9. 下一步该学什么

如果你现在已经理解：

- 为什么模板适合结构化文本输出
- 为什么模板要配合 `xvalue`
- 为什么解析和渲染要分开

那么下一步最自然的是二选一：

1. 看 [用 Template 渲染一个 HTML 页面](../case/template-render-html.md)，把模板输出真正落成文件。
2. 看 [一个完整的 HTTP + JSON + Template 服务链路](../case/http-json-template-chain.md)，把同一份 `xvalue` 同时接到 JSON 和 HTML 两个出口。

如果你发现自己对 `xvalue` 还不稳，先回去补：

- [xvalue 与 JSON 入门](xvalue-json-intro.md)

---

**一句话总结：** 模板不是为了少写几个引号，而是为了把“数据整理”和“文本结构”拆成两层。只要输出已经像一个页面、报表或长文本片段，就应该认真考虑让 `xvalue + template` 接管这条线。
