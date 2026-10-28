# xmqtt public API reference

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xmqtt/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `11` 个函数、`35` 个常量或宏、
`7` 个公共类型。

## `extlibs/xmqtt/include/xmqtt/export.h`

[查看带契约注释的公共头](../../include/xmqtt/export.h)

### 常量与宏 (2)

- `XMQTT_BUILD_SHARED`
- `XMQTT_USE_SHARED`

## `extlibs/xmqtt/include/xmqtt/wire.h`

[查看带契约注释的公共头](../../include/xmqtt/wire.h)

### 函数 (11)

- `xmqttBinaryRead`
- `xmqttBinaryWrite`
- `xmqttFrameRead`
- `xmqttHeaderRead`
- `xmqttHeaderWrite`
- `xmqttUtf8StringCheck`
- `xmqttUtf8StringRead`
- `xmqttUtf8StringWrite`
- `xmqttVbiRead`
- `xmqttVbiWrite`
- `xmqttWireConfigInit`

### 常量与宏 (33)

- `XMQTT_FEATURE_XMQTT_WIRE`
- `XMQTT_PACKET_AUTH`
- `XMQTT_PACKET_CONNACK`
- `XMQTT_PACKET_CONNECT`
- `XMQTT_PACKET_DISCONNECT`
- `XMQTT_PACKET_MAX_BYTES`
- `XMQTT_PACKET_PINGREQ`
- `XMQTT_PACKET_PINGRESP`
- `XMQTT_PACKET_PUBACK`
- `XMQTT_PACKET_PUBCOMP`
- `XMQTT_PACKET_PUBLISH`
- `XMQTT_PACKET_PUBREC`
- `XMQTT_PACKET_PUBREL`
- `XMQTT_PACKET_SUBACK`
- `XMQTT_PACKET_SUBSCRIBE`
- `XMQTT_PACKET_UNSUBACK`
- `XMQTT_PACKET_UNSUBSCRIBE`
- `XMQTT_PUBLISH_DUP`
- `XMQTT_PUBLISH_QOS_MASK`
- `XMQTT_PUBLISH_RETAIN`
- `XMQTT_STRING_MAX_BYTES`
- `XMQTT_UTF8_REJECT_DISALLOWED`
- `XMQTT_VARIABLE_MAX`
- `XMQTT_VERSION_311`
- `XMQTT_VERSION_5`
- `XMQTT_WIRE_ARGUMENT`
- `XMQTT_WIRE_CAPACITY`
- `XMQTT_WIRE_DONE`
- `XMQTT_WIRE_ERROR`
- `XMQTT_WIRE_LIMIT`
- `XMQTT_WIRE_MALFORMED`
- `XMQTT_WIRE_MORE`
- `XMQTT_WIRE_UTF8`

### 类型 (7)

- `xmqttfixedheader`
- `xmqttframe`
- `xmqttpackettype`
- `xmqttversion`
- `xmqttwireconfig`
- `xmqttwireerror`
- `xmqttwirestatus`
