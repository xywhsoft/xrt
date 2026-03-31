# xvalue、JSON 与 XSON 入门

> 目标：先建立正确心智模型，再用当前主线的真实 API 完成第一个 `xvalue / JSON / XSON` 示例。
>
> 更正：当前主线应使用 `xvoCreateTable()`、`xvoCreateArray()`、`xrtParseJSON()`、`xrtStringifyJSON()`、`xrtParseXSON()`、`xrtStringifyXSON()`，而不是旧写法 `xvoTable()`、`xvoArray()`、`xjsonParse()`、`xjsonStringify()`。

[返回教学文档](README.md)

---

## 1. 为什么要先学 xvalue

很多语言都有一套“程序内部的动态数据模型”：

- JavaScript 有 `object / array`
- Python 有 `dict / list`
- Lua 有 `table`

XRT 里承担这层角色的，就是 `xvalue`。

它不是“JSON 的附属品”，而是程序内部真正的数据骨架。当前主线里，下面这些场景最终都会回到 `xvalue`：

- 配置数据
- JSON 请求和响应
- 模板上下文
- 多任务结果
- 网络模块里的结构化数据

所以更稳的理解方式是：

- `xvalue`：程序内部的统一数据模型
- `JSON`：对外交换格式
- `XSON`：当 `xvalue` 里出现 JSON 无法完整表达的类型时，使用的扩展交换格式


## 2. 先记住 4 个高频参数

这篇文档里最容易写错的，不是函数名，而是参数。

- `kl`
	- key length，表键长度
	- 传 `0` 表示自动计算
- `iSize`
	- 字符串长度
	- 传 `0` 表示自动计算
- `bFormat`
	- `FALSE` 输出紧凑文本
	- `TRUE` 输出格式化文本
- `bColloc`
	- 用在容器接收一个已经存在的 `xvalue` 子对象时
	- `TRUE` 表示把这份引用直接托管给容器
	- `FALSE` 表示容器增加一份引用，调用方之后仍应把自己手里的那份 `xvoUnref()` 掉

入门阶段你可以先记一个最常见的规则：

- 字符串字面量通常写成 `..., 0, FALSE`
- 表键长度和字符串长度不想手算时，先统一传 `0`


## 3. 第一个 `xvalue` DEMO

先不要急着解析 JSON。第一步应该是先学会在程序内部直接构造一棵 `xvalue` 数据树。

```c
static void Demo_BuildUser(void)
{
	xvalue pUser;
	xvalue pTags;

	pUser = xvoCreateTable();

	xvoTableSetText(pUser, "name", 0, "Alice", 0, FALSE);
	xvoTableSetInt(pUser, "age", 0, 28);
	xvoTableSetBool(pUser, "active", 0, TRUE);

	xvoTableSetArray(pUser, "tags", 0);
	pTags = xvoTableGetValue(pUser, "tags", 0);
	xvoArrayAppendText(pTags, "c", 0, FALSE);
	xvoArrayAppendText(pTags, "runtime", 0, FALSE);
	xvoArrayAppendText(pTags, "json", 0, FALSE);

	printf("name = %s\n", xvoTableGetText(pUser, "name", 0));
	printf("age = %lld\n", xvoTableGetInt(pUser, "age", 0));
	printf("tag count = %u\n", xvoArrayItemCount(pTags));

	xvoUnref(pUser);
}
```

这段代码最值得你先看懂的只有两件事：

- `table` 很像脚本语言里的对象
- `array` 很像脚本语言里的数组

也就是说，`xvalue` 的核心不是“类型多”，而是“你能用一套 API 持续把结构搭起来”。


## 4. 如果你手里已经有一个子对象

上一个 DEMO 用的是 `xvoTableSetArray()`，这对入门最直观。

但真实项目里，你也经常会先单独构造一个子对象，再把它挂到父对象上。这时要用 `xvoTableSetValue()`：

```c
xvalue pUser = xvoCreateTable();
xvalue pTags = xvoCreateArray();

xvoArrayAppendText(pTags, "c", 0, FALSE);
xvoArrayAppendText(pTags, "network", 0, FALSE);

xvoTableSetValue(pUser, "tags", 0, pTags, TRUE);

xvoUnref(pUser);
```

上面最后一个参数写成 `TRUE`，表示这份 `pTags` 引用直接交给 `pUser` 托管。

如果你写成 `FALSE`，则表示父容器额外增加一份引用，这时你自己的临时引用仍然要手动释放：

```c
xvalue pUser = xvoCreateTable();
xvalue pTags = xvoCreateArray();

xvoArrayAppendText(pTags, "c", 0, FALSE);
xvoTableSetValue(pUser, "tags", 0, pTags, FALSE);

xvoUnref(pTags);
xvoUnref(pUser);
```

这就是很多初学者第一次接触 `xvalue` 时最容易忽略的地方：  
**容器 API 不只是“把值塞进去”，还同时涉及引用计数和所有权语义。**


## 5. `xvalue` 和 JSON 的关系

推荐你这样理解：

- 程序内部长期持有和修改的数据：优先用 `xvalue`
- 需要和外部系统交换的文本：优先用 `JSON`

所以标准流程通常不是“直接拼 JSON 字符串”，而是：

1. 用 `xrtParseJSON()` 把 JSON 解析成 `xvalue`
2. 程序内部统一操作 `xvalue`
3. 最后再用 `xrtStringifyJSON()` 输出 JSON

这样做的好处是：

- 结构清晰
- 修改字段时不需要手工拼字符串
- 同一份数据可以继续流向模板、网络、配置、任务结果


## 6. 第一个 JSON DEMO

这里开始用真实的当前 API：`xrtParseJSON()` 和 `xrtStringifyJSON()`。

```c
static void Demo_JSONRoundTrip(void)
{
	str sJson;
	xvalue pRoot;
	str sOut;
	size_t iOutSize;

	sJson = "{\"name\":\"Tom\",\"age\":25,\"tags\":[\"c\",\"xrt\"]}";
	iOutSize = 0;

	pRoot = xrtParseJSON(sJson, 0);
	if ( xvoIsNull(pRoot) ) {
		printf("parse json failed\n");
		xvoUnref(pRoot);
		return;
	}

	printf("name = %s\n", xvoTableGetText(pRoot, "name", 0));
	printf("age = %lld\n", xvoTableGetInt(pRoot, "age", 0));

	xvoTableSetInt(pRoot, "age", 0, 26);
	xvoTableSetBool(pRoot, "active", 0, TRUE);

	sOut = xrtStringifyJSON(pRoot, TRUE, &iOutSize);
	if ( sOut != NULL ) {
		printf("%.*s\n", (int)iOutSize, sOut);
		xrtFree(sOut);
	}

	xvoUnref(pRoot);
}
```

这里要特别记住两条：

- `xrtParseJSON()` 失败时，应用 `xvoIsNull()` 判断
- `xrtStringifyJSON()` 返回的是堆字符串，必须用 `xrtFree()` 释放


## 7. 为什么还需要 XSON

如果只有标准 JSON 类型，那么上一节已经够用了。

问题在于，`xvalue` 的能力比 JSON 更强。当前主线里，至少下面这些类型是标准 JSON 不能完整表达的：

- `time`
- `list`
- `coll(set)`
- `class`

这时如果你还坚持只用 JSON，就会出现一个问题：

- 程序内部能表示
- 但输出到 JSON 时，语义无法完整保留

这正是 `XSON` 存在的原因。

你可以把 `XSON` 理解成：

- 面向 `xvalue` 的完整序列化格式
- 对纯 JSON 保持兼容
- 在 JSON 基础上扩展 `time / list / set / class`

当前主线里：

- `xrtParseJSON()` 只处理 JSON
- `xrtParseXSON()` 既能处理纯 JSON，也能处理 XSON 扩展语法

也就是说，这段纯 JSON 文本同样可以交给 `xrtParseXSON()`：

```c
xvalue pRoot = xrtParseXSON("{\"name\":\"xrt\",\"nums\":[1,2,3]}", 0);
```


## 8. 第一个 XSON DEMO

下面这个例子演示的重点不是“怎么写文本”，而是“哪些 `xvalue` 类型只有 XSON 才能完整保留”。

```c
static void Demo_XSONRoundTrip(void)
{
	xvalue pRoot;
	xvalue pScores;
	xvalue pTags;
	xvalue pParsed;
	str sOut;
	size_t iOutSize;
	uint8 arrBlob[4] = { 0x01, 0x02, 0x03, 0x04 };

	pRoot = xvoCreateTable();
	iOutSize = 0;

	xvoTableSetText(pRoot, "name", 0, "demo", 0, FALSE);
	xvoTableSetTimeSerial(pRoot, "created", 0, 2000, 1, 2, 3, 4, 5);

	xvoTableSetList(pRoot, "scores", 0);
	pScores = xvoTableGetValue(pRoot, "scores", 0);
	xvoListSetInt(pScores, 1, 95);
	xvoListSetText(pScores, 10, "A", 0, FALSE);

	xvoTableSetColl(pRoot, "tags", 0);
	pTags = xvoTableGetValue(pRoot, "tags", 0);
	xvoCollSetText(pTags, "dev", 0, FALSE);
	xvoCollSetText(pTags, "runtime", 0, FALSE);

	xvoTableSetClass(pRoot, "blob", 0, 4);
	memcpy(xvoTableGetClass(pRoot, "blob", 0), arrBlob, 4);

	sOut = xrtStringifyXSON(pRoot, TRUE, 0, &iOutSize);
	if ( sOut != NULL ) {
		printf("%.*s\n", (int)iOutSize, sOut);

		pParsed = xrtParseXSON(sOut, 0);
		if ( !xvoIsNull(pParsed) ) {
			printf("created type = %d\n", xvoType(xvoTableGetValue(pParsed, "created", 0)));
			printf("scores type = %d\n", xvoType(xvoTableGetValue(pParsed, "scores", 0)));
			printf("tags type = %d\n", xvoType(xvoTableGetValue(pParsed, "tags", 0)));
			printf("blob size = %u\n", xvoGetSize(xvoTableGetValue(pParsed, "blob", 0)));
			xvoUnref(pParsed);
		}

		xrtFree(sOut);
	}

	xvoUnref(pRoot);
}
```

这段代码里有四个点最关键：

- `created` 是 `time`
- `scores` 是 `list`
- `tags` 是 `coll(set)`
- `blob` 是 `class`

这些值经过 `xrtStringifyXSON()` 再 `xrtParseXSON()` 后，类型仍然能被保留下来。


## 9. XSON 应该在什么场景用

最简单的判断标准是：

- 对外接口、开放协议、公共配置：优先 JSON
- 程序内部快照、完整落盘、需要无损保留 `xvalue` 类型：使用 XSON

尤其当你明确需要保留下面这些类型时，就不要继续写 JSON 教程式思路了：

- `time`
- `list`
- `coll(set)`
- `class`

另外还要知道一条边界：

- `point / func / custom` 不是 XSON 的常规可逆类型
- 默认情况下，遇到不支持类型会失败
- 需要“忽略不支持字段”时，再看 `XSON_F_IGNORE_UNSUPPORTED_ENCODE` 和 `XSON_F_IGNORE_UNSUPPORTED_DECODE`


## 10. 释放规则不要混

这一页里最重要的资源管理规则只有两类：

- `xvalue` 对象
	- 用完后 `xvoUnref()`
- `xrtStringifyJSON()` / `xrtStringifyXSON()` 返回的文本
	- 用完后 `xrtFree()`

可以先记成一句话：

- `xvalue` 走 `xvoUnref()`
- 文本结果走 `xrtFree()`

不要把这两套释放方式混用。


## 11. 与多线程的边界

还要提醒你一个经常被忽略的点：

- 普通 `xvoCreateTable()` / `xvoCreateArray()` 创建的是本线程 local root
- 如果你要跨线程共享，应显式使用 `xvoCreateTableEx(XRT_OBJMODE_SHARED)`、`xvoCreateArrayEx(XRT_OBJMODE_SHARED)` 等 shared root 入口

所以：

- 单线程里把 `xvalue` 当脚本语言对象来用，完全没问题
- 一旦跨线程共享，就要切到 shared root 语义

这也是为什么 `xvalue` 这条线和后面的多任务、队列、future 教程会连在一起。


## 12. 建议继续阅读

- [Value API](../api/api-value.md)
- [JSON API](../api/api-json.md)
- [XSON API](../api/api-xson.md)
- [用 xvalue + json 构造配置系统](../case/json-config-system.md)

---

**一句话总结：** `xvalue` 是程序内部的数据模型，`JSON` 是标准交换格式，`XSON` 是 `xvalue` 的完整序列化格式；先把这三者的职责边界想清楚，后面的模板、配置、网络和多任务文档才会真正串起来。
