# 用 xvalue + JSON 构造配置系统

> 这是一个最小但完整的组合案例：用 `xvalue` 表达运行时配置，用 JSON 进行读写，再把默认值、补丁和业务读取统一到一条主线上。

[返回范例解析](README.md) | [返回 API 索引](../api/README.md)

---

## 目录

- [场景](#场景)
- [为什么用这条主线](#为什么用这条主线)
- [目标结构](#目标结构)
- [最小实现](#最小实现)
- [这个案例体现了什么设计优势](#这个案例体现了什么设计优势)
- [后续可以如何扩展](#后续可以如何扩展)

---

## 场景

很多程序都需要下面这些能力：

- 从文件加载配置
- 有默认值
- 局部字段可以被后续覆盖
- 读取代码尽量简洁
- 最终还能再写回 JSON

如果用一堆零散字符串和手写结构体去做，通常会出现：

- 初始化很散
- 覆盖逻辑不好维护
- 输出回文件又要写一套单独序列化

XRT 当前主线更适合的做法是：

- 运行时配置统一用 `xvalue table`
- 配置文件格式统一用 JSON
- 读取、修改、合并都建立在同一套动态数据模型上

---

## 为什么用这条主线

这个案例体现的是 XRT 的一个核心优势：

- `json`
- `xvalue`
- 字符串
- 文件

这几层不是拼凑在一起的，而是天然连通。

因此你可以：

1. `xrtParseJSON_File()` 直接拿到 `xvalue`
2. 用 `xvoTableSet*()` 补默认值
3. 用 `xvoTableGet*()` 读取业务值
4. 用 `xrtStringifyJSON_File()` 再写回文件

整个链路只维护一份数据结构。

---

## 目标结构

假设配置文件 `config.json` 长这样：

```json
{
	"server": {
		"host": "127.0.0.1",
		"port": 8080
	},
	"log": {
		"level": "info"
	}
}
```

程序启动时希望做到：

- 如果文件不存在，就生成默认配置
- 如果文件存在但缺字段，就补上默认值
- 业务代码始终只面向统一配置对象读值

---

## 最小实现

```c
#include "xrt.h"
#include <stdio.h>

static xvalue CreateDefaultConfig(void)
{
	xvalue pConfig;
	xvalue pServer;
	xvalue pLog;

	pConfig = xvoCreateTable();
	pServer = xvoCreateTable();
	pLog = xvoCreateTable();

	xvoTableSetText(pServer, "host", 0, "127.0.0.1", 0, FALSE);
	xvoTableSetInt(pServer, "port", 0, 8080);
	xvoTableSetValue(pConfig, "server", 0, pServer, TRUE);

	xvoTableSetText(pLog, "level", 0, "info", 0, FALSE);
	xvoTableSetValue(pConfig, "log", 0, pLog, TRUE);

	return pConfig;
}

static void PatchDefaultConfig(xvalue pConfig)
{
	xvalue pServer;
	xvalue pLog;

	if ( !xvoTableExists(pConfig, "server", 0) ) {
		pServer = xvoCreateTable();
		xvoTableSetText(pServer, "host", 0, "127.0.0.1", 0, FALSE);
		xvoTableSetInt(pServer, "port", 0, 8080);
		xvoTableSetValue(pConfig, "server", 0, pServer, TRUE);
	} else {
		pServer = xvoTableGetValue(pConfig, "server", 0);
		if ( !xvoTableExists(pServer, "host", 0) ) {
			xvoTableSetText(pServer, "host", 0, "127.0.0.1", 0, FALSE);
		}
		if ( !xvoTableExists(pServer, "port", 0) ) {
			xvoTableSetInt(pServer, "port", 0, 8080);
		}
	}

	if ( !xvoTableExists(pConfig, "log", 0) ) {
		pLog = xvoCreateTable();
		xvoTableSetText(pLog, "level", 0, "info", 0, FALSE);
		xvoTableSetValue(pConfig, "log", 0, pLog, TRUE);
	} else {
		pLog = xvoTableGetValue(pConfig, "log", 0);
		if ( !xvoTableExists(pLog, "level", 0) ) {
			xvoTableSetText(pLog, "level", 0, "info", 0, FALSE);
		}
	}
}

int main(void)
{
	xvalue pConfig;
	xvalue pServer;
	xvalue pLog;
	str sHost;
	int64 iPort;
	str sLevel;

	xrtInit();

	pConfig = xrtParseJSON_File("config.json");
	if ( (pConfig == NULL) || xvoIsNull(pConfig) ) {
		pConfig = CreateDefaultConfig();
	} else {
		PatchDefaultConfig(pConfig);
	}

	pServer = xvoTableGetValue(pConfig, "server", 0);
	pLog = xvoTableGetValue(pConfig, "log", 0);

	sHost = xvoTableGetText(pServer, "host", 0);
	iPort = xvoTableGetInt(pServer, "port", 0);
	sLevel = xvoTableGetText(pLog, "level", 0);

	printf("host = %s\n", (char*)sHost);
	printf("port = %lld\n", (long long)iPort);
	printf("log.level = %s\n", (char*)sLevel);

	xrtStringifyJSON_File("config.json", pConfig, TRUE);

	xvoUnref(pConfig);
	xrtUnit();
	return 0;
}
```

---

## 这个案例体现了什么设计优势

### 1. 配置只维护一份数据

这里没有出现：

- JSON 结构一份
- 运行时结构体一份
- 写回文件结构再一份

而是始终围绕同一个 `xvalue table` 运转。

### 2. 默认值和补丁逻辑自然落在同一套 API 上

不管是：

- 初始化默认值
- 补缺字段
- 运行时修改

都统一用：

- `xvoTableSet*`
- `xvoTableGet*`
- `xvoTableExists`

### 3. 文件输入输出天然接上

XRT 当前主线里：

- `xrtParseJSON_File()`
- `xrtStringifyJSON_File()`

直接把文件系统和动态数据主线串起来了。

### 4. 后续可以无缝扩展到模板、HTTP、AI 场景

因为配置现在已经是 `xvalue`，所以你后面可以很自然地继续做：

- 模板渲染
- HTTP 请求参数组装
- AI 请求体构造
- 配置热更新

而不需要再做结构转换。

---

## 后续可以如何扩展

这条主线后续很适合继续扩展成：

1. 环境变量覆盖配置
2. 命令行参数覆盖配置
3. shared root 配置对象，供多线程读取
4. 配合 template 输出配置报表
5. 直接作为 HTTP / AI 请求体构造基础

如果后续要继续做更复杂的配置系统，这个案例就是最自然的起点。

---

**状态：第一批正式案例页**
