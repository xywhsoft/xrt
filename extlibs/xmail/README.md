# xmail

`xmail` 是只依赖 XRT 公共 API 的邮件底层扩展库：MIME 内容层与跨协议传输基座。SMTP、POP3
与 IMAP 协议客户端分别由 `xsmtp`、`xpop3`、`ximap` 扩展库提供，它们以本库为依赖
（`dependency_manifests`）组合出完整邮件能力。正式实现不复刻 socket、TLS、压缩、取消或
截止时间能力。

## 分层

- `mail_content`：CRLF、Quoted-Printable、MIME Base64、Header、编码词、地址、日期、
  Message-ID、RFC 2231 参数、multipart 游标、轻量消息视图、可选拥有型 MIME 树、流式
  Builder 和高层 Compose。
- `mail_transport`：增量线路读写、dot transparency、共享 TCP、TLS、SASL（PLAIN/LOGIN/
  XOAUTH2/OAUTHBEARER 编码）与 raw-DEFLATE 传输适配。
- `src/mail`：纯邮件内容实现。
- `src/transport`：跨协议共享的线路、网络、TLS、压缩和认证实现。

内容解析层使用借用视图与调用方缓冲，常用写出接口支持精确容量，不强制建立消息对象树。
传输层借用调用方 Engine、Resolver、TLS Context 和 Verifier，不隐藏共享对象生命周期；
所有阻塞等待统一接受 deadline 和 cancel。

邮件构建分为两层：`mail_build` 直接向 sink 写字段、原始正文和 multipart part，不持有
正文；`mail_compose` 才提供文本、HTML、内联资源和附件的常见结构。只需要高性能原始
报文路径时，不会被迫携带高层消息描述和自动生成逻辑。

## 协议扩展

- `xsmtp`：SMTP 协议、同步客户端、STARTTLS/隐式 TLS、认证与 Compose 流式提交。
- `xpop3`：POP3 协议、同步客户端、STLS、认证与 RETR/TOP 到 MIME 树桥接。
- `ximap`：IMAP 协议、命令层、数据视图、流式 APPEND 与 RFC 4978 COMPRESS=DEFLATE。

## 裁剪

只选择内容与传输原语：

```c
#define XMAIL_MODULE_XMAIL
#include <xmail.h>
```

也可以选择单个层，例如 `XMAIL_MODULE_MAIL_BUILD`、`XMAIL_MODULE_MAIL_COMPOSE`、
`XMAIL_MODULE_MAIL_MESSAGE`、`XMAIL_MODULE_MAIL_TREE`、`XMAIL_MODULE_MAIL_WIRE`、
`XMAIL_MODULE_MAIL_NET` 或 `XMAIL_MODULE_MAIL_NET_TLS`。每个模块宏只展开清单声明的
依赖闭包；内容、线路、网络与 TLS 互相独立。

## 验证

```text
python tools/amalgamate.py --manifest extlibs/xmail/config/modules.json
python tools/build.py --compiler gcc --manifest extlibs/xmail/config/modules.json --suite xmail_tests --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xmail/config/modules.json --suite xmail --trim-only --cflag=-Werror
python tools/check_api_docs.py --manifest extlibs/xmail/config/modules.json
python tools/check_release_maturity.py --release --manifest extlibs/xmail/config/modules.json
python tools/measure_performance.py --config extlibs/xmail/config/performance_profiles.json --manifest extlibs/xmail/config/modules.json --profiles '*'
python tools/measure_size.py --config extlibs/xmail/config/size_profiles.json --manifest extlibs/xmail/config/modules.json --profiles '*'
```

根目录中的旧协议设计稿和 `xmail_xlang` 文件是历史迁移资产，不进入当前模块清单、公共头、
单头或发布包。正式 API、测试和文档分别以 `include`、`tests` 与 `docs/api` 为准。
