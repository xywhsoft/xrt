# TLS 公共符号参考

此文件由 `tools/generate_api_reference.py` 从 `config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[tls.md](tls.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `265` 个函数、`241` 个常量或宏、
`101` 个公共类型。

## `include/xrt/tls.h`

[查看带契约注释的公共头](../../include/xrt/tls.h)

### 函数 (138)

- `xrtTls12CertificateRequestEncode`
- `xrtTls12CertificateRequestParse`
- `xrtTls12CertificateRequestSize`
- `xrtTls12ClientKeyExchangeEncode`
- `xrtTls12ClientKeyExchangeParse`
- `xrtTls12ClientKeyExchangeSize`
- `xrtTls12ServerKeyExchangeEncode`
- `xrtTls12ServerKeyExchangeParse`
- `xrtTls12ServerKeyExchangeSize`
- `xrtTls13CertificateRequestEncode`
- `xrtTls13CertificateRequestParse`
- `xrtTls13CertificateRequestSize`
- `xrtTls13CertificateVerifyContentEncode`
- `xrtTls13CertificateVerifyContentSize`
- `xrtTlsAlertEncode`
- `xrtTlsAlertName`
- `xrtTlsAlertParse`
- `xrtTlsAuthorities`
- `xrtTlsAuthoritiesEncode`
- `xrtTlsAuthoritiesRead`
- `xrtTlsAuthoritiesSize`
- `xrtTlsCertificateEncode`
- `xrtTlsCertificateEntries`
- `xrtTlsCertificateParse`
- `xrtTlsCertificateSize`
- `xrtTlsCertificateStatusEncode`
- `xrtTlsCertificateStatusParse`
- `xrtTlsCertificateStatusSize`
- `xrtTlsCertificateVerifyEncode`
- `xrtTlsCertificateVerifyParse`
- `xrtTlsCertificateVerifySize`
- `xrtTlsCertificatesRead`
- `xrtTlsCipherCompatible`
- `xrtTlsCipherInfo`
- `xrtTlsCipherName`
- `xrtTlsCipherSelect`
- `xrtTlsClientHelloEncode`
- `xrtTlsClientHelloParse`
- `xrtTlsClientHelloSize`
- `xrtTlsClientKeyShares`
- `xrtTlsClientPsks`
- `xrtTlsClientVersionSelect`
- `xrtTlsClientVersions`
- `xrtTlsCompressedCertificateEncode`
- `xrtTlsCompressedCertificateParse`
- `xrtTlsCompressedCertificateSize`
- `xrtTlsContextConfigInit`
- `xrtTlsContextCreate`
- `xrtTlsContextLimits`
- `xrtTlsContextPolicy`
- `xrtTlsContextRelease`
- `xrtTlsContextRetain`
- `xrtTlsEncryptedExtensionsEncode`
- `xrtTlsEncryptedExtensionsParse`
- `xrtTlsEncryptedExtensionsSize`
- `xrtTlsExtensionEncode`
- `xrtTlsExtensionName`
- `xrtTlsExtensionParse`
- `xrtTlsExtensionSize`
- `xrtTlsExtensionsFind`
- `xrtTlsExtensionsInit`
- `xrtTlsExtensionsRead`
- `xrtTlsExtensionsValidate`
- `xrtTlsFinishedEncode`
- `xrtTlsFinishedParse`
- `xrtTlsGroupAvailable`
- `xrtTlsGroupInfo`
- `xrtTlsGroups`
- `xrtTlsHandshakeEncode`
- `xrtTlsHandshakeName`
- `xrtTlsHandshakeParse`
- `xrtTlsHandshakeReaderConfigInit`
- `xrtTlsHandshakeReaderInit`
- `xrtTlsHandshakeReaderRead`
- `xrtTlsHandshakeReaderRequired`
- `xrtTlsHandshakeReaderReset`
- `xrtTlsHandshakeReaderUnit`
- `xrtTlsHandshakeSize`
- `xrtTlsHostName`
- `xrtTlsIdsContain`
- `xrtTlsIdsCount`
- `xrtTlsIdsGet`
- `xrtTlsIdsSelect`
- `xrtTlsKeyShareDerive`
- `xrtTlsKeyShareFind`
- `xrtTlsKeyShareGenerate`
- `xrtTlsKeyShareSelect`
- `xrtTlsKeySharesRead`
- `xrtTlsKeyUpdateEncode`
- `xrtTlsKeyUpdateParse`
- `xrtTlsLimitsInit`
- `xrtTlsLimitsValid`
- `xrtTlsPolicyInit`
- `xrtTlsPolicyValid`
- `xrtTlsProtocolFind`
- `xrtTlsProtocolSelect`
- `xrtTlsProtocolSelected`
- `xrtTlsProtocols`
- `xrtTlsProtocolsRead`
- `xrtTlsPskModes`
- `xrtTlsPsksRead`
- `xrtTlsRecordEncode`
- `xrtTlsRecordName`
- `xrtTlsRecordParse`
- `xrtTlsRecordSize`
- `xrtTlsRetryGroup`
- `xrtTlsServerHelloEncode`
- `xrtTlsServerHelloParse`
- `xrtTlsServerHelloSize`
- `xrtTlsServerKeyShare`
- `xrtTlsServerNames`
- `xrtTlsServerNamesRead`
- `xrtTlsServerPsk`
- `xrtTlsServerVersion`
- `xrtTlsSessionTicketEncode`
- `xrtTlsSessionTicketParse`
- `xrtTlsSessionTicketSize`
- `xrtTlsSignatureCompatible`
- `xrtTlsSignatureInfo`
- `xrtTlsSignatureSelect`
- `xrtTlsSignatures`
- `xrtTlsVersionName`
- `xrtTlsVersionSelect`
- `xrtTlsWriterClientKeyShares`
- `xrtTlsWriterClientPsks`
- `xrtTlsWriterClientVersions`
- `xrtTlsWriterData`
- `xrtTlsWriterExtension`
- `xrtTlsWriterHostName`
- `xrtTlsWriterIds`
- `xrtTlsWriterInit`
- `xrtTlsWriterProtocols`
- `xrtTlsWriterPskModes`
- `xrtTlsWriterReset`
- `xrtTlsWriterRetryGroup`
- `xrtTlsWriterServerKeyShare`
- `xrtTlsWriterServerPsk`
- `xrtTlsWriterServerVersion`

### 常量与宏 (199)

- `XTLS_AEAD_AES_GCM`
- `XTLS_AEAD_CHACHA20_POLY1305`
- `XTLS_AES_128_GCM_SHA256`
- `XTLS_AES_256_GCM_SHA384`
- `XTLS_AES_GCM_RECORD_LIMIT`
- `XTLS_AGAIN`
- `XTLS_ALERT_ACCESS_DENIED`
- `XTLS_ALERT_BAD_CERTIFICATE`
- `XTLS_ALERT_BAD_CERTIFICATE_STATUS_RESPONSE`
- `XTLS_ALERT_BAD_RECORD_MAC`
- `XTLS_ALERT_CERTIFICATE_EXPIRED`
- `XTLS_ALERT_CERTIFICATE_REQUIRED`
- `XTLS_ALERT_CERTIFICATE_REVOKED`
- `XTLS_ALERT_CERTIFICATE_UNKNOWN`
- `XTLS_ALERT_CLOSE_NOTIFY`
- `XTLS_ALERT_DECODE_ERROR`
- `XTLS_ALERT_DECRYPT_ERROR`
- `XTLS_ALERT_FATAL`
- `XTLS_ALERT_HANDSHAKE_FAILURE`
- `XTLS_ALERT_ILLEGAL_PARAMETER`
- `XTLS_ALERT_INAPPROPRIATE_FALLBACK`
- `XTLS_ALERT_INSUFFICIENT_SECURITY`
- `XTLS_ALERT_INTERNAL_ERROR`
- `XTLS_ALERT_MISSING_EXTENSION`
- `XTLS_ALERT_NO_APPLICATION_PROTOCOL`
- `XTLS_ALERT_PROTOCOL_VERSION`
- `XTLS_ALERT_RECORD_OVERFLOW`
- `XTLS_ALERT_UNEXPECTED_MESSAGE`
- `XTLS_ALERT_UNKNOWN_CA`
- `XTLS_ALERT_UNKNOWN_PSK_IDENTITY`
- `XTLS_ALERT_UNRECOGNIZED_NAME`
- `XTLS_ALERT_UNSUPPORTED_CERTIFICATE`
- `XTLS_ALERT_UNSUPPORTED_EXTENSION`
- `XTLS_ALERT_USER_CANCELED`
- `XTLS_ALERT_WARNING`
- `XTLS_CERTIFICATE_COMPRESSION_BROTLI`
- `XTLS_CERTIFICATE_COMPRESSION_ZLIB`
- `XTLS_CERTIFICATE_COMPRESSION_ZSTD`
- `XTLS_CERTIFICATE_DSS_SIGN`
- `XTLS_CERTIFICATE_ECDSA_SIGN`
- `XTLS_CERTIFICATE_RSA_SIGN`
- `XTLS_CERTIFICATE_STATUS_OCSP`
- `XTLS_CHACHA20_POLY1305_SHA256`
- `XTLS_CIPHER_AUTH_ECDSA`
- `XTLS_CIPHER_AUTH_INDEPENDENT`
- `XTLS_CIPHER_AUTH_RSA`
- `XTLS_CLIENT`
- `XTLS_CLOSED`
- `XTLS_DRIVE_HANDSHAKE_BUDGET_DEFAULT`
- `XTLS_DRIVE_RECORD_BUDGET_DEFAULT`
- `XTLS_ECDHE_ECDSA_AES_128_GCM_SHA256`
- `XTLS_ECDHE_ECDSA_AES_256_GCM_SHA384`
- `XTLS_ECDHE_ECDSA_CHACHA20_POLY1305_SHA256`
- `XTLS_ECDHE_RSA_AES_128_GCM_SHA256`
- `XTLS_ECDHE_RSA_AES_256_GCM_SHA384`
- `XTLS_ECDHE_RSA_CHACHA20_POLY1305_SHA256`
- `XTLS_ERROR`
- `XTLS_ERROR_ALERT`
- `XTLS_ERROR_ARGUMENT`
- `XTLS_ERROR_CERTIFICATE`
- `XTLS_ERROR_CIPHER`
- `XTLS_ERROR_CLOSED`
- `XTLS_ERROR_EXTENSION`
- `XTLS_ERROR_HANDSHAKE`
- `XTLS_ERROR_IDENTITY`
- `XTLS_ERROR_INTERNAL`
- `XTLS_ERROR_KEY_DERIVATION`
- `XTLS_ERROR_KEY_EXCHANGE`
- `XTLS_ERROR_LIMIT`
- `XTLS_ERROR_NEGOTIATION`
- `XTLS_ERROR_RECORD_BUFFER`
- `XTLS_ERROR_RECORD_SIZE`
- `XTLS_ERROR_RECORD_TYPE`
- `XTLS_ERROR_RECORD_VERSION`
- `XTLS_ERROR_RESUME`
- `XTLS_ERROR_STATE`
- `XTLS_ERROR_TRANSCRIPT`
- `XTLS_ERROR_TRUNCATED`
- `XTLS_ERROR_VERIFY`
- `XTLS_ERROR_VERSION`
- `XTLS_EXTENSION_ALPN`
- `XTLS_EXTENSION_CERTIFICATE_AUTHORITIES`
- `XTLS_EXTENSION_CLIENT_CERTIFICATE_TYPE`
- `XTLS_EXTENSION_COMPRESS_CERTIFICATE`
- `XTLS_EXTENSION_COOKIE`
- `XTLS_EXTENSION_DATA_MAX`
- `XTLS_EXTENSION_EARLY_DATA`
- `XTLS_EXTENSION_EC_POINT_FORMATS`
- `XTLS_EXTENSION_ENCRYPT_THEN_MAC`
- `XTLS_EXTENSION_EXTENDED_MASTER_SECRET`
- `XTLS_EXTENSION_HEADER_SIZE`
- `XTLS_EXTENSION_HEARTBEAT`
- `XTLS_EXTENSION_KEY_SHARE`
- `XTLS_EXTENSION_MAX_FRAGMENT_LENGTH`
- `XTLS_EXTENSION_OID_FILTERS`
- `XTLS_EXTENSION_PADDING`
- `XTLS_EXTENSION_POST_HANDSHAKE_AUTH`
- `XTLS_EXTENSION_PRE_SHARED_KEY`
- `XTLS_EXTENSION_PSK_KEY_EXCHANGE_MODES`
- `XTLS_EXTENSION_RECORD_SIZE_LIMIT`
- `XTLS_EXTENSION_RENEGOTIATION_INFO`
- `XTLS_EXTENSION_SERVER_CERTIFICATE_TYPE`
- `XTLS_EXTENSION_SERVER_NAME`
- `XTLS_EXTENSION_SESSION_TICKET`
- `XTLS_EXTENSION_SIGNATURE_ALGORITHMS`
- `XTLS_EXTENSION_SIGNATURE_ALGORITHMS_CERT`
- `XTLS_EXTENSION_SIGNED_CERTIFICATE_TIMESTAMP`
- `XTLS_EXTENSION_STATUS_REQUEST`
- `XTLS_EXTENSION_SUPPORTED_GROUPS`
- `XTLS_EXTENSION_SUPPORTED_VERSIONS`
- `XTLS_EXTENSION_USE_SRTP`
- `XTLS_FEED_LIMIT_DEFAULT`
- `XTLS_GROUP_FFDHE2048`
- `XTLS_GROUP_FFDHE3072`
- `XTLS_GROUP_FFDHE4096`
- `XTLS_GROUP_FFDHE6144`
- `XTLS_GROUP_FFDHE8192`
- `XTLS_GROUP_KIND_ECDH`
- `XTLS_GROUP_KIND_XDH`
- `XTLS_GROUP_SECP256R1`
- `XTLS_GROUP_SECP384R1`
- `XTLS_GROUP_SECP521R1`
- `XTLS_GROUP_X25519`
- `XTLS_GROUP_X448`
- `XTLS_HANDSHAKE_BODY_MAX`
- `XTLS_HANDSHAKE_CERTIFICATE`
- `XTLS_HANDSHAKE_CERTIFICATE_REQUEST`
- `XTLS_HANDSHAKE_CERTIFICATE_STATUS`
- `XTLS_HANDSHAKE_CERTIFICATE_VERIFY`
- `XTLS_HANDSHAKE_CLIENT_HELLO`
- `XTLS_HANDSHAKE_CLIENT_KEY_EXCHANGE`
- `XTLS_HANDSHAKE_COMPRESSED_CERTIFICATE`
- `XTLS_HANDSHAKE_ENCRYPTED_EXTENSIONS`
- `XTLS_HANDSHAKE_END_OF_EARLY_DATA`
- `XTLS_HANDSHAKE_FINISHED`
- `XTLS_HANDSHAKE_HEADER_SIZE`
- `XTLS_HANDSHAKE_HELLO_REQUEST`
- `XTLS_HANDSHAKE_KEY_UPDATE`
- `XTLS_HANDSHAKE_LIMIT_DEFAULT`
- `XTLS_HANDSHAKE_MESSAGE_HASH`
- `XTLS_HANDSHAKE_NEW_SESSION_TICKET`
- `XTLS_HANDSHAKE_RETAIN_DEFAULT`
- `XTLS_HANDSHAKE_SERVER_HELLO`
- `XTLS_HANDSHAKE_SERVER_HELLO_DONE`
- `XTLS_HANDSHAKE_SERVER_KEY_EXCHANGE`
- `XTLS_HANDSHAKE_SUPPLEMENTAL_DATA`
- `XTLS_HASH_SHA256`
- `XTLS_HASH_SHA384`
- `XTLS_IDENTITY_ECDSA_P256`
- `XTLS_IDENTITY_ECDSA_P384`
- `XTLS_IDENTITY_ECDSA_P521`
- `XTLS_IDENTITY_ED25519`
- `XTLS_IDENTITY_ED448`
- `XTLS_IDENTITY_NONE`
- `XTLS_IDENTITY_RSA`
- `XTLS_IDENTITY_RSA_PSS`
- `XTLS_ITEM_DONE`
- `XTLS_ITEM_ERROR`
- `XTLS_ITEM_VALUE`
- `XTLS_KEY_SHARE_PREFER_GROUP`
- `XTLS_KEY_SHARE_PREFER_READY`
- `XTLS_KEY_UPDATE_NOT_REQUESTED`
- `XTLS_KEY_UPDATE_REQUESTED`
- `XTLS_OK`
- `XTLS_PLAIN_LIMIT_DEFAULT`
- `XTLS_PSK_DHE_KE`
- `XTLS_PSK_KE`
- `XTLS_RANDOM_SIZE`
- `XTLS_RECORD_ALERT`
- `XTLS_RECORD_APPLICATION_DATA`
- `XTLS_RECORD_CHANGE_CIPHER_SPEC`
- `XTLS_RECORD_HANDSHAKE`
- `XTLS_RECORD_HEADER_SIZE`
- `XTLS_RECORD_PLAINTEXT_MAX`
- `XTLS_SEND_LIMIT_DEFAULT`
- `XTLS_SERVER`
- `XTLS_SESSION_ID_MAX`
- `XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256`
- `XTLS_SIGNATURE_ECDSA_SECP384R1_SHA384`
- `XTLS_SIGNATURE_ECDSA_SECP521R1_SHA512`
- `XTLS_SIGNATURE_ED25519`
- `XTLS_SIGNATURE_ED448`
- `XTLS_SIGNATURE_RSA_PKCS1_SHA256`
- `XTLS_SIGNATURE_RSA_PKCS1_SHA384`
- `XTLS_SIGNATURE_RSA_PKCS1_SHA512`
- `XTLS_SIGNATURE_RSA_PSS_PSS_SHA256`
- `XTLS_SIGNATURE_RSA_PSS_PSS_SHA384`
- `XTLS_SIGNATURE_RSA_PSS_PSS_SHA512`
- `XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256`
- `XTLS_SIGNATURE_RSA_PSS_RSAE_SHA384`
- `XTLS_SIGNATURE_RSA_PSS_RSAE_SHA512`
- `XTLS_STATE_CLOSED`
- `XTLS_STATE_CLOSING`
- `XTLS_STATE_FAILED`
- `XTLS_STATE_HANDSHAKE`
- `XTLS_STATE_NEW`
- `XTLS_STATE_READY`
- `XTLS_VERSION_12`
- `XTLS_VERSION_13`

### 类型 (61)

- `xtls12certificaterequest`
- `xtls12serverkeyexchange`
- `xtls13certificaterequest`
- `xtlsaead`
- `xtlsalert`
- `xtlsalertlevel`
- `xtlsauthoritycursor`
- `xtlscertificatecompression`
- `xtlscertificatecursor`
- `xtlscertificateentry`
- `xtlscertificatemessage`
- `xtlscertificatestatusmessage`
- `xtlscertificatestatustype`
- `xtlscertificatetype`
- `xtlscertificateverify`
- `xtlscipher`
- `xtlscipherauth`
- `xtlscipherinfo`
- `xtlsclienthello`
- `xtlscompressedcertificate`
- `xtlscontext`
- `xtlscontextconfig`
- `xtlserror`
- `xtlsextension`
- `xtlsextensioncursor`
- `xtlsextensiontype`
- `xtlsgroupinfo`
- `xtlsgroupkind`
- `xtlshandshake`
- `xtlshandshakereader`
- `xtlshandshakereaderconfig`
- `xtlshandshaketype`
- `xtlshash`
- `xtlsidentitytype`
- `xtlsids`
- `xtlsitemresult`
- `xtlskeyshare`
- `xtlskeysharecursor`
- `xtlskeysharepolicy`
- `xtlskeyshareselection`
- `xtlskeyupdate`
- `xtlslimits`
- `xtlsnamedgroup`
- `xtlspolicy`
- `xtlsprotocolcursor`
- `xtlspsk`
- `xtlspskcursor`
- `xtlspskmode`
- `xtlsrecord`
- `xtlsrecordtype`
- `xtlsresult`
- `xtlsrole`
- `xtlsserverhello`
- `xtlsservername`
- `xtlsservernamecursor`
- `xtlssessionticket`
- `xtlssignature`
- `xtlssignatureinfo`
- `xtlsstate`
- `xtlsversion`
- `xtlswriter`

## `include/xrt/tls_client.h`

[查看带契约注释的公共头](../../include/xrt/tls_client.h)

### 函数 (10)

- `xrtTlsClientCertificate`
- `xrtTlsClientCertificateCount`
- `xrtTlsClientConfigInit`
- `xrtTlsClientCreate`
- `xrtTlsClientDrive`
- `xrtTlsClientKeyUpdate`
- `xrtTlsClientResumeCount`
- `xrtTlsClientResumeDropped`
- `xrtTlsClientResumed`
- `xrtTlsClientTakeResume`

### 常量与宏 (2)

- `XTLS_CLIENT_RESUME_LIMIT_DEFAULT`
- `XTLS_CLIENT_RESUME_LIMIT_MAX`

### 类型 (4)

- `xtlsclientconfig`
- `xtlsresume`
- `xtlssession`
- `xtlsverifier`

## `include/xrt/tls_identity.h`

[查看带契约注释的公共头](../../include/xrt/tls_identity.h)

### 函数 (13)

- `xrtTlsIdentityCanSign`
- `xrtTlsIdentityCertificate`
- `xrtTlsIdentityCertificateCount`
- `xrtTlsIdentityCreate`
- `xrtTlsIdentityEd25519`
- `xrtTlsIdentityP256`
- `xrtTlsIdentityP384`
- `xrtTlsIdentityPublicKey`
- `xrtTlsIdentityRelease`
- `xrtTlsIdentityRetain`
- `xrtTlsIdentityRsa`
- `xrtTlsIdentitySign`
- `xrtTlsIdentityType`

### 类型 (5)

- `xtlsidentity`
- `xtlsidentityconfig`
- `xtlsidentityreleaseproc`
- `xtlsidentitysignproc`
- `xtlsidentitysupportsproc`

## `include/xrt/tls_resume.h`

[查看带契约注释的公共头](../../include/xrt/tls_resume.h)

### 函数 (7)

- `xrtTlsResumeConfigInit`
- `xrtTlsResumeCreate`
- `xrtTlsResumeInfo`
- `xrtTlsResumeRelease`
- `xrtTlsResumeRetain`
- `xrtTlsResumeTicketAge`
- `xrtTlsResumeValidAt`

### 类型 (2)

- `xtlsresumeconfig`
- `xtlsresumeinfo`

## `include/xrt/tls_server.h`

[查看带契约注释的公共头](../../include/xrt/tls_server.h)

### 函数 (8)

- `xrtTlsServerConfigInit`
- `xrtTlsServerCreate`
- `xrtTlsServerDrive`
- `xrtTlsServerKeyUpdate`
- `xrtTlsServerName`
- `xrtTlsServerResumed`
- `xrtTlsServerTicket`
- `xrtTlsServerTicketNew`

### 常量与宏 (4)

- `XTLS_SERVER_PROTOCOL_NONE`
- `XTLS_SERVER_RESUME_AGE_TOLERANCE_DEFAULT`
- `XTLS_SERVER_TICKET_LIFETIME_DEFAULT`
- `XTLS_SERVER_TICKET_SIZE_DEFAULT`

### 类型 (6)

- `xtlsserverchoice`
- `xtlsserverconfig`
- `xtlsserverrequest`
- `xtlsserverresumeproc`
- `xtlsserverresumerequest`
- `xtlsserverselectproc`

## `include/xrt/tls_session.h`

[查看带契约注释的公共头](../../include/xrt/tls_session.h)

### 函数 (29)

- `xrtTlsSessionCipher`
- `xrtTlsSessionClose`
- `xrtTlsSessionContext`
- `xrtTlsSessionDestroy`
- `xrtTlsSessionEof`
- `xrtTlsSessionFeed`
- `xrtTlsSessionFeedBorrow`
- `xrtTlsSessionFeedBuffer`
- `xrtTlsSessionFeedRef`
- `xrtTlsSessionFeedSize`
- `xrtTlsSessionFeedTake`
- `xrtTlsSessionPeerAlert`
- `xrtTlsSessionPlainConsume`
- `xrtTlsSessionPlainFront`
- `xrtTlsSessionPlainSize`
- `xrtTlsSessionPlainSpanCount`
- `xrtTlsSessionPlainSpans`
- `xrtTlsSessionProtocol`
- `xrtTlsSessionRead`
- `xrtTlsSessionRole`
- `xrtTlsSessionSendConsume`
- `xrtTlsSessionSendFront`
- `xrtTlsSessionSendSize`
- `xrtTlsSessionSendSpanCount`
- `xrtTlsSessionSendSpans`
- `xrtTlsSessionState`
- `xrtTlsSessionVersion`
- `xrtTlsSessionWait`
- `xrtTlsSessionWrite`

### 常量与宏 (6)

- `XTLS_WAIT_APPLICATION`
- `XTLS_WAIT_IDENTITY`
- `XTLS_WAIT_INPUT`
- `XTLS_WAIT_NONE`
- `XTLS_WAIT_OUTPUT`
- `XTLS_WAIT_VERIFY`

### 类型 (1)

- `xtlswait`

## `include/xrt/tls_stream.h`

[查看带契约注释的公共头](../../include/xrt/tls_stream.h)

### 函数 (52)

- `xrtTlsDial`
- `xrtTlsDialAsync`
- `xrtTlsDialCancel`
- `xrtTlsDialConfigInit`
- `xrtTlsDialDestroy`
- `xrtTlsDialError`
- `xrtTlsDialRef`
- `xrtTlsDialState`
- `xrtTlsDialTransportStats`
- `xrtTlsListenerAccept`
- `xrtTlsListenerAcceptAsync`
- `xrtTlsListenerAcceptWait`
- `xrtTlsListenerClose`
- `xrtTlsListenerConfigInit`
- `xrtTlsListenerData`
- `xrtTlsListenerDestroy`
- `xrtTlsListenerLocal`
- `xrtTlsListenerRef`
- `xrtTlsListenerStart`
- `xrtTlsListenerState`
- `xrtTlsListenerStats`
- `xrtTlsStreamAbort`
- `xrtTlsStreamAccept`
- `xrtTlsStreamAsyncBytes`
- `xrtTlsStreamAsyncCount`
- `xrtTlsStreamAttach`
- `xrtTlsStreamAvailable`
- `xrtTlsStreamBuffer`
- `xrtTlsStreamClient`
- `xrtTlsStreamClose`
- `xrtTlsStreamConfigInit`
- `xrtTlsStreamConnect`
- `xrtTlsStreamConsume`
- `xrtTlsStreamData`
- `xrtTlsStreamDestroy`
- `xrtTlsStreamError`
- `xrtTlsStreamPending`
- `xrtTlsStreamPullup`
- `xrtTlsStreamRead`
- `xrtTlsStreamReadMore`
- `xrtTlsStreamRecvAsync`
- `xrtTlsStreamRef`
- `xrtTlsStreamSend`
- `xrtTlsStreamSendAsync`
- `xrtTlsStreamSendBound`
- `xrtTlsStreamSendVec`
- `xrtTlsStreamSendVecAsync`
- `xrtTlsStreamSession`
- `xrtTlsStreamSetEvents`
- `xrtTlsStreamState`
- `xrtTlsStreamTransport`
- `xrtTlsStreamWaitAsync`

### 常量与宏 (26)

- `XTLS_DIAL_CANCELLED`
- `XTLS_DIAL_CONNECTED`
- `XTLS_DIAL_CONNECTING`
- `XTLS_DIAL_FAILED`
- `XTLS_DIAL_HANDSHAKE`
- `XTLS_DIAL_RESOLVING`
- `XTLS_LISTENER_CLOSED`
- `XTLS_LISTENER_CLOSING`
- `XTLS_LISTENER_OPEN`
- `XTLS_STREAM_ASYNC_BATCH_DEFAULT`
- `XTLS_STREAM_ASYNC_BYTES_DEFAULT`
- `XTLS_STREAM_ASYNC_COUNT_DEFAULT`
- `XTLS_STREAM_CLOSED`
- `XTLS_STREAM_CLOSE_TIMEOUT_DEFAULT`
- `XTLS_STREAM_CLOSING`
- `XTLS_STREAM_CONNECTING`
- `XTLS_STREAM_FAILED`
- `XTLS_STREAM_HANDSHAKE`
- `XTLS_STREAM_HANDSHAKE_TIMEOUT_DEFAULT`
- `XTLS_STREAM_OPEN`
- `XTLS_STREAM_WAIT_CLOSE`
- `XTLS_STREAM_WAIT_DRAIN`
- `XTLS_STREAM_WAIT_END`
- `XTLS_STREAM_WAIT_OPEN`
- `XTLS_STREAM_WAIT_READ`
- `XTLS_STREAM_WAIT_WRITE`

### 类型 (14)

- `xtlsdial`
- `xtlsdialconfig`
- `xtlsdialproc`
- `xtlsdialstate`
- `xtlslistener`
- `xtlslistenerconfig`
- `xtlslistenerevents`
- `xtlslistenerstate`
- `xtlslistenerstats`
- `xtlsstream`
- `xtlsstreamconfig`
- `xtlsstreamevents`
- `xtlsstreamstate`
- `xtlsstreamwait`

## `include/xrt/tls_verify.h`

[查看带契约注释的公共头](../../include/xrt/tls_verify.h)

### 函数 (8)

- `xrtTls12ServerKeyExchangeVerify`
- `xrtTls13CertificateVerifySignature`
- `xrtTlsPeerVerify`
- `xrtTlsVerifierConfigInit`
- `xrtTlsVerifierCreate`
- `xrtTlsVerifierRelease`
- `xrtTlsVerifierRetain`
- `xrtTlsVerifierVerify`

### 常量与宏 (4)

- `XTLS_VERIFY_ACCEPT`
- `XTLS_VERIFY_DEFAULT`
- `XTLS_VERIFY_ERROR`
- `XTLS_VERIFY_REJECT`

### 类型 (8)

- `xtlspeer`
- `xtlsverifiedpeer`
- `xtlsverifierconfig`
- `xtlsverifydecision`
- `xtlsverifypolicyproc`
- `xtlsverifyproc`
- `xtlsverifyreleaseproc`
- `xtlsverifytimeproc`
