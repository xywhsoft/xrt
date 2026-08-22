# SSH Wire API

`ssh_wire` 是不分配内存、不创建网络连接的 SSH 二进制协议底层。Reader 和 Writer 只借用
调用方内存，适合 packet、KEX、认证、channel 以及自定义扩展直接复用。

## 结果与原子性

`xsshcode` 将成功、增量输入和错误分开：`XSSH_NEED_MORE` 表示保留现有输入并继续接收，
不是协议错误。所有读取在失败时保持 `xsshreader.Position` 和输出参数不变；所有写入在
失败时保持 `xsshwriter.Size` 和未提交目标内容不变。视图只在下一次修改或释放源内存前
有效。

## 基础类型

`xrtSshReadByte`、`xrtSshReadBool`、`xrtSshReadU32`、`xrtSshReadU64` 和对应写入函数实现
RFC 4251 的网络字节序基础类型。`xrtSshReadString` 返回 uint32 长度前缀后的借用视图，
`xrtSshReadBytes` 用于 packet 内已知长度片段。写入接口在读取输入前完成长度和容量检查。

`xrtSshWriterRemaining` 返回有效 writer 的剩余容量；无效状态返回零。
`xrtSshWriterReserve` 区分无效状态与空间不足，并且只检查、不推进 `Size`。协议构建器可以
先预留整条消息，再直接写入多个字段，从而保持失败原子性并避免重复容量检查。
`xrtSshWriterReserveInputs` 进一步校验整段待写区域不覆盖 writer、输入视图数组或任何输入
内容，适用于组合多字段的零拷贝协议构建器。Reader、Writer 初始化同时拒绝末地址回绕的范围。

## Name-list

`xrtSshNameListValid` 要求每一项非空，并且只包含 33 到 126 范围内、除逗号外的可打印
US-ASCII。空列表本身有效；`xrtSshNameValid` 用于校验不含逗号的单个非空名称。
`xrtSshLanguageValid` 校验协议各层共用的可空 ASCII language tag，拒绝空格、控制字符、
DEL 与非 ASCII 字节。
`xrtSshNameListContains` 和
`xrtSshNameListHasDuplicate` 不分配内存；`xrtSshNameListFirstMatch` 按第一个参数的顺序
选择共同算法，因此调用方必须把自己真正的优先级放在 `Preferred` 中。

## Mpint

`xrtSshReadMpint` 只接受规范的非负二进制补码；负值、单字节零和多余前导零均返回
`XSSH_ERROR_PROTOCOL`。`xrtSshWriteMpint` 接受大端无符号 magnitude，去除多余前导零，
并在最高位为 1 时补符号零。

`xrtSshReadSignedMpint` 保留线路上的规范二进制补码视图，并通过 `pNegative` 报告符号。
`xrtSshWriteSignedMpint` 接受 magnitude 与独立符号，无需调用方自行生成负数补码。零值
始终写成长度为零的 string。

## Identification

`xrtSshBannerRead` 可以跳过服务端前置行，接受 `SSH-2.0-` 和兼容 SSH-2 的
`SSH-1.99-`。完整行可以使用 CRLF 或兼容性的 LF；包含行尾的整行上限由
`XSSH_IDENTIFICATION_MAX` 给出。
NUL、DEL、裸 CR 和其他控制字符会作为协议错误返回。部分行返回 `XSSH_NEED_MORE`，调用方
不能消费现有前缀。

`xrtSshBannerWrite` 用于生成本端 identification。它只接受 `SSH-2.0-`，要求 software version
非空、不含空白或连字符，并拒绝控制字符、DEL、非 ASCII 字节和超过上限的内容。调用方传入不含
换行的完整 identification，函数自动追加 CRLF，因此最多接受 253 字节内容；空间不足、输入与
目标重叠或格式错误时，writer 与目标内容保持不变。

示例见 `examples/wire/main.c`。
