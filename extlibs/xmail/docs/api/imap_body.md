# IMAP BODYSTRUCTURE

`imap_body` 在通用 `imap_data` 之上提供独立、可裁剪的 RFC 9051
BODY/BODYSTRUCTURE 语义层。解析过程不分配内存，所有结果都借用输入文本；模块不依赖
IMAP 客户端、任务或网络。

## 视图

`xrtImapBodyParse` 接受一个完整括号值，递归校验普通、TEXT、MESSAGE/RFC822、
MESSAGE/GLOBAL 和 multipart 结构。`ximapbodyview` 投影常用字段：

- 单部分的 type、subtype、参数、标识、描述、编码、字节数和可选行数。
- 嵌套消息的 envelope 与子 BODYSTRUCTURE 原始值。
- multipart 的直接子部分区、subtype 和子部分数量。
- MD5、disposition、language、location 及未知扩展尾。

multipart 没有线路中的 type 字段，因此 `Type.Kind` 为零，由 `Kind == XIMAP_BODY_MULTIPART`
表示其类别。`xrtImapFetchNext` 返回 BODYSTRUCTURE 属性后，可直接把 `Item.Value.Source`
传给 `xrtImapBodyParse`，无需复制或重新拼接响应。

未出现的可选 `ximapdataview` 字段 `Kind` 为零。服务器定义的未知扩展不丢失，
`Extensions` 保留其线路表示，调用方仍可用 `xrtImapDataNext` 继续解释。

## 游标

`xrtImapBodyChildCursorInit` 与 `xrtImapBodyChildNext` 逐项读取 multipart 的直接子部分。
每个子结果仍完整校验自己的递归结构，但不会创建拥有型树。

`xrtImapBodyParamCursorInit` 与 `xrtImapBodyParamNext` 把 body-fld-param 转换为名称和值对；
两项均保留 IMAP string 表示，可用 `xrtImapStringWrite` 解码 quoted string。

## 边界

递归结构固定限制为 `XIMAP_BODY_DEPTH_MAX`。完整线路视图不能承载跨行 literal；遇到
literal 标记会返回协议错误。需要处理 literal 字段时，继续使用 `imap_client` 的流式 literal
接口读取和归一化数据，再把完整 BODYSTRUCTURE 交给本模块。未知扩展值按通用
body-extension 语法递归校验并保留，未来协议扩展不要求修改固定结构。
