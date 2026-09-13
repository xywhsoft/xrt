# ximap 公共符号参考

此文件由 `tools/generate_api_reference.py` 从 `extlibs/ximap/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `101` 个函数、`94` 个常量或宏、
`42` 个公共类型。

## `extlibs/ximap/include/xrt/imap.h`

[查看带契约注释的公共头](../../include/xrt/imap.h)

### 函数 (13)

- `xrtImapAtomCursorInit`
- `xrtImapAtomNext`
- `xrtImapAtomValid`
- `xrtImapCapability`
- `xrtImapCodeParse`
- `xrtImapCommand`
- `xrtImapCommandWrite`
- `xrtImapLiteralParse`
- `xrtImapNumberParse`
- `xrtImapQuote`
- `xrtImapQuoteWrite`
- `xrtImapResponseParse`
- `xrtImapSequenceSetValid`

### 常量与宏 (42)

- `XIMAP_CAP_ACL`
- `XIMAP_CAP_APPENDLIMIT`
- `XIMAP_CAP_AUTH_OAUTHBEARER`
- `XIMAP_CAP_AUTH_PLAIN`
- `XIMAP_CAP_AUTH_XOAUTH2`
- `XIMAP_CAP_BINARY`
- `XIMAP_CAP_COMPRESS_DEFLATE`
- `XIMAP_CAP_CONDSTORE`
- `XIMAP_CAP_ENABLE`
- `XIMAP_CAP_ESEARCH`
- `XIMAP_CAP_IDLE`
- `XIMAP_CAP_IMAP4REV1`
- `XIMAP_CAP_IMAP4REV2`
- `XIMAP_CAP_LIST_EXTENDED`
- `XIMAP_CAP_LITERAL_MINUS`
- `XIMAP_CAP_LITERAL_PLUS`
- `XIMAP_CAP_LOGIN_DISABLED`
- `XIMAP_CAP_METADATA`
- `XIMAP_CAP_MOVE`
- `XIMAP_CAP_NAMESPACE`
- `XIMAP_CAP_NOTIFY`
- `XIMAP_CAP_QRESYNC`
- `XIMAP_CAP_QUOTA`
- `XIMAP_CAP_SASL_IR`
- `XIMAP_CAP_SORT`
- `XIMAP_CAP_SPECIAL_USE`
- `XIMAP_CAP_STARTTLS`
- `XIMAP_CAP_THREAD_REFERENCES`
- `XIMAP_CAP_UIDPLUS`
- `XIMAP_CAP_UNSELECT`
- `XIMAP_CAP_UTF8_ACCEPT`
- `XIMAP_COMMAND_LINE_DEFAULT`
- `XIMAP_FEATURE_IMAP`
- `XIMAP_RESPONSE_CONTINUATION`
- `XIMAP_RESPONSE_TAGGED`
- `XIMAP_RESPONSE_UNTAGGED`
- `XIMAP_STATUS_BAD`
- `XIMAP_STATUS_BYE`
- `XIMAP_STATUS_NO`
- `XIMAP_STATUS_NONE`
- `XIMAP_STATUS_OK`
- `XIMAP_STATUS_PREAUTH`

### 类型 (7)

- `ximapatomcursor`
- `ximapcodeview`
- `ximapliteralview`
- `ximapnumberview`
- `ximapresponsekind`
- `ximapresponseview`
- `ximapstatus`

## `extlibs/ximap/include/xrt/imap_append.h`

[查看带契约注释的公共头](../../include/xrt/imap_append.h)

### 函数 (7)

- `xrtImapAppendConfigInit`
- `xrtImapAppendResultInit`
- `xrtImapClientAppend`
- `xrtImapClientAppendBegin`
- `xrtImapClientAppendEnd`
- `xrtImapClientAppendRemaining`
- `xrtImapClientAppendWrite`

### 常量与宏 (5)

- `XIMAP_FEATURE_IMAP_APPEND`
- `XIMAP_FEATURE_IMAP_CLIENT`
- `XIMAP_LITERAL_AUTO`
- `XIMAP_LITERAL_NONSYNC`
- `XIMAP_LITERAL_SYNC`

### 类型 (4)

- `ximapappendconfig`
- `ximapappendresult`
- `ximapclient`
- `ximapliteralmode`

## `extlibs/ximap/include/xrt/imap_auth.h`

[查看带契约注释的公共头](../../include/xrt/imap_auth.h)

### 函数 (3)

- `xrtImapAuthConfigInit`
- `xrtImapAuthConfigValid`
- `xrtImapClientAuth`

### 常量与宏 (5)

- `XIMAP_AUTH_LOGIN`
- `XIMAP_AUTH_OAUTHBEARER`
- `XIMAP_AUTH_PLAIN`
- `XIMAP_AUTH_XOAUTH2`
- `XIMAP_FEATURE_IMAP_AUTH`

### 类型 (2)

- `ximapauthconfig`
- `ximapauthmethod`

## `extlibs/ximap/include/xrt/imap_body.h`

[查看带契约注释的公共头](../../include/xrt/imap_body.h)

### 函数 (5)

- `xrtImapBodyChildCursorInit`
- `xrtImapBodyChildNext`
- `xrtImapBodyParamCursorInit`
- `xrtImapBodyParamNext`
- `xrtImapBodyParse`

### 常量与宏 (7)

- `XIMAP_BODY_BASIC`
- `XIMAP_BODY_DEPTH_MAX`
- `XIMAP_BODY_MESSAGE`
- `XIMAP_BODY_MULTIPART`
- `XIMAP_BODY_TEXT`
- `XIMAP_FEATURE_IMAP_BODY`
- `XIMAP_FEATURE_IMAP_DATA`

### 类型 (7)

- `ximapbodycursor`
- `ximapbodykind`
- `ximapbodyparam`
- `ximapbodyparamcursor`
- `ximapbodyview`
- `ximapdatacursor`
- `ximapdataview`

## `extlibs/ximap/include/xrt/imap_client.h`

[查看带契约注释的公共头](../../include/xrt/imap_client.h)

### 函数 (25)

- `xrtImapClientAbort`
- `xrtImapClientAppendLimit`
- `xrtImapClientBegin`
- `xrtImapClientBeginParts`
- `xrtImapClientCapabilities`
- `xrtImapClientClose`
- `xrtImapClientCommandLimit`
- `xrtImapClientConfigInit`
- `xrtImapClientConfigValid`
- `xrtImapClientContinue`
- `xrtImapClientDestroy`
- `xrtImapClientLastResponse`
- `xrtImapClientLiteralRemaining`
- `xrtImapClientLogout`
- `xrtImapClientNext`
- `xrtImapClientOpen`
- `xrtImapClientReadLiteral`
- `xrtImapClientReceive`
- `xrtImapClientRefresh`
- `xrtImapClientSecurity`
- `xrtImapClientSend`
- `xrtImapClientSendParts`
- `xrtImapClientState`
- `xrtImapClientTag`
- `xrtImapClientWrite`

### 常量与宏 (10)

- `XIMAP_APPEND_LIMIT_UNKNOWN`
- `XIMAP_CLIENT_AUTHENTICATED`
- `XIMAP_CLIENT_CLOSED`
- `XIMAP_CLIENT_FAILED`
- `XIMAP_CLIENT_NOT_AUTHENTICATED`
- `XIMAP_CLIENT_SELECTED`
- `XIMAP_CLIENT_TAG_MAX`
- `XIMAP_EVENT_FRAGMENT`
- `XIMAP_EVENT_RESPONSE`
- `XIMAP_FEATURE_IMAP_CLIENT_TLS`

### 类型 (4)

- `ximapclientconfig`
- `ximapclientstate`
- `ximapevent`
- `ximapeventkind`

## `extlibs/ximap/include/xrt/imap_command.h`

[查看带契约注释的公共头](../../include/xrt/imap_command.h)

### 函数 (23)

- `xrtImapClientBeginCopy`
- `xrtImapClientBeginExpunge`
- `xrtImapClientBeginFetch`
- `xrtImapClientBeginIdle`
- `xrtImapClientBeginList`
- `xrtImapClientBeginMove`
- `xrtImapClientBeginSearch`
- `xrtImapClientBeginStatus`
- `xrtImapClientBeginStore`
- `xrtImapClientCheck`
- `xrtImapClientCloseMailbox`
- `xrtImapClientCreateMailbox`
- `xrtImapClientDeleteMailbox`
- `xrtImapClientEndIdle`
- `xrtImapClientExamine`
- `xrtImapClientNoop`
- `xrtImapClientRenameMailbox`
- `xrtImapClientSelect`
- `xrtImapClientSubscribe`
- `xrtImapClientUnselect`
- `xrtImapClientUnsubscribe`
- `xrtImapMailboxInfoInit`
- `xrtImapMailboxInfoUpdate`

### 常量与宏 (14)

- `XIMAP_FEATURE_IMAP_COMMAND`
- `XIMAP_MAILBOX_ACCESS`
- `XIMAP_MAILBOX_EXISTS`
- `XIMAP_MAILBOX_HIGHEST_MODSEQ`
- `XIMAP_MAILBOX_RECENT`
- `XIMAP_MAILBOX_UID_NEXT`
- `XIMAP_MAILBOX_UID_VALIDITY`
- `XIMAP_MAILBOX_UNSEEN`
- `XIMAP_STORE_ADD`
- `XIMAP_STORE_ADD_SILENT`
- `XIMAP_STORE_REMOVE`
- `XIMAP_STORE_REMOVE_SILENT`
- `XIMAP_STORE_SET`
- `XIMAP_STORE_SET_SILENT`

### 类型 (2)

- `ximapmailboxinfo`
- `ximapstoremode`

## `extlibs/ximap/include/xrt/imap_compress.h`

[查看带契约注释的公共头](../../include/xrt/imap_compress.h)

### 函数 (4)

- `xrtImapClientCompress`
- `xrtImapClientCompressed`
- `xrtImapCompressConfigInit`
- `xrtImapCompressConfigValid`

### 常量与宏 (1)

- `XIMAP_FEATURE_IMAP_COMPRESS`

### 类型 (1)

- `ximapcompressconfig`

## `extlibs/ximap/include/xrt/imap_data.h`

[查看带契约注释的公共头](../../include/xrt/imap_data.h)

### 函数 (18)

- `xrtImapDataCursorInit`
- `xrtImapDataNext`
- `xrtImapESearchCursorInit`
- `xrtImapESearchNext`
- `xrtImapESearchParse`
- `xrtImapFetchCursorContinue`
- `xrtImapFetchCursorInit`
- `xrtImapFetchNext`
- `xrtImapFetchParse`
- `xrtImapFlagCursorInit`
- `xrtImapFlagNext`
- `xrtImapListParse`
- `xrtImapSearchCursorInit`
- `xrtImapSearchNext`
- `xrtImapStatusCursorInit`
- `xrtImapStatusNext`
- `xrtImapStatusParse`
- `xrtImapStringWrite`

### 常量与宏 (8)

- `XIMAP_DATA_ATOM`
- `XIMAP_DATA_LIST`
- `XIMAP_DATA_LITERAL`
- `XIMAP_DATA_NIL`
- `XIMAP_DATA_NUMBER`
- `XIMAP_DATA_QUOTED`
- `XIMAP_SEARCH_ID`
- `XIMAP_SEARCH_MODSEQ`

### 类型 (15)

- `ximapdatakind`
- `ximapesearchcursor`
- `ximapesearchitem`
- `ximapesearchview`
- `ximapfetchcursor`
- `ximapfetchitem`
- `ximapfetchview`
- `ximapflagcursor`
- `ximaplistview`
- `ximapmailboxstatusview`
- `ximapsearchcursor`
- `ximapsearchitem`
- `ximapsearchitemkind`
- `ximapstatuscursor`
- `ximapstatusitem`

## `extlibs/ximap/include/xrt/imap_message.h`

[查看带契约注释的公共头](../../include/xrt/imap_message.h)

### 函数 (3)

- `xrtImapClientBodyBytes`
- `xrtImapClientBodyWrite`
- `xrtImapClientMessageTree`

### 常量与宏 (2)

- `XIMAP_FEATURE_IMAP_MESSAGE`
- `XIMAP_MESSAGE_BYTES_DEFAULT`
