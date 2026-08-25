# 邮件日期

`mail_date` 把邮件日期语义建立在 XRT 的 `xtime` 上，不另建时间结构。它依赖
`time_text`，可以单独裁剪。

`xrtMailDateWrite` 写出 `Tue, 02 Jan 2024 03:04:05 +0800` 形式，UTC 偏移必须
精确到分钟，年份必须位于 1900 到 9999。空输出查询长度，实际容量包含末尾零字节，
短缓冲不会发布部分文本。`xrtMailDate` 提供由 `xrtFree` 释放的一行式分配入口。

`xrtMailDateParse` 接受 RFC 5322 日期，星期和秒可以省略，返回绝对 `xtime` 并可选
返回原 UTC 偏移。`XMAIL_DATE_STRICT` 只接受四位年份和数字时区；显式设置
`XMAIL_DATE_RELAXED` 后，还接受 RFC 5322 过时语法中的两位/三位年份、UT/GMT、
北美命名时区和除 J 外的军用时区。星期存在时始终验证它与日期一致。输入应先通过
`xrtMailHeaderUnfoldWrite` 展开折行，兼容模式不会静默接受注释或非法折行。

```c
char text[64];
size_t size;

xrtMailDateWrite(xrtNow(), 0, text, sizeof(text), &size);
```
