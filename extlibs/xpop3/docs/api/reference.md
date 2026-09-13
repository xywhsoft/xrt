# xpop3 公共符号参考

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xpop3/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `47` 个函数、`32` 个常量或宏、
`11` 个公共类型。

## `extlibs/xpop3/include/xrt/pop3.h`

[查看带契约注释的公共头](../../include/xrt/pop3.h)

### 函数 (9)

- `xrtPop3Capability`
- `xrtPop3CapabilityParse`
- `xrtPop3Command`
- `xrtPop3CommandWrite`
- `xrtPop3ListParse`
- `xrtPop3ReplyParse`
- `xrtPop3SaslMechanism`
- `xrtPop3StatParse`
- `xrtPop3UidlParse`

### 常量与宏 (17)

- `XPOP3_AUTH_COMMAND_MAX`
- `XPOP3_AUTH_RESPONSE_MAX`
- `XPOP3_CAP_EXPIRE`
- `XPOP3_CAP_IMPLEMENTATION`
- `XPOP3_CAP_LOGIN_DELAY`
- `XPOP3_CAP_PIPELINING`
- `XPOP3_CAP_RESP_CODES`
- `XPOP3_CAP_SASL`
- `XPOP3_CAP_STLS`
- `XPOP3_CAP_TOP`
- `XPOP3_CAP_UIDL`
- `XPOP3_CAP_USER`
- `XPOP3_COMMAND_MAX`
- `XPOP3_FEATURE_POP3`
- `XPOP3_SASL_OAUTHBEARER`
- `XPOP3_SASL_PLAIN`
- `XPOP3_SASL_XOAUTH2`

### 类型 (5)

- `xpop3capabilityview`
- `xpop3listview`
- `xpop3replyview`
- `xpop3stat`
- `xpop3uidlview`

## `extlibs/xpop3/include/xrt/pop3_auth.h`

[查看带契约注释的公共头](../../include/xrt/pop3_auth.h)

### 函数 (4)

- `xrtPop3AuthConfigInit`
- `xrtPop3AuthConfigValid`
- `xrtPop3ClientAuth`
- `xrtPop3ClientLogin`

### 常量与宏 (6)

- `XPOP3_AUTH_OAUTHBEARER`
- `XPOP3_AUTH_PLAIN`
- `XPOP3_AUTH_USER_PASS`
- `XPOP3_AUTH_XOAUTH2`
- `XPOP3_FEATURE_POP3_AUTH`
- `XPOP3_FEATURE_POP3_CLIENT`

### 类型 (3)

- `xpop3authconfig`
- `xpop3authmethod`
- `xpop3client`

## `extlibs/xpop3/include/xrt/pop3_client.h`

[查看带契约注释的公共头](../../include/xrt/pop3_client.h)

### 函数 (29)

- `xrtPop3ClientAbort`
- `xrtPop3ClientAuthLine`
- `xrtPop3ClientBegin`
- `xrtPop3ClientCapabilities`
- `xrtPop3ClientClose`
- `xrtPop3ClientCommand`
- `xrtPop3ClientConfigInit`
- `xrtPop3ClientConfigValid`
- `xrtPop3ClientDelete`
- `xrtPop3ClientDestroy`
- `xrtPop3ClientLastReply`
- `xrtPop3ClientLine`
- `xrtPop3ClientList`
- `xrtPop3ClientListAll`
- `xrtPop3ClientNext`
- `xrtPop3ClientNoop`
- `xrtPop3ClientOpen`
- `xrtPop3ClientQuit`
- `xrtPop3ClientReceive`
- `xrtPop3ClientReset`
- `xrtPop3ClientRetr`
- `xrtPop3ClientSaslMechanisms`
- `xrtPop3ClientSecurity`
- `xrtPop3ClientSend`
- `xrtPop3ClientStat`
- `xrtPop3ClientState`
- `xrtPop3ClientTop`
- `xrtPop3ClientUidl`
- `xrtPop3ClientUidlAll`

### 常量与宏 (7)

- `XPOP3_CLIENT_AUTHORIZATION`
- `XPOP3_CLIENT_CLOSED`
- `XPOP3_CLIENT_FAILED`
- `XPOP3_CLIENT_MULTILINE`
- `XPOP3_CLIENT_TRANSACTION`
- `XPOP3_CLIENT_UPDATE`
- `XPOP3_FEATURE_POP3_CLIENT_TLS`

### 类型 (3)

- `xpop3clientconfig`
- `xpop3clientstate`
- `xpop3reply`

## `extlibs/xpop3/include/xrt/pop3_message.h`

[查看带契约注释的公共头](../../include/xrt/pop3_message.h)

### 函数 (5)

- `xrtPop3ClientRetrBytes`
- `xrtPop3ClientRetrTree`
- `xrtPop3ClientRetrWrite`
- `xrtPop3ClientTopBytes`
- `xrtPop3ClientTopWrite`

### 常量与宏 (2)

- `XPOP3_FEATURE_POP3_MESSAGE`
- `XPOP3_MESSAGE_BYTES_DEFAULT`
