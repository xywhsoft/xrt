# xmail 公共符号参考

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xmail/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `280` 个函数、`251` 个常量或宏、
`103` 个公共类型。

## `extlibs/xmail/include/xrt/imap.h`

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

### 常量与宏 (43)

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
- `XIMAP_RESPONSE_CONTINUATION`
- `XIMAP_RESPONSE_TAGGED`
- `XIMAP_RESPONSE_UNTAGGED`
- `XIMAP_STATUS_BAD`
- `XIMAP_STATUS_BYE`
- `XIMAP_STATUS_NO`
- `XIMAP_STATUS_NONE`
- `XIMAP_STATUS_OK`
- `XIMAP_STATUS_PREAUTH`
- `XMAIL_FEATURE_IMAP`
- `XMAIL_FEATURE_MAIL_WIRE`

### 类型 (8)

- `ximapatomcursor`
- `ximapcodeview`
- `ximapliteralview`
- `ximapnumberview`
- `ximapresponsekind`
- `ximapresponseview`
- `ximapstatus`
- `xmailnext`

## `extlibs/xmail/include/xrt/imap_append.h`

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

- `XIMAP_LITERAL_AUTO`
- `XIMAP_LITERAL_NONSYNC`
- `XIMAP_LITERAL_SYNC`
- `XMAIL_FEATURE_IMAP_APPEND`
- `XMAIL_FEATURE_IMAP_CLIENT`

### 类型 (4)

- `ximapappendconfig`
- `ximapappendresult`
- `ximapclient`
- `ximapliteralmode`

## `extlibs/xmail/include/xrt/imap_auth.h`

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
- `XMAIL_FEATURE_IMAP_AUTH`

### 类型 (2)

- `ximapauthconfig`
- `ximapauthmethod`

## `extlibs/xmail/include/xrt/imap_body.h`

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
- `XMAIL_FEATURE_IMAP_BODY`
- `XMAIL_FEATURE_IMAP_DATA`

### 类型 (7)

- `ximapbodycursor`
- `ximapbodykind`
- `ximapbodyparam`
- `ximapbodyparamcursor`
- `ximapbodyview`
- `ximapdatacursor`
- `ximapdataview`

## `extlibs/xmail/include/xrt/imap_client.h`

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

### 常量与宏 (12)

- `XIMAP_APPEND_LIMIT_UNKNOWN`
- `XIMAP_CLIENT_AUTHENTICATED`
- `XIMAP_CLIENT_CLOSED`
- `XIMAP_CLIENT_FAILED`
- `XIMAP_CLIENT_NOT_AUTHENTICATED`
- `XIMAP_CLIENT_SELECTED`
- `XIMAP_CLIENT_TAG_MAX`
- `XIMAP_EVENT_FRAGMENT`
- `XIMAP_EVENT_RESPONSE`
- `XMAIL_FEATURE_IMAP_CLIENT_TLS`
- `XMAIL_FEATURE_MAIL_NET`
- `XMAIL_FEATURE_MAIL_NET_TLS`

### 类型 (6)

- `ximapclientconfig`
- `ximapclientstate`
- `ximapevent`
- `ximapeventkind`
- `xmailnetconfig`
- `xmailsecurity`

## `extlibs/xmail/include/xrt/imap_command.h`

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
- `XMAIL_FEATURE_IMAP_COMMAND`

### 类型 (2)

- `ximapmailboxinfo`
- `ximapstoremode`

## `extlibs/xmail/include/xrt/imap_compress.h`

[查看带契约注释的公共头](../../include/xrt/imap_compress.h)

### 函数 (4)

- `xrtImapClientCompress`
- `xrtImapClientCompressed`
- `xrtImapCompressConfigInit`
- `xrtImapCompressConfigValid`

### 常量与宏 (2)

- `XMAIL_FEATURE_IMAP_COMPRESS`
- `XMAIL_FEATURE_MAIL_NET_DEFLATE`

### 类型 (1)

- `ximapcompressconfig`

## `extlibs/xmail/include/xrt/imap_data.h`

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

## `extlibs/xmail/include/xrt/imap_message.h`

[查看带契约注释的公共头](../../include/xrt/imap_message.h)

### 函数 (3)

- `xrtImapClientBodyBytes`
- `xrtImapClientBodyWrite`
- `xrtImapClientMessageTree`

### 常量与宏 (4)

- `XIMAP_MESSAGE_BYTES_DEFAULT`
- `XMAIL_FEATURE_IMAP_MESSAGE`
- `XMAIL_FEATURE_MAIL_TREE`
- `XMAIL_TREE_SOURCE_BYTES_DEFAULT`

### 类型 (3)

- `xmailtree`
- `xmailtreelimits`
- `xmailwriteproc`

## `extlibs/xmail/include/xrt/mail.h`

[查看带契约注释的公共头](../../include/xrt/mail.h)

### 函数 (3)

- `xrtMailBoundaryValid`
- `xrtMailCrlf`
- `xrtMailCrlfWrite`

### 常量与宏 (42)

- `XMAIL_BASE64_LINE_DEFAULT`
- `XMAIL_BOUNDARY_MAX`
- `XMAIL_ERROR_ADDRESS`
- `XMAIL_ERROR_AUTH`
- `XMAIL_ERROR_CALLBACK`
- `XMAIL_ERROR_CHARSET`
- `XMAIL_ERROR_CONFIG`
- `XMAIL_ERROR_ENCODING`
- `XMAIL_ERROR_HEADER`
- `XMAIL_ERROR_LIMIT`
- `XMAIL_ERROR_LINE`
- `XMAIL_ERROR_MIME`
- `XMAIL_ERROR_PROTOCOL`
- `XMAIL_FEATURE_MAIL_ADDRESS`
- `XMAIL_FEATURE_MAIL_BUILD`
- `XMAIL_FEATURE_MAIL_CHARSET`
- `XMAIL_FEATURE_MAIL_CODEC`
- `XMAIL_FEATURE_MAIL_COMPOSE`
- `XMAIL_FEATURE_MAIL_CORE`
- `XMAIL_FEATURE_MAIL_DATE`
- `XMAIL_FEATURE_MAIL_HEADER`
- `XMAIL_FEATURE_MAIL_ID`
- `XMAIL_FEATURE_MAIL_MESSAGE`
- `XMAIL_FEATURE_MAIL_MULTIPART`
- `XMAIL_FEATURE_MAIL_PARAM`
- `XMAIL_FEATURE_MAIL_WORD`
- `XMAIL_FEATURE_POP3`
- `XMAIL_FEATURE_POP3_AUTH`
- `XMAIL_FEATURE_POP3_CLIENT`
- `XMAIL_FEATURE_POP3_CLIENT_TLS`
- `XMAIL_FEATURE_POP3_MESSAGE`
- `XMAIL_FEATURE_SMTP`
- `XMAIL_FEATURE_SMTP_AUTH`
- `XMAIL_FEATURE_SMTP_CLIENT`
- `XMAIL_FEATURE_SMTP_CLIENT_TLS`
- `XMAIL_FEATURE_SMTP_SUBMIT`
- `XMAIL_HEADER_LINE_DEFAULT`
- `XMAIL_HEADER_LINE_HARD`
- `XMAIL_NEXT_END`
- `XMAIL_NEXT_ERROR`
- `XMAIL_NEXT_ITEM`
- `XMAIL_QP_LINE_DEFAULT`

### 类型 (1)

- `xmailerror`

## `extlibs/xmail/include/xrt/mail_address.h`

[查看带契约注释的公共头](../../include/xrt/mail_address.h)

### 函数 (7)

- `xrtMailAddress`
- `xrtMailAddressCursorInit`
- `xrtMailAddressList`
- `xrtMailAddressListWrite`
- `xrtMailAddressNext`
- `xrtMailAddressValid`
- `xrtMailAddressWrite`

### 常量与宏 (6)

- `XMAIL_ADDRESS_COMMENT_DEPTH`
- `XMAIL_ADDRESS_DEFAULT`
- `XMAIL_ADDRESS_GROUP_BEGIN`
- `XMAIL_ADDRESS_GROUP_END`
- `XMAIL_ADDRESS_MAILBOX`
- `XMAIL_ADDRESS_SMTPUTF8`

### 类型 (6)

- `xmailaddress`
- `xmailaddresscursor`
- `xmailaddressflag`
- `xmailaddresskind`
- `xmailaddressview`
- `xmailwordencoding`

## `extlibs/xmail/include/xrt/mail_build.h`

[查看带契约注释的公共头](../../include/xrt/mail_build.h)

### 函数 (10)

- `xrtMailBuilderAddressHeader`
- `xrtMailBuilderBody`
- `xrtMailBuilderFinish`
- `xrtMailBuilderHeader`
- `xrtMailBuilderHeaderBlock`
- `xrtMailBuilderHeadersEnd`
- `xrtMailBuilderInit`
- `xrtMailBuilderMultipart`
- `xrtMailBuilderPartBegin`
- `xrtMailBuilderWordHeader`

### 常量与宏 (4)

- `XMAIL_BUILDER_BODY`
- `XMAIL_BUILDER_CLOSED`
- `XMAIL_BUILDER_FAILED`
- `XMAIL_BUILDER_HEADERS`

### 类型 (3)

- `xmailbuilder`
- `xmailbuilderstate`
- `xmailmultipartmark`

## `extlibs/xmail/include/xrt/mail_charset.h`

[查看带契约注释的公共头](../../include/xrt/mail_charset.h)

### 函数 (3)

- `xrtMailCharsetSupported`
- `xrtMailCharsetToUtf8`
- `xrtMailCharsetToUtf8Write`

## `extlibs/xmail/include/xrt/mail_codec.h`

[查看带契约注释的公共头](../../include/xrt/mail_codec.h)

### 函数 (8)

- `xrtMailBase64`
- `xrtMailBase64Decode`
- `xrtMailBase64DecodeWrite`
- `xrtMailBase64Write`
- `xrtMailQp`
- `xrtMailQpDecode`
- `xrtMailQpDecodeWrite`
- `xrtMailQpWrite`

### 常量与宏 (3)

- `XMAIL_QP_BINARY`
- `XMAIL_QP_RELAXED_SOFT_BREAK`
- `XMAIL_QP_TEXT`

### 类型 (1)

- `xmailqpflag`

## `extlibs/xmail/include/xrt/mail_compose.h`

[查看带契约注释的公共头](../../include/xrt/mail_compose.h)

### 函数 (4)

- `xrtMailCompose`
- `xrtMailComposeWrite`
- `xrtMailMessageInit`
- `xrtMailMessageValid`

### 类型 (3)

- `xmailattachment`
- `xmailheaderview`
- `xmailmessage`

## `extlibs/xmail/include/xrt/mail_date.h`

[查看带契约注释的公共头](../../include/xrt/mail_date.h)

### 函数 (3)

- `xrtMailDate`
- `xrtMailDateParse`
- `xrtMailDateWrite`

### 常量与宏 (2)

- `XMAIL_DATE_RELAXED`
- `XMAIL_DATE_STRICT`

### 类型 (1)

- `xmaildateflag`

## `extlibs/xmail/include/xrt/mail_header.h`

[查看带契约注释的公共头](../../include/xrt/mail_header.h)

### 函数 (8)

- `xrtMailHeader`
- `xrtMailHeaderCursorInit`
- `xrtMailHeaderNameValid`
- `xrtMailHeaderNext`
- `xrtMailHeaderUnfold`
- `xrtMailHeaderUnfoldWrite`
- `xrtMailHeaderValueValid`
- `xrtMailHeaderWrite`

### 类型 (1)

- `xmailheadercursor`

## `extlibs/xmail/include/xrt/mail_id.h`

[查看带契约注释的公共头](../../include/xrt/mail_id.h)

### 函数 (5)

- `xrtMailBoundary`
- `xrtMailBoundaryWrite`
- `xrtMailMessageId`
- `xrtMailMessageIdParse`
- `xrtMailMessageIdWrite`

### 常量与宏 (2)

- `XMAIL_ID_DEFAULT`
- `XMAIL_ID_UTF8`

### 类型 (2)

- `xmailidflag`
- `xmailmessageidview`

## `extlibs/xmail/include/xrt/mail_message.h`

[查看带契约注释的公共头](../../include/xrt/mail_message.h)

### 函数 (6)

- `xrtMailMessageBody`
- `xrtMailMessageBodyWrite`
- `xrtMailMessageHeader`
- `xrtMailMessageParse`
- `xrtMailMessageTransfer`
- `xrtMailTransferParse`

### 常量与宏 (8)

- `XMAIL_MESSAGE_HEADERS_DEFAULT`
- `XMAIL_MESSAGE_HEADER_BYTES_DEFAULT`
- `XMAIL_TRANSFER_7BIT`
- `XMAIL_TRANSFER_8BIT`
- `XMAIL_TRANSFER_BASE64`
- `XMAIL_TRANSFER_BINARY`
- `XMAIL_TRANSFER_QUOTED_PRINTABLE`
- `XMAIL_TRANSFER_UNKNOWN`

### 类型 (2)

- `xmailmessageview`
- `xmailtransfer`

## `extlibs/xmail/include/xrt/mail_multipart.h`

[查看带契约注释的公共头](../../include/xrt/mail_multipart.h)

### 函数 (3)

- `xrtMailMultipartCursorInit`
- `xrtMailMultipartMarkWrite`
- `xrtMailMultipartNext`

### 常量与宏 (4)

- `XMAIL_MULTIPART_CLOSE`
- `XMAIL_MULTIPART_FIRST`
- `XMAIL_MULTIPART_NEXT`
- `XMAIL_MULTIPART_PARTS_DEFAULT`

### 类型 (2)

- `xmailmultipartcursor`
- `xmailmultipartview`

## `extlibs/xmail/include/xrt/mail_net.h`

[查看带契约注释的公共头](../../include/xrt/mail_net.h)

### 函数 (2)

- `xrtMailNetConfigInit`
- `xrtMailNetConfigValid`

### 常量与宏 (6)

- `XMAIL_NET_HOST_MAX`
- `XMAIL_NET_READ_CHUNK_DEFAULT`
- `XMAIL_NET_WRITE_CHUNK_DEFAULT`
- `XMAIL_SECURITY_PLAIN`
- `XMAIL_SECURITY_STARTTLS`
- `XMAIL_SECURITY_TLS`

## `extlibs/xmail/include/xrt/mail_param.h`

[查看带契约注释的公共头](../../include/xrt/mail_param.h)

### 函数 (9)

- `xrtMailDispositionParse`
- `xrtMailMediaTypeParse`
- `xrtMailParam`
- `xrtMailParamCursorInit`
- `xrtMailParamDecodeWrite`
- `xrtMailParamFind`
- `xrtMailParamFindWrite`
- `xrtMailParamNext`
- `xrtMailParamWrite`

### 常量与宏 (7)

- `XMAIL_PARAM_ENCODING_AUTO`
- `XMAIL_PARAM_ENCODING_QUOTED`
- `XMAIL_PARAM_ENCODING_TOKEN`
- `XMAIL_PARAM_ENCODING_UTF8`
- `XMAIL_PARAM_SECTIONS_MAX`
- `XMAIL_PARAM_SECTION_NONE`
- `XMAIL_PARAM_SECTION_SIZE`

### 类型 (6)

- `xmaildispositionview`
- `xmailmediatypeview`
- `xmailparamcursor`
- `xmailparamencoding`
- `xmailparaminfo`
- `xmailparamview`

## `extlibs/xmail/include/xrt/mail_tree.h`

[查看带契约注释的公共头](../../include/xrt/mail_tree.h)

### 函数 (4)

- `xrtMailTreeFree`
- `xrtMailTreeLimitsInit`
- `xrtMailTreeLimitsValid`
- `xrtMailTreeParse`

### 常量与宏 (7)

- `XMAIL_TREE_ALLOW_UNKNOWN_CHARSET`
- `XMAIL_TREE_ALLOW_UNKNOWN_TRANSFER`
- `XMAIL_TREE_DECODED_BYTES_DEFAULT`
- `XMAIL_TREE_DEPTH_DEFAULT`
- `XMAIL_TREE_DEPTH_MAX`
- `XMAIL_TREE_PARTS_DEFAULT`
- `XMAIL_TREE_RELAXED_QP`

### 类型 (2)

- `xmailpart`
- `xmailtreeflag`

## `extlibs/xmail/include/xrt/mail_wire.h`

[查看带契约注释的公共头](../../include/xrt/mail_wire.h)

### 函数 (9)

- `xrtMailDot`
- `xrtMailDotDecode`
- `xrtMailDotDecodeWrite`
- `xrtMailDotLine`
- `xrtMailDotWrite`
- `xrtMailDotWriterFinish`
- `xrtMailDotWriterInit`
- `xrtMailDotWriterWrite`
- `xrtMailLineRead`

### 常量与宏 (1)

- `XMAIL_WIRE_LINE_DEFAULT`

### 类型 (1)

- `xmaildotwriter`

## `extlibs/xmail/include/xrt/mail_word.h`

[查看带契约注释的公共头](../../include/xrt/mail_word.h)

### 函数 (5)

- `xrtMailWordDecode`
- `xrtMailWordDecodeWrite`
- `xrtMailWordEncode`
- `xrtMailWordEncodeWrite`
- `xrtMailWordParse`

### 常量与宏 (4)

- `XMAIL_WORD_BASE64`
- `XMAIL_WORD_Q`
- `XMAIL_WORD_RELAXED`
- `XMAIL_WORD_STRICT`

### 类型 (2)

- `xmailwordflag`
- `xmailwordview`

## `extlibs/xmail/include/xrt/pop3.h`

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

### 常量与宏 (16)

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
- `XPOP3_SASL_OAUTHBEARER`
- `XPOP3_SASL_PLAIN`
- `XPOP3_SASL_XOAUTH2`

### 类型 (5)

- `xpop3capabilityview`
- `xpop3listview`
- `xpop3replyview`
- `xpop3stat`
- `xpop3uidlview`

## `extlibs/xmail/include/xrt/pop3_auth.h`

[查看带契约注释的公共头](../../include/xrt/pop3_auth.h)

### 函数 (4)

- `xrtPop3AuthConfigInit`
- `xrtPop3AuthConfigValid`
- `xrtPop3ClientAuth`
- `xrtPop3ClientLogin`

### 常量与宏 (4)

- `XPOP3_AUTH_OAUTHBEARER`
- `XPOP3_AUTH_PLAIN`
- `XPOP3_AUTH_USER_PASS`
- `XPOP3_AUTH_XOAUTH2`

### 类型 (3)

- `xpop3authconfig`
- `xpop3authmethod`
- `xpop3client`

## `extlibs/xmail/include/xrt/pop3_client.h`

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

### 常量与宏 (6)

- `XPOP3_CLIENT_AUTHORIZATION`
- `XPOP3_CLIENT_CLOSED`
- `XPOP3_CLIENT_FAILED`
- `XPOP3_CLIENT_MULTILINE`
- `XPOP3_CLIENT_TRANSACTION`
- `XPOP3_CLIENT_UPDATE`

### 类型 (3)

- `xpop3clientconfig`
- `xpop3clientstate`
- `xpop3reply`

## `extlibs/xmail/include/xrt/pop3_message.h`

[查看带契约注释的公共头](../../include/xrt/pop3_message.h)

### 函数 (5)

- `xrtPop3ClientRetrBytes`
- `xrtPop3ClientRetrTree`
- `xrtPop3ClientRetrWrite`
- `xrtPop3ClientTopBytes`
- `xrtPop3ClientTopWrite`

### 常量与宏 (1)

- `XPOP3_MESSAGE_BYTES_DEFAULT`

## `extlibs/xmail/include/xrt/smtp.h`

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

### 常量与宏 (16)

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
- `XSMTP_REPLY_LINES_DEFAULT`

### 类型 (3)

- `xsmtpcapabilityview`
- `xsmtpreplyline`
- `xsmtpreplyparser`

## `extlibs/xmail/include/xrt/smtp_auth.h`

[查看带契约注释的公共头](../../include/xrt/smtp_auth.h)

### 函数 (3)

- `xrtSmtpAuthConfigInit`
- `xrtSmtpAuthConfigValid`
- `xrtSmtpClientAuth`

### 常量与宏 (4)

- `XSMTP_AUTH_LOGIN`
- `XSMTP_AUTH_OAUTHBEARER`
- `XSMTP_AUTH_PLAIN`
- `XSMTP_AUTH_XOAUTH2`

### 类型 (3)

- `xsmtpauthconfig`
- `xsmtpauthmethod`
- `xsmtpclient`

## `extlibs/xmail/include/xrt/smtp_client.h`

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

### 常量与宏 (8)

- `XSMTP_CLIENT_CHUNK`
- `XSMTP_CLIENT_CLOSED`
- `XSMTP_CLIENT_DATA`
- `XSMTP_CLIENT_FAILED`
- `XSMTP_CLIENT_MAIL`
- `XSMTP_CLIENT_READY`
- `XSMTP_CLIENT_RECIPIENT`
- `XSMTP_HELLO_MAX`

### 类型 (3)

- `xsmtpclientconfig`
- `xsmtpclientstate`
- `xsmtpreply`

## `extlibs/xmail/include/xrt/smtp_submit.h`

[查看带契约注释的公共头](../../include/xrt/smtp_submit.h)

### 函数 (2)

- `xrtSmtpSubmit`
- `xrtSmtpSubmitEnvelope`

### 类型 (2)

- `xsmtpenvelope`
- `xsmtprecipient`
