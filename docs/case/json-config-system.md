# 用 `file + path + value + json` 构造配置系统

> 这个案例要解决的不是“能不能读一个 JSON 文件”，而是配置系统最容易变乱的那几层边界：配置文件放在哪里，默认值在哪里补，业务代码读什么，最后又怎么安全写回。

[返回案例索引](README.md)

---

## 1. 场景

很多程序都会遇到同一组需求：

- 配置文件放在一个稳定路径
- 第一次启动时，文件可能根本不存在
- 旧配置升级后，文件里可能缺字段
- 业务层希望直接读“最终配置”，而不是自己再兜默认值
- 修改完配置后，还希望能再写回 JSON

如果这几件事拆成几套模型，最后通常会变成：

- 文件里一套字段
- 运行时结构体一套字段
- 默认值逻辑再写一套
- 写回文件又自己拼一套 JSON

XRT 当前主线更适合的做法是：

- `path` 决定配置文件路径
- `file` 决定怎么读写文件
- `json` 负责交换格式
- `xvalue` 负责运行时唯一配置模型

也就是说，文件只是入口，`xvalue table` 才是配置系统真正的中轴。


## 2. 这次为什么选 `file + path + value + json`

这条主线的分工非常清楚：

- `path` 负责拼路径、取目录、判断是否存在
- `file` 负责文本读写
- `json` 负责“字符串 <-> 结构化配置”
- `xvalue` 负责默认值补丁、业务读取、运行时修改

这会带来 3 个直接收益：

- 配置系统始终只维护一份运行时对象树
- 默认值补丁和业务读取都落在同一套 API 上
- 后面接模板、HTTP、AI 请求体时，不需要再把配置重塑一次

这也是为什么这页故意不先把配置切成一大坨 `struct`。  
不是结构体不好，而是在“字段可能增减、文件可能升级”的阶段，`xvalue` 更适合作为主线。


## 3. 完整骨架

下面这份代码会完成一条完整配置链：

1. 用 `xrtPathJoin()` 决定配置文件路径
2. 文件不存在时创建默认配置
3. 文件存在时读取文本并 `xrtParseJSON()`
4. 对缺失字段打默认补丁
5. 业务层只读取补完后的统一配置
6. 最后再格式化写回磁盘

```c
#include "xrt.h"
#include <stdio.h>

static xvalue procCreateDefaultConfig(void)
{
	xvalue vConfig = NULL;
	xvalue vServer = NULL;
	xvalue vLog = NULL;
	xvalue vFeature = NULL;

	vConfig = xvoCreateTable();
	if ( vConfig == NULL ) {
		return NULL;
	}

	xvoTableSetTable(vConfig, "server", 0);
	vServer = xvoTableGetValue(vConfig, "server", 0);
	xvoTableSetText(vServer, "host", 0, "127.0.0.1", 0, FALSE);
	xvoTableSetInt(vServer, "port", 0, 8080);

	xvoTableSetTable(vConfig, "log", 0);
	vLog = xvoTableGetValue(vConfig, "log", 0);
	xvoTableSetText(vLog, "level", 0, "info", 0, FALSE);

	xvoTableSetTable(vConfig, "feature", 0);
	vFeature = xvoTableGetValue(vConfig, "feature", 0);
	xvoTableSetBool(vFeature, "hot_reload", 0, FALSE);

	return vConfig;
}


static void procPatchServerConfig(xvalue vConfig)
{
	xvalue vServer = NULL;

	if ( !xvoTableExists(vConfig, "server", 0) || (xvoType(xvoTableGetValue(vConfig, "server", 0)) != XVO_DT_TABLE) ) {
		xvoTableSetTable(vConfig, "server", 0);
	}

	vServer = xvoTableGetValue(vConfig, "server", 0);
	if ( !xvoTableExists(vServer, "host", 0) ) {
		xvoTableSetText(vServer, "host", 0, "127.0.0.1", 0, FALSE);
	}
	if ( !xvoTableExists(vServer, "port", 0) ) {
		xvoTableSetInt(vServer, "port", 0, 8080);
	}
}


static void procPatchLogConfig(xvalue vConfig)
{
	xvalue vLog = NULL;

	if ( !xvoTableExists(vConfig, "log", 0) || (xvoType(xvoTableGetValue(vConfig, "log", 0)) != XVO_DT_TABLE) ) {
		xvoTableSetTable(vConfig, "log", 0);
	}

	vLog = xvoTableGetValue(vConfig, "log", 0);
	if ( !xvoTableExists(vLog, "level", 0) ) {
		xvoTableSetText(vLog, "level", 0, "info", 0, FALSE);
	}
}


static void procPatchFeatureConfig(xvalue vConfig)
{
	xvalue vFeature = NULL;

	if ( !xvoTableExists(vConfig, "feature", 0) || (xvoType(xvoTableGetValue(vConfig, "feature", 0)) != XVO_DT_TABLE) ) {
		xvoTableSetTable(vConfig, "feature", 0);
	}

	vFeature = xvoTableGetValue(vConfig, "feature", 0);
	if ( !xvoTableExists(vFeature, "hot_reload", 0) ) {
		xvoTableSetBool(vFeature, "hot_reload", 0, FALSE);
	}
}


static void procPatchDefaultConfig(xvalue vConfig)
{
	procPatchServerConfig(vConfig);
	procPatchLogConfig(vConfig);
	procPatchFeatureConfig(vConfig);
}


static xvalue procLoadConfig(const char* sConfigPath)
{
	str sJson = NULL;
	size_t iJsonSize = 0;
	xvalue vConfig = NULL;

	if ( !xrtPathExists(sConfigPath) ) {
		return procCreateDefaultConfig();
	}

	sJson = xrtFileReadAll(sConfigPath, XRT_CP_UTF8, &iJsonSize);
	if ( sJson == NULL ) {
		return procCreateDefaultConfig();
	}

	vConfig = xrtParseJSON(sJson, iJsonSize);
	xrtFree(sJson);

	if ( (vConfig == NULL) || xvoIsNull(vConfig) || (xvoType(vConfig) != XVO_DT_TABLE) ) {
		if ( vConfig != NULL ) {
			xvoUnref(vConfig);
		}
		return procCreateDefaultConfig();
	}

	procPatchDefaultConfig(vConfig);
	return vConfig;
}


static bool procSaveConfig(const char* sConfigPath, xvalue vConfig)
{
	str sConfigDir = NULL;
	str sJson = NULL;
	size_t iJsonSize = 0;
	bool bOk = false;

	if ( (sConfigPath == NULL) || (vConfig == NULL) ) {
		return false;
	}

	sConfigDir = xrtPathGetDir((str)sConfigPath, 0u);
	if ( (sConfigDir != NULL) && (xrtDirCreateAll(sConfigDir) != TRUE) ) {
		goto cleanup;
	}

	sJson = xrtStringifyJSON(vConfig, TRUE, &iJsonSize);
	if ( sJson == NULL ) {
		goto cleanup;
	}

	bOk = xrtFileWriteAll((str)sConfigPath, sJson, iJsonSize, XRT_CP_UTF8) > 0;

cleanup:
	if ( sJson != NULL ) {
		xrtFree(sJson);
	}
	if ( sConfigDir != NULL ) {
		xrtFree(sConfigDir);
	}
	return bOk;
}


int main(void)
{
	xrtGlobalData* pCore = NULL;
	xvalue vConfig = NULL;
	xvalue vServer = NULL;
	xvalue vLog = NULL;
	xvalue vFeature = NULL;
	str sConfigPath = NULL;
	const char* sHost = NULL;
	int64 iPort = 0;
	const char* sLogLevel = NULL;
	bool bHotReload = false;
	int iRet = 1;

	pCore = xrtInit();
	if ( pCore == NULL ) {
		return 1;
	}

	sConfigPath = xrtPathJoin(4, pCore->AppPath, "build", "demo-config", "config.json");
	if ( sConfigPath == NULL ) {
		fprintf(stderr, "build config path failed\n");
		goto cleanup;
	}

	vConfig = procLoadConfig(sConfigPath);
	if ( vConfig == NULL ) {
		fprintf(stderr, "load config failed\n");
		goto cleanup;
	}

	vServer = xvoTableGetValue(vConfig, "server", 0);
	vLog = xvoTableGetValue(vConfig, "log", 0);
	vFeature = xvoTableGetValue(vConfig, "feature", 0);

	sHost = xvoTableGetText(vServer, "host", 0);
	iPort = xvoTableGetInt(vServer, "port", 0);
	sLogLevel = xvoTableGetText(vLog, "level", 0);
	bHotReload = xvoTableGetBool(vFeature, "hot_reload", 0);

	printf("config path : %s\n", sConfigPath);
	printf("server.host : %s\n", sHost ? sHost : "");
	printf("server.port : %lld\n", (long long)iPort);
	printf("log.level   : %s\n", sLogLevel ? sLogLevel : "");
	printf("hot_reload  : %s\n", bHotReload ? "true" : "false");

	/* 模拟一次运行时修改 */
	xvoTableSetText(vLog, "level", 0, "debug", 0, FALSE);

	if ( !procSaveConfig(sConfigPath, vConfig) ) {
		fprintf(stderr, "save config failed\n");
		goto cleanup;
	}

	printf("config saved\n");
	iRet = 0;

cleanup:
	if ( vConfig != NULL ) {
		xvoUnref(vConfig);
	}
	if ( sConfigPath != NULL ) {
		xrtFree(sConfigPath);
	}

	xrtUnit();
	return iRet;
}
```


## 4. 这份代码真正想教会你的事

### 4.1 配置文件路径和配置对象是两层问题

这页故意把路径单独拿出来讲，因为很多配置系统一开始就把“去哪里找文件”和“文件里是什么内容”混成一件事。

更稳的拆法应该是：

- `xrtPathJoin()` 决定配置文件路径
- `xrtPathGetDir()` 决定父目录
- `xrtDirCreateAll()` 负责确保目录存在
- `xvalue` 负责真正的配置对象

路径层只管“放哪”，对象层只管“里面是什么”。


### 4.2 默认值补丁不应该散落在业务层

这页里最重要的函数其实不是 `main()`，而是：

- `procPatchServerConfig()`
- `procPatchLogConfig()`
- `procPatchFeatureConfig()`

这些函数的意义是：

- 让默认值补丁只存在一处
- 让业务层永远读取“已经补好的最终配置”

如果你把默认值判断散到每个业务模块里，配置系统很快就会失去边界。


### 4.3 文件读写和 JSON 解析最好拆成两步

这页没有直接依赖 `xrtParseJSON_File()` / `xrtStringifyJSON_File()`，而是刻意拆成：

- `xrtFileReadAll()` + `xrtParseJSON()`
- `xrtStringifyJSON()` + `xrtFileWriteAll()`

原因不是便捷函数不好，而是这里想把四层边界讲清楚：

- `path`
- `file`
- `json`
- `value`

只有先看清这 4 层，后面你才知道什么时候适合用便捷函数，什么时候需要自己接管其中一层。


### 4.4 这时先不要急着切结构体

在“配置格式还会变、字段还在增、还需要兼容旧文件”的阶段，过早把所有配置都切成固定结构体，通常会让升级成本变高。

当前阶段更合理的做法是：

- 运行时主线先保持 `xvalue`
- 业务读取只取自己关心的字段

等配置稳定下来，再决定是否需要再投影成更窄的业务结构体。


## 5. 运行与验证

第一次运行时，程序应该会：

- 发现 `build/demo-config/config.json` 不存在
- 创建默认配置
- 写回一个格式化 JSON 文件

第二次运行时，程序应该会：

- 读回这个文件
- 补齐缺失字段
- 把 `log.level` 改成 `debug`
- 再次写回磁盘

你可以手工做两种验证：

1. 删除整个 `build/demo-config` 目录，再运行一次，确认默认文件会被重建。
2. 手工删掉 `feature.hot_reload` 或 `server.host`，再运行一次，确认缺字段会被补回。


## 6. 为什么这比“文件一套、结构体一套、写回再一套”更稳

如果你把这几层拆成三份独立模型，很快会遇到这些问题：

- 字段名同步成本很高
- 默认值逻辑写两遍甚至三遍
- 配置升级时要改多个位置
- 写回文件时容易漏字段

而统一到 `xvalue` 之后，变化路径会很清楚：

- 加字段时，先改默认补丁
- 再改业务读取
- 最后保存时自然会带上新字段

这正是配置系统最需要的稳定性。


## 7. 常见错误

### 错误一：业务代码自己到处补默认值

这会让配置边界彻底散掉。  
默认值补丁应该在加载阶段一次做完。

### 错误二：把文件不存在、文件损坏、根节点类型错误混成一种情况

这页里把这些都统一兜回默认配置，是为了保证“业务层拿到的一定是可用配置表”。

### 错误三：一上来就把所有配置全切成固定结构体

在配置格式还不稳定的时候，这通常会增加演进成本，而不是降低它。

### 错误四：写回文件时重新手工拼 JSON

既然运行时主线已经是 `xvalue`，就应该直接 `xrtStringifyJSON()`，不要再走回字符串硬拼那条旧路。


## 8. 建议继续阅读

- [Value API](../api/api-value.md)
- [JSON API](../api/api-json.md)
- [用 Template 渲染一个 HTML 页面](template-render-html.md)
- [一个完整的 HTTP + JSON + Template 服务链路](http-json-template-chain.md)

---

**一句话总结：** 配置系统真正稳定的关键，不是“读到了一个 JSON 文件”，而是“从路径、文件、JSON 到运行时配置对象，始终只围绕同一份 `xvalue table` 运转”。这条主线一旦立住，后面不管接模板、HTTP 还是 AI 输出，都会顺很多。
