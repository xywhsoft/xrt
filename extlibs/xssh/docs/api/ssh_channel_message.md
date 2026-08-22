# SSH Channel Message API

`ssh_channel_message` 实现 RFC 4254 的 channel open、流控、数据、EOF、close、请求与请求响应
报文。它只处理 payload，不创建网络连接、不分配 channel 表，也不替调用方维护窗口和生命周期。

## 扩展边界

`xrtSshChannelOpenRead` 和 `xrtSshChannelRequestRead` 解析公共前缀后，把未知 channel 类型或
request 类型的专用字段作为 `Fields` 原样借出。对应写入函数接受同样的原始字段，因此第三方
扩展无需修改 xssh 私有解析器。Open confirmation 同样保留 channel 类型专用确认字段。

## 流控与数据

`xrtSshChannelWindowAdjustWrite/Read` 保留完整 `uint32` 增量；本层不判断累加后的远端窗口是否
溢出，这属于后续 channel window 状态层。Open 与 confirmation 的 `MaxPacket` 必须非零，窗口
本身允许为零。`xrtSshChannelDataWrite/Read` 和 extended-data 接口接受空或任意二进制 string，
`XSSH_CHANNEL_EXTENDED_DATA_STDERR` 表示标准错误流类型码。

## 生命周期

EOF、close、success 和 failure 都采用严格的固定长度消息，拒绝尾随字段。它们只编码线路事件；
“发送 EOF 后仍可接收”“close 必须双向确认”和请求回复顺序由独立状态模块约束。

所有读取返回借用视图，失败不修改输出对象。所有写入先预留完整输出并拒绝输入输出重叠，失败不
推进 writer。Open failure 描述必须是 UTF-8，language tag 复用 wire 层统一 ASCII 校验。

示例见 `examples/channel_message/main.c`。
