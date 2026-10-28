# xmqtt public API reference

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xmqtt/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `23` 个函数、`121` 个常量或宏、
`30` 个公共类型。

## `extlibs/xmqtt/include/xmqtt/codec311.h`

[查看带契约注释的公共头](../../include/xmqtt/codec311.h)

### 函数 (3)

- `xmqttPacketRead311`
- `xmqttPacketSize311`
- `xmqttPacketWrite311`

### 常量与宏 (2)

- `XMQTT_FEATURE_XMQTT_CODEC311`
- `XMQTT_FEATURE_XMQTT_PACKET`

### 类型 (6)

- `xmqttcodecbuffers`
- `xmqttcodecconfig`
- `xmqttcodecissue`
- `xmqttcodecstatus`
- `xmqttdirection`
- `xmqttpacket`

## `extlibs/xmqtt/include/xmqtt/codec5.h`

[查看带契约注释的公共头](../../include/xmqtt/codec5.h)

### 函数 (3)

- `xmqttPacketRead5`
- `xmqttPacketSize5`
- `xmqttPacketWrite5`

### 常量与宏 (2)

- `XMQTT_FEATURE_XMQTT_CODEC5`
- `XMQTT_FEATURE_XMQTT_PROPERTIES`

## `extlibs/xmqtt/include/xmqtt/export.h`

[查看带契约注释的公共头](../../include/xmqtt/export.h)

### 常量与宏 (2)

- `XMQTT_BUILD_SHARED`
- `XMQTT_USE_SHARED`

## `extlibs/xmqtt/include/xmqtt/packet.h`

[查看带契约注释的公共头](../../include/xmqtt/packet.h)

### 函数 (2)

- `xmqttCodecConfigInit`
- `xmqttPacketInit`

### 常量与宏 (22)

- `XMQTT_CODEC_ARGUMENT`
- `XMQTT_CODEC_CAPACITY`
- `XMQTT_CODEC_CLIENT_ID`
- `XMQTT_CODEC_DONE`
- `XMQTT_CODEC_FORMAT`
- `XMQTT_CODEC_LIMIT`
- `XMQTT_CODEC_LOCAL_ERROR`
- `XMQTT_CODEC_MALFORMED`
- `XMQTT_CODEC_MORE`
- `XMQTT_CODEC_OK`
- `XMQTT_CODEC_PAYLOAD_ERROR`
- `XMQTT_CODEC_PAYLOAD_FORMAT`
- `XMQTT_CODEC_PROTOCOL`
- `XMQTT_CODEC_PROTOCOL_ERROR`
- `XMQTT_CODEC_TOPIC`
- `XMQTT_CODEC_UNSUPPORTED`
- `XMQTT_CODEC_VERSION`
- `XMQTT_FEATURE_XMQTT_WIRE`
- `XMQTT_SUBSCRIBE_NO_LOCAL`
- `XMQTT_SUBSCRIBE_QOS_MASK`
- `XMQTT_SUBSCRIBE_RETAIN_AS_PUBLISHED`
- `XMQTT_SUBSCRIBE_RETAIN_HANDLING_MASK`

### 类型 (9)

- `xmqttcodecerror`
- `xmqttconnectpacket`
- `xmqttpacketbody`
- `xmqttpackettype`
- `xmqttproperty`
- `xmqttpropertylist`
- `xmqttpublishpacket`
- `xmqttsubscription`
- `xmqttsubscriptionlist`

## `extlibs/xmqtt/include/xmqtt/properties.h`

[查看带契约注释的公共头](../../include/xmqtt/properties.h)

### 函数 (4)

- `xmqttPropertiesConfigInit`
- `xmqttPropertiesRead`
- `xmqttPropertiesSize`
- `xmqttPropertiesWrite`

### 常量与宏 (60)

- `XMQTT_PROPERTIES_AUTH`
- `XMQTT_PROPERTIES_CONNACK`
- `XMQTT_PROPERTIES_CONNECT`
- `XMQTT_PROPERTIES_DISCONNECT`
- `XMQTT_PROPERTIES_DONE`
- `XMQTT_PROPERTIES_LOCAL_ERROR`
- `XMQTT_PROPERTIES_MALFORMED`
- `XMQTT_PROPERTIES_MORE`
- `XMQTT_PROPERTIES_PROTOCOL_ERROR`
- `XMQTT_PROPERTIES_PUBACK`
- `XMQTT_PROPERTIES_PUBCOMP`
- `XMQTT_PROPERTIES_PUBLISH`
- `XMQTT_PROPERTIES_PUBREC`
- `XMQTT_PROPERTIES_PUBREL`
- `XMQTT_PROPERTIES_SUBACK`
- `XMQTT_PROPERTIES_SUBSCRIBE`
- `XMQTT_PROPERTIES_UNSUBACK`
- `XMQTT_PROPERTIES_UNSUBSCRIBE`
- `XMQTT_PROPERTIES_WILL`
- `XMQTT_PROPERTY_ARGUMENT`
- `XMQTT_PROPERTY_ASSIGNED_CLIENT_ID`
- `XMQTT_PROPERTY_AUTH_DATA`
- `XMQTT_PROPERTY_AUTH_METHOD`
- `XMQTT_PROPERTY_BINARY`
- `XMQTT_PROPERTY_BYTE`
- `XMQTT_PROPERTY_CAPACITY`
- `XMQTT_PROPERTY_CONTENT_TYPE`
- `XMQTT_PROPERTY_CORRELATION_DATA`
- `XMQTT_PROPERTY_LIMIT`
- `XMQTT_PROPERTY_MALFORMED`
- `XMQTT_PROPERTY_MAXIMUM_PACKET_SIZE`
- `XMQTT_PROPERTY_MAXIMUM_QOS`
- `XMQTT_PROPERTY_MESSAGE_EXPIRY`
- `XMQTT_PROPERTY_PAYLOAD_FORMAT`
- `XMQTT_PROPERTY_PROTOCOL`
- `XMQTT_PROPERTY_REASON_STRING`
- `XMQTT_PROPERTY_RECEIVE_MAXIMUM`
- `XMQTT_PROPERTY_REQUEST_PROBLEM_INFO`
- `XMQTT_PROPERTY_REQUEST_RESPONSE_INFO`
- `XMQTT_PROPERTY_RESPONSE_INFO`
- `XMQTT_PROPERTY_RESPONSE_TOPIC`
- `XMQTT_PROPERTY_RETAIN_AVAILABLE`
- `XMQTT_PROPERTY_SECTION_MAX`
- `XMQTT_PROPERTY_SERVER_KEEP_ALIVE`
- `XMQTT_PROPERTY_SERVER_REFERENCE`
- `XMQTT_PROPERTY_SESSION_EXPIRY`
- `XMQTT_PROPERTY_SHARED_AVAILABLE`
- `XMQTT_PROPERTY_STRING`
- `XMQTT_PROPERTY_STRING_PAIR`
- `XMQTT_PROPERTY_SUBSCRIPTION_ID`
- `XMQTT_PROPERTY_SUBSCRIPTION_ID_AVAILABLE`
- `XMQTT_PROPERTY_TOPIC_ALIAS`
- `XMQTT_PROPERTY_TOPIC_ALIAS_MAXIMUM`
- `XMQTT_PROPERTY_UINT16`
- `XMQTT_PROPERTY_UINT32`
- `XMQTT_PROPERTY_USER`
- `XMQTT_PROPERTY_VARIABLE`
- `XMQTT_PROPERTY_WILDCARD_AVAILABLE`
- `XMQTT_PROPERTY_WILL_DELAY`
- `XMQTT_VARIABLE_MAX`

### 类型 (9)

- `xmqttpropertyconfig`
- `xmqttpropertydocument`
- `xmqttpropertyerror`
- `xmqttpropertyid`
- `xmqttpropertyscope`
- `xmqttpropertystatus`
- `xmqttpropertytype`
- `xmqttpropertyvalue`
- `xmqttstringpair`

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

- `XMQTT_CLIENT_TO_SERVER`
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
- `XMQTT_SERVER_TO_CLIENT`
- `XMQTT_STRING_MAX_BYTES`
- `XMQTT_UTF8_REJECT_DISALLOWED`
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

### 类型 (6)

- `xmqttfixedheader`
- `xmqttframe`
- `xmqttversion`
- `xmqttwireconfig`
- `xmqttwireerror`
- `xmqttwirestatus`
