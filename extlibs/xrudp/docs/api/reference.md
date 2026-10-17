# xrudp public API reference

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xrudp/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `29` 个函数、`59` 个常量或宏、
`18` 个公共类型。

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

### 函数 (7)

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

## `extlibs/xrudp/include/xrudp/export.h`

[查看带契约注释的公共头](../../include/xrudp/export.h)

### 常量与宏 (2)

- `XRUDP_BUILD_SHARED`
- `XRUDP_USE_SHARED`

## `extlibs/xrudp/include/xrudp/noise.h`

[查看带契约注释的公共头](../../include/xrudp/noise.h)

### 函数 (7)

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

### 类型 (5)

- `xrudpnoise`
- `xrudpnoiseconfig`
- `xrudpnoiseinfo`
- `xrudpnoisetrustproc`
- `xrudprole`

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
- `XRUDP_PACKET_MAX`
- `XRUDP_PACKET_RETRY`
- `XRUDP_PROTOCOL_MISMATCH`
- `XRUDP_REPLAY_BITS`
- `XRUDP_REPLAY_WORDS`
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
