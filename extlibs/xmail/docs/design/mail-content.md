# 邮件内容层设计

## 分层

邮件体系先建立无网络依赖的内容原语，再建立 MIME 视图/树，最后由 SMTP、POP3 和
IMAP 复用网络、TLS、Future 与任务体系。协议客户端可以直接发送或接收原始 RFC
报文，不强制构造高级对象；完整邮件对象只是常见路径的便捷层。

当前分层如下：

- `mail_core`：错误、线路常量、CRLF 规范化。
- `mail_codec`：Quoted-Printable 与 MIME Base64，复用 XRT 唯一 Base64 实现。
- `mail_header`：字段安全谓词、零分配游标、展开和折叠。
- `mail_word`：RFC 2047 无分配视图、UTF-8 B/Q 编码与严格/容错解码。
- `mail_address`：addr-spec、mailbox、地址列表和 group，显示名复用 `mail_word`。
- `mail_date`：RFC 5322 日期与 XRT `xtime` 的无损映射。
- `mail_id`：安全随机 Message-ID、boundary 生成和无分配校验。
- `mail_param`：独立 MIME 类型、处置类型、RFC 2231 参数游标和连续段合并。
- `mail_multipart`：严格行边界、借用 part 视图和可直接发送的构建片段。
- `mail_message`：受预算约束的借用消息视图、字段查找和传输编码解码。
- `mail_tree`：拥有输入、统一 arena、带全树预算的 multipart 与嵌套消息解析。
- `mail_build`：sink 驱动的字段、正文与嵌套 part 流式写入器。
- `mail_compose`：可独立裁剪的 UTF-8 文本、HTML、内联资源与附件便利层。
- `mail_wire`：增量 CRLF 行探测和 SMTP/POP3 dot transparency。
- 后续 `smtp`、`pop3`、`imap`：共享网络/TLS/等待契约上的独立裁剪客户端。

## 性能路径

基础入口全部接受明确长度，不调用 `strlen`，并提供长度查询和调用方缓冲。查询和验证
先于写入，因此格式错误或容量不足不会产生部分结果。Base64 按单行块直接编码到最终
输出；字段和编码词读取只移动游标并返回借用视图。编码词的固定 75 字节上限使单词
解码只需要 64 字节栈缓冲，不建立中间堆对象。

MIME 参数不依赖通用字符串容器，multipart 不复制字段或正文。分隔片段可以直接接入
TCP/TLS 发送队列，调用方不必先拼接整封邮件；高层对象树不得替换这条路径。

`mail_tree` 只在明确选择拥有型解析时复制原报文，并用统一 arena 保存节点、展开字段和
解码正文。part 数量、递归深度、字段与字节预算在分配前检查；低层借用视图不携带这部分
容量成本。

`mail_build` 的正文和预构建字段块直接借用调用方内存并同步提交；它只在普通字段需要
折叠时使用栈缓冲或按需临时分配。`mail_compose` 复用该 Builder，附件 Base64 使用固定
小块输出，不创建随附件大小增长的编码副本。完整报文 owned 结果只是同一 sink 契约的
内存实现，不存在第二套拼接器。

便捷分配函数只包装相应写入入口，所有内存继续由 XRT 分配器拥有。后续对象树不得
替代流式路径，也不得把 POP3/IMAP 下载的整个邮箱强制载入内存。

## 边界

邮件 MIME 参数与 HTTP 参数语义相似但标准和兼容规则不同。`xmail` 不依赖完整
`xhttp`，也不复制其私有实现；真正协议无关且值得共享的原语应进入 XRT 公共底座。
历史文件中对旧 `xrtHttpMediaType*`、`xrtMultipart*` 和 socket/TLS 私有流程的调用
必须在迁移时移除。

RFC 2047 的高层解码只内置 UTF-8 与 US-ASCII。未知字符集不会被猜测；需要旧字符集
时，调用方可通过 `xrtMailWordParse` 取得字符集和编码正文，再接入独立转码扩展。这让
默认路径保持小巧，同时不存在必须重写协议解析器的功能死角。

历史单头暂时只作为测试向量和已验证行为来源。新清单是唯一生产边界，不提供旧函数
别名、多版本结构或 `singlehead/xrt.h` 兼容入口。
