# xmail

`xmail` 是只依赖 XRT 公共 API 的邮件内容与传输协议扩展。正式实现不复刻 socket、TLS、
压缩、取消或截止时间能力，也不依赖 XRT 内部头文件。

## 分层

- `mail_content`：CRLF、Quoted-Printable、MIME Base64、Header、编码词、地址、日期、
  Message-ID、RFC 2231 参数、multipart 游标、轻量消息视图、可选拥有型 MIME 树、流式
  Builder 和高层 Compose。
- `mail_transport`：增量线路、dot transparency、SMTP、POP3、IMAP，以及共享 TCP、TLS、
  SASL 和 raw-DEFLATE 传输适配。
- `src/mail`：纯邮件内容实现。
- `src/transport`：跨协议共享的线路、网络、TLS、压缩和认证实现。
- `src/smtp`、`src/pop3`、`src/imap`：协议专属解析、状态机和便利层。

内容和协议解析层使用借用视图与调用方缓冲，常用写出接口支持精确容量，不强制建立消息
对象树。网络客户端借用调用方 Engine、Resolver、TLS Context 和 Verifier，不隐藏共享对象
生命周期；所有阻塞等待统一接受 deadline 和 cancel。

邮件构建分为两层：`mail_build` 直接向 sink 写字段、原始正文和 multipart part，不持有
正文；`mail_compose` 才提供文本、HTML、内联资源和附件的常见结构。只需要高性能原始
报文路径时，不会被迫携带高层消息描述和自动生成逻辑。

## 能力

SMTP 提供响应与 EHLO 能力解析、安全命令写出、流式 DATA、零拷贝 CHUNKING/BDAT、
BINARYMIME 能力检查、独立或消息派生 envelope 提交、隐式 TLS、STARTTLS，以及 PLAIN、
LOGIN、XOAUTH2 认证。高层提交把 Compose 片段直接送入 dot writer 和网络，不创建整封
临时报文。

POP3 提供状态、STAT/LIST/UIDL、CAPA、安全命令写出、流式 RETR/TOP、owned 消息与 MIME
树桥接、隐式 TLS、STLS，以及统一 USER/PASS、PLAIN、XOAUTH2、OAUTHBEARER 认证。普通
命令和 SASL continuation 使用独立行长限制，密码默认禁止明文发送，bearer 凭据强制 TLS。

IMAP 提供响应、literal、响应码、数字响应、CAPABILITY、借用数据游标、零分配
BODYSTRUCTURE 语义视图、显式 tag 流水线、顺序命令、IDLE、常用邮箱与消息命令、
BODY section 流式/owned/MIME 树桥接、流式
APPEND、隐式 TLS、STARTTLS、LOGIN/PLAIN/XOAUTH2/OAUTHBEARER，以及 RFC 4978
COMPRESS=DEFLATE。未知扩展仍可通过公开低层线路和原始响应视图实现。

## 裁剪

只选择内容与协议原语：

```c
#define XMAIL_MODULE_XMAIL
#include <xmail.h>
```

选择完整扩展：

```c
#define XMAIL_MODULE_ALL
#include <xmail.h>
```

也可以选择单个层，例如 `XMAIL_MODULE_MAIL_BUILD`、`XMAIL_MODULE_MAIL_COMPOSE`、
`XMAIL_MODULE_MAIL_MESSAGE`、`XMAIL_MODULE_MAIL_TREE`、`XMAIL_MODULE_SMTP_SUBMIT`、
`XMAIL_MODULE_SMTP_CLIENT_TLS`、
`XMAIL_MODULE_POP3_AUTH`、`XMAIL_MODULE_POP3_MESSAGE`、`XMAIL_MODULE_IMAP_BODY`、
`XMAIL_MODULE_IMAP_COMMAND`、
`XMAIL_MODULE_IMAP_MESSAGE`、`XMAIL_MODULE_IMAP_APPEND` 或
`XMAIL_MODULE_IMAP_COMPRESS`。每个模块宏只展开清单声明的依赖闭包；TLS、认证、常用命令、
APPEND 和压缩互相独立。

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
