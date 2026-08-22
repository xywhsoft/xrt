# 邮件标识

`mail_id` 依赖 XRT `random_secure`，负责 Message-ID 与 multipart boundary，不依赖
完整 MIME 构建器。

`xmailmessageidview` 的 `Source`、`Left`、`Right` 都借用输入。`xmailidflag` 提供
`XMAIL_ID_DEFAULT` 与显式 `XMAIL_ID_UTF8`。`xrtMailMessageIdParse` 解析现代
`<id-left@id-right>`，拒绝空 atom、重复点、折行和过时 quoted id-left。

`xrtMailMessageIdWrite` 使用 128 位安全随机值和调用方提供的 id-right 直接写入；
`xrtMailMessageId` 返回由 `xrtFree` 释放的文本。生成器不使用时间或进程级全局计数，
因此没有共享锁和可预测序列。

`XMAIL_BOUNDARY_MAX` 是 70。`xrtMailBoundaryValid` 验证 MIME boundary 字符集和
末尾空格规则；`xrtMailBoundaryWrite` 使用安全随机源写入固定长度 boundary，
`xrtMailBoundary` 提供分配入口。

```c
str id = xrtMailMessageId(XRT_STR_LITERAL("example.com"), NULL);
str boundary = xrtMailBoundary(NULL);
```
