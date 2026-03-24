# XSON 设计草案

版本: `0.1`
状态: `draft`
更新时间: `2026-03-24`



## 1. 背景

当前 `JSON -> xvalue` 和 `xvalue -> JSON` 的适配只覆盖了标准 JSON 能直接表达的主类型：

- `null`
- `bool`
- `int`
- `float`
- `text`
- `array`
- `table`

这意味着以下 `xvalue` 类型目前无法完整往返：

- `time`
- `list`
- `coll(set)`
- `class`
- `custom`
- `point`
- `func`

其中前 4 类是明确需要进入持久化体系的；`custom / point / func` 则需要更谨慎地定义边界。

因此需要在现有 JSON 设施之外，引入一个私有扩展格式 `XSON`，用于完成 `xvalue` 的完整序列化与反序列化。



## 2. 设计目标

- 保持 `JSON` 作为通用、标准、对外交换格式
- 新增 `XSON` 作为私有、面向 `xvalue` 的完整序列化格式
- 不污染现有 JSON API 与 JSON 语义
- 尽量复用现有 JSON 的字符串、数字、容器、格式化输出等基础设施
- 支持 `time / list / coll / class` 的稳定往返
- 为 `custom` 预留编解码器扩展点
- 明确区分“同构快照能力”和“跨平台可移植能力”



## 3. 非目标

- 不让 `xrtParseJSON()` 直接接受 XSON
- 不扩展现有 `json_type_t` 去承载 `time / list / coll / class / custom`
- 不承诺 `class` 的原始二进制在不同 ABI、不同编译器、不同打包方式之间天然可移植
- 不默认支持 `point / func` 的通用反序列化



## 4. 共存策略

`JSON` 和 `XSON` 采用双入口并存：

- `JSON`
	- 面向开放协议、配置文件、通用交换
	- 语义保持标准 JSON
	- 继续使用现有 `xrtParseJSON()` / `xrtStringifyJSON()`
- `XSON`
	- 面向内部快照、缓存、调试落盘、私有数据交换
	- 允许扩展语法
	- 新增独立 API

推荐 API：

```c
xvalue xrtParseXSON( str sText, size_t iSize );
xvalue xrtParseXSON_File( str sFile );

char* xrtStringifyXSON( xvalue varVal, int bFormat, uint32 iFlags, size_t* pRetSize );
bool xrtStringifyXSON_File( str sFile, xvalue varVal, int bFormat, uint32 iFlags );
```

建议同时补一组带错误结构的扩展接口：

```c
typedef struct xson_error_t
{
	int iCode;
	size_t iOffset;
	size_t iLine;
	size_t iColumn;
	char sMsg[128];
} xson_error_t;

xvalue xrtParseXSONEx( str sText, size_t iSize, uint32 iFlags, xson_error_t* pErr );
```

原因：

- 当前 JSON 入口存在“解析失败”和“值本身就是 `null`”难以区分的问题
- `XSON` 作为私有格式，应该从一开始就把错误定位补完整



## 5. 类型映射

### 5.1 与 JSON 完全一致的类型

以下类型在 `XSON` 中沿用 JSON 原生写法：

- `null`
- `bool`
- `int`
- `float`
- `text`
- `array`
- `table`

示例：

```xson
null
true
123
3.14
"hello"
[1, 2, 3]
{"name":"xrt","ok":true}
```


### 5.2 `time`

语法：

```xson
#2026-03-24 16:30:00#
```

规则：

- 使用 `#` 作为包裹符
- 内部时间文本固定为 `YYYY-MM-DD HH:MM:SS`
- 解析时转换为 `XVO_DT_TIME`
- 输出时默认也使用这一格式

说明：

- `#...#` 不属于 JSON 语法，因此只能在 `XSON` 入口使用
- 如后续需要毫秒、UTC 偏移、纯日期等能力，应新增 flag 或扩展写法，但 V1 不建议一次做太多


### 5.3 `coll(set)`

语法：

```xson
{1, 2, "abc", #2026-03-24 16:30:00#}
```

规则：

- 非空花括号中，如果首个顶层分隔关系不是 `key:value`，则按 `set` 解析
- 输出时按集合元素顺序打印

说明：

- `coll` 底层是集合语义，不保留 JSON object 的键值结构
- 集合顺序应遵循当前 `coll` 容器的遍历顺序，不额外伪造插入顺序


### 5.4 `list`

语法：

```xson
[1:"a", 2:100, -5:#2026-03-24 16:30:00#]
```

规则：

- 非空方括号中，如果首个顶层元素形如 `int64:value`，则按 `list` 解析
- `list` 的索引允许为负数，也允许稀疏
- 输出时按当前 `list` 的遍历顺序打印

说明：

- `list` 在 XRT 中不是 JSON array 的别名，而是“整型索引到值”的映射结构
- 因此必须与普通 `array` 做语义区分


### 5.5 `class`

默认采用 tagged form：

```xson
{
	"__xson_type__":"class",
	"size":24,
	"data":"BASE64..."
}
```

建议可选保留元信息：

```xson
{
	"__xson_type__":"class",
	"size":24,
	"data":"BASE64...",
	"abi":"msvc-x64",
	"name":"my_struct_v1"
}
```

规则：

- `data` 为结构体原始内存的 `BASE64`
- `size` 为原始字节数
- V1 默认只保证“同构快照”可逆，不保证跨 ABI 可移植

说明：

- 如果结构体中包含指针、函数指针、平台相关对齐、不同编译器打包差异，则原始字节只适合同平台、同 ABI、同版本回灌
- 若需要跨平台可移植，应由业务自行提供 codec，把结构体展开为稳定字段


### 5.6 `custom`

默认不做裸内存序列化，采用 codec 扩展：

```xson
{
	"__xson_type__":"custom",
	"codec":"MyCodecV1",
	"payload":{...}
}
```

规则：

- `codec` 必须能在运行时注册并查找到对应编解码器
- `payload` 的具体结构由 codec 自己定义

说明：

- `custom` 不适合做“通用自动持久化”
- 如果没有 codec，反序列化应明确报错，而不是静默退化为 `null`


### 5.7 `point / func`

V1 默认策略：

- 不支持通用序列化
- `xrtStringifyXSON()` 遇到此类值时返回错误

未来若确有需求，可引入符号注册方式：

```xson
{
	"__xson_type__":"func",
	"symbol":"procTest"
}
```

但这不应进入 V1 主线。



## 6. 语法规则

### 6.1 根值

`XSON` 根值允许为任意单值，与当前 JSON 宽松根值策略保持一致：

- 标量
- `array`
- `table`
- `time`
- `set`
- `list`


### 6.2 容器歧义处理

XSON 必须处理以下歧义：

- `{...}` 可能是 `object`
- `{...}` 也可能是 `set`
- `[...]` 可能是 `array`
- `[...]` 也可能是 `list`

推荐判定规则：

- `{}` 固定表示空 `object`
- `[]` 固定表示空 `array`
- 非空 `{...}`：
	- 如果首个顶层分隔关系是 `name:value`，按 `object`
	- 否则按 `set`
- 非空 `[...]`：
	- 如果首个顶层分隔关系是 `int64:value`，按 `list`
	- 否则按 `array`

这意味着：

- 空 `set` 不能写成 `{}`
- 空 `list` 不能写成 `[]`

V1 对空 `set / list` 统一采用 tagged form：

```xson
{"__xson_type__":"coll","items":[]}
{"__xson_type__":"list","items":[]}
```


### 6.3 保留字段

为避免和普通业务表冲突，XSON 预留以下保留字段：

- `__xson_type__`
- `codec`
- `payload`
- `size`
- `data`
- `items`

约束：

- 仅当 object 命中 `__xson_type__` 时，才进入 tagged form 分支
- 普通 object 即使包含 `size`、`data` 等字段，也不应被误判为特殊类型



## 7. 格式化输出策略

### 7.1 一般规则

- 复用现有 JSON 格式化风格
- 支持压缩模式和格式化模式
- 对 `time / set / list` 也使用与 JSON 一致的缩进、换行和逗号策略


### 7.2 稳定性

建议增加以下可选输出 flag：

```c
#define XSON_F_NONE				0x0000
#define XSON_F_CANONICAL			0x0001
#define XSON_F_PORTABLE_CLASS		0x0002
#define XSON_F_ALLOW_TAGGED_EMPTY	0x0004
```

建议语义：

- `XSON_F_CANONICAL`
	- 尽量输出稳定、可比对的文本
	- `table` / tagged form 字段顺序固定
- `XSON_F_PORTABLE_CLASS`
	- 遇到 `class` 时不走裸字节方案，必须交由 codec 或直接报错
- `XSON_F_ALLOW_TAGGED_EMPTY`
	- 允许空 `set / list` 强制使用 tagged form



## 8. 实现分层建议

推荐新增独立模块，而不是继续堆在 `lib/json.h` 里：

- `lib/xson.h`
	- XSON parser / printer 主实现
- `docs/api/api-xson.md`
	- 面向用户的 API 文档
- `test/test_xson.h`
	- 单元测试

头文件导出建议：

- 在 `xrt.h` 中公开 XSON API
- 不修改现有 JSON 公共结构体
- 不新增 XSON 到 `json_type_t`

内部复用建议：

- 复用现有字符串转义与打印逻辑
- 复用现有数字解析与输出逻辑
- 复用 `xrtStrToTime()` / `xrtDecodeSerial()` 等时间工具
- 复用 `xrtBase64Encode()` / `xrtBase64Decode()` 处理 `class`
- 复用 `xarray / xlist / xdict / xavltree` 遍历能力组装 `xvalue`



## 9. 解析器方案

### 9.1 不污染 JSON

不建议在现有 `xrtJsonParseSAX()` 上继续打补丁去兼容 XSON。

原因：

- JSON 是标准格式，不应被私有语法污染
- `time / set / list` 语义判断明显超出 JSON 范畴
- 若继续混用，后续错误处理、文档、兼容性都会变得混乱

正确做法：

- 独立入口 `xrtParseXSON()`
- 内部可借鉴 JSON 的 tokenizer、字符串处理、数字处理思路
- 但语法状态机单独维护


### 9.2 解析流程

建议流程：

1. 跳过空白
2. 看首字符决定进入哪类分支
3. `"` 进入字符串
4. 数字、`+`、`-`、`.` 进入数值
5. `#` 进入时间
6. `{` 进入 `object/set` 二义性解析
7. `[` 进入 `array/list` 二义性解析
8. 命中 `true / false / null / nan / inf` 等关键字时，沿用当前宽松 JSON 数值策略


### 9.3 object 与 set 判定

`{` 后建议先做一次轻量探测：

- 扫描首个顶层元素
- 若遇到合法 `key:value` 结构，则按 object
- 若遇到逗号或 `}` 前都没有出现 object 的 `:` 关系，则按 set

注意：

- 这里的 `:` 必须是“首个元素顶层层级”的 `:`
- 字符串、嵌套对象、嵌套数组内部的 `:` 不参与判定


### 9.4 array 与 list 判定

`[` 后也做轻量探测：

- 扫描首个顶层元素
- 若左侧为合法 `int64`，且顶层出现 `:`，则按 list
- 否则按 array

注意：

- `list` 左侧索引必须是整数文本
- 不接受浮点索引
- `1 : "a"` 这种含空白的形式允许，但最终仍按 `int64:value` 解释



## 10. 序列化器方案

打印逻辑建议分 3 层：

### 10.1 基础打印层

复用或抽出通用能力：

- 缓冲扩容
- 缩进
- 逗号控制
- 字符串转义
- 数字输出


### 10.2 容器打印层

为以下类型单独提供打印函数：

- `array`
- `table`
- `list`
- `coll`
- tagged object


### 10.3 特殊值打印层

为以下类型提供专门逻辑：

- `time`
- `class`
- `custom`
- 错误分支

建议默认行为：

- `time` 直接输出 `#YYYY-MM-DD HH:MM:SS#`
- `class` 输出 tagged form
- `custom` 若无 codec，则报错
- `point / func` 报错



## 11. `class` 的可移植性边界

这是 XSON 设计中最需要提前说清楚的点。

`class -> BASE64(raw bytes)` 只能保证以下场景可逆：

- 同平台
- 同 CPU 架构
- 同 ABI
- 同编译器或兼容 ABI
- 同结构体定义

以下情况默认不保证：

- Windows 写入，Linux 读取
- 32 位写入，64 位读取
- 不同编译器打包方式不同
- 结构体内部包含指针，需要深层对象重建

因此建议把 `class` 支持分成两档：

- `Snapshot Mode`
	- 直接保存 raw bytes
	- 适合缓存、调试快照、同构进程落盘
- `Portable Mode`
	- 必须由 codec 将结构体映射为稳定字段
	- 适合跨平台、跨版本持久化

`XSON_F_PORTABLE_CLASS` 就是为此准备的。



## 12. `custom` 的扩展模型

推荐未来提供一套注册接口：

```c
typedef bool (*xson_custom_encode_proc)( xvalue varVal, xvalue* pOutPayload );
typedef xvalue (*xson_custom_decode_proc)( xvalue varPayload );

bool xrtXSONRegisterCustomCodec(
	str sCodec,
	xson_custom_encode_proc procEncode,
	xson_custom_decode_proc procDecode
);
```

约束：

- `payload` 本身仍应是可由 XSON 表达的值
- codec 名称必须稳定
- 找不到 codec 时应返回明确错误

V1 可以只预留设计，不立即实现完整注册体系。



## 13. 推荐落地步骤

### Phase 1

- 新增 `dev` 设计文档
- 整理 API 草案
- 清理 JSON/XSON 职责边界


### Phase 2

- 新增 `lib/xson.h`
- 实现 `time / list / coll / class` 的 parser + printer
- 增加 `xrtParseXSONEx()` 错误结构
- 增加 `test/test_xson.h`


### Phase 3

- 增加 `custom` codec 扩展
- 增加 `XSON_F_CANONICAL`
- 补齐 `docs/api/api-xson.md`
- 为单头生成器补充 XSON 导出


### Phase 4

- 评估是否要支持 `point / func` 的符号化序列化
- 评估是否要支持更丰富的时间文本格式
- 评估是否需要 XSON SAX 或流式接口



## 14. 最终建议

当前最稳妥的方案是：

- `JSON` 保持标准和纯净
- `XSON` 单独建立入口
- `time / set / list` 使用轻量语法糖
- `class / custom` 使用 tagged form
- `class` 明确区分“同构快照”与“可移植持久化”
- `point / func` 暂不纳入 V1

这样做的优点是：

- 对现有 JSON 主线侵入最小
- 对 `xvalue` 的覆盖最完整
- 语法可读性较好
- 后续可渐进扩展，不会一开始就把体系做死
