# HTTP Content-Encoding Body 解码

`<xrt/http_body_decode.h>` 把公共 `Content-Encoding` 计划组合为一个可流式读取的
`xhttpbody`。它只依赖协议解析和单层 Inflate Body，不依赖 HTTP 客户端、服务器或
网络，因此调用方可以在任意协议流程中决定是否解码、保留原始线路正文或接入自定义
编码。

## 裁剪与分层

- **XRT_FEATURE_HTTP_BODY_DECODE**：依赖 **XRT_FEATURE_HTTP_ENCODING** 和
  **XRT_FEATURE_HTTP_BODY_INFLATE**。
- 单层 `xrtHttpBodyInflate` 不再包含或依赖本头文件；不需要协议组合时可完整裁掉。
- 本模块只识别内置 `gzip`、`x-gzip`、HTTP `deflate` 和 `identity`，未知合法编码
  通过 `UNSUPPORTED` 交给调用方扩展，不会被静默丢弃。

## 配置

```c
typedef struct xhttpbodydecodeconfig {
	xhttpbodyinflateconfig Inflate;
	size_t MaxCodings;
} xhttpbodydecodeconfig;
```

`xrtHttpBodyDecodeConfigInit` 初始化四层内容编码上限，并继承单层 Inflate 的 32 KiB
推进粒度、64 MiB 每层累计输出上限、64 MiB 每层内部队列上限和 gzip Header 上限。
`Inflate.Format` 会被每个 `Content-Encoding` 成员覆盖，其余配置复制到每一层。

默认最多四层，硬上限为 `XHTTP_CONTENT_CODINGS_MAX`（十六层）。层数限制统计所有
合法成员，包括 `identity`，避免无变换成员绕过协议复杂度上限。多层组合的最坏内部
队列预算不超过 `MaxCodings * Inflate.QueueLimit`；处理不可信输入时应同时按应用
内存预算降低层数、每层输出和队列上限。`QueueLimit` 为零表示显式取消该层限制。

## 解码入口

```c
xhttpbodydecoderesult xrtHttpBodyDecodeFields(
	xhttpbody* source,
	const xhttpfield* fields,
	size_t count,
	const xhttpbodydecodeconfig* config,
	xhttpbody** output
);

xhttpbodydecoderesult xrtHttpBodyDecode(
	xhttpbody* source,
	xstrview contentEncoding,
	const xhttpbodydecodeconfig* config,
	xhttpbody** output
);
```

`xrtHttpBodyDecodeFields` 合并全部同名字段并按线路应用顺序的反向组合解码器。例如
`Content-Encoding: gzip, deflate` 表示先 gzip、再 deflate，接收端会先移除 deflate，
再移除 gzip。`xrtHttpBodyDecode` 是单字段值的便利入口。

结果契约：

- `UNCHANGED`：没有实际解码层，返回来源的独立引用。
- `APPLIED`：返回持有完整解码链的 Body。
- `UNSUPPORTED`：存在未知合法编码，返回完整原始 Body 引用，绝不做部分解码。
- `ERROR`：参数、语法、层数、配置或分配失败，不发布输出。

所有非错误结果都由调用方销毁。组合阶段只创建轻量包装，不打开或消费来源；一次性
来源仍由原 Body 和结果 Body 共享，因此只能选择其中一个开始读取。Reader 的
`AGAIN/Wait`、稳定错误、Chunk 租约和关闭语义由通用 Body 变换层保持。

配置在调用时完整快照，允许来自非对齐存储；回绕地址会在读取前拒绝。输出槽不得覆盖
Source、配置、字段描述符或字段借用文本。失败不会修改来源，也不会留下部分包装链。

## 示例与验证

完整示例位于 `examples/http/body_decode/main.c`。模块测试覆盖重复字段逆序解码、
`x-gzip`、`identity`、未知编码原始回退、语法错误、层数和队列上限、非对齐配置、
地址回绕、别名、OOM 回滚、Deflate roundtrip、单头文件和裁剪依赖。
