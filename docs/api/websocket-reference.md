# WebSocket 公共符号参考

此文件由 `tools/generate_api_reference.py` 从 `config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[websocket.md](websocket.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `103` 个函数、`138` 个常量或宏、
`40` 个公共类型。

## `include/xrt/websocket.h`

[查看带契约注释的公共头](../../include/xrt/websocket.h)

### 函数 (56)

- `xrtWsAccept`
- `xrtWsAcceptValid`
- `xrtWsCloseCodeValid`
- `xrtWsCloseParse`
- `xrtWsCloseWrite`
- `xrtWsDeflateAccept`
- `xrtWsDeflateDirection`
- `xrtWsDeflateInit`
- `xrtWsDeflateIs`
- `xrtWsDeflateOfferParse`
- `xrtWsDeflateOfferWrite`
- `xrtWsDeflateResponseCheck`
- `xrtWsDeflateResponseParse`
- `xrtWsDeflateResponseWrite`
- `xrtWsDeflaterAbort`
- `xrtWsDeflaterBegin`
- `xrtWsDeflaterBound`
- `xrtWsDeflaterConfigApply`
- `xrtWsDeflaterConfigInit`
- `xrtWsDeflaterCreate`
- `xrtWsDeflaterDestroy`
- `xrtWsDeflaterEnd`
- `xrtWsDeflaterFlush`
- `xrtWsDeflaterReset`
- `xrtWsDeflaterSize`
- `xrtWsDeflaterWrite`
- `xrtWsExtensionCount`
- `xrtWsExtensionNext`
- `xrtWsExtensionParamNext`
- `xrtWsExtensionWrite`
- `xrtWsFrameConfigInit`
- `xrtWsFrameInit`
- `xrtWsFrameParse`
- `xrtWsFrameWrite`
- `xrtWsInflaterBegin`
- `xrtWsInflaterConfigApply`
- `xrtWsInflaterConfigInit`
- `xrtWsInflaterCreate`
- `xrtWsInflaterDestroy`
- `xrtWsInflaterEnd`
- `xrtWsInflaterReset`
- `xrtWsInflaterSize`
- `xrtWsInflaterWrite`
- `xrtWsKeyGenerate`
- `xrtWsKeyValid`
- `xrtWsMask`
- `xrtWsMessageConfigInit`
- `xrtWsMessageFrameBegin`
- `xrtWsMessageFrameEnd`
- `xrtWsMessageInit`
- `xrtWsMessagePayload`
- `xrtWsMessageReset`
- `xrtWsProtocolNext`
- `xrtWsProtocolSelect`
- `xrtWsProtocolsHas`
- `xrtWsProtocolsValid`

### 常量与宏 (113)

- `XWS_ACCEPT_CAPACITY`
- `XWS_ACCEPT_SIZE`
- `XWS_CLOSE_ABNORMAL`
- `XWS_CLOSE_BAD_GATEWAY`
- `XWS_CLOSE_ERROR_ARGUMENT`
- `XWS_CLOSE_ERROR_CODE`
- `XWS_CLOSE_ERROR_OUTPUT`
- `XWS_CLOSE_ERROR_SIZE`
- `XWS_CLOSE_ERROR_UTF8`
- `XWS_CLOSE_EXTENSION_REQUIRED`
- `XWS_CLOSE_GOING_AWAY`
- `XWS_CLOSE_INTERNAL`
- `XWS_CLOSE_INVALID_DATA`
- `XWS_CLOSE_NORMAL`
- `XWS_CLOSE_NO_STATUS`
- `XWS_CLOSE_PAYLOAD_MAX`
- `XWS_CLOSE_POLICY`
- `XWS_CLOSE_PROTOCOL`
- `XWS_CLOSE_REASON_MAX`
- `XWS_CLOSE_RESTART`
- `XWS_CLOSE_TLS`
- `XWS_CLOSE_TOO_BIG`
- `XWS_CLOSE_TRY_AGAIN`
- `XWS_CLOSE_UNSUPPORTED`
- `XWS_DEFLATE_CLIENT_MAX_WINDOW`
- `XWS_DEFLATE_CLIENT_MAX_WINDOW_ANY`
- `XWS_DEFLATE_CLIENT_NO_CONTEXT`
- `XWS_DEFLATE_ERROR_ARGUMENT`
- `XWS_DEFLATE_ERROR_CODEC`
- `XWS_DEFLATE_ERROR_CONFIG`
- `XWS_DEFLATE_ERROR_DATA`
- `XWS_DEFLATE_ERROR_DUPLICATE`
- `XWS_DEFLATE_ERROR_EXTENSION`
- `XWS_DEFLATE_ERROR_LIMIT`
- `XWS_DEFLATE_ERROR_OUTPUT`
- `XWS_DEFLATE_ERROR_PARAMETER`
- `XWS_DEFLATE_ERROR_RESPONSE`
- `XWS_DEFLATE_ERROR_STATE`
- `XWS_DEFLATE_ERROR_WINDOW`
- `XWS_DEFLATE_MAX_SIZE`
- `XWS_DEFLATE_NAME`
- `XWS_DEFLATE_SERVER_MAX_WINDOW`
- `XWS_DEFLATE_SERVER_NO_CONTEXT`
- `XWS_DEFLATE_WINDOW_MAX`
- `XWS_DEFLATE_WINDOW_MIN`
- `XWS_FRAME_ERROR`
- `XWS_FRAME_ERROR_ARGUMENT`
- `XWS_FRAME_ERROR_CLOSE`
- `XWS_FRAME_ERROR_CONFIG`
- `XWS_FRAME_ERROR_CONTROL`
- `XWS_FRAME_ERROR_LENGTH`
- `XWS_FRAME_ERROR_MASK`
- `XWS_FRAME_ERROR_OPCODE`
- `XWS_FRAME_ERROR_OUTPUT`
- `XWS_FRAME_ERROR_RSV`
- `XWS_FRAME_FIN`
- `XWS_FRAME_HEAD_MAX`
- `XWS_FRAME_MASKED`
- `XWS_FRAME_MORE`
- `XWS_FRAME_PAYLOAD_MAX`
- `XWS_FRAME_READY`
- `XWS_FRAME_RSV`
- `XWS_FRAME_RSV1`
- `XWS_FRAME_RSV2`
- `XWS_FRAME_RSV3`
- `XWS_HANDSHAKE_ERROR_ACCEPT`
- `XWS_HANDSHAKE_ERROR_ARGUMENT`
- `XWS_HANDSHAKE_ERROR_BODY`
- `XWS_HANDSHAKE_ERROR_CONNECTION`
- `XWS_HANDSHAKE_ERROR_EXTENSION`
- `XWS_HANDSHAKE_ERROR_FIELD`
- `XWS_HANDSHAKE_ERROR_HOST`
- `XWS_HANDSHAKE_ERROR_KEY`
- `XWS_HANDSHAKE_ERROR_METHOD`
- `XWS_HANDSHAKE_ERROR_OUTPUT`
- `XWS_HANDSHAKE_ERROR_PROTOCOL`
- `XWS_HANDSHAKE_ERROR_RANDOM`
- `XWS_HANDSHAKE_ERROR_STATUS`
- `XWS_HANDSHAKE_ERROR_UPGRADE`
- `XWS_HANDSHAKE_ERROR_VERSION`
- `XWS_INFLATE_OUTPUT_DEFAULT`
- `XWS_KEY_BYTES`
- `XWS_KEY_CAPACITY`
- `XWS_KEY_SIZE`
- `XWS_MASK_ANY`
- `XWS_MASK_FORBIDDEN`
- `XWS_MASK_REQUIRED`
- `XWS_MASK_SIZE`
- `XWS_MESSAGE_BEGIN`
- `XWS_MESSAGE_COMPRESSED`
- `XWS_MESSAGE_CONTROL`
- `XWS_MESSAGE_END`
- `XWS_MESSAGE_ERROR_ARGUMENT`
- `XWS_MESSAGE_ERROR_CLOSE`
- `XWS_MESSAGE_ERROR_CONFIG`
- `XWS_MESSAGE_ERROR_FRAGMENT`
- `XWS_MESSAGE_ERROR_OPCODE`
- `XWS_MESSAGE_ERROR_PAYLOAD`
- `XWS_MESSAGE_ERROR_RSV`
- `XWS_MESSAGE_ERROR_SIZE`
- `XWS_MESSAGE_ERROR_STATE`
- `XWS_MESSAGE_ERROR_UTF8`
- `XWS_MESSAGE_EXTENDED`
- `XWS_OPCODES_STANDARD`
- `XWS_OPCODE_BINARY`
- `XWS_OPCODE_CLOSE`
- `XWS_OPCODE_CONTINUATION`
- `XWS_OPCODE_PING`
- `XWS_OPCODE_PONG`
- `XWS_OPCODE_TEXT`
- `XWS_ROLE_CLIENT`
- `XWS_ROLE_SERVER`
- `XWS_VERSION`

### 类型 (29)

- `xwsclose`
- `xwsclosecode`
- `xwscloseerror`
- `xwsdeflate`
- `xwsdeflatedirection`
- `xwsdeflateerror`
- `xwsdeflateflag`
- `xwsdeflater`
- `xwsdeflaterconfig`
- `xwsextension`
- `xwsframe`
- `xwsframeconfig`
- `xwsframeerror`
- `xwsframeerrorinfo`
- `xwsframeflag`
- `xwsframestatus`
- `xwshandshakeerror`
- `xwsinflater`
- `xwsinflaterconfig`
- `xwsmaskpolicy`
- `xwsmessageconfig`
- `xwsmessageerror`
- `xwsmessageerrorinfo`
- `xwsmessageflag`
- `xwsmessageinfo`
- `xwsmessagestate`
- `xwsopcode`
- `xwsoutputproc`
- `xwsrole`

## `include/xrt/websocket_stream.h`

[查看带契约注释的公共头](../../include/xrt/websocket_stream.h)

### 函数 (38)

- `xrtWsStreamAbort`
- `xrtWsStreamAttach`
- `xrtWsStreamAttachTls`
- `xrtWsStreamBinary`
- `xrtWsStreamBinaryCompressed`
- `xrtWsStreamBinaryRef`
- `xrtWsStreamBinaryTake`
- `xrtWsStreamClose`
- `xrtWsStreamCloseInfo`
- `xrtWsStreamConfigInit`
- `xrtWsStreamConfigValid`
- `xrtWsStreamDeflate`
- `xrtWsStreamDestroy`
- `xrtWsStreamError`
- `xrtWsStreamPause`
- `xrtWsStreamPaused`
- `xrtWsStreamPending`
- `xrtWsStreamPing`
- `xrtWsStreamPong`
- `xrtWsStreamProtocol`
- `xrtWsStreamRef`
- `xrtWsStreamResume`
- `xrtWsStreamRole`
- `xrtWsStreamSend`
- `xrtWsStreamSendCompressed`
- `xrtWsStreamSendRef`
- `xrtWsStreamSendTake`
- `xrtWsStreamState`
- `xrtWsStreamTcp`
- `xrtWsStreamTcpRef`
- `xrtWsStreamText`
- `xrtWsStreamTextCompressed`
- `xrtWsStreamTextRef`
- `xrtWsStreamTextTake`
- `xrtWsStreamTls`
- `xrtWsStreamTlsRef`
- `xrtWsStreamWorker`
- `xrtWsStreamWritable`

### 常量与宏 (23)

- `XWS_STREAM_CLOSED`
- `XWS_STREAM_CLOSE_CLEAN`
- `XWS_STREAM_CLOSE_RECEIVED`
- `XWS_STREAM_CLOSE_REMOTE`
- `XWS_STREAM_CLOSE_SENT`
- `XWS_STREAM_CLOSE_TIMEOUT_DEFAULT`
- `XWS_STREAM_CLOSING`
- `XWS_STREAM_CONTROL_RESERVE_DEFAULT`
- `XWS_STREAM_ERROR_ARGUMENT`
- `XWS_STREAM_ERROR_CONFIG`
- `XWS_STREAM_ERROR_FRAME`
- `XWS_STREAM_ERROR_LIMIT`
- `XWS_STREAM_ERROR_MEMORY`
- `XWS_STREAM_ERROR_MESSAGE`
- `XWS_STREAM_ERROR_RANDOM`
- `XWS_STREAM_ERROR_SEND`
- `XWS_STREAM_ERROR_STATE`
- `XWS_STREAM_ERROR_TIMEOUT`
- `XWS_STREAM_ERROR_TRANSPORT`
- `XWS_STREAM_FRAME_LIMIT_DEFAULT`
- `XWS_STREAM_MESSAGE_LIMIT_DEFAULT`
- `XWS_STREAM_OPEN`
- `XWS_STREAM_SEND_LIMIT_DEFAULT`

### 类型 (7)

- `xwsstream`
- `xwsstreamclose`
- `xwsstreamcloseflag`
- `xwsstreamconfig`
- `xwsstreamerror`
- `xwsstreamevents`
- `xwsstreamstate`

## `include/xrt/websocket_upgrade.h`

[查看带契约注释的公共头](../../include/xrt/websocket_upgrade.h)

### 函数 (9)

- `xrtWsUpgradeClientConfigInit`
- `xrtWsUpgradeClientConfigValid`
- `xrtWsUpgradeRequestCheck`
- `xrtWsUpgradeRequestFields`
- `xrtWsUpgradeResponseCheck`
- `xrtWsUpgradeResponseFields`
- `xrtWsUpgradeServerConfigInit`
- `xrtWsUpgradeServerConfigValid`
- `xrtWsUpgradeStreamConfig`

### 常量与宏 (2)

- `XWS_UPGRADE_REQUEST_FIELDS_MAX`
- `XWS_UPGRADE_RESPONSE_FIELDS_MAX`

### 类型 (4)

- `xwsupgrade`
- `xwsupgradeacceptproc`
- `xwsupgradeclientconfig`
- `xwsupgradeserverconfig`
