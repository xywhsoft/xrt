# IMAP 数据层

`imap_data` 在线路解析和客户端之间提供独立可裁剪的数据层。正常解析不分配内存，视图借用输入，
不会创建邮箱、搜索结果或邮件对象树；调用方可以逐项消费 LIST、STATUS、SEARCH、ESEARCH、
FETCH 和 FLAGS 数据。

FETCH literal 只返回长度和标记。正文仍由 `imap_client` 流式读取，完成后通过
`xrtImapFetchCursorContinue` 接入下一行片段，因此消息大小不会转化为客户端固定缓冲或临时拼接。
输入文本在视图/游标存活期间必须保持有效且不改写。初始化检查完整片段；后续迭代检查当前
消费的数据及范围，避免每读一项都重扫完整文本。平坦 SEARCH/FETCH 迭代随输入长度线性增长。

## 通用值

`xrtImapDataNext` 区分 atom、number、quoted string、NIL、完整括号 list 和行尾 literal。
`Source` 保留线路表示，`Value` 去除引号或括号但不隐式分配；`xrtImapStringWrite` 负责按需解码
quoted string，也支持先查询精确输出长度。

## 专用视图

- `xrtImapListParse` 保留未知 LIST 扩展，并允许邮箱名使用 literal。
- `xrtImapStatusNext` 返回开放的名称和值对，未知 STATUS 项不会丢失。
- `xrtImapSearchNext` 增量返回 ID 和 MODSEQ；`ESEARCH ALL` 保持 sequence-set，不展开数组。
- `xrtImapFetchNext` 支持 section 名内的字段列表、嵌套值和多个 literal 续段。
- `xrtImapFlagNext` 同时适用于 LIST 属性、系统 flag 和用户关键字。

邮箱名按 astring 解析：未加引号的数字（包括超过 uint64 的名称）和 `NIL` 都是邮箱名，
以 `XIMAP_DATA_ATOM` 返回，不转换成数值或空值。LIST 的 quoted 分隔符必须恰为一个字符，
允许一个合法 UTF-8 字符或反转义后的引号/反斜线；`NIL` 表示没有层级分隔符。

SEARCH ID 为非零 uint32，MODSEQ 必须是最后一项。STATUS/ESEARCH 的已知数值项校验各自
类型和范围；SIZE 的合法值为 0 到 INT64_MAX，CONDSTORE MODSEQ 仍支持完整 uint64。
未知扩展保留通用数据值。ESEARCH correlator 校验
`TAG`、标签及终止位置，字段和数据对之间必须有空格。FETCH 支持 literal 后直接结束括号，
续段中的下一属性仍需要空格。语法依据 [RFC 9051 §9](https://www.rfc-editor.org/rfc/rfc9051.html#section-9)。

## 失败与借用关系

专用 FLAGS/STATUS/SEARCH/ESEARCH/FETCH 游标在错误时保留游标与输出；结束时可正常提交
终止状态。借用输出和游标不得覆盖输入文本；FETCH 初始化也检查整个游标对象。底层扫描
已经返回错误时直接传播诊断，包括 RANGE 和错误对象分配失败时的 MEMORY。每个错误路径
都设置诊断，成功调用保留调用方已有错误。错误构造可能分配，不属于正常解析的零分配保证。

`xrtImapStringWrite` 的输出缓冲、长度指针必须与输入视图对象、Source/Value 借用文本以及
彼此分离，即便调用方提供的 Source 与 Value 位于不同存储区。查询模式要求输出缓冲为
NULL、容量为零。容量不足时保留缓冲字节并报告所需长度；其他失败不写入输出或长度。
