# HTTP 内容编码协商

`<xrt/http_encoding.h>` 提供与网络、正文和服务端 Reply 解耦的
`Accept-Encoding` 解析与选择。模块不分配内存，适合协议库、原始封包路径和
高层服务器共同复用。

## 分层

`xrtHttpQualityParse` 与 `xrtHttpWeightedTokenNext` 位于
`<xrt/http.h>`，可解析任意 `token [ weight ]` 字段。需要支持 Brotli、Zstd
或应用私有编码时，可以直接使用这一层，不受内置编码集合限制。

`xrtHttpAcceptEncodingInit`、`Add` 和 `Parse` 在此基础上维护 gzip、deflate、
identity 与 wildcard 的有效质量。重复成员保留最高 qvalue，显式成员覆盖
wildcard。

`xrtHttpAcceptEncodingValid` 可验证由调用方保存或修改的公开协商状态；它是
零分配纯查询，不改变线程原有错误。

`xrtHttpAcceptEncodingSelect` 只在调用方给出的可用编码中选择。最高质量相同
时先使用 `Preferred`，再按 gzip、deflate、identity 排序。

## Content-Encoding

同一模块也提供接收方 `Content-Encoding` 计划，但不绑定 Inflate 或 Body。
`xrtHttpCodingParse` 把 identity、gzip、兼容别名 x-gzip 和 deflate 映射到
内置枚举；未知合法 token 返回 `XHTTP_CODING_NONE`，调用方仍可通过原 token
接入 Brotli、Zstd 或应用私有解码器。

`xrtHttpContentEncodingNext` 按字段出现顺序遍历全部重复字段与列表成员，忽略
RFC 列表中的空成员。`xrtHttpContentEncodingPlan` 一次汇总字段数、总层数、
内置解码器数、未知层数和原字段值合并后的精确大小。计划不分配内存，也不把
未知层当作错误，因此代理可以保留原始表示，高级客户端则可以选择自动解码、
原样交付或拒绝。

解码顺序必须与字段中的应用顺序相反。例如 `gzip, deflate` 表示先 gzip、
再 deflate，接收端必须先解 deflate、再解 gzip。`identity` 按无变换层容错，
但发送方不应在 Content-Encoding 中生成它。

`xrtHttpContentEncodingWrite` 以 `", "` 连接重复字段的原始值，适合删除
Content-Encoding 后保留诊断元数据。它不附加零字符，支持空输出查询精确大小，
容量不足时不会写出部分结果。游标、计划、大小槽和输出区都不能覆盖字段描述符或
借用文本，协议层会在写入前拒绝重叠参数。

## 缺失与空值

- 没有 `Accept-Encoding` Header：RFC 9110 规定任意内容编码都可接受。
- 存在空 Header：客户端不希望响应使用内容编码，只选择 identity。
- 未显式列出 identity：默认可接受；只有 `identity;q=0`，或没有更具体
  identity 时的 `*;q=0`，才排除 identity。
- 已知和 wildcard 质量均为零且 identity 也被排除：选择结果为
  `XHTTP_CODING_NONE`，高层服务器通常应返回 406。

协议接受能力不等于服务器策略。为兼容不完整的旧客户端，高层自动压缩默认可
选择在 Header 缺失时仍发送 identity；纯协商层仍完整保留 RFC 语义。

```c
xhttpacceptencoding Accept;
xhttpcoding Coding;

xrtHttpAcceptEncodingInit(&Accept);
xrtHttpAcceptEncodingAdd(
	&Accept,
	XRT_STR_LITERAL("gzip;q=0.8, deflate;q=0.4")
);
Coding = xrtHttpAcceptEncodingSelect(
	&Accept,
	XHTTP_CODING_IDENTITY |
		XHTTP_CODING_GZIP |
		XHTTP_CODING_DEFLATE,
	XHTTP_CODING_GZIP
);
```
