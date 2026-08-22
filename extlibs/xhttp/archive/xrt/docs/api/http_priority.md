# HTTP Priority

`http_priority` 实现 RFC 9218 与 HTTP 版本无关的 `Priority` 字段。它只处理基于 Structured Fields Dictionary 的优先级信号，不绑定客户端、服务器、连接或调度器；HTTP/2 与 HTTP/3 的 `PRIORITY_UPDATE` 帧留给对应协议层。

## 裁剪与依赖

解析层：

```c
#define XRT_FEATURE_HTTP_PRIORITY
```

该层依赖 `http_structured`，保持零堆分配。规范写出是独立裁剪层：

```c
#define XRT_FEATURE_HTTP_PRIORITY_WRITE
```

写出层依赖 `http_priority` 和 `http_structured_write`，不会复制 Structured Dictionary 的序列化实现。

## 数据模型

```c
typedef struct xhttppriority {
	uint8 Urgency;
	uint8 Incremental;
	uint8 Flags;
} xhttppriority;
```

`Urgency` 的有效范围是 `0..7`，数值越小优先级越高，默认值是 `3`。`Incremental` 表示响应能否被增量消费，默认值为假。

`XHTTP_PRIORITY_HAS_URGENCY` 和 `XHTTP_PRIORITY_HAS_INCREMENTAL` 区分“当前有效值”与“线路中是否显式提供”。这一差异是响应语义必需的：请求省略参数表示采用默认值，响应省略参数表示服务端不要求改变客户端提供的值。

## 解析

```c
xhttppriority Priority;

if ( !xrtHttpPriorityParse(Fields, FieldCount, &Priority) ) {
	/* 整个 Structured Dictionary 畸形。 */
}
```

`xrtHttpPriorityValueParse` 解析一个已经提取的字段值；`xrtHttpPriorityParse` 跨越全部大小写不敏感的重复 `Priority` 字段行。缺失字段和空 Dictionary 都成功产生 `u=3`、`i=false`，但不设置显式标志。

整个 Structured Dictionary 语法错误时解析失败且输出不变。Dictionary 成功解析后，未知参数会被忽略；`u` 类型不是 Integer、超出 `0..7`，或 `i` 类型不是 Boolean 时，相应参数也会被忽略。重复 key 遵循 Structured Fields 的最后值规则，最后一次出现无效时等价于该已知参数未出现。

## 覆盖

```c
xrtHttpPriorityOverlay(&Request, &Response, &Effective);
```

`xrtHttpPriorityOverlay` 只用 `Response.Flags` 中显式出现的参数覆盖请求参数，因此直接表达 RFC 9218 的响应省略语义。它只是一个确定性的字段覆盖原语，不规定中间件或服务器必须采用何种优先级合并、队列和公平调度策略。

输入与输出描述符可以相同。未设置标志的输入字段会被规范化为协议默认值，不携带隐式自定义含义。

## 规范写出

```c
char Value[32];
size_t Size;
xhttppriority Priority = {
	1, 1,
	XHTTP_PRIORITY_HAS_URGENCY |
	XHTTP_PRIORITY_HAS_INCREMENTAL
};

xrtHttpPriorityWrite(&Priority, Value, sizeof(Value), &Size);
/* Value 是 "u=1, i"，不附加零字节。 */
```

写出器只发布标志位指定的参数，顺序固定为 `u`、`i`。显式 true 写成 `i`，显式 false 写成 `i=?0`；显式默认 urgency 仍写成 `u=3`。没有显式参数时产生长度为零的省略值。输出为 `NULL` 且容量为零可查询精确长度；容量不足不写部分结果。

## 内存与错误契约

- 解析与写出均不分配堆内存；
- 解析输出允许未对齐存储，实现通过 `memcpy` 发布；
- 解析输出不得覆盖字段数组或字段值借用区；
- 协议格式错误设置值错误，参数、范围或别名错误设置参数错误；
- 解析和容量失败保持结果或输出字节不变。

## 示例与测试

- `examples/http/priority/main.c`
- `examples/http/priority_write/main.c`
- `tests/http/test_http_priority.c`
- `tests/http/test_http_priority_noalloc.c`
- `tests/http/test_http_priority_write.c`
- `tests/http/test_http_priority_write_noalloc.c`
- `tests/single/test_single_http_priority.c`
- `tests/single/test_single_http_priority_write.c`

实现遵循 [RFC 9218](https://www.rfc-editor.org/rfc/rfc9218.html) 第 4、5、8 节，并复用 [RFC 9651](https://www.rfc-editor.org/rfc/rfc9651.html) Structured Fields 底座。
