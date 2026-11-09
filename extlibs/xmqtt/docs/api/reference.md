# xmqtt public API reference

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xmqtt/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `90` 个函数、`197` 个常量或宏、
`62` 个公共类型。

## `extlibs/xmqtt/include/xmqtt/client.h`

[查看带契约注释的公共头](../../include/xmqtt/client.h)

### 函数 (19)

- `xmqttClientClose`
- `xmqttClientConfigInit`
- `xmqttClientConnect`
- `xmqttClientDestroy`
- `xmqttClientDisconnect`
- `xmqttClientDisconnectAsync`
- `xmqttClientInfo`
- `xmqttClientPublish`
- `xmqttClientPublishAsync`
- `xmqttClientReady`
- `xmqttClientSubscribe`
- `xmqttClientSubscribeAsync`
- `xmqttClientTakeMessage`
- `xmqttClientUnsubscribe`
- `xmqttClientUnsubscribeAsync`
- `xmqttClientWaitFor`
- `xmqttMessagePacket`
- `xmqttMessageRelease`
- `xmqttMessageRetain`

### 常量与宏 (29)

- `XMQTT_ADMISSION_ACCEPTED`
- `XMQTT_ADMISSION_CLOSED`
- `XMQTT_ADMISSION_ERROR`
- `XMQTT_ADMISSION_OVERLOADED`
- `XMQTT_CLIENT_CLOSED`
- `XMQTT_CLIENT_CLOSING`
- `XMQTT_CLIENT_CONNECTING`
- `XMQTT_CLIENT_OPEN`
- `XMQTT_FEATURE_XMQTT_CLIENT`
- `XMQTT_FEATURE_XMQTT_NEGOTIATION`
- `XMQTT_MAY_HAVE_REACHED_PEER`
- `XMQTT_NOT_SENT`
- `XMQTT_PEER_ACCEPTED`
- `XMQTT_PEER_REJECTED`
- `XMQTT_STATUS_AUTHENTICATION`
- `XMQTT_STATUS_CANCELLED`
- `XMQTT_STATUS_CLOSED`
- `XMQTT_STATUS_INITIALIZATION`
- `XMQTT_STATUS_LIMIT`
- `XMQTT_STATUS_MEMORY`
- `XMQTT_STATUS_OK`
- `XMQTT_STATUS_PROTOCOL`
- `XMQTT_STATUS_REJECTED`
- `XMQTT_STATUS_RESTRICTED`
- `XMQTT_STATUS_SERVER_DISCONNECT`
- `XMQTT_STATUS_SESSION`
- `XMQTT_STATUS_TIMEOUT`
- `XMQTT_STATUS_TRANSPORT`
- `XMQTT_STATUS_UNSUPPORTED`

### 类型 (16)

- `xmqttclient`
- `xmqttclientadmission`
- `xmqttclientconfig`
- `xmqttclientinfo`
- `xmqttclientnotify`
- `xmqttclientstate`
- `xmqttclientstatus`
- `xmqttcodecconfig`
- `xmqttdelivery`
- `xmqttmessage`
- `xmqttnegotiated`
- `xmqttpacket`
- `xmqttreadyresult`
- `xmqttsubscriptionresult`
- `xmqttversion`
- `xmqttwriteresult`

## `extlibs/xmqtt/include/xmqtt/client_auth.h`

[查看带契约注释的公共头](../../include/xmqtt/client_auth.h)

### 函数 (10)

- `xmqttAuthComplete`
- `xmqttAuthConfigInit`
- `xmqttAuthMessage`
- `xmqttAuthPhase`
- `xmqttAuthRelease`
- `xmqttAuthRespond`
- `xmqttClientConnectAuth`
- `xmqttClientReauthenticate`
- `xmqttClientReauthenticateAsync`
- `xmqttClientTakeAuth`

### 常量与宏 (4)

- `XMQTT_AUTH_CHALLENGE`
- `XMQTT_AUTH_VERIFY_CONNECT`
- `XMQTT_AUTH_VERIFY_REAUTH`
- `XMQTT_FEATURE_XMQTT_CLIENT_AUTH`

### 类型 (4)

- `xmqttauthconfig`
- `xmqttauthphase`
- `xmqttauthresult`
- `xmqttauthticket`

## `extlibs/xmqtt/include/xmqtt/client_dial.h`

[查看带契约注释的公共头](../../include/xmqtt/client_dial.h)

### 函数 (5)

- `xmqttClientDial`
- `xmqttClientDialAddresses`
- `xmqttClientDialAddressesAuth`
- `xmqttClientDialAuth`
- `xmqttClientDialConfigInit`

### 常量与宏 (3)

- `XMQTT_DIAL_ADDRESSES_MAX`
- `XMQTT_DIAL_HOST_MAX`
- `XMQTT_FEATURE_XMQTT_CLIENT_DIAL`

### 类型 (1)

- `xmqttclientdialconfig`

## `extlibs/xmqtt/include/xmqtt/client_tls.h`

[查看带契约注释的公共头](../../include/xmqtt/client_tls.h)

### 函数 (7)

- `xmqttClientTlsConfigInit`
- `xmqttClientTlsConnect`
- `xmqttClientTlsConnectAuth`
- `xmqttClientTlsDial`
- `xmqttClientTlsDialAddresses`
- `xmqttClientTlsDialAddressesAuth`
- `xmqttClientTlsDialAuth`

### 常量与宏 (1)

- `XMQTT_FEATURE_XMQTT_CLIENT_TLS`

### 类型 (1)

- `xmqttclienttlsconfig`

## `extlibs/xmqtt/include/xmqtt/client_tls_resume.h`

[查看带契约注释的公共头](../../include/xmqtt/client_tls_resume.h)

### 函数 (2)

- `xmqttClientTlsResumeInfo`
- `xmqttClientTlsTakeResume`

### 常量与宏 (1)

- `XMQTT_FEATURE_XMQTT_CLIENT_TLS_RESUME`

### 类型 (1)

- `xmqttclienttlsresumeinfo`

## `extlibs/xmqtt/include/xmqtt/codec311.h`

[查看带契约注释的公共头](../../include/xmqtt/codec311.h)

### 函数 (3)

- `xmqttPacketRead311`
- `xmqttPacketSize311`
- `xmqttPacketWrite311`

### 常量与宏 (2)

- `XMQTT_FEATURE_XMQTT_CODEC311`
- `XMQTT_FEATURE_XMQTT_PACKET`

### 类型 (4)

- `xmqttcodecbuffers`
- `xmqttcodecissue`
- `xmqttcodecstatus`
- `xmqttdirection`

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

## `extlibs/xmqtt/include/xmqtt/negotiation.h`

[查看带契约注释的公共头](../../include/xmqtt/negotiation.h)

### 函数 (3)

- `xmqttNegotiate`
- `xmqttNegotiatedPacketCheck`
- `xmqttNegotiationConfigInit`

### 常量与宏 (21)

- `XMQTT_NEGOTIATION_ARGUMENT`
- `XMQTT_NEGOTIATION_AUTH_DATA`
- `XMQTT_NEGOTIATION_AUTH_METHOD`
- `XMQTT_NEGOTIATION_CLIENT_ID`
- `XMQTT_NEGOTIATION_DONE`
- `XMQTT_NEGOTIATION_LOCAL_ERROR`
- `XMQTT_NEGOTIATION_OK`
- `XMQTT_NEGOTIATION_PACKET_SIZE`
- `XMQTT_NEGOTIATION_PROTOCOL_ERROR`
- `XMQTT_NEGOTIATION_QOS`
- `XMQTT_NEGOTIATION_REJECTED`
- `XMQTT_NEGOTIATION_RESPONSE_INFO`
- `XMQTT_NEGOTIATION_RESTRICTED`
- `XMQTT_NEGOTIATION_RETAIN`
- `XMQTT_NEGOTIATION_SESSION`
- `XMQTT_NEGOTIATION_SESSION_EXPIRY`
- `XMQTT_NEGOTIATION_SHARED`
- `XMQTT_NEGOTIATION_STATE_ERROR`
- `XMQTT_NEGOTIATION_SUBSCRIPTION_ID`
- `XMQTT_NEGOTIATION_TOPIC_ALIAS`
- `XMQTT_NEGOTIATION_WILDCARD`

### 类型 (5)

- `xmqttnegotiationconfig`
- `xmqttnegotiationerror`
- `xmqttnegotiationissue`
- `xmqttnegotiationstatus`
- `xmqttpropertyid`

## `extlibs/xmqtt/include/xmqtt/packet.h`

[查看带契约注释的公共头](../../include/xmqtt/packet.h)

### 函数 (2)

- `xmqttCodecConfigInit`
- `xmqttPacketInit`

### 常量与宏 (24)

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
- `XMQTT_CODEC_TOPIC_ALIAS`
- `XMQTT_CODEC_TOPIC_ALIAS_REPEAT`
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

### 类型 (8)

- `xmqttpropertyconfig`
- `xmqttpropertydocument`
- `xmqttpropertyerror`
- `xmqttpropertyscope`
- `xmqttpropertystatus`
- `xmqttpropertytype`
- `xmqttpropertyvalue`
- `xmqttstringpair`

## `extlibs/xmqtt/include/xmqtt/session.h`

[查看带契约注释的公共头](../../include/xmqtt/session.h)

### 函数 (21)

- `xmqttSessionAcknowledge`
- `xmqttSessionConfigInit`
- `xmqttSessionCreate`
- `xmqttSessionDestroy`
- `xmqttSessionEnd`
- `xmqttSessionExpire`
- `xmqttSessionInfo`
- `xmqttSessionNext`
- `xmqttSessionOpen`
- `xmqttSessionPacket`
- `xmqttSessionPacketInfo`
- `xmqttSessionPacketRelease`
- `xmqttSessionPacketRetain`
- `xmqttSessionPublish`
- `xmqttSessionReceiveAbort`
- `xmqttSessionReceiveCommit`
- `xmqttSessionReceiveComplete`
- `xmqttSessionReceivePrepare`
- `xmqttSessionReceiveRelease`
- `xmqttSessionReset`
- `xmqttSessionSent`

### 常量与宏 (15)

- `XMQTT_FEATURE_XMQTT_SESSION`
- `XMQTT_SESSION_COMPLETE`
- `XMQTT_SESSION_DISCARDED`
- `XMQTT_SESSION_DONE`
- `XMQTT_SESSION_DUPLICATE`
- `XMQTT_SESSION_EMPTY`
- `XMQTT_SESSION_ERROR`
- `XMQTT_SESSION_LIMIT`
- `XMQTT_SESSION_PROTOCOL_ERROR`
- `XMQTT_SESSION_PUBLISH`
- `XMQTT_SESSION_PUBREL`
- `XMQTT_SESSION_REJECTED`
- `XMQTT_SESSION_RESET`
- `XMQTT_SESSION_RESTRICTED`
- `XMQTT_SESSION_STATE_ERROR`

### 类型 (8)

- `xmqttsession`
- `xmqttsessionconfig`
- `xmqttsessionframe`
- `xmqttsessioninfo`
- `xmqttsessionpacket`
- `xmqttsessionpacketinfo`
- `xmqttsessionstage`
- `xmqttsessionstatus`

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

### 类型 (5)

- `xmqttfixedheader`
- `xmqttframe`
- `xmqttwireconfig`
- `xmqttwireerror`
- `xmqttwirestatus`
