# xmail 公共符号参考

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xmail/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `89` 个函数、`112` 个常量或宏、
`39` 个公共类型。

## `extlibs/xmail/include/xrt/mail.h`

[查看带契约注释的公共头](../../include/xrt/mail.h)

### 函数 (3)

- `xrtMailBoundaryValid`
- `xrtMailCrlf`
- `xrtMailCrlfWrite`

### 常量与宏 (57)

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
- `XMAIL_FEATURE_IMAP`
- `XMAIL_FEATURE_IMAP_APPEND`
- `XMAIL_FEATURE_IMAP_AUTH`
- `XMAIL_FEATURE_IMAP_BODY`
- `XMAIL_FEATURE_IMAP_CLIENT`
- `XMAIL_FEATURE_IMAP_CLIENT_TLS`
- `XMAIL_FEATURE_IMAP_COMMAND`
- `XMAIL_FEATURE_IMAP_COMPRESS`
- `XMAIL_FEATURE_IMAP_DATA`
- `XMAIL_FEATURE_IMAP_MESSAGE`
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
- `XMAIL_FEATURE_MAIL_NET`
- `XMAIL_FEATURE_MAIL_NET_DEFLATE`
- `XMAIL_FEATURE_MAIL_NET_TLS`
- `XMAIL_FEATURE_MAIL_PARAM`
- `XMAIL_FEATURE_MAIL_TREE`
- `XMAIL_FEATURE_MAIL_WIRE`
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

### 类型 (3)

- `xmailerror`
- `xmailnext`
- `xmailwriteproc`

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

### 类型 (2)

- `xmailnetconfig`
- `xmailsecurity`

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

### 常量与宏 (8)

- `XMAIL_TREE_ALLOW_UNKNOWN_CHARSET`
- `XMAIL_TREE_ALLOW_UNKNOWN_TRANSFER`
- `XMAIL_TREE_DECODED_BYTES_DEFAULT`
- `XMAIL_TREE_DEPTH_DEFAULT`
- `XMAIL_TREE_DEPTH_MAX`
- `XMAIL_TREE_PARTS_DEFAULT`
- `XMAIL_TREE_RELAXED_QP`
- `XMAIL_TREE_SOURCE_BYTES_DEFAULT`

### 类型 (4)

- `xmailpart`
- `xmailtree`
- `xmailtreeflag`
- `xmailtreelimits`

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
