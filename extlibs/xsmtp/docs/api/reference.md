# xsmtp 公共符号参考

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xsmtp/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `43` 个函数、`33` 个常量或宏、
`11` 个公共类型。

## `extlibs/xsmtp/include/xrt/smtp.h`

[查看带契约注释的公共头](../../include/xrt/smtp.h)

### 函数 (9)

- `xrtSmtpCapability`
- `xrtSmtpCapabilityAdd`
- `xrtSmtpCapabilityParse`
- `xrtSmtpCommand`
- `xrtSmtpCommandWrite`
- `xrtSmtpPathValid`
- `xrtSmtpReplyLineParse`
- `xrtSmtpReplyParserInit`
- `xrtSmtpReplyRead`

### 常量与宏 (17)

- `XSMTP_AUTH_RESPONSE_MAX`
- `XSMTP_CAP_8BITMIME`
- `XSMTP_CAP_AUTH_LOGIN`
- `XSMTP_CAP_AUTH_OAUTHBEARER`
- `XSMTP_CAP_AUTH_PLAIN`
- `XSMTP_CAP_AUTH_XOAUTH2`
- `XSMTP_CAP_BINARYMIME`
- `XSMTP_CAP_CHUNKING`
- `XSMTP_CAP_DSN`
- `XSMTP_CAP_ENHANCED_STATUS`
- `XSMTP_CAP_PIPELINING`
- `XSMTP_CAP_SIZE`
- `XSMTP_CAP_SMTPUTF8`
- `XSMTP_CAP_STARTTLS`
- `XSMTP_COMMAND_MAX`
- `XSMTP_FEATURE_SMTP`
- `XSMTP_REPLY_LINES_DEFAULT`

### 类型 (3)

- `xsmtpcapabilityview`
- `xsmtpreplyline`
- `xsmtpreplyparser`

## `extlibs/xsmtp/include/xrt/smtp_auth.h`

[查看带契约注释的公共头](../../include/xrt/smtp_auth.h)

### 函数 (3)

- `xrtSmtpAuthConfigInit`
- `xrtSmtpAuthConfigValid`
- `xrtSmtpClientAuth`

### 常量与宏 (6)

- `XSMTP_AUTH_LOGIN`
- `XSMTP_AUTH_OAUTHBEARER`
- `XSMTP_AUTH_PLAIN`
- `XSMTP_AUTH_XOAUTH2`
- `XSMTP_FEATURE_SMTP_AUTH`
- `XSMTP_FEATURE_SMTP_CLIENT`

### 类型 (3)

- `xsmtpauthconfig`
- `xsmtpauthmethod`
- `xsmtpclient`

## `extlibs/xsmtp/include/xrt/smtp_client.h`

[查看带契约注释的公共头](../../include/xrt/smtp_client.h)

### 函数 (29)

- `xrtSmtpClientAbort`
- `xrtSmtpClientAuthLine`
- `xrtSmtpClientAuthenticated`
- `xrtSmtpClientBdat`
- `xrtSmtpClientBdatBegin`
- `xrtSmtpClientBdatEnd`
- `xrtSmtpClientBdatWrite`
- `xrtSmtpClientCapabilities`
- `xrtSmtpClientClose`
- `xrtSmtpClientCommand`
- `xrtSmtpClientConfigInit`
- `xrtSmtpClientConfigValid`
- `xrtSmtpClientData`
- `xrtSmtpClientDataBegin`
- `xrtSmtpClientDataEnd`
- `xrtSmtpClientDataWrite`
- `xrtSmtpClientDestroy`
- `xrtSmtpClientLastReply`
- `xrtSmtpClientMail`
- `xrtSmtpClientNoop`
- `xrtSmtpClientOpen`
- `xrtSmtpClientQuit`
- `xrtSmtpClientRcpt`
- `xrtSmtpClientReceive`
- `xrtSmtpClientReset`
- `xrtSmtpClientSecurity`
- `xrtSmtpClientSend`
- `xrtSmtpClientSizeLimit`
- `xrtSmtpClientState`

### 常量与宏 (9)

- `XSMTP_CLIENT_CHUNK`
- `XSMTP_CLIENT_CLOSED`
- `XSMTP_CLIENT_DATA`
- `XSMTP_CLIENT_FAILED`
- `XSMTP_CLIENT_MAIL`
- `XSMTP_CLIENT_READY`
- `XSMTP_CLIENT_RECIPIENT`
- `XSMTP_FEATURE_SMTP_CLIENT_TLS`
- `XSMTP_HELLO_MAX`

### 类型 (3)

- `xsmtpclientconfig`
- `xsmtpclientstate`
- `xsmtpreply`

## `extlibs/xsmtp/include/xrt/smtp_submit.h`

[查看带契约注释的公共头](../../include/xrt/smtp_submit.h)

### 函数 (2)

- `xrtSmtpSubmit`
- `xrtSmtpSubmitEnvelope`

### 常量与宏 (1)

- `XSMTP_FEATURE_SMTP_SUBMIT`

### 类型 (2)

- `xsmtpenvelope`
- `xsmtprecipient`
