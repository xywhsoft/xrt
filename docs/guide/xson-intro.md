# XSON 入门：什么时候该从 JSON 升级到完整 `xvalue` 序列化

> 目标：把 `xrtParseXSON()`、`xrtParseXSONEx()`、`xrtParseXSON_File()`、`xrtStringifyXSON()`、`xrtStringifyXSON_File()` 讲成第 3 阶段里“无损结构化快照”这条正式主线。读完这页后，你应该明确知道：为什么 `xvalue` 一旦出现 `time / list / set / class` 就不能继续假装自己只在写 JSON，当前 `XSON` 的真实语法和真实边界是什么，以及什么时候它应该进内部快照，什么时候它根本不该出现在对外协议里。

[返回教学文档](README.md)

---

## 1. 为什么 `xson` 要单独讲，而不是塞在 JSON 后面一句带过

学完 [xvalue、JSON 与 XSON 入门](xvalue-json-intro.md) 以后，很多人会先得到一个大概印象：

- `xvalue` 是程序内部数据模型
- `JSON` 是对外交换格式
- `XSON` 是扩展格式

这个印象没有错，但还不够落地。

因为真实项目里，问题很快就会变成：

- 这份数据里已经不只有 `text / int / array / dict`
- 我有 `time`
- 我有带洞位索引的 `list`
- 我有去重语义的 `set(coll)`
- 我还有一块原始二进制 `class`

这时如果你继续只讲 JSON，就会立刻遇到两个硬问题：

1. 语义丢失  
例如：
	- `list[1:"a",5:"b"]` 不是普通数组
	- `set{"dev","ops"}` 也不是普通对象

2. 手工降级越来越乱  
例如：
	- 时间改成字符串
	- 集合改成数组
	- 二进制改成临时 `BASE64`
	- 读回时再手工恢复

所以 `xson` 不是“给 JSON 多加一点语法糖”。  
它的真正位置是：

- 面向 `xvalue` 的完整序列化格式
- 用来做内部快照、完整落盘、无损往返


## 2. 先把 8 个边界分清楚

### 2.1 `JSON` 和 `XSON` 的职责不同

推荐你把它们硬拆开理解：

| 格式 | 主要用途 |
|------|----------|
| `JSON` | 对外协议、开放配置、标准交换 |
| `XSON` | 程序内部快照、完整落盘、需要保留 `xvalue` 类型 |

所以：

- HTTP API
- 第三方接口
- 给别人看的公共配置

优先仍然是：

- `JSON`

而：

- 内部缓存快照
- 测试基线
- 调试导出
- 无损保存 `xvalue`

更接近：

- `XSON`


### 2.2 `XSON` 保持对纯 JSON 的兼容

当前主线里：

- 所有合法 `JSON` 文本
	- 都应该是合法 `XSON`
- `xrtParseXSON()`
	- 可以直接解析纯 JSON

也就是说，这样写是合法的：

```c
xvalue pRoot = xrtParseXSON("{\"project\":\"xrt\",\"items\":[1,2,3]}", 0);
```

所以 `XSON` 不是和 JSON 平行的另一套完全断裂语法。  
更准确地说，它是：

- 兼容 JSON
- 在 JSON 之上补足 `xvalue` 的缺口


### 2.3 只有这几类类型超出了标准 JSON

当前主线里，最常需要 `XSON` 的是这 4 类：

- `time`
- `list`
- `set(coll)`
- `class`

对应的显式语法分别是：

```xson
time(2000-01-02 12:00:00)
list[1:"alpha",5:"beta"]
set{"dev","ops"}
class(AQIDBA==)
```

如果你的数据树里没有这些扩展类型，那么：

- `JSON` 往往已经够用

如果已经出现它们，就不要再假装“只是一个普通 JSON 对象”。


### 2.4 `XSON` 里有隐式判定，但正式文档和长期文件更推荐显式写法

当前解析器支持一些隐式容器判定：

- `[1:"Alice",5:time(...)]`
	- 会被识别成 `list`
- `{"dev","ops","qa"}`
	- 会被识别成 `set`

这对人手写调试文本很方便，但正式教程和长期落盘更推荐：

- `list[...]`
- `set{...}`

原因很简单：

- 一眼就能看懂
- 不依赖“首个元素长什么样”的判定规则
- 后面回看文件时不容易误解


### 2.5 解析和 getter 的“空值语义”都不是 `NULL`

这是当前 `xson` 最容易把示例写错的地方。

当前主线里：

- `xrtParseXSON()` 失败
	- 返回的是 `xvalue null`
	- 不是 C `NULL`
- `xrtParseXSON_File()` 失败
	- 也返回 `xvalue null`
- `xvoTableGetValue()` / `xvoListGetValue()` / `xvoArrayGetValue()`
	- 取不到值时也返回 `xvalue null`
	- 不是 C `NULL`

所以不要写成：

```c
if ( pRoot ) {
	/* 错误示范 */
}
```

更稳的写法是：

```c
if ( !xvoIsNull(pRoot) ) {
	/* 正确示范 */
}
```

还要再知道一个更细的边界：

- 当前公开解析 API 里
	- “解析失败”
	- 和“根值本来就是文本字面量 `null`”
	- 都会得到 `xvalue null`

所以教学和业务里更稳的做法通常是：

- 让根值优先使用 object / array / list / set 这类容器


### 2.6 `class()` 只是原始字节快照，不是跨 ABI 的结构描述

`class(BASE64)` 很容易被误用。

当前它能保证的是：

- 原始字节可逆

它不能保证的是：

- 自动带类型名
- 自动带字段名
- 自动带版本号
- 自动跨平台 / 跨 ABI 可移植

所以它更适合：

- 同构环境里的内部快照
- 简单二进制载荷

不适合：

- 当作公开协议字段
- 当作长期跨版本兼容格式


### 2.7 `IGNORE_UNSUPPORTED_*` 只是跳过，不是自动补救

当前 flags 只有两类：

- `XSON_F_IGNORE_UNSUPPORTED_ENCODE`
- `XSON_F_IGNORE_UNSUPPORTED_DECODE`

它们的作用是：

- 遇到当前不支持的类型时，跳过该值

它们不做的是：

- 自动转成 `null`
- 自动保留占位
- 自动把 `point / func / custom` 变成可逆文本

所以这些 flags 更像：

- “允许导出一个删减版快照”

而不是：

- “把所有类型都安全编码出来”


### 2.8 `XSON` 解决的是表示与无损问题，不是多任务问题

如果只有一条主线程，你的问题通常不是“XSON 会不会卡线程”，而是：

- 你把内部数据反复降级成字符串
- 又在主线程里手工恢复语义
- 让表示层和业务层纠缠在一起

所以 `XSON` 解决的是：

- 表示和无损往返

不是：

- 调度和并发

如果你的瓶颈是：

- 网络等待
- 文件等待
- 主线程阻塞

那要回到：

- [多任务总论](multitask-overview.md)


## 3. 最小 DEMO：先用 `xrtParseXSON()` 解析纯 JSON，确认兼容边界

第一步先不要一上来就写 `time()` 或 `set{}`。

先确认一件最重要的事实：

- `xrtParseXSON()` 可以直接吃纯 JSON

这样你后面才知道，什么时候继续留在 JSON，什么时候再升级到 XSON 扩展。

```c
#include "xrt.h"
#include <stdio.h>


int main(void)
{
	const char* sJson = "{\"project\":\"xrt\",\"items\":[1,2,3],\"active\":true}";
	xvalue pRoot = NULL;
	xvalue pItems = NULL;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	pRoot = xrtParseXSON((char*)sJson, 0);
	if ( xvoIsNull(pRoot) ) {
		printf("parse failed or root is literal null\n");
		xvoUnref(pRoot);
		xrtUnit();
		return 1;
	}

	printf("project = %s\n", xvoTableGetText(pRoot, "project", 0));

	pItems = xvoTableGetValue(pRoot, "items", 0);
	if ( !xvoIsNull(pItems) ) {
		printf("items count = %u\n", xvoArrayItemCount(pItems));
	}

	xvoUnref(pRoot);
	xrtUnit();
	return 0;
}
```

这个 DEMO 想让你先记住两件事：

1. `XSON` 入口可以兼容纯 JSON。
2. 判断结果和取字段都应该优先用 `xvoIsNull()`，不要写 `if ( pRoot )`。


## 4. 升级 DEMO：把 `time / list / set / class` 无损 round-trip 一次

真正进入 `XSON` 主线以后，最重要的不是“怎么解析文本”，而是：

- 一份完整 `xvalue`
- 经过 `xrtStringifyXSON()`
- 再经过 `xrtParseXSON()`
- 类型还能不能保住

下面这个例子就是最典型的一次无损往返：

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>


int main(void)
{
	xvalue pRoot = NULL;
	xvalue pQueue = NULL;
	xvalue pFlags = NULL;
	xvalue pBlob = NULL;
	xvalue pParsed = NULL;
	size_t iSize = 0;
	str sXson = NULL;
	uint8* pData = NULL;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	pRoot = xvoCreateTable();
	xvoTableSetText(pRoot, "name", 0, "demo", 0, FALSE);
	xvoTableSetTimeSerial(pRoot, "created", 0, 2026, 4, 1, 9, 30, 0);

	pQueue = xvoCreateList();
	xvoListSetText(pQueue, 1, "alpha", 0, FALSE);
	xvoListSetText(pQueue, 5, "beta", 0, FALSE);
	xvoTableSetValue(pRoot, "queue", 0, pQueue, FALSE);
	xvoUnref(pQueue);

	pFlags = xvoCreateColl();
	xvoCollSetText(pFlags, "xson", 0, FALSE);
	xvoCollSetText(pFlags, "demo", 0, FALSE);
	xvoTableSetValue(pRoot, "flags", 0, pFlags, FALSE);
	xvoUnref(pFlags);

	pBlob = xvoCreateClass(4);
	pData = (uint8*)xvoGetClass(pBlob);
	pData[0] = 0x01;
	pData[1] = 0x02;
	pData[2] = 0x03;
	pData[3] = 0x04;
	xvoTableSetValue(pRoot, "blob", 0, pBlob, FALSE);
	xvoUnref(pBlob);

	sXson = xrtStringifyXSON(pRoot, TRUE, 0, &iSize);
	if ( sXson == NULL ) {
		xvoUnref(pRoot);
		xrtUnit();
		return 1;
	}

	printf("%.*s\n", (int)iSize, sXson);

	pParsed = xrtParseXSON(sXson, iSize);
	if ( !xvoIsNull(pParsed) ) {
		printf("created type = %d\n", xvoTableItemType(pParsed, "created", 0));
		printf("queue type   = %d\n", xvoTableItemType(pParsed, "queue", 0));
		printf("flags type   = %d\n", xvoTableItemType(pParsed, "flags", 0));
		printf("blob size    = %u\n", xvoTableItemSize(pParsed, "blob", 0));
		xvoUnref(pParsed);
	}

	xrtFree(sXson);
	xvoUnref(pRoot);
	xrtUnit();
	return 0;
}
```

这段 DEMO 里最关键的不是输出文本本身，而是这 4 个字段的语义：

- `created`
	- 仍然是 `time`
- `queue`
	- 仍然是 `list`
- `flags`
	- 仍然是 `set(coll)`
- `blob`
	- 仍然是 `class`

这就是 `XSON` 相比 JSON 真正值钱的地方：

- 类型没有被你手工降级成“看起来差不多的字符串”


## 5. 文件 DEMO：什么时候该把 `XSON` 直接写盘

如果你明确要保存的是：

- 内部快照
- 调试导出
- 基线文件
- 需要完整 round-trip 的中间状态

那么直接用文件 API 会比“自己先 stringify，再自己写文件”更顺。

```c
#include "xrt.h"
#include <stdio.h>


int main(void)
{
	const char* sFile = "snapshot.xson";
	xvalue pRoot = NULL;
	xvalue pRead = NULL;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	pRoot = xvoCreateTable();
	xvoTableSetText(pRoot, "name", 0, "file-demo", 0, FALSE);
	xvoTableSetTimeSerial(pRoot, "created", 0, 2026, 4, 1, 10, 0, 0);
	xvoTableSetList(pRoot, "queue", 0);
	xvoListSetText(xvoTableGetValue(pRoot, "queue", 0), 1, "alpha", 0, FALSE);

	if ( xrtStringifyXSON_File((char*)sFile, pRoot, TRUE, 0) > 0 ) {
		pRead = xrtParseXSON_File((char*)sFile);
		if ( !xvoIsNull(pRead) ) {
			printf("name = %s\n", xvoTableGetText(pRead, "name", 0));
			xvoUnref(pRead);
		}
	}

	xrtFileDelete((char*)sFile);
	xvoUnref(pRoot);
	xrtUnit();
	return 0;
}
```

这里要记住两条：

1. 文件解析失败同样返回 `xvalue null`，不是 `NULL`。
2. `XSON` 文件更适合内部产物，不适合作为公开交换格式默认外露。


## 6. 什么时候应该继续用 JSON，而不是急着全切到 XSON

一个最实用的判断标准是：

### 6.1 继续用 JSON

如果你满足下面这些条件，继续用 JSON 更对：

- 数据只包含标准 JSON 类型
- 这是对外协议或开放配置
- 需要和其他语言或外部系统稳定互通

### 6.2 切到 XSON

如果你已经出现这些特征，切到 XSON 会很值：

- 数据里已经出现 `time / list / set / class`
- 你不想再手工做类型降级和恢复
- 这份文本主要给 XRT 自己读回
- 你需要无损 round-trip

### 6.3 既有对外 JSON，又有内部 XSON

很多真实程序最后都会变成两层：

- 对外接口继续 JSON
- 内部快照、缓存和调试导出用 XSON

这不是重复设计，而是：

- 把“开放交换”和“内部无损表达”分开


## 7. 常见错误

### 7.1 继续写 `if ( pRoot )`

错。  
当前解析失败和 getter 取空值都走 `xvalue null` 哨兵，不是 `NULL`。

### 7.2 以为 `class()` 是带 schema 的结构描述

错。  
当前只是原始字节快照，不自动携带类型和 ABI 信息。

### 7.3 用 XSON 做公开 HTTP / 配置协议

通常不推荐。  
对外默认还是 JSON 更稳。

### 7.4 长期文件里大量依赖隐式 `list` / `set` 写法

不推荐。  
正式落盘和正式文档更稳的做法仍然是显式前缀。

### 7.5 以为 `IGNORE_UNSUPPORTED_*` 会自动补 null

错。  
它只是跳过，不会替你保留占位。


## 8. 建议继续阅读

- [XSON API](../api/api-xson.md)
- [xvalue、JSON 与 XSON 入门](xvalue-json-intro.md)
- [JSON API](../api/api-json.md)
- [Template 入门：什么时候该用模板，而不是拼字符串](template-intro.md)
- [用 xvalue + json 构造配置系统](../case/json-config-system.md)
