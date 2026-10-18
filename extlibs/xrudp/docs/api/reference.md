# xrudp public API reference

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xrudp/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `125` 个函数、`123` 个常量或宏、
`54` 个公共类型。

## `extlibs/xrudp/include/xrudp/ack.h`

[查看带契约注释的公共头](../../include/xrudp/ack.h)

### 函数 (5)

- `xrtRudpAckDecode`
- `xrtRudpAckSetContains`
- `xrtRudpAckSetEncode`
- `xrtRudpAckSetInit`
- `xrtRudpAckSetRecord`

### 常量与宏 (2)

- `XRUDP_ACK_RANGES_MAX`
- `XRUDP_FEATURE_RUDP_ACK`

### 类型 (4)

- `xrudpack`
- `xrudpackrange`
- `xrudpackset`
- `xrudpresult`

## `extlibs/xrudp/include/xrudp/congestion.h`

[查看带契约注释的公共头](../../include/xrudp/congestion.h)

### 函数 (5)

- `xrtRudpCongestionApply`
- `xrtRudpCongestionCanSend`
- `xrtRudpCongestionCollapse`
- `xrtRudpCongestionInit`
- `xrtRudpCongestionOnSent`

### 常量与宏 (7)

- `XRUDP_CC_BURST_BYTES`
- `XRUDP_CC_DEFAULT_MAXIMUM`
- `XRUDP_CC_INITIAL_WINDOW`
- `XRUDP_CC_MAXIMUM_LIMIT`
- `XRUDP_CC_MINIMUM_WINDOW`
- `XRUDP_FEATURE_RUDP_CONGESTION`
- `XRUDP_PACKET_MAX`

### 类型 (2)

- `xrudpcongestion`
- `xrudpsentevent`

## `extlibs/xrudp/include/xrudp/cookie.h`

[查看带契约注释的公共头](../../include/xrudp/cookie.h)

### 函数 (2)

- `xrtRudpCookieCreate`
- `xrtRudpCookieVerify`

### 常量与宏 (3)

- `XRUDP_COOKIE_MAX_AGE_US`
- `XRUDP_COOKIE_SIZE`
- `XRUDP_FEATURE_RUDP_COOKIE`

### 类型 (1)

- `xrudpaddress`

## `extlibs/xrudp/include/xrudp/crypto.h`

[查看带契约注释的公共头](../../include/xrudp/crypto.h)

### 函数 (8)

- `xrtRudpCipherAllocationSize`
- `xrtRudpCipherCreate`
- `xrtRudpCipherDestroy`
- `xrtRudpCipherOpen`
- `xrtRudpCipherRetirePrevious`
- `xrtRudpCipherSeal`
- `xrtRudpCipherState`
- `xrtRudpCipherUpdateKey`

### 常量与宏 (3)

- `XRUDP_FEATURE_RUDP_CRYPTO`
- `XRUDP_KEY_PACKETS_MAX`
- `XRUDP_KEY_SIZE`

### 类型 (2)

- `xrudpcipher`
- `xrudpcipherstate`

## `extlibs/xrudp/include/xrudp/endpoint.h`

[查看带契约注释的公共头](../../include/xrudp/endpoint.h)

### 函数 (17)

- `xrtRudpEndpointBind`
- `xrtRudpEndpointClose`
- `xrtRudpEndpointConfigInit`
- `xrtRudpEndpointConnect`
- `xrtRudpEndpointDestroy`
- `xrtRudpEndpointInfo`
- `xrtRudpEndpointRef`
- `xrtRudpEndpointState`
- `xrtRudpPeerAbort`
- `xrtRudpPeerDestroy`
- `xrtRudpPeerFinish`
- `xrtRudpPeerInfo`
- `xrtRudpPeerRead`
- `xrtRudpPeerRef`
- `xrtRudpPeerSessionState`
- `xrtRudpPeerState`
- `xrtRudpPeerWrite`

### 常量与宏 (14)

- `XRUDP_ENDPOINT_ADDRESS_BUCKETS`
- `XRUDP_ENDPOINT_CLOSED`
- `XRUDP_ENDPOINT_CLOSING`
- `XRUDP_ENDPOINT_OPEN`
- `XRUDP_ENDPOINT_OPENING`
- `XRUDP_ENDPOINT_PEERS_MAX`
- `XRUDP_FEATURE_RUDP_ENDPOINT`
- `XRUDP_PEER_CLOSED`
- `XRUDP_PEER_CONFIRMED`
- `XRUDP_PEER_NEGOTIATING`
- `XRUDP_PEER_READ_EOF`
- `XRUDP_PEER_READ_READY`
- `XRUDP_PEER_WRITE_DONE`
- `XRUDP_PEER_WRITE_READY`

### 类型 (12)

- `xrudpconnectconfig`
- `xrudpendpoint`
- `xrudpendpointconfig`
- `xrudpendpointevents`
- `xrudpendpointinfo`
- `xrudpendpointstate`
- `xrudphandshakeinfo`
- `xrudpnoisetrustproc`
- `xrudppeer`
- `xrudppeerstate`
- `xrudpsessionconfig`
- `xrudpsessionstate`

## `extlibs/xrudp/include/xrudp/export.h`

[查看带契约注释的公共头](../../include/xrudp/export.h)

### 常量与宏 (2)

- `XRUDP_BUILD_SHARED`
- `XRUDP_USE_SHARED`

## `extlibs/xrudp/include/xrudp/handshake.h`

[查看带契约注释的公共头](../../include/xrudp/handshake.h)

### 函数 (12)

- `xrtRudpHandshakeAccept`
- `xrtRudpHandshakeAllocationSize`
- `xrtRudpHandshakeCreate`
- `xrtRudpHandshakeDestroy`
- `xrtRudpHandshakeInfo`
- `xrtRudpHandshakeInput`
- `xrtRudpHandshakeNext`
- `xrtRudpHandshakeRetry`
- `xrtRudpHandshakeTakeCipher`
- `xrtRudpParametersDecode`
- `xrtRudpParametersDefault`
- `xrtRudpParametersEncode`

### 常量与宏 (11)

- `XRUDP_FEATURE_RUDP_HANDSHAKE`
- `XRUDP_HANDSHAKE_DEFAULT_TIMEOUT_US`
- `XRUDP_HANDSHAKE_ESTABLISHED`
- `XRUDP_HANDSHAKE_FAILED`
- `XRUDP_HANDSHAKE_MAX_TIMEOUT_US`
- `XRUDP_HANDSHAKE_NOISE`
- `XRUDP_HANDSHAKE_PACKET_MAX`
- `XRUDP_HANDSHAKE_PROOF`
- `XRUDP_HANDSHAKE_RETRY`
- `XRUDP_HELLO_SIZE`
- `XRUDP_PARAMETERS_SIZE`

### 类型 (5)

- `xrudphandshake`
- `xrudphandshakeconfig`
- `xrudphandshakephase`
- `xrudpparameters`
- `xrudprole`

## `extlibs/xrudp/include/xrudp/noise.h`

[查看带契约注释的公共头](../../include/xrudp/noise.h)

### 函数 (8)

- `xrtRudpNoiseAllocationSize`
- `xrtRudpNoiseCreate`
- `xrtRudpNoiseCreateCipher`
- `xrtRudpNoiseDestroy`
- `xrtRudpNoiseInfo`
- `xrtRudpNoisePeerParameters`
- `xrtRudpNoiseRead`
- `xrtRudpNoiseWrite`

### 常量与宏 (5)

- `XRUDP_FEATURE_RUDP_NOISE`
- `XRUDP_INITIATOR`
- `XRUDP_NOISE_MESSAGE_MAX`
- `XRUDP_NOISE_PARAMETERS_MAX`
- `XRUDP_RESPONDER`

### 类型 (3)

- `xrudpnoise`
- `xrudpnoiseconfig`
- `xrudpnoiseinfo`

## `extlibs/xrudp/include/xrudp/receive.h`

[查看带契约注释的公共头](../../include/xrudp/receive.h)

### 函数 (13)

- `xrtRudpReceiveAllocationSize`
- `xrtRudpReceiveCheck`
- `xrtRudpReceiveCheckBatch`
- `xrtRudpReceiveCreate`
- `xrtRudpReceiveDestroy`
- `xrtRudpReceiveExtend`
- `xrtRudpReceivePush`
- `xrtRudpReceivePushBatch`
- `xrtRudpReceiveRead`
- `xrtRudpReceiveReady`
- `xrtRudpReceiveReset`
- `xrtRudpReceiveRestart`
- `xrtRudpReceiveState`

### 常量与宏 (4)

- `XRUDP_FEATURE_RUDP_RECEIVE`
- `XRUDP_RECEIVE_CAPACITY_DEFAULT`
- `XRUDP_RECEIVE_CAPACITY_MAX`
- `XRUDP_RECEIVE_CAPACITY_MIN`

### 类型 (5)

- `xrudpreceive`
- `xrudpreceivebatchchange`
- `xrudpreceivechange`
- `xrudpreceiveframe`
- `xrudpreceivestate`

## `extlibs/xrudp/include/xrudp/sent.h`

[查看带契约注释的公共头](../../include/xrudp/sent.h)

### 函数 (11)

- `xrtRudpSentAbort`
- `xrtRudpSentAcknowledge`
- `xrtRudpSentAllocationSize`
- `xrtRudpSentCanRecord`
- `xrtRudpSentCheckAcknowledge`
- `xrtRudpSentCreate`
- `xrtRudpSentCreateAfter`
- `xrtRudpSentDestroy`
- `xrtRudpSentDetectLoss`
- `xrtRudpSentRecord`
- `xrtRudpSentState`

### 常量与宏 (11)

- `XRUDP_FEATURE_RUDP_SENT`
- `XRUDP_SENT_ACKED`
- `XRUDP_SENT_ACK_ELICITING`
- `XRUDP_SENT_CANCELLED`
- `XRUDP_SENT_CAPACITY_DEFAULT`
- `XRUDP_SENT_CAPACITY_MAX`
- `XRUDP_SENT_EXPIRED`
- `XRUDP_SENT_LOST`
- `XRUDP_SENT_PADDING`
- `XRUDP_SENT_PROBE`
- `XRUDP_SENT_PROBE_RESERVE`

### 类型 (4)

- `xrudpackchange`
- `xrudpsenteventkind`
- `xrudpsentstate`
- `xrudpsentwindow`

## `extlibs/xrudp/include/xrudp/session.h`

[查看带契约注释的公共头](../../include/xrudp/session.h)

### 函数 (20)

- `xrtRudpSessionAbort`
- `xrtRudpSessionAllocationSize`
- `xrtRudpSessionConfigInit`
- `xrtRudpSessionCreate`
- `xrtRudpSessionDestroy`
- `xrtRudpSessionFinish`
- `xrtRudpSessionInput`
- `xrtRudpSessionPoll`
- `xrtRudpSessionRead`
- `xrtRudpSessionState`
- `xrtRudpSessionStreamFinish`
- `xrtRudpSessionStreamNextEvent`
- `xrtRudpSessionStreamOpen`
- `xrtRudpSessionStreamRead`
- `xrtRudpSessionStreamReset`
- `xrtRudpSessionStreamRetire`
- `xrtRudpSessionStreamState`
- `xrtRudpSessionStreamWrite`
- `xrtRudpSessionWrite`
- `xrtRudpStreamConfigInit`

### 常量与宏 (9)

- `XRUDP_FEATURE_RUDP_SESSION`
- `XRUDP_SESSION_PACKETS_MAX`
- `XRUDP_SESSION_POLL_BURST`
- `XRUDP_SESSION_STREAM_CAPACITY_MAX`
- `XRUDP_STREAM_OPENED`
- `XRUDP_STREAM_READ_EOF`
- `XRUDP_STREAM_READ_READY`
- `XRUDP_STREAM_WRITE_DONE`
- `XRUDP_STREAM_WRITE_READY`

### 类型 (7)

- `xrudprtt`
- `xrudpsession`
- `xrudpsessionsendproc`
- `xrudpstreamconfig`
- `xrudpstreamevent`
- `xrudpstreamstate`
- `xrudptransmitstate`

## `extlibs/xrudp/include/xrudp/timing.h`

[查看带契约注释的公共头](../../include/xrudp/timing.h)

### 函数 (4)

- `xrtRudpRttInit`
- `xrtRudpRttLossDelay`
- `xrtRudpRttPto`
- `xrtRudpRttSample`

### 常量与宏 (4)

- `XRUDP_ACK_DELAY_MAX_US`
- `XRUDP_FEATURE_RUDP_TIMING`
- `XRUDP_INITIAL_RTT_US`
- `XRUDP_TIMER_GRANULARITY_US`

## `extlibs/xrudp/include/xrudp/transmit.h`

[查看带契约注释的公共头](../../include/xrudp/transmit.h)

### 函数 (12)

- `xrtRudpTransmitAllocationSize`
- `xrtRudpTransmitCancel`
- `xrtRudpTransmitCreate`
- `xrtRudpTransmitDestroy`
- `xrtRudpTransmitFinish`
- `xrtRudpTransmitMarkSent`
- `xrtRudpTransmitPeek`
- `xrtRudpTransmitResolve`
- `xrtRudpTransmitRestart`
- `xrtRudpTransmitSetLimit`
- `xrtRudpTransmitState`
- `xrtRudpTransmitWrite`

### 常量与宏 (4)

- `XRUDP_FEATURE_RUDP_TRANSMIT`
- `XRUDP_TRANSMIT_CAPACITY_DEFAULT`
- `XRUDP_TRANSMIT_CAPACITY_MAX`
- `XRUDP_TRANSMIT_CAPACITY_MIN`

### 类型 (3)

- `xrudptransmit`
- `xrudptransmitchange`
- `xrudptransmitframe`

## `extlibs/xrudp/include/xrudp/wire.h`

[查看带契约注释的公共头](../../include/xrudp/wire.h)

### 函数 (8)

- `xrtRudpFrameAppend`
- `xrtRudpFrameNext`
- `xrtRudpFramesValidate`
- `xrtRudpPacketDecode`
- `xrtRudpPacketEncode`
- `xrtRudpReplayAccept`
- `xrtRudpReplayCheck`
- `xrtRudpReplayInit`

### 常量与宏 (44)

- `XRUDP_ARGUMENT`
- `XRUDP_AUTH_FAILED`
- `XRUDP_BACKPRESSURE`
- `XRUDP_CANCELLED`
- `XRUDP_CLOSED`
- `XRUDP_DATAGRAM_SEQUENCED`
- `XRUDP_DONE`
- `XRUDP_DUPLICATE`
- `XRUDP_FEATURE_RUDP_WIRE`
- `XRUDP_FRAMES_MAX`
- `XRUDP_FRAME_ACK`
- `XRUDP_FRAME_CLOSE`
- `XRUDP_FRAME_DATAGRAM`
- `XRUDP_FRAME_HEADER_SIZE`
- `XRUDP_FRAME_KEY_CONFIRM`
- `XRUDP_FRAME_MAX_DATA`
- `XRUDP_FRAME_MAX_STREAMS`
- `XRUDP_FRAME_MAX_STREAM_DATA`
- `XRUDP_FRAME_OPEN`
- `XRUDP_FRAME_PADDING`
- `XRUDP_FRAME_PATH_CHALLENGE`
- `XRUDP_FRAME_PATH_RESPONSE`
- `XRUDP_FRAME_PING`
- `XRUDP_FRAME_RESET`
- `XRUDP_FRAME_STREAM`
- `XRUDP_HEADER_SIZE`
- `XRUDP_IO_ERROR`
- `XRUDP_LIMIT`
- `XRUDP_MALFORMED`
- `XRUDP_NO_MEMORY`
- `XRUDP_OK`
- `XRUDP_PACKET_DATA`
- `XRUDP_PACKET_HANDSHAKE`
- `XRUDP_PACKET_HELLO`
- `XRUDP_PACKET_RETRY`
- `XRUDP_PROTOCOL_MISMATCH`
- `XRUDP_REPLAY_BITS`
- `XRUDP_REPLAY_WORDS`
- `XRUDP_STREAM_CHUNK_MAX`
- `XRUDP_STREAM_FIN`
- `XRUDP_TAG_SIZE`
- `XRUDP_TIMEOUT`
- `XRUDP_TOO_OLD`
- `XRUDP_TRUNCATED`

### 类型 (6)

- `xrudpframetype`
- `xrudppackettype`
- `xrudpreplay`
- `xrudpwireframe`
- `xrudpwireheader`
- `xrudpwirepacket`
