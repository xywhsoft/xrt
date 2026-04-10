# XSON 处理库

> `xvalue` 完整序列化格式，保持对 `JSON` 的兼容，并扩展 `time / list / set / class`

[English](api-xson.en.md) | [中文](api-xson.md) | [返回索引](README.md)

---

## 目录

- [重要说明](#重要说明)
- [与 JSON 的关系](#与-json-的关系)
- [支持范围](#支持范围)
- [语法规则](#语法规则)
- [解析 API](#解析-api)
- [序列化 API](#序列化-api)
- [错误处理与 Flags](#错误处理与-flags)
- [推荐输出形式](#推荐输出形式)
- [使用示例](#使用示例)
- [性能基准](#性能基准)
- [相关文档](#相关文档)

---

## 重要说明

`XSON` 是 XRT 当前主线里的私有扩展格式，用来完整表达 `xvalue`。

当前主线建议这样理解：

- `JSON` 是通用交换格式
- `XSON` 是面向 `xvalue` 的完整序列化格式
- 所有合法 `JSON` 文本都必须是合法 `XSON` 文本
- 所有合法 `JSON` 文本在 `XSON` 中的语义必须保持不变

因此：

- 标准接口交换、开放协议、对外配置，优先用 `JSON`
- 程序内部快照、完整落盘、需要保留 `time / list / set / class` 时，使用 `XSON`
- `xrtParseJSON()` 只处理 JSON
- `xrtParseXSON()` 既能处理纯 JSON，也能处理 XSON 扩展语法



## 与 JSON 的关系

`JSON` 和 `XSON` 的职责边界如下：

- `xrtParseJSON()` / `xrtStringifyJSON()`
	- 面向标准 JSON
	- 只负责 `null / bool / int / float / text / array / dict(table)`
- `xrtParseXSON()` / `xrtStringifyXSON()`
	- 面向 `xvalue` 完整表达
	- 在 JSON 基础上增加 `time / list / set / class`

如果你的数据里出现以下类型，就不应继续使用 `xrtStringifyJSON()`：

- `time`
- `list`
- `set(coll)`
- `class`

这类数据应切换到 `xrtStringifyXSON()`，否则标准 JSON 无法完整表达。



## 支持范围

### 当前支持的 `xvalue` 类型

| `xvalue` 类型 | XSON 表示 |
| --- | --- |
| `null` | `null` |
| `bool` | `true` / `false` |
| `int` | `123` |
| `float` | `3.14` |
| `text` | `"hello"` |
| `time` | `time(2000-01-02 12:00:00)` |
| `array` | `[1,2,3]` 或 `array[1,2,3]` |
| `list` | `list[1:"a",5:100]` |
| `dict(table)` | `{"a":1}` 或 `dict{"a":1}` |
| `set(coll)` | `set{1,2,3}` 或 `{1,2,3}` |
| `class` | `class(AQIDBA==)` |

### 当前不支持的类型

- `ptr(point)`
- `func`
- `custom`

默认情况下：

- 序列化遇到不支持类型会失败
- 反序列化遇到不支持类型前缀会失败

但可以通过 flags 改为“忽略不支持类型”模式，详见后文。



## 语法规则

### 1. 纯 JSON 保持原义

以下写法在 `XSON` 中保持和 `JSON` 完全相同的语义：

```xson
null
true
123
3.14
"hello"
[1,2,3]
{"name":"xrt","ok":true}
```

### 2. 扩展前缀

XSON 在 JSON 基础上增加以下前缀语法：

- `array[ ... ]`
- `list[ ... ]`
- `dict{ ... }`
- `set{ ... }`
- `time( ... )`
- `class( ... )`

约束：

- 前缀和开启符之间不允许插入空白
- 扩展前缀只在 `xrtParseXSON()` 入口生效

### 3. 空容器规则

空容器在 XSON 中固定解释为：

- `[]` 表示空数组
- `{}` 表示空字典
- `list[]` 表示空列表
- `set{}` 表示空集合

### 4. 非空无前缀容器的判定

对于不带前缀的非空容器：

- 非空 `[...]`
	- 如果首个顶层元素匹配 `int64:value`，则按 `list` 解析
	- 否则按 `array` 解析
- 非空 `{...}`
	- 如果首个顶层元素匹配 `key:value`，则按 `dict` 解析
	- 否则按 `set` 解析

附加约束：

- 一旦按 `list` 进入解析，则所有顶层元素都必须满足 `int64:value`
- 一旦按 `dict` 进入解析，则所有顶层元素都必须满足 `key:value`
- `list` 索引允许负数
- `list` 索引不允许浮点数

### 5. 扩展值格式

`time`：

```xson
time(2000-01-02 12:00:00)
```

约束：

- 内部文本固定为 `YYYY-MM-DD HH:MM:SS`
- 当前不支持毫秒和时区偏移

`class`：

```xson
class(AQIDBA==)
```

约束：

- `class(...)` 内部是结构体原始字节的 `BASE64`
- 当前不携带类型名、字段信息和 ABI 信息
- 只保证“同构快照”可逆，不保证跨平台可移植



## 解析 API

### 从字符串解析

```c
XXAPI xvalue xrtParseXSON(str sText, size_t iSize);
XXAPI xvalue xrtParseXSONEx(str sText, size_t iSize, uint32 iFlags);
```

参数：

| 参数 | 说明 |
| --- | --- |
| `sText` | XSON 或纯 JSON 文本 |
| `iSize` | 文本长度，传 `0` 表示自动计算 |
| `iFlags` | 解析 flags，见后文 |

说明：

- `xrtParseXSON()` 等价于 `xrtParseXSONEx(..., 0)`
- 纯 `JSON` 文本可以直接传给 `xrtParseXSON()`
- 解析失败时返回 `xvalue null`，而不是 C `NULL`
- 根值如果本来就是文本字面量 `null`，当前公开解析 API 也同样返回 `xvalue null`
- 因此：对象 / 数组 / `list` / `set` 这类根值场景，可直接结合 `xvoIsNull()` 判断；如果你必须区分“解析失败”和“根值就是 `null`”，当前公开 API 没有单独错误对象

### 从文件解析

```c
XXAPI xvalue xrtParseXSON_File(str sFile);
XXAPI xvalue xrtParseXSON_FileEx(str sFile, uint32 iFlags);
```

参数：

| 参数 | 说明 |
| --- | --- |
| `sFile` | XSON 文件路径 |
| `iFlags` | 解析 flags，见后文 |



## 序列化 API

### 序列化到字符串

```c
XXAPI str xrtStringifyXSON(xvalue varVal, int bFormat, uint32 iFlags, size_t* pRetSize);
```

参数：

| 参数 | 说明 |
| --- | --- |
| `varVal` | 要序列化的 `xvalue` |
| `bFormat` | 是否格式化输出，`FALSE` 为紧凑模式，`TRUE` 为格式化模式 |
| `iFlags` | 序列化 flags，见后文 |
| `pRetSize` | 输出结果长度，可传 `NULL` |

返回值：

- 成功返回堆字符串，需要用 `xrtFree()` 释放
- 失败返回 `NULL`

### 序列化到文件

```c
XXAPI int xrtStringifyXSON_File(str sFile, xvalue varVal, int bFormat, uint32 iFlags);
```

参数：

| 参数 | 说明 |
| --- | --- |
| `sFile` | 输出文件路径 |
| `varVal` | 要序列化的 `xvalue` |
| `bFormat` | 是否格式化输出 |
| `iFlags` | 序列化 flags |



## 错误处理与 Flags

当前实现的 flags：

```c
#define XSON_F_IGNORE_UNSUPPORTED_ENCODE	0x0001u
#define XSON_F_IGNORE_UNSUPPORTED_DECODE	0x0002u
```

### `XSON_F_IGNORE_UNSUPPORTED_ENCODE`

含义：

- 序列化时，如果容器内部遇到 `ptr(point) / func / custom`
- 跳过该值，而不是整体失败

约束：

- 只对“不支持的类型”生效
- 根值如果本身是不支持类型，仍然失败
- 不会自动补成 `null`
- `dict` 中被跳过的键值会整体移除
- `array / list / set` 中被跳过的元素会直接省略

### `XSON_F_IGNORE_UNSUPPORTED_DECODE`

含义：

- 反序列化时，如果容器内部遇到当前未支持的类型前缀
- 跳过该值，而不是整体失败

约束：

- 只对“不支持的类型前缀”生效
- 根值如果本身是不支持类型，仍然失败
- 普通语法错误仍然失败
- 例如括号不匹配、时间格式错误、`BASE64` 非法，都不会被忽略



## 推荐输出形式

为了兼顾可读性和避免歧义，`xrtStringifyXSON()` 当前推荐输出为：

- `array` 输出为 `[...]`
- `dict(table)` 输出为 `{...}`
- `list` 输出为 `list[...]`
- `set(coll)` 输出为 `set{...}`
- `time` 输出为 `time(YYYY-MM-DD HH:MM:SS)`
- `class` 输出为 `class(BASE64)`

也就是说：

- 对标准 JSON 可表达的类型，尽量保持 JSON 风格
- 对 `list / set / time / class` 这类扩展类型，默认输出显式前缀形式



## 使用示例

### 示例 1：解析混合 XSON 对象

```c
str sText = "{\"name\":\"Alice\",\"scores\":list[1:95,3:88],\"tags\":set{\"admin\",\"dev\"},\"created\":time(2000-01-02 12:00:00),\"blob\":class(AQIDBA==)}";
xvalue pRoot = xrtParseXSON(sText, 0);

if ( xvoIsNull(pRoot) == FALSE ) {
	xvalue pScores = xvoTableGetValue(pRoot, "scores", 0);
	xvalue pTags = xvoTableGetValue(pRoot, "tags", 0);
	xvalue pCreated = xvoTableGetValue(pRoot, "created", 0);

	if ( xvoIsNull(pScores) == FALSE ) {
		printf("scores[1]=%lld\n", xvoListGetInt(pScores, 1));
	}
	if ( xvoIsNull(pTags) == FALSE ) {
		printf("tags type=%d\n", xvoType(pTags));
	}
	if ( xvoIsNull(pCreated) == FALSE ) {
		printf("created=%s\n", xvoGetText(pCreated));
	}

	xvoUnref(pRoot);
}
```

### 示例 2：完整序列化 `list / set / time / class`

```c
xvalue pObj = xvoCreateTable();
xvalue pList = xvoCreateList();
xvalue pSet = xvoCreateColl();
xvalue pBlob = xvoCreateClass(4);
uint8 arrBytes[4] = { 0x01, 0x02, 0x03, 0x04 };
str sOut = NULL;

xvoListSetInt(pList, 1, 95);
xvoCollSetText(pSet, "dev", 0, FALSE);
memcpy(xvoGetClass(pBlob), arrBytes, 4);

xvoTableSetValue(pObj, "scores", 0, pList, FALSE);
xvoUnref(pList);
xvoTableSetValue(pObj, "tags", 0, pSet, FALSE);
xvoUnref(pSet);
xvoTableSetTimeSerial(pObj, "created", 0, 2000, 1, 2, 3, 4, 5);
xvoTableSetValue(pObj, "blob", 0, pBlob, FALSE);
xvoUnref(pBlob);

sOut = xrtStringifyXSON(pObj, TRUE, 0, NULL);
if ( sOut != NULL ) {
	printf("%s\n", (char*)sOut);
	xrtFree(sOut);
}

xvoUnref(pObj);
```

### 示例 3：忽略不支持类型

```c
xvalue pObj = xvoCreateTable();
str sOut = NULL;

xvoTableSetInt(pObj, "keep", 0, 1);
xvoTableSetPoint(pObj, "skip", 0, (ptr)0x1234);

sOut = xrtStringifyXSON(pObj, FALSE, XSON_F_IGNORE_UNSUPPORTED_ENCODE, NULL);
if ( sOut != NULL ) {
	printf("%s\n", (char*)sOut);
	xrtFree(sOut);
}

xvoUnref(pObj);
```



## 性能基准

当前仓库已内置 `json_xson_bench` 基准入口：

```bash
release/x64/test.exe json_xson_bench
release/x64/test.exe json_xson_bench [rounds] [items]
```

本文档记录的数据来自 `2026-03-25` 在当前工作机上的一次实测，参数为：

- `rounds=100`
- `warmup=10`
- `items=128`

测试说明：

- `JSON-compatible payload`
	- 使用同一份纯 JSON 文本
	- 对比 `xrtParseJSON()/xrtStringifyJSON()` 与 `xrtParseXSON()/xrtStringifyXSON()`
- `XSON extended payload`
	- 使用包含 `time / list / set / class` 的扩展负载
	- 只反映 XSON 自身绝对性能，不与 JSON 直接对比

负载规模：

- `json_text=21075 bytes`
- `xson_text=19500 bytes`

### 纯 JSON 负载对比

解析：

| 项目 | total | avg | throughput |
| --- | --- | --- | --- |
| JSON parse | `1376.023 ms` | `13760.234 us/op` | `1.46 MiB/s` |
| XSON parse | `1420.410 ms` | `14204.102 us/op` | `1.41 MiB/s` |

结论：

- 当前这组纯 JSON 负载里，`XSON parse` 比 `JSON parse` 慢 `3.23%`

紧凑序列化：

| 项目 | total | avg | throughput |
| --- | --- | --- | --- |
| JSON stringify | `8.230 ms` | `82.297 us/op` | `244.22 MiB/s` |
| XSON stringify | `13.010 ms` | `130.097 us/op` | `154.49 MiB/s` |

结论：

- 当前这组纯 JSON 负载里，`XSON stringify` 比 `JSON stringify` 慢 `58.08%`

格式化序列化：

| 项目 | total | avg | throughput |
| --- | --- | --- | --- |
| JSON stringify fmt | `7.880 ms` | `78.805 us/op` | `353.04 MiB/s` |
| XSON stringify fmt | `15.366 ms` | `153.658 us/op` | `196.14 MiB/s` |

结论：

- 当前这组纯 JSON 负载里，`XSON stringify fmt` 比 `JSON stringify fmt` 慢 `94.99%`

### XSON 扩展负载

| 项目 | total | avg | throughput |
| --- | --- | --- | --- |
| XSON parse ext | `3829.897 ms` | `38298.974 us/op` | `0.49 MiB/s` |
| XSON stringify ext | `17.548 ms` | `175.477 us/op` | `105.98 MiB/s` |
| XSON stringify ext fmt | `23.420 ms` | `234.195 us/op` | `117.05 MiB/s` |

说明：

- 这组结果对应包含 `time / list / set / class` 的扩展负载
- 由于标准 JSON 无法表达该负载，因此不提供 JSON 对照组
- 这些数值只代表当前机器、当前编译产物和当前默认负载，不应直接外推到所有业务场景



## 相关文档

- [JSON 处理库](api-json.md)
- [Value 动态类型系统](api-value.md)
- [Time 时间库](api-time.md)
- [API 索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#xson-处理库)

</div>
