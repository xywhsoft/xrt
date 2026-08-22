# PEM

`pem` 为 X.509、PKCS、TLS 和其他文本封装提供统一的 RFC 7468 底层。模块同时公开借用式遍历、精确标签查找、严格 Base64 解码和规范文本编码，协议层无需再私有实现边界扫描或 Base64 包装。

## 裁剪

```c
#define XRT_FEATURE_PEM
#define XRT_FEATURE_CODEC_BASE64
```

`pem` 只依赖 `codec_base64`；不依赖 ASN.1、加密、文件或网络。只处理二进制与文本的封装关系，证书和密钥语义留给上层模块。

## 借用式遍历

```c
xpemcursor cursor;
xpemblock block;

xrtPemInit(&cursor, text, text_size);
while ( xrtPemRead(&cursor, &block) == XPEM_BLOCK ) {
	/* block.Label、block.Body 和 block.Raw 都借用 text。 */
}
```

`xrtPemRead` 允许块前后存在说明文本，并识别 LF、CRLF 和 CR。开始与结束标签必须精确匹配；嵌套开始边界、缺失结束边界、非法标签和边界尾部文本都会被拒绝。返回 `XPEM_DONE` 表示正常结束，不设置错误。

游标和块只在成功时更新。`Raw` 精确覆盖本次消费的块，从开始边界到结束边界的行尾；原输入必须在所有借用视图使用完之前保持有效，输入不要求以零字节结尾。

## 查找与解码

```c
xpemblock block;
size_t size;
bytes data;

if ( xrtPemFind(text, text_size, "CERTIFICATE", &block) ) {
	data = xrtPemDecodeNew(&block, &size);
}
```

`xrtPemFind` 按出现顺序查找第一个标签完全相同的块。`xrtPemDecode` 支持查询长度和调用方缓冲；`xrtPemDecodeNew` 返回由 `xrtFree` 释放的字节，并额外保留一个不计入长度的末尾零字节。

正文使用严格、规范的 Base64 解码，只忽略 RFC 文本封装允许的 SP、HT、VT、FF、CR 和 LF。正文格式失败使用 `xrt.pem` 的 `XPEM_ERROR_BODY`，并通过 `xrtErrorCause` 保留原始 `xrt.codec` 错误。

## 规范编码

```c
size_t text_size;
str text = xrtPemEncodeNew("PUBLIC KEY", der, der_size);

xrtPemEncode(
	"PUBLIC KEY", der, der_size,
	buffer, buffer_capacity, &text_size
);
```

编码器统一输出五连字符边界、每行恰好最多 64 个 Base64 字符和 LF 换行。`xrtPemEncode` 在输出为空且容量为零时只查询长度；实际写入要求容量额外包含末尾零字节。容量和重叠失败不会修改输出缓冲。

空二进制正文生成相邻的开始行与结束行，不添加无意义空行。

## 错误

PEM 错误使用 `xrt.pem` 域：

- `XPEM_ERROR_BOUNDARY`：边界缺失、嵌套或行格式错误；
- `XPEM_ERROR_LABEL`：标签非法或开始、结束标签不一致；
- `XPEM_ERROR_BODY`：正文不是规范 Base64；
- `XPEM_ERROR_NOT_FOUND`：目标标签不存在。

结构错误在适用时把 `offset=<字节偏移>` 写入错误数据，供上层错误映射和 C 日志定位。

## 示例与测试

- `examples/asn1/pem/main.c`
- `tests/asn1/test_pem.c`
- `tests/asn1/test_pem_oom.c`
- `tests/single/test_single_pem.c`

测试覆盖多块与说明文本、三种换行、非零结尾输入、空正文、64 字符换行、错误原因链、非法边界、容量与重叠原子性、OOM 和单头生成。
