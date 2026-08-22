# 用 Template 渲染一个 HTML 页面

> 这页真正要教的不是“模板语法怎么写”，而是怎么把页面数据、模板结构和文件输出整理成一条稳定主线。写页面时，最容易烂掉的往往不是 HTML，而是数据模型和输出方式。

[返回案例索引](README.md)

---

## 1. 场景

假设你现在要生成一个本地 HTML 页面文件，用来展示一组运行状态信息，例如：

- 页面标题
- 当前用户
- 环境状态
- 若干统计卡片

它最终会被写成一个真实文件，比如：

- `build/dashboard.html`

很多 C 程序做这种事情时会直接：

- `snprintf()`
- 拼几十行 HTML
- 再一次性写盘

短期当然能跑，但只要页面和数据一起增长，后面就会越来越难维护。

当前 XRT 主线更推荐：

- 用 `xvalue` 组织页面数据
- 用 `template` 保持 HTML 结构清晰
- 最后统一把渲染结果写到文件


## 2. 这次为什么选 `value + template + file`

这个组合的分工非常清楚：

- `xvalue` 负责页面上下文
- `template` 负责页面结构和条件 / 列表渲染
- `file` 负责把最终产物落盘

这样做最直接的收益有 3 个：

- 页面结构和数据构造不再混在一起
- 同一份 `xvalue` 后面还能继续复用到 JSON、HTTP、配置或 AI 输出
- 你可以先把“离线产出页面”这条线跑通，再自然接到 HTTP 服务里

也就是说，这页不是孤立技巧，而是后面 [HTTP + JSON + Template 完整链路](http-json-template-chain.md) 的前置台阶。

如果你还没系统读过 `template` 的教学页，建议先读：

- [Template 入门：什么时候该用模板，而不是拼字符串](../guide/template-intro.md)


## 3. 完整骨架

下面这份代码会完成 5 件事：

1. 启动 runtime
2. 预编译模板
3. 组织一份完整 `xvalue` 页面模型
4. 渲染成 HTML
5. 输出到 `build/dashboard.html`

```c
#include "xrt.h"
#include <stdio.h>

static const char g_sDashboardTemplate[] =
	"<!doctype html>\n"
	"<html>\n"
	"<head>\n"
	"\t<meta charset=\"utf-8\">\n"
	"\t<title>{$title}</title>\n"
	"\t<style>\n"
	"\t\tbody { font-family: sans-serif; margin: 32px; }\n"
	"\t\t.card-list { padding-left: 18px; }\n"
	"\t</style>\n"
	"</head>\n"
	"<body>\n"
	"\t<h1>{$title}</h1>\n"
	"\t<p>author={$user.name}</p>\n"
	"\t<p>status={$status_text}</p>\n"
	"\t{#if:show_cards}\n"
	"\t\t<ul class=\"card-list\">\n"
	"\t\t{#foreach:cards}\n"
	"\t\t\t<li><strong>{$title}</strong>: {$value_text}</li>\n"
	"\t\t{#end}\n"
	"\t\t</ul>\n"
	"\t{#else}\n"
	"\t\t<p>no cards</p>\n"
	"\t{#end}\n"
	"</body>\n"
	"</html>\n";


static xvalue procBuildPageModel(void)
{
	xvalue vPage = NULL;
	xvalue vUser = NULL;
	xvalue vCards = NULL;
	xvalue vCard = NULL;

	vPage = xvoCreateTable();
	if ( vPage == NULL ) {
		return NULL;
	}

	xvoTableSetText(vPage, "title", 0, "XRT Dashboard", 0, FALSE);
	xvoTableSetText(vPage, "status_text", 0, "running", 0, FALSE);
	xvoTableSetBool(vPage, "show_cards", 0, TRUE);

	xvoTableSetTable(vPage, "user", 0);
	vUser = xvoTableGetValue(vPage, "user", 0);
	if ( vUser != NULL ) {
		xvoTableSetText(vUser, "name", 0, "Alice", 0, FALSE);
		xvoTableSetText(vUser, "role", 0, "maintainer", 0, FALSE);
	}

	xvoTableSetArray(vPage, "cards", 0);
	vCards = xvoTableGetValue(vPage, "cards", 0);
	if ( vCards != NULL ) {
		vCard = xvoCreateTable();
		if ( vCard != NULL ) {
			xvoTableSetText(vCard, "title", 0, "workers", 0, FALSE);
			xvoTableSetText(vCard, "value_text", 0, "1", 0, FALSE);
			if ( !xvoArrayAppendValue(vCards, vCard, TRUE) ) {
				xvoUnref(vCard);
			}
		}

		vCard = xvoCreateTable();
		if ( vCard != NULL ) {
			xvoTableSetText(vCard, "title", 0, "mode", 0, FALSE);
			xvoTableSetText(vCard, "value_text", 0, "offline-render", 0, FALSE);
			if ( !xvoArrayAppendValue(vCards, vCard, TRUE) ) {
				xvoUnref(vCard);
			}
		}

		vCard = xvoCreateTable();
		if ( vCard != NULL ) {
			xvoTableSetText(vCard, "title", 0, "output", 0, FALSE);
			xvoTableSetText(vCard, "value_text", 0, "build/dashboard.html", 0, FALSE);
			if ( !xvoArrayAppendValue(vCards, vCard, TRUE) ) {
				xvoUnref(vCard);
			}
		}
	}

	return vPage;
}


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

	hTemplate = xteParseEx(hEngine, g_sDashboardTemplate, 0u, NULL, &tError);
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

	vPage = procBuildPageModel();
	if ( vPage == NULL ) {
		fprintf(stderr, "build page model failed\n");
		goto cleanup;
	}

	sHtml = xteMake(hTemplate, vPage, NULL, NULL, &iHtmlSize);
	if ( sHtml == NULL ) {
		fprintf(stderr, "render html failed\n");
		goto cleanup;
	}

	if ( xrtDirCreateAll("build") != TRUE ) {
		fprintf(stderr, "create build directory failed\n");
		goto cleanup;
	}

	if ( xrtFileWriteAll("build/dashboard.html", sHtml, iHtmlSize, XRT_CP_UTF8) <= 0 ) {
		fprintf(stderr, "write html file failed\n");
		goto cleanup;
	}

	printf("render ok: build/dashboard.html\n");
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


## 4. 这份代码里最关键的 4 个点

### 4.1 `procBuildPageModel()` 才是页面的真正数据入口

这个函数做完以后，页面已经有了一棵完整对象树：

- 顶层字段：`title`、`status_text`、`show_cards`
- 嵌套对象：`user`
- 列表对象：`cards`

这意味着后面模板只是消费这份数据，而不是重新发明一套页面变量系统。


### 4.2 模板应该先编译，再反复渲染

这页里模板处理刻意拆成两步：

- `xteParseEx()` 负责预编译
- `xteMake()` 负责基于数据渲染

这样做的原因非常实际：

- 模板结构通常是稳定的
- request 或业务变化通常发生在数据层
- 把模板解析放到热路径里重复做，通常没有必要

即使你当前只是离线生成一个 HTML 文件，也应该先养成这个结构习惯。


### 4.3 `xteMake()` 返回的是新字符串，必须自己释放

这是这类页面输出代码里很容易忽略的一点：

- `xteMake()` 返回 `char*`
- 这个字符串要用 `xrtFree()` 释放

所以这页里把：

- 渲染
- 写盘
- 释放字符串

明确拆开写，就是为了把所有权关系讲清楚。


### 4.4 文件输出只是最后一步，不该反过来支配页面结构

很多旧写法会先想着“我要写一段 HTML 文件”，于是直接开始拼字符串。  
而这页反过来做：

1. 先得到页面数据
2. 再得到页面结构
3. 最后才写文件

这会让扩展方向清晰很多，因为后面你只要把最后一步从“写文件”换成“返回 HTTP 响应”，主线几乎不用改。


## 5. 为什么这比手工拼 HTML 更稳

如果你直接用 `snprintf()` 拼整个页面，很快会遇到这些问题：

- 条件分支和列表渲染很乱
- 页面结构不容易整体阅读
- 字段一多就容易漏改
- 同一份数据以后再接 JSON 或 HTTP 时要重写一次

而 `template + xvalue` 的好处正好相反：

- 结构由模板保持可读性
- 数据由 `xvalue` 保持完整性
- 输出文件只是最后一个出口

这就是为什么它更适合作为长期主线，而不只是一个“看起来高级一点”的模板技巧。


## 6. 运行与验证

运行这段程序后，应该能得到：

- 输出文件 `build/dashboard.html`

你可以直接打开它确认：

- 标题是否来自 `title`
- `author` 是否来自 `user.name`
- 列表是否来自 `cards`

如果你把 `procBuildPageModel()` 里的：

- `show_cards` 改成 `FALSE`
- `status_text` 改成别的值
- `cards` 里再追加一项

页面会立刻跟着变，这就是“模板结构稳定，数据模型驱动输出”的直接体现。


## 7. 这页和 HTTP 服务是什么关系

这页故意不引入 `xhttpd`，因为它只想先把页面渲染本身讲稳。  
但它和 HTTP 服务不是两条无关的线，反而是天然连续的：

- 这页负责“从 `xvalue` 渲染出 HTML”
- [最小 HTTP 服务](minimal-http-service.md) 负责“从 `xhttpd` 返回结构化响应”
- [HTTP + JSON + Template 完整链路](http-json-template-chain.md) 负责“同一份 `xvalue` 同时喂给 JSON 和 HTML”

所以更合理的学习顺序是：

1. 先学这页，弄清模板和数据模型怎么配合
2. 再学 HTTP 服务页，弄清服务边界
3. 最后看完整链路页，把两条线接起来


## 8. 常见错误

### 错误一：模板里能写逻辑，于是把业务计算都塞进模板

模板应该负责展示结构，不应该接管业务整理。  
真正的业务数据准备仍然应该收敛在 `procBuildPageModel()` 这种函数里。

### 错误二：页面字段不是对象树，而是一堆零散变量

如果模板上下文不是一棵完整 `xvalue` 树，页面一复杂，模型还是会散掉。

### 错误三：渲染完字符串就忘了释放

`xteMake()` 产出的字符串要自己 `xrtFree()`，这一点必须写清楚。

### 错误四：离线渲染时就开始手工拼 HTML，想着以后再重构

通常不会“以后再重构”，而是会一路堆到变得很难改。  
页面只要准备长期保留，从第一版开始就值得站在 `template + xvalue` 这条线上。


## 9. 建议继续阅读

- [Template 入门：什么时候该用模板，而不是拼字符串](../guide/template-intro.md)
- [Template API](../api/api-template.md)
- [Value API](../api/api-value.md)
- [JSON API](../api/api-json.md)
- [用 XRT 写一个最小 HTTP 服务](minimal-http-service.md)
- [一个完整的 HTTP + JSON + Template 服务链路](http-json-template-chain.md)

---

**一句话总结：** 这页真正要建立的习惯是：先用 `xvalue` 把页面数据组织完整，再用 `template` 负责结构，最后才把结果写成文件。页面输出一旦按这个顺序组织，后面接 HTTP、JSON 或更复杂布局都会顺很多。
